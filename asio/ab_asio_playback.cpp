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
 * ab_asio_playback.cpp
 * ASIO Audio Playback Tool for audio-bench
 *
 * Windows-only ASIO interface for professional audio hardware
 * Plays WAV files through ASIO output with multi-channel support and seeking
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
// Version Information
//------------------------------------------------------------------------------
#define AB_ASIO_PLAYBACK_VERSION "1.0.0"
#define AB_ASIO_PLAYBACK_DATE "2025-12-18"

//------------------------------------------------------------------------------
// Global ASIO state
//------------------------------------------------------------------------------
static IASIO* asioDriver = nullptr;
static ASIODriverInfo driverInfo;
static ASIOBufferInfo* bufferInfos = nullptr;          // Dynamic array for N channels
static ASIOCallbacks asioCallbacks;
static long numInputChannels = 0;
static long numOutputChannels = 0;
static long preferredBufferSize = 0;
static long minBufferSize = 0;
static long maxBufferSize = 0;
static long bufferGranularity = 0;
static ASIOSampleRate currentSampleRate = 48000.0;
static bool playbackActive = false;
static bool verbose = false;  // Global verbose flag

//------------------------------------------------------------------------------
// Cached channel info (retrieved once during setup)
//------------------------------------------------------------------------------
static ASIOChannelInfo* channelInfos = nullptr;        // Dynamic array for N channels

//------------------------------------------------------------------------------
// Cached sample size (calculated once during setup)
//------------------------------------------------------------------------------
static size_t outputSampleSize = 0;

//------------------------------------------------------------------------------
// Pre-converted playback signal in ASIO output format (per-channel buffers)
//------------------------------------------------------------------------------
static void** preconvertedChannels = nullptr;          // Array of per-channel buffers
static long numWavChannels = 0;                        // Number of channels in WAV file
static long startOutputChannel = 0;                    // Starting ASIO output channel

//------------------------------------------------------------------------------
// Playback control
//------------------------------------------------------------------------------
static long totalFrames = 0;                           // Total frames to play (after seeking)
static long currentFrame = 0;                          // Current playback position
static long offsetFrames = 0;                          // Frames skipped due to --offset

//------------------------------------------------------------------------------
// Sample format conversion: Float to ASIO
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
static ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long index, ASIOBool processNow)
{
    if (!playbackActive || !preconvertedChannels) {
        // Zero all output buffers to prevent noise/buzz after playback ends
        if (bufferInfos && numWavChannels > 0 && outputSampleSize > 0) {
            for (long ch = 0; ch < numWavChannels; ch++) {
                memset(bufferInfos[ch].buffers[index], 0,
                       preferredBufferSize * outputSampleSize);
            }
        }
        return nullptr;
    }

    long bufferSize = preferredBufferSize;
    long framesToProcess = bufferSize;

    // Check if we're nearing end of playback
    if (currentFrame + framesToProcess > totalFrames) {
        framesToProcess = totalFrames - currentFrame;
    }

    // Fill output buffers for all channels
    if (currentFrame < totalFrames && framesToProcess > 0) {
        // Copy pre-converted data to ASIO output buffers for each channel
        for (long ch = 0; ch < numWavChannels; ch++) {
            size_t offsetBytes = currentFrame * outputSampleSize;
            size_t copyBytes = framesToProcess * outputSampleSize;

            memcpy(bufferInfos[ch].buffers[index],
                   (char*)preconvertedChannels[ch] + offsetBytes,
                   copyBytes);

            // Zero remaining buffer if partial fill
            if (framesToProcess < bufferSize) {
                size_t silenceOffset = framesToProcess * outputSampleSize;
                size_t silenceBytes = (bufferSize - framesToProcess) * outputSampleSize;
                memset((char*)bufferInfos[ch].buffers[index] + silenceOffset,
                       0, silenceBytes);
            }
        }

        currentFrame += framesToProcess;
    } else {
        // Playback complete - output silence on all channels
        for (long ch = 0; ch < numWavChannels; ch++) {
            memset(bufferInfos[ch].buffers[index], 0,
                   bufferSize * outputSampleSize);
        }
        playbackActive = false;
    }

    return nullptr;
}

static void bufferSwitch(long index, ASIOBool processNow)
{
    bufferSwitchTimeInfo(nullptr, index, processNow);
}

