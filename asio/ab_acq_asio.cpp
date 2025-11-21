/*
 * ab_acq_asio.cpp - ASIO Audio Acquisition Tool
 * Part of the audio_bench suite
 * 
 * Enumerates and acquires audio samples from ASIO devices
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <popt.h>
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
static FILE* outputFile = nullptr;
static long channelToRecord = 0;
static long samplesToAcquire = 0;
static long samplesAcquired = 0;

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
        channelInfo.channel = channelToRecord;
        channelInfo.isInput = ASIOTrue;
        asioDriver->getChannelInfo(&channelInfo);
        
        long bufferSize = preferredBufferSize;
        long samplesToWrite = bufferSize;
        
        if (samplesAcquired + samplesToWrite > samplesToAcquire) {
            samplesToWrite = samplesToAcquire - samplesAcquired;
        }
        
        // Convert samples based on ASIO sample type
        void* buffer = info->buffers[index];
        
        switch (channelInfo.type) {
            case ASIOSTInt16LSB:
            {
                short* samples = (short*)buffer;
                for (long i = 0; i < samplesToWrite; i++) {
                    float sample = samples[i] / 32768.0f;
                    fwrite(&sample, sizeof(float), 1, outputFile);
                }
                break;
            }
            case ASIOSTInt24LSB:
            {
                unsigned char* samples = (unsigned char*)buffer;
                for (long i = 0; i < samplesToWrite; i++) {
                    int s24 = (samples[i*3] << 8) | (samples[i*3+1] << 16) | (samples[i*3+2] << 24);
                    float sample = s24 / 2147483648.0f;
                    fwrite(&sample, sizeof(float), 1, outputFile);
                }
                break;
            }
            case ASIOSTInt32LSB:
            {
                int* samples = (int*)buffer;
                for (long i = 0; i < samplesToWrite; i++) {
                    float sample = samples[i] / 2147483648.0f;
                    fwrite(&sample, sizeof(float), 1, outputFile);
                }
                break;
            }
            case ASIOSTFloat32LSB:
            {
                float* samples = (float*)buffer;
                fwrite(samples, sizeof(float), samplesToWrite, outputFile);
                break;
            }
            case ASIOSTFloat64LSB:
            {
                double* samples = (double*)buffer;
                for (long i = 0; i < samplesToWrite; i++) {
                    float sample = (float)samples[i];
                    fwrite(&sample, sizeof(float), 1, outputFile);
                }
                break;
            }
            default:
                printf("Unsupported sample type: %ld\n", channelInfo.type);
                break;
        }
        
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
    printf("Driver Version: %ld\n", driverInfo.driverVersion);
    
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
    char* driverName = nullptr;
    long inputChannel = 0;
    long samples = 48000;
    char* outputFilename = nullptr;
    double requestedRate = 0.0;

    struct poptOption options[] = {
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
        {"samples", 's', POPT_ARG_LONG, &samples, 0,
         "Number of samples to acquire (default: 48000)", "NUM"},
        {"output", 'o', POPT_ARG_STRING, &outputFilename, 0,
         "Output file (32-bit float raw PCM, default: output.raw)", "FILE"},
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
        "  ab_acq_asio -d \"ASIO4ALL v2\" -a -c 0 -s 96000 -o test.raw -r 48000\n");

    int rc = poptGetNextOpt(popt_ctx);
    if (rc < -1) {
        fprintf(stderr, "Error: %s: %s\n",
                poptBadOption(popt_ctx, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    // Set default output filename if not specified
    if (outputFilename == nullptr && acquireMode) {
        outputFilename = const_cast<char*>("output.raw");
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
                    printf("  %2ld: %s (Type: %ld)\n", i, channelInfo.name, channelInfo.type);
                }
            }

            printf("\nOutput Channels:\n");
            for (long i = 0; i < numOutputChannels; i++) {
                ASIOChannelInfo channelInfo;
                channelInfo.channel = i;
                channelInfo.isInput = ASIOFalse;
                if (ASIOGetChannelInfo(&channelInfo) == ASE_OK) {
                    printf("  %2ld: %s (Type: %ld)\n", i, channelInfo.name, channelInfo.type);
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

            // Open output file
            outputFile = fopen(outputFilename, "wb");
            if (!outputFile) {
                printf("Error: Cannot open output file: %s\n", outputFilename);
                shutdownASIO();
                poptFreeContext(popt_ctx);
                CoUninitialize();
                return 1;
            }
            
            printf("\nAcquiring %ld samples from channel %ld at %.0f Hz\n",
                   samples, inputChannel, currentSampleRate);
            printf("Output file: %s\n", outputFilename);
            printf("Format: 32-bit float raw PCM\n\n");
            
            // Setup buffers and start acquisition
            if (!setupASIOBuffers(inputChannel)) {
                fclose(outputFile);
                shutdownASIO();
                poptFreeContext(popt_ctx);
                CoUninitialize();
                return 1;
            }

            samplesToAcquire = samples;
            samplesAcquired = 0;
            acquisitionActive = true;

            ASIOError err = ASIOStart();
            if (err != ASE_OK) {
                printf("ASIOStart failed with error: %ld\n", err);
                fclose(outputFile);
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
            
            fclose(outputFile);
            printf("\nAcquisition complete. Output written to: %s\n", outputFilename);
            printf("To convert to WAV: ffmpeg -f f32le -ar %.0f -ac 1 -i %s output.wav\n",
                   currentSampleRate, outputFilename);
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
