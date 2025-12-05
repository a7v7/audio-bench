//------------------------------------------------------------------------------
// MIT License
//
// Copyright (c) 2025 Anthony Verbeck
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//------------------------------------------------------------------------------

/*
 * ab_asio_loopback.cpp
 * ASIO Audio Loopback Tool for audio-bench
 *
 * Windows-only ASIO interface for professional audio hardware
 * Plays audio from a WAV file through ASIO output while simultaneously
 * recording ASIO input to a new WAV file
 */

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstring>
#include <sndfile.h>
#include <popt.h>
#include "asiosys.h"
#include "asio.h"
#include "iasiodrv.h"
#include "asiodrivers.h"

//------------------------------------------------------------------------------
// Global ASIO state
//------------------------------------------------------------------------------
static IASIO* asioDriver = nullptr;
static ASIODriverInfo driverInfo;
static ASIOBufferInfo bufferInfos[2];                       // Input and output channels
static ASIOCallbacks asioCallbacks;
static long numInputChannels = 0;
static long numOutputChannels = 0;
static long preferredBufferSize = 0;
static long minBufferSize = 0;
static long maxBufferSize = 0;
static long bufferGranularity = 0;
static ASIOSampleRate currentSampleRate = 48000.0;
static bool loopbackActive = false;

//------------------------------------------------------------------------------
// Cached channel info (retrieved once during setup)
//------------------------------------------------------------------------------
static ASIOChannelInfo inputChannelInfo;
static ASIOChannelInfo outputChannelInfo;

//------------------------------------------------------------------------------
// Cached sample sizes (calculated once during setup)
//------------------------------------------------------------------------------
static size_t outputSampleSize = 0;

//------------------------------------------------------------------------------
// Pre-allocated conversion buffers (for real-time callback)
//------------------------------------------------------------------------------
static float* tempInBuffer = nullptr;

//------------------------------------------------------------------------------
// Pre-converted playback signal in ASIO output format
//------------------------------------------------------------------------------
static void* playbackSignalASIO = nullptr;
static size_t playbackSignalASIOSize = 0;

//------------------------------------------------------------------------------
// Audio data structure
//------------------------------------------------------------------------------
typedef struct {
    float *playback_signal;                                 // Loaded from WAV file
    float *recorded_signal;                                 // Recorded response
    int total_frames;                                       // Total length (samples)
    int current_frame;                                      // Current playback/record position
} AudioData;

static AudioData audioData;

//------------------------------------------------------------------------------
// Output file parameters
//------------------------------------------------------------------------------
static SNDFILE* outputFile = nullptr;
static SF_INFO outputFileInfo;
static long outputBitDepth = 32;

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------
static void bufferSwitch(long index, ASIOBool processNow);
static void sampleRateChanged(ASIOSampleRate sRate);
static long asioMessages(long selector, long value, void* message, double* opt);
static ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long index, ASIOBool processNow);

//------------------------------------------------------------------------------
//  Name:       convertASIOToFloat
//
//  Returns:    none
//
//------------------------------------------------------------------------------
//  Detailed description:
//  - Converts ASIO sample format to float
//  - Handles various ASIO sample types
//  - Normalizes to -1.0 to +1.0 range
//------------------------------------------------------------------------------
static void convertASIOToFloat(void* asioBuffer, float* floatBuffer, long numSamples, ASIOSampleType sampleType)
{
    switch (sampleType) {
        case ASIOSTInt16LSB:
        {
            short* samples = static_cast<short*>(asioBuffer);
            for (long i = 0; i < numSamples; i++) {
                floatBuffer[i] = samples[i] / 32768.0f;
            }
            break;
        }
        case ASIOSTInt24LSB:
        {
            unsigned char* samples = static_cast<unsigned char*>(asioBuffer);
            for (long i = 0; i < numSamples; i++) {
                // Reconstruct 24-bit signed integer from 3 bytes (LSB first)
                int s24 = samples[i*3] | (samples[i*3+1] << 8) | (samples[i*3+2] << 16);
                // Sign extend from 24-bit to 32-bit
                if (s24 & 0x800000) {
                    s24 |= 0xFF000000;
                }
                floatBuffer[i] = s24 / 8388608.0f;  // 2^23
            }
            break;
        }
        case ASIOSTInt32LSB:
        {
            int* samples = static_cast<int*>(asioBuffer);
            for (long i = 0; i < numSamples; i++) {
                floatBuffer[i] = samples[i] / 2147483648.0f;
            }
            break;
        }
        case ASIOSTFloat32LSB:
        {
            float* samples = static_cast<float*>(asioBuffer);
            memcpy(floatBuffer, samples, numSamples * sizeof(float));
            break;
        }
        case ASIOSTFloat64LSB:
        {
            double* samples = static_cast<double*>(asioBuffer);
            for (long i = 0; i < numSamples; i++) {
                floatBuffer[i] = static_cast<float>(samples[i]);
            }
            break;
        }
        default:
            memset(floatBuffer, 0, numSamples * sizeof(float));
            break;
    }
}