static void sampleRateChanged(ASIOSampleRate sRate)
{
    currentSampleRate = sRate;
    if (verbose) {
        printf("Sample rate changed to: %.0f Hz\n", sRate);
    }
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
                value == kAsioSupportsInputMonitor) {
                return 1L;
            }
            return 0L;

        case kAsioResetRequest:
            playbackActive = false;
            return 1L;

        case kAsioResyncRequest:
            return 1L;

        case kAsioLatenciesChanged:
            return 1L;

        case kAsioEngineVersion:
            return 2L;

        case kAsioSupportsTimeInfo:
            return 1L;

        case kAsioSupportsTimeCode:
            return 0L;
    }
    return 0L;
}

//------------------------------------------------------------------------------
// ASIO Initialization
//------------------------------------------------------------------------------
static bool initASIO(const char* driverName)
{
    AsioDrivers* asioDrivers = new AsioDrivers();

    if (!asioDrivers->loadDriver(const_cast<char*>(driverName))) {
        printf("Failed to load ASIO driver: %s\n", driverName);
        delete asioDrivers;
        return false;
    }

    ASIOError err = ASIOInit(&driverInfo);
    if (err != ASE_OK) {
        printf("ASIOInit failed with error: %ld\n", err);
        asioDrivers->removeCurrentDriver();
        delete asioDrivers;
        return false;
    }

    if (verbose) {
        printf("ASIO Driver: %s\n", driverInfo.name);
        printf("ASIO Version: %ld\n", driverInfo.asioVersion);
        printf("Driver Version: 0x%08lx\n", driverInfo.driverVersion);
    }

    // Get channels
    err = ASIOGetChannels(&numInputChannels, &numOutputChannels);
    if (err != ASE_OK) {
        printf("ASIOGetChannels failed\n");
        ASIOExit();
        delete asioDrivers;
        return false;
    }

    if (verbose) {
        printf("Input channels: %ld\n", numInputChannels);
        printf("Output channels: %ld\n", numOutputChannels);
    }

    // Get buffer size range
    err = ASIOGetBufferSize(&minBufferSize, &maxBufferSize,
                            &preferredBufferSize, &bufferGranularity);
    if (err != ASE_OK) {
        printf("ASIOGetBufferSize failed\n");
        ASIOExit();
        delete asioDrivers;
        return false;
    }

    if (verbose) {
        printf("Buffer size: min=%ld, max=%ld, preferred=%ld, granularity=%ld\n",
               minBufferSize, maxBufferSize, preferredBufferSize, bufferGranularity);
    }

    // Get sample rate
    err = ASIOGetSampleRate(&currentSampleRate);
    if (err != ASE_OK) {
        printf("ASIOGetSampleRate failed\n");
    } else {
        if (verbose) {
            printf("Current sample rate: %.0f Hz\n", currentSampleRate);
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// ASIO Buffer Setup
//------------------------------------------------------------------------------
static bool setupASIOBuffers(long startChannel, long numChannels)
{
    // Setup callbacks
    asioCallbacks.bufferSwitch = bufferSwitch;
    asioCallbacks.sampleRateDidChange = sampleRateChanged;
    asioCallbacks.asioMessage = asioMessages;
    asioCallbacks.bufferSwitchTimeInfo = bufferSwitchTimeInfo;

    // Allocate buffer info array for N output channels
    bufferInfos = new ASIOBufferInfo[numChannels];
    if (!bufferInfos) {
        printf("Failed to allocate buffer info array\n");
        return false;
    }
    memset(bufferInfos, 0, sizeof(ASIOBufferInfo) * numChannels);

    // Configure output buffers for each channel
    for (long i = 0; i < numChannels; i++) {
        bufferInfos[i].isInput = ASIOFalse;
        bufferInfos[i].channelNum = startChannel + i;
        bufferInfos[i].buffers[0] = bufferInfos[i].buffers[1] = nullptr;
    }

    // Create buffers
    ASIOError err = ASIOCreateBuffers(bufferInfos, numChannels,
                                      preferredBufferSize, &asioCallbacks);
    if (err != ASE_OK) {
        printf("ASIOCreateBuffers failed with error: %ld\n", err);
        delete[] bufferInfos;
        bufferInfos = nullptr;
        return false;
    }

    // Allocate and cache channel info for all output channels
    channelInfos = new ASIOChannelInfo[numChannels];
    if (!channelInfos) {
        printf("Failed to allocate channel info array\n");
        return false;
    }

    for (long i = 0; i < numChannels; i++) {
        channelInfos[i].channel = startChannel + i;
        channelInfos[i].isInput = ASIOFalse;
        err = ASIOGetChannelInfo(&channelInfos[i]);
        if (err == ASE_OK) {
            if (verbose) {
                printf("Output Channel %ld: %s, Type: %ld\n",
                       startChannel + i, channelInfos[i].name, channelInfos[i].type);
            }
        } else {
            printf("Failed to get channel info for output channel %ld\n",
                   startChannel + i);
            return false;
        }

        // Verify all channels have same sample type (simplifies pre-conversion)
        if (i > 0 && channelInfos[i].type != channelInfos[0].type) {
            if (verbose) {
                printf("Warning: Output channels have different sample types!\n");
                printf("  Channel %ld: type %ld\n", startChannel, channelInfos[0].type);
                printf("  Channel %ld: type %ld\n", startChannel + i, channelInfos[i].type);
            }
            // Continue anyway - pre-conversion will handle per-channel
        }
    }

    if (verbose) {
        printf("Successfully configured %ld output buffers\n", numChannels);
    }
    return true;
}

//------------------------------------------------------------------------------
// Pre-convert playback signal to ASIO format
//------------------------------------------------------------------------------
static bool preconvertPlaybackSignal(SNDFILE* inputFile, long numChannels, long numFrames)
{
    // Allocate per-channel pre-converted buffers
    preconvertedChannels = (void**)malloc(sizeof(void*) * numChannels);
    if (!preconvertedChannels) {
        printf("Error: Failed to allocate channel pointer array\n");
        return false;
    }

    // Determine sample size (assume all channels use same type as channel 0)
    switch (channelInfos[0].type) {
        case ASIOSTInt16LSB:   outputSampleSize = 2; break;
        case ASIOSTInt24LSB:   outputSampleSize = 3; break;
        case ASIOSTInt32LSB:   outputSampleSize = 4; break;
        case ASIOSTFloat32LSB: outputSampleSize = 4; break;
        case ASIOSTFloat64LSB: outputSampleSize = 8; break;
        default:
            printf("Error: Unsupported output sample type: %ld\n",
                   channelInfos[0].type);
            return false;
    }

    // Allocate ASIO-format buffer for each channel
    size_t channelBufferSize = numFrames * outputSampleSize;
    for (long ch = 0; ch < numChannels; ch++) {
        preconvertedChannels[ch] = malloc(channelBufferSize);
        if (!preconvertedChannels[ch]) {
            printf("Error: Failed to allocate channel %ld buffer (%zu bytes)\n",
                   ch, channelBufferSize);
            // Cleanup previously allocated channels
            for (long c = 0; c < ch; c++) {
                free(preconvertedChannels[c]);
            }
            free(preconvertedChannels);
            return false;
        }
    }

    if (verbose) {
        printf("Allocated %ld channel buffers, %zu bytes each\n",
               numChannels, channelBufferSize);
    }

    // Read entire WAV file in interleaved float format
    float* interleavedBuffer = (float*)malloc(numFrames * numChannels * sizeof(float));
    if (!interleavedBuffer) {
        printf("Error: Failed to allocate interleaved read buffer\n");
        for (long ch = 0; ch < numChannels; ch++) {
            free(preconvertedChannels[ch]);
        }
        free(preconvertedChannels);
        return false;
    }

    // Read using sf_readf_float (reads frames, not samples)
    sf_count_t framesRead = sf_readf_float(inputFile, interleavedBuffer, numFrames);
    if (framesRead != numFrames) {
        printf("Error: Failed to read all frames (read %lld of %ld)\n",
               (long long)framesRead, numFrames);
        free(interleavedBuffer);
        for (long ch = 0; ch < numChannels; ch++) {
            free(preconvertedChannels[ch]);
        }
        free(preconvertedChannels);
        return false;
    }

    if (verbose) {
        printf("Read %lld frames from WAV file\n", (long long)framesRead);
    }

    // De-interleave and convert to ASIO format for each channel
    for (long ch = 0; ch < numChannels; ch++) {
        // Extract single channel from interleaved data
        float* channelFloat = (float*)malloc(numFrames * sizeof(float));
        if (!channelFloat) {
            printf("Error: Failed to allocate temporary channel buffer\n");
            free(interleavedBuffer);
            for (long c = 0; c < numChannels; c++) {
                free(preconvertedChannels[c]);
            }
            free(preconvertedChannels);
            return false;
        }

        // De-interleave: extract channel samples
        for (long frame = 0; frame < numFrames; frame++) {
            channelFloat[frame] = interleavedBuffer[frame * numChannels + ch];
        }

        // Convert to ASIO format (use channel-specific type if needed)
        ASIOSampleType sampleType = channelInfos[ch].type;
        convertFloatToASIO(channelFloat, preconvertedChannels[ch],
                          numFrames, sampleType);

        free(channelFloat);

        if (verbose) {
            printf("Pre-converted channel %ld: %ld frames to ASIO format\n",
                   ch, numFrames);
        }
    }

    free(interleavedBuffer);

    if (verbose) {
        printf("Pre-conversion complete: %ld channels, %ld frames, %zu bytes/sample\n",
               numChannels, numFrames, outputSampleSize);
    }

    return true;
}

//------------------------------------------------------------------------------
// ASIO Shutdown
//------------------------------------------------------------------------------
static void shutdownASIO()
{
    if (asioDriver) {
        ASIOStop();
        ASIODisposeBuffers();
        ASIOExit();
        asioDriver = nullptr;
    }

    // Clean up dynamically allocated buffers
    if (bufferInfos) {
        delete[] bufferInfos;
        bufferInfos = nullptr;
    }

    if (channelInfos) {
        delete[] channelInfos;
        channelInfos = nullptr;
    }

    // Clean up pre-converted channel data
    if (preconvertedChannels) {
        for (long ch = 0; ch < numWavChannels; ch++) {
            if (preconvertedChannels[ch]) {
                free(preconvertedChannels[ch]);
                preconvertedChannels[ch] = nullptr;
            }
        }
        free(preconvertedChannels);
        preconvertedChannels = nullptr;
    }
}

//------------------------------------------------------------------------------
// List available ASIO drivers
//------------------------------------------------------------------------------
static void listASIODrivers()
{
    AsioDrivers asioDrivers;
    char* driverNames[100];
    char driverNameBuffer[100][256];

    for (int i = 0; i < 100; i++) {
        driverNames[i] = driverNameBuffer[i];
    }

    long numDrivers = asioDrivers.getDriverNames(driverNames, 100);

    printf("Available ASIO drivers (%ld):\n", numDrivers);
    for (long i = 0; i < numDrivers; i++) {
        printf("  [%ld] %s\n", i, driverNames[i]);
    }
}

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    // Initialize COM for Windows ASIO driver registry access
    CoInitialize(nullptr);

    // Command-line options
    int version_flag = 0;
    int about_flag = 0;
    int list_flag = 0;
    int play_flag = 0;
    int verbose_flag = 0;
    char* driverName = nullptr;
    char* inputFilename = nullptr;
    long startChannel = 0;
    double offsetSeconds = 0.0;

    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0,
         "Show version information", nullptr},
        {"about", 'a', POPT_ARG_NONE, &about_flag, 0,
         "Show detailed about information", nullptr},
        {"list", 'l', POPT_ARG_NONE, &list_flag, 0,
         "List available ASIO drivers", nullptr},
        {"driver", 'd', POPT_ARG_STRING, &driverName, 0,
         "ASIO driver name (required for playback)", "NAME"},
        {"channel", 'c', POPT_ARG_LONG, &startChannel, 0,
         "Starting output channel number (default: 0)", "N"},
        {"play", 'p', POPT_ARG_NONE, &play_flag, 0,
         "Play mode flag", nullptr},
        {"file", 'f', POPT_ARG_STRING, &inputFilename, 0,
         "WAV file to play (required)", "FILE"},
        {"offset", 'o', POPT_ARG_DOUBLE, &offsetSeconds, 0,
         "Start playback from time position in seconds (default: 0.0)", "SECONDS"},
        {"verbose", 'V', POPT_ARG_NONE, &verbose_flag, 0,
         "Enable verbose output", nullptr},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(nullptr, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "ASIO Audio Playback Tool for audio-bench.\n\n"
        "Plays WAV files through ASIO audio drivers with multi-channel support.\n"
        "Mono files play to single channel, stereo plays channels 0->0 and 1->1,\n"
        "multi-channel files play to consecutive ASIO output channels.\n\n"
        "Examples:\n"
        "  ab_asio_playback --list                                    # List drivers\n"
        "  ab_asio_playback -d \"Driver\" -p -f mono.wav              # Play mono (quiet)\n"
        "  ab_asio_playback -d \"Driver\" -p -f stereo.wav -c 2 -V    # Play with verbose\n"
        "  ab_asio_playback -d \"Driver\" -p -f music.wav -o 30.5     # Start at 30.5s\n"
        "  ab_asio_playback -d \"Driver\" -p -f 8ch.wav -c 0          # Play 8 channels\n");

    int rc;
    while ((rc = poptGetNextOpt(popt_ctx)) > 0) {
        // Process options
    }

    if (rc < -1) {
        fprintf(stderr, "Error parsing options: %s\n", poptStrerror(rc));
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    // Handle --version flag
    if (version_flag) {
        printf("ab_asio_playback version %s (%s)\n",
               AB_ASIO_PLAYBACK_VERSION, AB_ASIO_PLAYBACK_DATE);
        printf("Part of audio-bench ASIO extension\n");
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

    // Handle --about flag
    if (about_flag) {
        printf("ab_asio_playback - ASIO Audio Playback Tool\n");
        printf("Version: %s (%s)\n\n", AB_ASIO_PLAYBACK_VERSION, AB_ASIO_PLAYBACK_DATE);
        printf("Part of audio-bench project\n");
        printf("Windows-only ASIO interface for professional audio hardware\n\n");
        printf("Features:\n");
        printf("  - Multi-channel WAV file playback\n");
        printf("  - Support for all ASIO sample formats (Int16/24/32, Float32/64)\n");
        printf("  - Seeking support (start playback from specific time)\n");
        printf("  - Pre-conversion optimization for glitch-free playback\n");
        printf("  - Configurable output channel routing\n\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        printf("Licensed under MIT License\n");
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

    // Handle --list flag
    if (list_flag) {
        listASIODrivers();
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

    // Validate playback mode requirements
    if (!driverName && play_flag) {
        fprintf(stderr, "Error: --driver required for playback\n");
        poptPrintUsage(popt_ctx, stderr, 0);
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    if (play_flag && !inputFilename) {
        fprintf(stderr, "Error: --file required for playback\n");
        poptPrintUsage(popt_ctx, stderr, 0);
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    if (offsetSeconds < 0.0) {
        fprintf(stderr, "Error: --offset must be >= 0.0 seconds\n");
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    if (!play_flag) {
        fprintf(stderr, "Error: Must specify --play mode\n");
        poptPrintUsage(popt_ctx, stderr, 0);
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    poptFreeContext(popt_ctx);

    // Set global verbose flag
    verbose = (verbose_flag != 0);

    // Load WAV file
    if (verbose) {
        printf("\n");
        printf("========================================\n");
        printf("ab_asio_playback - ASIO Playback Tool\n");
        printf("========================================\n\n");
        printf("Loading input file: %s\n", inputFilename);
    }

    SF_INFO inputFileInfo;
    memset(&inputFileInfo, 0, sizeof(inputFileInfo));
    SNDFILE* inputFile = sf_open(inputFilename, SFM_READ, &inputFileInfo);

    if (!inputFile) {
        fprintf(stderr, "Error: Cannot open input file: %s\n", inputFilename);
        fprintf(stderr, "libsndfile error: %s\n", sf_strerror(nullptr));
        CoUninitialize();
        return 1;
    }

    if (verbose) {
        printf("Input file info:\n");
        printf("  Sample rate: %d Hz\n", inputFileInfo.samplerate);
        printf("  Channels: %d\n", inputFileInfo.channels);
        printf("  Frames: %lld\n", (long long)inputFileInfo.frames);
        printf("  Duration: %.3f seconds\n",
               (double)inputFileInfo.frames / inputFileInfo.samplerate);
    }

    numWavChannels = inputFileInfo.channels;

    // Calculate and apply offset
    offsetFrames = (long)(offsetSeconds * inputFileInfo.samplerate);

    // Validate offset
    if (offsetFrames >= inputFileInfo.frames) {
        fprintf(stderr, "Error: Offset %.3f seconds (%ld frames) "
                        "exceeds file duration (%lld frames)\n",
                offsetSeconds, offsetFrames, (long long)inputFileInfo.frames);
        sf_close(inputFile);
        CoUninitialize();
        return 1;
    }

    // Seek to offset position
    if (offsetFrames > 0) {
        sf_count_t seekResult = sf_seek(inputFile, offsetFrames, SEEK_SET);
        if (seekResult < 0) {
            fprintf(stderr, "Error: Failed to seek to offset position\n");
            sf_close(inputFile);
            CoUninitialize();
            return 1;
        }
        if (verbose) {
            printf("Starting playback at offset: %.3f seconds (%ld frames)\n",
                   offsetSeconds, offsetFrames);
        }
    }

    totalFrames = inputFileInfo.frames - offsetFrames;
    if (verbose) {
        printf("Will play %ld frames (%.3f seconds)\n\n",
               totalFrames, (double)totalFrames / inputFileInfo.samplerate);
    }

    // Initialize ASIO
    if (verbose) {
        printf("Initializing ASIO driver: %s\n", driverName);
    }
    if (!initASIO(driverName)) {
        fprintf(stderr, "Error: Failed to initialize ASIO\n");
        sf_close(inputFile);
        CoUninitialize();
        return 1;
    }
    if (verbose) {
        printf("\n");
    }

    // Validate channel mapping
    startOutputChannel = startChannel;
    if (startOutputChannel + numWavChannels > numOutputChannels) {
        fprintf(stderr, "Error: WAV has %ld channels, starting at channel %ld "
                        "would exceed available outputs (%ld)\n",
                numWavChannels, startOutputChannel, numOutputChannels);
        shutdownASIO();
        sf_close(inputFile);
        CoUninitialize();
        return 1;
    }

    if (verbose) {
        printf("Will play %ld channels: ASIO outputs %ld-%ld\n\n",
               numWavChannels, startOutputChannel, startOutputChannel + numWavChannels - 1);
    }

    // Validate and set sample rate
    double requestedSampleRate = inputFileInfo.samplerate;

    ASIOError err = ASIOCanSampleRate(requestedSampleRate);
    if (err != ASE_OK) {
        fprintf(stderr, "Error: Driver does not support sample rate %.0f Hz\n",
                requestedSampleRate);
        fprintf(stderr, "Current driver sample rate: %.0f Hz\n", currentSampleRate);
        shutdownASIO();
        sf_close(inputFile);
        CoUninitialize();
        return 1;
    }

    // Set sample rate to match WAV file
    err = ASIOSetSampleRate(requestedSampleRate);
    if (err != ASE_OK) {
        fprintf(stderr, "Error: Failed to set sample rate to %.0f Hz\n",
                requestedSampleRate);
        shutdownASIO();
        sf_close(inputFile);
        CoUninitialize();
        return 1;
    }

    currentSampleRate = requestedSampleRate;
    if (verbose) {
        printf("Sample rate configured: %.0f Hz\n\n", currentSampleRate);
    }

    // Setup ASIO buffers
    if (verbose) {
        printf("Setting up ASIO buffers...\n");
    }
    if (!setupASIOBuffers(startOutputChannel, numWavChannels)) {
        fprintf(stderr, "Error: Failed to setup ASIO buffers\n");
        shutdownASIO();
        sf_close(inputFile);
        CoUninitialize();
        return 1;
    }
    if (verbose) {
        printf("\n");
    }

    // Pre-convert playback signal
    if (verbose) {
        printf("Pre-converting playback signal...\n");
    }
    if (!preconvertPlaybackSignal(inputFile, numWavChannels, totalFrames)) {
        fprintf(stderr, "Error: Failed to pre-convert playback signal\n");
        shutdownASIO();
        sf_close(inputFile);
        CoUninitialize();
        return 1;
    }
    if (verbose) {
        printf("\n");
    }

    // Close input file (no longer needed)
    sf_close(inputFile);

    // Start playback
    if (verbose) {
        printf("========================================\n");
        printf("Starting playback...\n");
        printf("========================================\n");
        printf("Driver: %s\n", driverName);
        printf("File: %s\n", inputFilename);
        printf("Channels: %ld (ASIO outputs %ld-%ld)\n",
               numWavChannels, startOutputChannel, startOutputChannel + numWavChannels - 1);
        printf("Sample rate: %.0f Hz\n", currentSampleRate);
        printf("Frames: %ld (%.3f seconds)\n", totalFrames,
               (double)totalFrames / currentSampleRate);
        printf("========================================\n\n");
    }

    playbackActive = true;
    currentFrame = 0;

    err = ASIOStart();
    if (err != ASE_OK) {
        fprintf(stderr, "Error: ASIOStart failed with error: %ld\n", err);
        shutdownASIO();
        CoUninitialize();
        return 1;
    }

    // Wait for playback completion
    if (verbose) {
        printf("Playing");
    }
    while (playbackActive) {
        Sleep(100);
        if (verbose) {
            printf(".");
            fflush(stdout);
        }
    }
    if (verbose) {
        printf(" Done!\n\n");
        printf("Playback complete: %ld frames\n", currentFrame);
    }

    // Cleanup
    if (verbose) {
        printf("Shutting down ASIO...\n");
    }
    shutdownASIO();

    CoUninitialize();

    if (verbose) {
        printf("Playback completed successfully.\n");
    }
    return 0;
}
