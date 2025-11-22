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
 * ab_acq_asio.cpp
 * ASIO Audio Acquisition Tool for audio-bench
 *
 * Windows-only ASIO interface for professional audio hardware
 * Acquires audio samples from ASIO devices with low latency
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <popt.h>
#include <sndfile.h>
#include "asiosys.h"
#include "asio.h"
#include "iasiodrv.h"
#include "asiodrivers.h"

// Global ASIO state
static IASIO* asioDriver = nullptr;
static ASIODriverInfo driverInfo;
static ASIOBufferInfo bufferInfos[32]; // Support up to 32 channels
static ASIOCallbacks asioCallbacks;
static long numInputChannels = 0;
static long numOutputChannels = 0;
static long preferredBufferSize = 0;
static ASIOSampleRate currentSampleRate = 48000.0;
static bool acquisitionActive = false;
static long totalSamplesProcessed = 0;

// Acquisition parameters
static SNDFILE* outputFile = nullptr;
static SF_INFO sfInfo;
static long channelToRecord = 0;
static long samplesToAcquire = 0;
static long samplesAcquired = 0;
static long outputBitDepth = 32;

// Forward declarations
static void bufferSwitch(long index, ASIOBool processNow);
static void sampleRateChanged(ASIOSampleRate sRate);
static long asioMessages(long selector, long value, void* message, double* opt);
static ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long index, ASIOBool processNow);

//------------------------------------------------------------------------------
// ASIO Callbacks
//------------------------------------------------------------------------------

static void bufferSwitch(long index, ASIOBool processNow)
{
    bufferSwitchTimeInfo(nullptr, index, processNow);
}

static ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long index, ASIOBool processNow)
{
    if (!acquisitionActive || !outputFile) {
        return nullptr;
    }

    // Process the input buffer for the channel we're recording
    ASIOBufferInfo* info = &bufferInfos[channelToRecord];

    if (info->buffers[index]) {
        ASIOChannelInfo channelInfo;
        channelInfo.channel = info->channelNum;
        channelInfo.isInput = ASIOTrue;
        ASIOGetChannelInfo(&channelInfo);
        
        long bufferSize = preferredBufferSize;
        long samplesToWrite = bufferSize;
        
        if (samplesAcquired + samplesToWrite > samplesToAcquire) {
            samplesToWrite = samplesToAcquire - samplesAcquired;
        }
        
        // Convert samples based on ASIO sample type
        void* buffer = info->buffers[index];
        float* floatBuffer = new float[samplesToWrite];

        switch (channelInfo.type) {
            case ASIOSTInt16LSB:
            {
                short* samples = (short*)buffer;
                for (long i = 0; i < samplesToWrite; i++) {
                    floatBuffer[i] = samples[i] / 32768.0f;
                }
                break;
            }
            case ASIOSTInt24LSB:
            {
                unsigned char* samples = (unsigned char*)buffer;
                for (long i = 0; i < samplesToWrite; i++) {
                    int s24 = (samples[i*3] << 8) | (samples[i*3+1] << 16) | (samples[i*3+2] << 24);
                    floatBuffer[i] = s24 / 2147483648.0f;
                }
                break;
            }
            case ASIOSTInt32LSB:
            {
                int* samples = (int*)buffer;
                for (long i = 0; i < samplesToWrite; i++) {
                    floatBuffer[i] = samples[i] / 2147483648.0f;
                }
                break;
            }
            case ASIOSTFloat32LSB:
            {
                float* samples = (float*)buffer;
                memcpy(floatBuffer, samples, samplesToWrite * sizeof(float));
                break;
            }
            case ASIOSTFloat64LSB:
            {
                double* samples = (double*)buffer;
                for (long i = 0; i < samplesToWrite; i++) {
                    floatBuffer[i] = (float)samples[i];
                }
                break;
            }
            default:
                printf("Unsupported sample type: %ld\n", channelInfo.type);
                delete[] floatBuffer;
                return nullptr;
        }

        // Write to WAV file based on bit depth
        if (outputBitDepth == 16) {
            // Convert float to 16-bit signed integer
            short* shortBuffer = new short[samplesToWrite];
            for (long i = 0; i < samplesToWrite; i++) {
                // Clamp to [-1.0, 1.0] and convert to 16-bit
                float sample = floatBuffer[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                shortBuffer[i] = (short)(sample * 32767.0f);
            }
            sf_write_short(outputFile, shortBuffer, samplesToWrite);
            delete[] shortBuffer;
        } else if (outputBitDepth == 24) {
            // Convert float to 32-bit integer (libsndfile handles 24-bit packing)
            int* intBuffer = new int[samplesToWrite];
            for (long i = 0; i < samplesToWrite; i++) {
                // Clamp to [-1.0, 1.0] and convert to 24-bit (stored in 32-bit int)
                float sample = floatBuffer[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                intBuffer[i] = (int)(sample * 8388607.0f);  // 2^23 - 1
            }
            sf_write_int(outputFile, intBuffer, samplesToWrite);
            delete[] intBuffer;
        } else {  // 32-bit float
            sf_write_float(outputFile, floatBuffer, samplesToWrite);
        }
        delete[] floatBuffer;
        
        samplesAcquired += samplesToWrite;
        totalSamplesProcessed += samplesToWrite;
        
        if (samplesAcquired >= samplesToAcquire) {
            acquisitionActive = false;
            printf("\nAcquisition complete: %ld samples acquired\n", samplesAcquired);
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
    
    // Get buffer size
    long minSize, maxSize, granularity;
    err = ASIOGetBufferSize(&minSize, &maxSize, &preferredBufferSize, &granularity);
    if (err != ASE_OK) {
        printf("ASIOGetBufferSize failed\n");
        ASIOExit();
        delete asioDrivers;
        return false;
    }
    
    printf("Buffer size: min=%ld, max=%ld, preferred=%ld, granularity=%ld\n",
           minSize, maxSize, preferredBufferSize, granularity);
    
    // Get sample rate
    err = ASIOGetSampleRate(&currentSampleRate);
    if (err != ASE_OK) {
        printf("ASIOGetSampleRate failed\n");
    } else {
        printf("Current sample rate: %.0f Hz\n", currentSampleRate);
    }
    
    return true;
}

static bool setupASIOBuffers(long inputChannel)
{
    // Setup callbacks
    asioCallbacks.bufferSwitch = bufferSwitch;
    asioCallbacks.sampleRateDidChange = sampleRateChanged;
    asioCallbacks.asioMessage = asioMessages;
    asioCallbacks.bufferSwitchTimeInfo = bufferSwitchTimeInfo;
    
    // Create buffer info for the input channel we want to record
    memset(bufferInfos, 0, sizeof(bufferInfos));
    
    bufferInfos[0].isInput = ASIOTrue;
    bufferInfos[0].channelNum = inputChannel;
    bufferInfos[0].buffers[0] = bufferInfos[0].buffers[1] = nullptr;
    
    channelToRecord = 0; // Index in bufferInfos array
    
    // Create buffers
    ASIOError err = ASIOCreateBuffers(bufferInfos, 1, preferredBufferSize, &asioCallbacks);
    if (err != ASE_OK) {
        printf("ASIOCreateBuffers failed with error: %ld\n", err);
        return false;
    }
    
    // Get channel info
    ASIOChannelInfo channelInfo;
    channelInfo.channel = inputChannel;
    channelInfo.isInput = ASIOTrue;
    err = ASIOGetChannelInfo(&channelInfo);
    if (err == ASE_OK) {
        printf("Channel %ld: %s, Type: %ld\n", 
               inputChannel, channelInfo.name, channelInfo.type);
    }
    
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
// Main Program
//------------------------------------------------------------------------------

int main(int argc, const char** argv)
{
    CoInitialize(nullptr);

    // Command-line options
    int listMode = 0;
    int channelsMode = 0;
    int acquireMode = 0;
    int versionFlag = 0;
    char* driverName = nullptr;
    long inputChannel = 0;
    double duration = 1.0;  // Duration in seconds
    long bitDepth = 32;     // Bit depth: 16, 24, or 32
    char* outputFilename = nullptr;
    double requestedRate = 0.0;

    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &versionFlag, 0,
         "Show version information", nullptr},
        {"list", 'l', POPT_ARG_NONE, &listMode, 0,
         "List available ASIO drivers", nullptr},
        {"driver", 'd', POPT_ARG_STRING, &driverName, 0,
         "ASIO driver name", "NAME"},
        {"channels", 'C', POPT_ARG_NONE, &channelsMode, 0,
         "List channels for specified driver", nullptr},
        {"acquire", 'a', POPT_ARG_NONE, &acquireMode, 0,
         "Acquire audio samples", nullptr},
        {"channel", 'c', POPT_ARG_LONG, &inputChannel, 0,
         "Input channel number (default: 0)", "NUM"},
        {"time", 't', POPT_ARG_DOUBLE, &duration, 0,
         "Recording duration in seconds (default: 1.0)", "SECONDS"},
        {"bits", 'b', POPT_ARG_LONG, &bitDepth, 0,
         "Bit depth: 16, 24, or 32 (default: 32)", "BITS"},
        {"output", 'o', POPT_ARG_STRING, &outputFilename, 0,
         "Output WAV file (default: output.wav)", "FILE"},
        {"rate", 'r', POPT_ARG_DOUBLE, &requestedRate, 0,
         "Sample rate in Hz (default: use current driver rate)", "HZ"},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(nullptr, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "ASIO Audio Acquisition Tool - Windows-only ASIO interface support\n\n"
        "Operation Modes:\n"
        "  --list              List all available ASIO drivers\n"
        "  --driver <name> --channels    Show channels for specified driver\n"
        "  --driver <name> --acquire     Acquire audio samples\n\n"
        "Examples:\n"
        "  ab_acq_asio --list\n"
        "  ab_acq_asio -d \"ASIO4ALL v2\" --channels\n"
        "  ab_acq_asio -d \"ASIO4ALL v2\" -a -c 0 -t 2.0 -o test.wav -r 48000\n"
        "  ab_acq_asio -d \"ASIO4ALL v2\" -a -c 0 -t 5.0 -b 24 -o test_24bit.wav\n");

    int rc = poptGetNextOpt(popt_ctx);
    if (rc < -1) {
        fprintf(stderr, "Error: %s: %s\n",
                poptBadOption(popt_ctx, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    // Handle version mode
    if (versionFlag) {
        printf("ab_acq_asio version 1.0.0\n");
        printf("ASIO Audio Acquisition Tool for audio-bench\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

    // Set default output filename if not specified
    if (outputFilename == nullptr && acquireMode) {
        outputFilename = const_cast<char*>("output.wav");
    }

    // Execute requested operation
    if (listMode) {
        listASIODrivers();
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    } else if (driverName) {
        if (!initASIO(driverName)) {
            poptFreeContext(popt_ctx);
            CoUninitialize();
            return 1;
        }

        if (channelsMode) {
            printf("\nInput Channels:\n");
            for (long i = 0; i < numInputChannels; i++) {
                ASIOChannelInfo channelInfo;
                channelInfo.channel = i;
                channelInfo.isInput = ASIOTrue;
                if (ASIOGetChannelInfo(&channelInfo) == ASE_OK) {
                    printf("  %2ld: %s (Type: 0x%lx)\n", i, channelInfo.name, channelInfo.type);
                }
            }

            printf("\nOutput Channels:\n");
            for (long i = 0; i < numOutputChannels; i++) {
                ASIOChannelInfo channelInfo;
                channelInfo.channel = i;
                channelInfo.isInput = ASIOFalse;
                if (ASIOGetChannelInfo(&channelInfo) == ASE_OK) {
                    printf("  %2ld: %s (Type: 0x%lx)\n", i, channelInfo.name, channelInfo.type);
                }
            }
        } else if (acquireMode) {
            // Set sample rate if requested
            if (requestedRate > 0.0) {
                if (ASIOCanSampleRate(requestedRate) == ASE_OK) {
                    if (ASIOSetSampleRate(requestedRate) == ASE_OK) {
                        currentSampleRate = requestedRate;
                        printf("Sample rate set to: %.0f Hz\n", currentSampleRate);
                    } else {
                        printf("Warning: Failed to set sample rate to %.0f Hz\n", requestedRate);
                    }
                } else {
                    printf("Warning: Sample rate %.0f Hz not supported\n", requestedRate);
                }
            }
            
            // Validate channel
            if (inputChannel < 0 || inputChannel >= numInputChannels) {
                printf("Error: Invalid input channel %ld (available: 0-%ld)\n",
                       inputChannel, numInputChannels - 1);
                shutdownASIO();
                poptFreeContext(popt_ctx);
                CoUninitialize();
                return 1;
            }

            // Validate bit depth
            if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
                printf("Error: Bit depth must be 16, 24, or 32 (got %ld)\n", bitDepth);
                shutdownASIO();
                poptFreeContext(popt_ctx);
                CoUninitialize();
                return 1;
            }

            // Validate and calculate sample count from duration
            if (duration <= 0.0) {
                printf("Error: Duration must be greater than 0 seconds\n");
                shutdownASIO();
                poptFreeContext(popt_ctx);
                CoUninitialize();
                return 1;
            }

            long samples = (long)(duration * currentSampleRate);
            if (samples == 0) {
                printf("Error: Duration too short for sample rate %.0f Hz\n", currentSampleRate);
                shutdownASIO();
                poptFreeContext(popt_ctx);
                CoUninitialize();
                return 1;
            }

            // Open output file
            memset(&sfInfo, 0, sizeof(sfInfo));
            sfInfo.samplerate = (int)currentSampleRate;
            sfInfo.channels = 1;  // Mono recording

            // Set format based on bit depth
            if (bitDepth == 16) {
                sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
            } else if (bitDepth == 24) {
                sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
            } else {  // 32-bit
                sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
            }

            outputFile = sf_open(outputFilename, SFM_WRITE, &sfInfo);
            if (!outputFile) {
                printf("Error: Cannot open output file: %s\n", outputFilename);
                printf("libsndfile error: %s\n", sf_strerror(nullptr));
                shutdownASIO();
                poptFreeContext(popt_ctx);
                CoUninitialize();
                return 1;
            }
            
            printf("\nAcquiring %.2f seconds (%" "ld samples) from channel %ld at %.0f Hz\n",
                   duration, samples, inputChannel, currentSampleRate);
            printf("Output file: %s\n", outputFilename);

            // Display format based on bit depth
            if (bitDepth == 16) {
                printf("Format: WAV file (16-bit PCM, mono)\n\n");
            } else if (bitDepth == 24) {
                printf("Format: WAV file (24-bit PCM, mono)\n\n");
            } else {
                printf("Format: WAV file (32-bit float, mono)\n\n");
            }
            
            // Setup buffers and start acquisition
            if (!setupASIOBuffers(inputChannel)) {
                sf_close(outputFile);
                shutdownASIO();
                poptFreeContext(popt_ctx);
                CoUninitialize();
                return 1;
            }

            samplesToAcquire = samples;
            samplesAcquired = 0;
            outputBitDepth = bitDepth;
            acquisitionActive = true;

            ASIOError err = ASIOStart();
            if (err != ASE_OK) {
                printf("ASIOStart failed with error: %ld\n", err);
                sf_close(outputFile);
                shutdownASIO();
                poptFreeContext(popt_ctx);
                CoUninitialize();
                return 1;
            }
            
            printf("Acquiring... (Press Ctrl+C to stop)\n");
            
            // Wait for acquisition to complete
            while (acquisitionActive) {
                Sleep(100);
                if (totalSamplesProcessed % 48000 == 0 && totalSamplesProcessed > 0) {
                    printf("Samples: %ld / %ld\r", samplesAcquired, samplesToAcquire);
                    fflush(stdout);
                }
            }

            sf_close(outputFile);
            printf("\nAcquisition complete. WAV file written to: %s\n", outputFilename);
        }

        shutdownASIO();
    } else {
        // No valid mode specified, show help
        poptPrintHelp(popt_ctx, stderr, 0);
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    poptFreeContext(popt_ctx);
    CoUninitialize();
    return 0;
}