//------------------------------------------------------------------------------
//  Name:       convertFloatToASIO
//
//  Returns:    none
//
//------------------------------------------------------------------------------
//  Detailed description:
//  - Converts float samples to ASIO sample format
//  - Handles various ASIO sample types
//  - Assumes input is normalized -1.0 to +1.0
//------------------------------------------------------------------------------
static void convertFloatToASIO(float* floatBuffer, void* asioBuffer, long numSamples, ASIOSampleType sampleType)
{
    switch (sampleType) {
        case ASIOSTInt16LSB:
        {
            short* samples = static_cast<short*>(asioBuffer);
            for (long i = 0; i < numSamples; i++) {
                float sample = floatBuffer[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                samples[i] = static_cast<short>(sample * 32767.0f);
            }
            break;
        }
        case ASIOSTInt24LSB:
        {
            unsigned char* samples = static_cast<unsigned char*>(asioBuffer);
            for (long i = 0; i < numSamples; i++) {
                float sample = floatBuffer[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                int s24 = static_cast<int>(sample * 8388607.0f);  // 2^23 - 1
                // Pack as 24-bit LSB (3 bytes, LSB first)
                samples[i*3] = s24 & 0xFF;
                samples[i*3+1] = (s24 >> 8) & 0xFF;
                samples[i*3+2] = (s24 >> 16) & 0xFF;
            }
            break;
        }
        case ASIOSTInt32LSB:
        {
            int* samples = static_cast<int*>(asioBuffer);
            for (long i = 0; i < numSamples; i++) {
                float sample = floatBuffer[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                samples[i] = static_cast<int>(sample * 2147483647.0f);
            }
            break;
        }
        case ASIOSTFloat32LSB:
        {
            float* samples = static_cast<float*>(asioBuffer);
            memcpy(samples, floatBuffer, numSamples * sizeof(float));
            break;
        }
        case ASIOSTFloat64LSB:
        {
            double* samples = static_cast<double*>(asioBuffer);
            for (long i = 0; i < numSamples; i++) {
                samples[i] = static_cast<double>(floatBuffer[i]);
            }
            break;
        }
        default:
            memset(asioBuffer, 0, numSamples * sizeof(float));
            break;
    }
}

//------------------------------------------------------------------------------
// ASIO Callbacks
//------------------------------------------------------------------------------

static void bufferSwitch(long index, ASIOBool processNow)
{
    bufferSwitchTimeInfo(nullptr, index, processNow);
}

static ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long index, ASIOBool processNow)
{
    if (!loopbackActive || !tempInBuffer || !playbackSignalASIO || !outputFile) {
        return nullptr;
    }

    long bufferSize = preferredBufferSize;
    long samplesToProcess = bufferSize;

    if (audioData.current_frame + samplesToProcess > audioData.total_frames) {
        samplesToProcess = audioData.total_frames - audioData.current_frame;
    }

    // Prepare output buffer (playback from pre-converted ASIO format)
    if (audioData.current_frame < audioData.total_frames && samplesToProcess > 0) {
        // Direct copy of pre-converted playback signal to ASIO output buffer
        size_t offsetBytes = audioData.current_frame * outputSampleSize;
        memcpy(bufferInfos[1].buffers[index],
               (char*)playbackSignalASIO + offsetBytes,
               samplesToProcess * outputSampleSize);

        // Zero out remaining samples if needed
        if (samplesToProcess < bufferSize) {
            size_t silenceOffset = samplesToProcess * outputSampleSize;
            size_t silenceSize = (bufferSize - samplesToProcess) * outputSampleSize;
            memset((char*)bufferInfos[1].buffers[index] + silenceOffset, 0, silenceSize);
        }

        audioData.current_frame += samplesToProcess;
    } else {
        // Playback complete, output silence
        memset(bufferInfos[1].buffers[index], 0, bufferSize * outputSampleSize);
        loopbackActive = false;
    }

    // Convert input from ASIO format to float
    convertASIOToFloat(bufferInfos[0].buffers[index], tempInBuffer, bufferSize, inputChannelInfo.type);

    // Write recorded input to WAV file based on bit depth
    if (audioData.current_frame <= audioData.total_frames && audioData.current_frame > 0) {
        long copyFrame = audioData.current_frame - samplesToProcess;
        if (copyFrame >= 0 && copyFrame + samplesToProcess <= audioData.total_frames) {
            // Write to output file based on bit depth
            if (outputBitDepth == 16) {
                // Convert float to 16-bit signed integer
                short* shortBuffer = new short[samplesToProcess];
                for (long i = 0; i < samplesToProcess; i++) {
                    float sample = tempInBuffer[i];
                    if (sample > 1.0f) sample = 1.0f;
                    if (sample < -1.0f) sample = -1.0f;
                    shortBuffer[i] = static_cast<short>(sample * 32767.0f);
                }
                sf_write_short(outputFile, shortBuffer, samplesToProcess);
                delete[] shortBuffer;
            } else if (outputBitDepth == 24) {
                // Convert float to 32-bit integer (libsndfile scales to 24-bit internally)
                int* intBuffer = new int[samplesToProcess];
                for (long i = 0; i < samplesToProcess; i++) {
                    // Clamp to [-1.0, 1.0] and convert to full 32-bit range
                    // libsndfile will scale this down to 24-bit when writing
                    float sample = tempInBuffer[i];
                    if (sample > 1.0f) sample = 1.0f;
                    if (sample < -1.0f) sample = -1.0f;
                    intBuffer[i] = static_cast<int>(sample * 2147483647.0f);  // 2^31 - 1
                }
                sf_write_int(outputFile, intBuffer, samplesToProcess);
                delete[] intBuffer;
            } else {  // 32-bit float
                sf_write_float(outputFile, tempInBuffer, samplesToProcess);
            }
        }
    }

    return nullptr;
}

static void sampleRateChanged(ASIOSampleRate sRate)
{
    currentSampleRate = sRate;
    printf("Sample rate changed to: %.0f Hz\n", sRate);
}

static long asioMessages(long selector, long value, void* message, double* opt)
{
    switch (selector) {
        case kAsioSelectorSupported:
            if (value == kAsioResetRequest ||
                value == kAsioEngineVersion ||
                value == kAsioResyncRequest ||
                value == kAsioLatenciesChanged ||
                value == kAsioSupportsTimeInfo ||
                value == kAsioSupportsTimeCode ||
                value == kAsioSupportsInputMonitor)
                return 1;
            break;

        case kAsioResetRequest:
            printf("ASIO: Reset request\n");
            return 1;

        case kAsioResyncRequest:
            return 1;

        case kAsioLatenciesChanged:
            printf("ASIO: Latencies changed\n");
            return 1;

        case kAsioEngineVersion:
            return 2;

        case kAsioSupportsTimeInfo:
            return 1;

        case kAsioSupportsTimeCode:
            return 0;
    }
    return 0;
}

//------------------------------------------------------------------------------
// ASIO Driver Management
//------------------------------------------------------------------------------

static bool initASIO(const char* driverName)
{
    AsioDrivers* asioDrivers = new AsioDrivers();

    // Load the driver
    if (!asioDrivers->loadDriver(const_cast<char*>(driverName))) {
        printf("Failed to load ASIO driver: %s\n", driverName);
        delete asioDrivers;
        return false;
    }

    // Initialize the driver
    ASIOError err = ASIOInit(&driverInfo);
    if (err != ASE_OK) {
        printf("ASIOInit failed with error: %ld\n", err);
        asioDrivers->removeCurrentDriver();
        delete asioDrivers;
        return false;
    }

    printf("ASIO Driver: %s\n", driverInfo.name);
    printf("Version: %ld\n", driverInfo.asioVersion);
    printf("Driver Version: 0x%08lx\n", driverInfo.driverVersion);

    // Get channels
    err = ASIOGetChannels(&numInputChannels, &numOutputChannels);
    if (err != ASE_OK) {
        printf("ASIOGetChannels failed\n");
        ASIOExit();
        delete asioDrivers;
        return false;
    }

    printf("Input channels: %ld\n", numInputChannels);
    printf("Output channels: %ld\n", numOutputChannels);

    // Get buffer size range
    err = ASIOGetBufferSize(&minBufferSize, &maxBufferSize, &preferredBufferSize, &bufferGranularity);
    if (err != ASE_OK) {
        printf("ASIOGetBufferSize failed\n");
        ASIOExit();
        delete asioDrivers;
        return false;
    }

    printf("Buffer size range: min=%ld, max=%ld, preferred=%ld, granularity=%ld\n",
           minBufferSize, maxBufferSize, preferredBufferSize, bufferGranularity);

    // Get sample rate
    err = ASIOGetSampleRate(&currentSampleRate);
    if (err != ASE_OK) {
        printf("ASIOGetSampleRate failed\n");
    } else {
        printf("Current sample rate: %.0f Hz\n", currentSampleRate);
    }

    return true;
}

static bool setupASIOBuffers(long inputChannel, long outputChannel, double requestedSampleRate)
{
    // Set sample rate if requested
    if (requestedSampleRate > 0) {
        ASIOError err = ASIOSetSampleRate(requestedSampleRate);
        if (err != ASE_OK) {
            printf("Warning: Could not set sample rate to %.0f Hz\n", requestedSampleRate);
        } else {
            currentSampleRate = requestedSampleRate;
            printf("Sample rate set to: %.0f Hz\n", currentSampleRate);
        }
    }

    // Setup callbacks
    asioCallbacks.bufferSwitch = bufferSwitch;
    asioCallbacks.sampleRateDidChange = sampleRateChanged;
    asioCallbacks.asioMessage = asioMessages;
    asioCallbacks.bufferSwitchTimeInfo = bufferSwitchTimeInfo;

    // Create buffer info for input and output channels
    memset(bufferInfos, 0, sizeof(bufferInfos));

    // Input buffer
    bufferInfos[0].isInput = ASIOTrue;
    bufferInfos[0].channelNum = inputChannel;
    bufferInfos[0].buffers[0] = bufferInfos[0].buffers[1] = nullptr;

    // Output buffer
    bufferInfos[1].isInput = ASIOFalse;
    bufferInfos[1].channelNum = outputChannel;
    bufferInfos[1].buffers[0] = bufferInfos[1].buffers[1] = nullptr;

    // Create buffers
    ASIOError err = ASIOCreateBuffers(bufferInfos, 2, preferredBufferSize, &asioCallbacks);
    if (err != ASE_OK) {
        printf("ASIOCreateBuffers failed with error: %ld\n", err);
        return false;
    }

    // Get and cache channel info
    inputChannelInfo.channel = inputChannel;
    inputChannelInfo.isInput = ASIOTrue;
    err = ASIOGetChannelInfo(&inputChannelInfo);
    if (err == ASE_OK) {
        printf("Input Channel %ld: %s, Type: %ld\n",
               inputChannel, inputChannelInfo.name, inputChannelInfo.type);
    } else {
        printf("Failed to get input channel info\n");
        return false;
    }

    outputChannelInfo.channel = outputChannel;
    outputChannelInfo.isInput = ASIOFalse;
    err = ASIOGetChannelInfo(&outputChannelInfo);
    if (err == ASE_OK) {
        printf("Output Channel %ld: %s, Type: %ld\n",
               outputChannel, outputChannelInfo.name, outputChannelInfo.type);
    } else {
        printf("Failed to get output channel info\n");
        return false;
    }

    // Allocate temporary input conversion buffer
    tempInBuffer = new float[preferredBufferSize];

    if (!tempInBuffer) {
        printf("Failed to allocate input conversion buffer\n");
        return false;
    }

    printf("Pre-allocated input conversion buffer: %ld samples\n", preferredBufferSize);

    return true;
}

static void shutdownASIO()
{
    if (asioDriver) {
        ASIOStop();
        ASIODisposeBuffers();
        ASIOExit();
        asioDriver = nullptr;
    }

    // Clean up pre-allocated conversion buffer
    if (tempInBuffer) {
        delete[] tempInBuffer;
        tempInBuffer = nullptr;
    }

    // Clean up pre-converted playback signal
    if (playbackSignalASIO) {
        free(playbackSignalASIO);
        playbackSignalASIO = nullptr;
        playbackSignalASIOSize = 0;
    }
}

//------------------------------------------------------------------------------
// Enumeration
//------------------------------------------------------------------------------

static void listASIODrivers()
{
    AsioDrivers asioDrivers;
    char* driverNames[32];
    char driverNameBuffer[32][256];

    for (int i = 0; i < 32; i++) {
        driverNames[i] = driverNameBuffer[i];
    }

    long numDrivers = asioDrivers.getDriverNames(driverNames, 32);

    printf("Available ASIO Drivers (%ld):\n", numDrivers);
    printf("----------------------------------------\n");

    for (long i = 0; i < numDrivers; i++) {
        printf("%2ld: %s\n", i, driverNames[i]);
    }

    printf("\n");
}

//------------------------------------------------------------------------------
//  Name:       preconvertPlaybackSignal
//
//  Returns:    true on success, false on failure
//
//------------------------------------------------------------------------------
//  Detailed description:
//  - Pre-converts the playback signal from float to ASIO output format
//  - Called after loading WAV file but before starting audio stream
//  - Eliminates conversion overhead in real-time callback
//------------------------------------------------------------------------------
static bool preconvertPlaybackSignal()
{
    if (!audioData.playback_signal || audioData.total_frames <= 0) {
        printf("Error: Playback signal not initialized\n");
        return false;
    }

    // Calculate and cache sample size for output format
    switch (outputChannelInfo.type) {
        case ASIOSTInt16LSB: outputSampleSize = 2; break;
        case ASIOSTInt24LSB: outputSampleSize = 3; break;
        case ASIOSTInt32LSB: outputSampleSize = 4; break;
        case ASIOSTFloat32LSB: outputSampleSize = 4; break;
        case ASIOSTFloat64LSB: outputSampleSize = 8; break;
        default:
            printf("Error: Unsupported output sample type: %ld\n", outputChannelInfo.type);
            return false;
    }

    // Allocate buffer for pre-converted playback signal
    playbackSignalASIOSize = audioData.total_frames * outputSampleSize;
    playbackSignalASIO = malloc(playbackSignalASIOSize);

    if (!playbackSignalASIO) {
        printf("Error: Failed to allocate ASIO playback signal buffer (%zu bytes)\n", playbackSignalASIOSize);
        return false;
    }

    // Convert entire playback signal to ASIO format
    convertFloatToASIO(audioData.playback_signal, playbackSignalASIO, audioData.total_frames, outputChannelInfo.type);

    printf("Pre-converted playback signal to ASIO format: %d samples, %zu bytes (sample size: %zu)\n",
           audioData.total_frames, playbackSignalASIOSize, outputSampleSize);

    return true;
}

//------------------------------------------------------------------------------
//  Main application
//
//  This application:
//  - Loads audio from a WAV file
//  - Plays audio through ASIO audio output
//  - Records ASIO audio input simultaneously
//  - Saves recorded audio to WAV file
//
//  Libraries:
//  - ASIO SDK: Audio playback and recording
//  - libsndfile: WAV file I/O
//  - libpopt: Command-line parsing
//------------------------------------------------------------------------------
int main(int argc, const char **argv)
{
    CoInitialize(nullptr);

//------------------------------------------------------------------------------
//  Command-line options
//------------------------------------------------------------------------------
    int version_flag = 0;
    int list_flag = 0;
    int about_flag = 0;
    char* driverName = nullptr;
    char* inputFilename = nullptr;
    char* outputFilename = nullptr;
    long inputChannel = 0;
    long outputChannel = 0;
    double requestedSampleRate = 0.0;
    long bitDepth = 32;

    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0, "Show version information", nullptr},
        {"about", 'a', POPT_ARG_NONE, &about_flag, 0, "Show about information", nullptr},
        {"list", 'l', POPT_ARG_NONE, &list_flag, 0, "List available ASIO drivers", nullptr},
        {"driver", 'd', POPT_ARG_STRING, &driverName, 0, "ASIO driver name", "NAME"},
        {"play", 'p', POPT_ARG_STRING, &inputFilename, 0, "Input WAV file to play", "FILE"},
        {"capture", 'o', POPT_ARG_STRING, &outputFilename, 0, "Output WAV file to record", "FILE"},
        {"inchan", 'i', POPT_ARG_LONG, &inputChannel, 0, "Input channel (default: 0)", "N"},
        {"outchan", 'C', POPT_ARG_LONG, &outputChannel, 0, "Output channel (default: 0)", "N"},
        {"rate", 'r', POPT_ARG_DOUBLE, &requestedSampleRate, 0, "Sample rate (default: use input file rate)", "HZ"},
        {"bits", 'b', POPT_ARG_LONG, &bitDepth, 0, "Output bit depth: 16, 24, or 32 (default: 32)", "BITS"},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(nullptr, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "ASIO Audio Loopback Tool for audio-bench.\n\n"
        "This tool plays a mono WAV file through an ASIO output channel while\n"
        "simultaneously recording from an ASIO input channel to a new WAV file.\n\n"
        "Examples:\n"
        "  ab_asio_loopback --list                                      # List ASIO drivers\n"
        "  ab_asio_loopback --about                                     # Show about information\n"
        "  ab_asio_loopback -d \"Driver\" -p in.wav -o out.wav          # Basic loopback\n"
        "  ab_asio_loopback -d \"Driver\" -p in.wav -o out.wav -i 0 -C 1  # Specify channels\n"
        "  ab_asio_loopback -d \"Driver\" -p in.wav -o out.wav -r 96000 -b 24  # Custom rate and bits\n");

    int rc = poptGetNextOpt(popt_ctx);
    if (rc < -1) {
        fprintf(stderr, "Error: %s: %s\n",
                poptBadOption(popt_ctx, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//  Handle version mode
//------------------------------------------------------------------------------
    if (version_flag) {
        printf("ab_asio_loopback version 1.0.0\n");
        printf("ASIO Audio Loopback Tool for audio-bench\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        printf("Built: %s %s\n", __DATE__, __TIME__);
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

//------------------------------------------------------------------------------
//  Handle about mode
//------------------------------------------------------------------------------
    if (about_flag) {
        printf("ab_asio_loopback - ASIO Audio Loopback Tool\n");
        printf("============================================\n\n");
        printf("Part of the audio-bench suite\n");
        printf("Version: 1.0.0\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        printf("License: MIT\n\n");
        printf("Description:\n");
        printf("  This tool plays a mono WAV file through an ASIO output channel while\n");
        printf("  simultaneously recording from an ASIO input channel to a new WAV file.\n");
        printf("  Designed for professional audio interfaces using the ASIO protocol.\n\n");
        printf("Features:\n");
        printf("  - Direct ASIO driver access for low-latency audio\n");
        printf("  - Simultaneous playback and recording\n");
        printf("  - Support for 16-bit, 24-bit, and 32-bit float formats\n");
        printf("  - Configurable sample rates and channel selection\n\n");
        printf("Platform: Windows only (ASIO SDK)\n");
        printf("Built: %s %s\n", __DATE__, __TIME__);
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

//------------------------------------------------------------------------------
//  Handle list mode
//------------------------------------------------------------------------------
    if (list_flag) {
        listASIODrivers();
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

//------------------------------------------------------------------------------
//  Validate required arguments
//------------------------------------------------------------------------------
    if (!driverName) {
        fprintf(stderr, "Error: ASIO driver name is required (use --list to see available drivers)\n");
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    if (!inputFilename) {
        fprintf(stderr, "Error: Input WAV file is required (use --play)\n");
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    if (!outputFilename) {
        fprintf(stderr, "Error: Output WAV file is required (use --capture)\n");
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    // Validate bit depth
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        fprintf(stderr, "Error: Bit depth must be 16, 24, or 32 (got %ld)\n", bitDepth);
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    outputBitDepth = bitDepth;

    poptFreeContext(popt_ctx);

    printf("ASIO Audio Loopback Tool\n");
    printf("==========================================\n\n");

//------------------------------------------------------------------------------
//  Load input WAV file
//------------------------------------------------------------------------------
    printf("Loading input file: %s\n", inputFilename);

    SF_INFO inputFileInfo;
    memset(&inputFileInfo, 0, sizeof(inputFileInfo));
    SNDFILE* inputFile = sf_open(inputFilename, SFM_READ, &inputFileInfo);

    if (!inputFile) {
        fprintf(stderr, "Error: Cannot open input file: %s\n", inputFilename);
        fprintf(stderr, "libsndfile error: %s\n", sf_strerror(nullptr));
        CoUninitialize();
        return 1;
    }

    // Verify mono file
    if (inputFileInfo.channels != 1) {
        fprintf(stderr, "Error: Input file must be mono (has %d channels)\n", inputFileInfo.channels);
        sf_close(inputFile);
        CoUninitialize();
        return 1;
    }

    printf("Input file info:\n");
    printf("  Sample rate: %d Hz\n", inputFileInfo.samplerate);
    printf("  Channels: %d (mono)\n", inputFileInfo.channels);
    printf("  Frames: %lld\n", (long long)inputFileInfo.frames);
    printf("  Duration: %.3f seconds\n\n", (double)inputFileInfo.frames / inputFileInfo.samplerate);

    // Use file sample rate if not specified
    if (requestedSampleRate == 0.0) {
        requestedSampleRate = inputFileInfo.samplerate;
    }

    // Verify sample rate match
    if (requestedSampleRate != inputFileInfo.samplerate) {
        printf("Warning: Requested sample rate (%.0f Hz) differs from file rate (%d Hz)\n",
               requestedSampleRate, inputFileInfo.samplerate);
        printf("         This may cause pitch/speed changes!\n\n");
    }

    // Allocate memory and load audio data
    audioData.total_frames = static_cast<int>(inputFileInfo.frames);
    audioData.playback_signal = static_cast<float*>(malloc(audioData.total_frames * sizeof(float)));
    audioData.recorded_signal = static_cast<float*>(calloc(audioData.total_frames, sizeof(float)));
    audioData.current_frame = 0;

    if (!audioData.playback_signal || !audioData.recorded_signal) {
        fprintf(stderr, "Error: Failed to allocate memory for audio buffers\n");
        sf_close(inputFile);
        CoUninitialize();
        return 1;
    }

    // Read entire file
    sf_count_t framesRead = sf_read_float(inputFile, audioData.playback_signal, audioData.total_frames);
    sf_close(inputFile);

    if (framesRead != audioData.total_frames) {
        fprintf(stderr, "Error: Failed to read all frames from input file\n");
        free(audioData.playback_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

    printf("Loaded %d frames from input file\n\n", audioData.total_frames);

//------------------------------------------------------------------------------
//  Initialize ASIO
//------------------------------------------------------------------------------
    if (!initASIO(driverName)) {
        fprintf(stderr, "Failed to initialize ASIO driver\n");
        free(audioData.playback_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//  Validate channel selections
//------------------------------------------------------------------------------
    if (inputChannel >= numInputChannels) {
        fprintf(stderr, "Error: Input channel %ld out of range (0-%ld)\n", inputChannel, numInputChannels - 1);
        shutdownASIO();
        free(audioData.playback_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

    if (outputChannel >= numOutputChannels) {
        fprintf(stderr, "Error: Output channel %ld out of range (0-%ld)\n", outputChannel, numOutputChannels - 1);
        shutdownASIO();
        free(audioData.playback_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//  Setup ASIO buffers
//------------------------------------------------------------------------------
    printf("\n");
    if (!setupASIOBuffers(inputChannel, outputChannel, requestedSampleRate)) {
        fprintf(stderr, "Failed to setup ASIO buffers\n");
        shutdownASIO();
        free(audioData.playback_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//  Open output WAV file
//------------------------------------------------------------------------------
    memset(&outputFileInfo, 0, sizeof(outputFileInfo));
    outputFileInfo.samplerate = static_cast<int>(currentSampleRate);
    outputFileInfo.channels = 1;  // Mono recording

    // Set format based on bit depth
    if (outputBitDepth == 16) {
        outputFileInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    } else if (outputBitDepth == 24) {
        outputFileInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
    } else {  // 32-bit
        outputFileInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    }

    outputFile = sf_open(outputFilename, SFM_WRITE, &outputFileInfo);
    if (!outputFile) {
        fprintf(stderr, "Error: Cannot open output file: %s\n", outputFilename);
        fprintf(stderr, "libsndfile error: %s\n", sf_strerror(nullptr));
        shutdownASIO();
        free(audioData.playback_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

    printf("\nOutput file: %s\n", outputFilename);
    if (outputBitDepth == 16) {
        printf("Format: WAV file (16-bit PCM, mono, %.0f Hz)\n", currentSampleRate);
    } else if (outputBitDepth == 24) {
        printf("Format: WAV file (24-bit PCM, mono, %.0f Hz)\n", currentSampleRate);
    } else {
        printf("Format: WAV file (32-bit float, mono, %.0f Hz)\n", currentSampleRate);
    }

//------------------------------------------------------------------------------
//  Pre-convert playback signal to ASIO format
//------------------------------------------------------------------------------
    if (!preconvertPlaybackSignal()) {
        fprintf(stderr, "Failed to pre-convert playback signal\n");
        sf_close(outputFile);
        shutdownASIO();
        free(audioData.playback_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//  Start loopback
//------------------------------------------------------------------------------
    printf("\nStarting loopback...\n");
    printf("Playing: Input channel %ld -> Output channel %ld\n", outputChannel, inputChannel);
    printf("Recording: Input channel %ld -> %s\n\n", inputChannel, outputFilename);

    loopbackActive = true;

    ASIOError err = ASIOStart();
    if (err != ASE_OK) {
        fprintf(stderr, "ASIOStart failed with error: %ld\n", err);
        sf_close(outputFile);
        shutdownASIO();
        free(audioData.playback_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//  Wait for loopback to complete
//------------------------------------------------------------------------------
    while (loopbackActive) {
        Sleep(100);
        printf("\rProgress: %d / %d frames", audioData.current_frame, audioData.total_frames);
        fflush(stdout);
    }
    printf("\n\n");

//------------------------------------------------------------------------------
//  Stop ASIO and close output file
//------------------------------------------------------------------------------
    shutdownASIO();
    sf_close(outputFile);

    printf("Loopback complete!\n");
    printf("Recorded audio saved to: %s\n", outputFilename);

//------------------------------------------------------------------------------
//  Cleanup
//------------------------------------------------------------------------------
    free(audioData.playback_signal);
    free(audioData.recorded_signal);

    CoUninitialize();
    return 0;
}
