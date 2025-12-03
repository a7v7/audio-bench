//------------------------------------------------------------------------------
//	The MIT License (MIT)
//
//	Copyright (c) 2025 A.C. Verbeck
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files (the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//	THE SOFTWARE.
//------------------------------------------------------------------------------

/*
 * ab_freq_response_asio.cpp
 * ASIO Frequency Response Measurement Tool for audio-bench
 *
 * Windows-only ASIO interface for professional audio hardware
 * Measures frequency response using logarithmic sine sweep
 */

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstring>
#include <sndfile.h>
#include <fftw3.h>
#include <popt.h>
#include "asiosys.h"
#include "asio.h"
#include "iasiodrv.h"
#include "asiodrivers.h"

//------------------------------------------------------------------------------
// Configuration parameters
//------------------------------------------------------------------------------
#define DESIRED_SWEEP_DURATION	5.0										//	seconds (will be adjusted to power of 2)
#define START_FREQ				20.0
#define END_FREQ				20000.0
#define SWEEP_LEVEL_DB			-12.0									//	Output level in dB (negative = below 0dBFS)
#define LEAD_IN_DURATION		0.2										//	Silence before sweep (seconds) - prevents startup pop

//------------------------------------------------------------------------------
// Global ASIO state
//------------------------------------------------------------------------------
static IASIO* asioDriver = nullptr;
static ASIODriverInfo driverInfo;
static ASIOBufferInfo bufferInfos[2];									//	Input and output channels
static ASIOCallbacks asioCallbacks;
static long numInputChannels = 0;
static long numOutputChannels = 0;
static long preferredBufferSize = 0;
static long minBufferSize = 0;
static long maxBufferSize = 0;
static long bufferGranularity = 0;
static ASIOSampleRate currentSampleRate = 48000.0;
static bool measurementActive = false;

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
// Pre-converted sweep signal in ASIO output format (eliminates conversion in callback)
//------------------------------------------------------------------------------
static void* sweepSignalASIO = nullptr;
static size_t sweepSignalASIOSize = 0;

//------------------------------------------------------------------------------
// Audio data structure
//------------------------------------------------------------------------------
typedef struct {
    float *sweep_signal;											//	Generated sweep signal (float format, includes lead-in)
    float *recorded_signal;											//	Recorded response
    int sweep_length;												//	Total length including lead-in (samples)
    int sweep_only_length;											//	Length of just the sweep portion (samples)
    int lead_in_samples;											//	Lead-in silence samples
    int current_frame;												//	Current playback position
} AudioData;

static AudioData audioData;

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------
static void bufferSwitch(long index, ASIOBool processNow);
static void sampleRateChanged(ASIOSampleRate sRate);
static long asioMessages(long selector, long value, void* message, double* opt);
static ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long index, ASIOBool processNow);

//------------------------------------------------------------------------------
//	Name:		calculate_power_of_2_length
//
//	Returns:	Power of 2 length closest to desired duration
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Calculates sweep length that is power of 2
//	- Finds nearest power of 2 to desired duration
//	- Ensures efficient FFT processing
//------------------------------------------------------------------------------
static int calculate_power_of_2_length(double desired_duration, int sample_rate)
{
    int desired_samples = static_cast<int>(desired_duration * sample_rate);

//------------------------------------------------------------------------------
//	Find nearest power of 2
//------------------------------------------------------------------------------
    int power = 1;
    while (power < desired_samples) {
        power *= 2;
    }

//------------------------------------------------------------------------------
//	Check if previous power of 2 is closer
//------------------------------------------------------------------------------
    int prev_power = power / 2;
    if (abs(desired_samples - prev_power) < abs(desired_samples - power)) {
        power = prev_power;
    }

    return power;
}

//------------------------------------------------------------------------------
//	Name:		generate_log_sweep
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Generates logarithmic sine sweep (exponential chirp)
//	- Sweep from f1 to f2 over specified duration
//	- Uses exponential frequency progression
//	- Formula: φ(t) = 2π * f1 * L * (exp(t/L) - 1), where L = T/ln(f2/f1)
//	- Applies fade-in/out to prevent clicks at start/end
//------------------------------------------------------------------------------
void generate_log_sweep(float *buffer, int length, float fs, float f1, float f2)
{
    double duration = static_cast<double>(length) / fs;
    double L = duration / log(f2 / f1);

    // Fade duration: 50ms at start and end (smooth, visible fade)
    int fade_samples = static_cast<int>(0.05 * fs);  // 50ms
    if (fade_samples > length / 4) fade_samples = length / 4;  // Max 25% of signal

    for (int i = 0; i < length; i++) {
        double t = static_cast<double>(i) / fs;
        double phase = 2.0 * M_PI * f1 * L * (exp(t / L) - 1.0);
        float sample = static_cast<float>(sin(phase));

        // Apply fade-in (cosine fade from 0 to 1)
        if (i < fade_samples) {
            float fade = 0.5f * (1.0f - std::cos(M_PI * static_cast<float>(i) / fade_samples));
            sample *= fade;
        }

        // Apply fade-out (cosine fade from 1 to 0)
        if (i >= length - fade_samples) {
            int fade_idx = i - (length - fade_samples);
            float fade = 0.5f * (1.0f + std::cos(M_PI * static_cast<float>(fade_idx) / fade_samples));
            sample *= fade;
        }

        buffer[i] = sample;
    }
}

//------------------------------------------------------------------------------
//	Name:		convertASIOToFloat
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Converts ASIO sample format to float
//	- Handles various ASIO sample types
//	- Normalizes to -1.0 to +1.0 range
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
                int s24 = (samples[i*3] << 8) | (samples[i*3+1] << 16) | (samples[i*3+2] << 24);
                floatBuffer[i] = s24 / 2147483648.0f;
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
//	Name:		convertFloatToASIO
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Converts float samples to ASIO sample format
//	- Handles various ASIO sample types
//	- Assumes input is normalized -1.0 to +1.0
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
                int s32 = static_cast<int>(sample * 8388607.0f);
                samples[i*3] = (s32 >> 8) & 0xFF;
                samples[i*3+1] = (s32 >> 16) & 0xFF;
                samples[i*3+2] = (s32 >> 24) & 0xFF;
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
    if (!measurementActive || !tempInBuffer || !sweepSignalASIO) {
        return nullptr;
    }

    // Use pre-allocated buffers and cached values (no allocation or calculation in callback!)
    long bufferSize = preferredBufferSize;
    long samplesToProcess = bufferSize;

    if (audioData.current_frame + samplesToProcess > audioData.sweep_length) {
        samplesToProcess = audioData.sweep_length - audioData.current_frame;
    }

    // Prepare output buffer (direct copy from pre-converted ASIO format - no conversion!)
    if (audioData.current_frame < audioData.sweep_length && samplesToProcess > 0) {
        // Direct copy of pre-converted sweep signal to ASIO output buffer
        size_t offsetBytes = audioData.current_frame * outputSampleSize;
        memcpy(bufferInfos[1].buffers[index],
               (char*)sweepSignalASIO + offsetBytes,
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
        measurementActive = false;
    }

    // Convert input from ASIO format to float (done after output for better timing)
    convertASIOToFloat(bufferInfos[0].buffers[index], tempInBuffer, bufferSize, inputChannelInfo.type);

    // Copy recorded input to buffer (after output processing)
    if (audioData.current_frame <= audioData.sweep_length && audioData.current_frame > 0) {
        long copyFrame = audioData.current_frame - samplesToProcess;
        if (copyFrame >= 0 && copyFrame + samplesToProcess <= audioData.sweep_length) {
            memcpy(&audioData.recorded_signal[copyFrame], tempInBuffer, samplesToProcess * sizeof(float));
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

static bool setupASIOBuffers(long inputChannel, long outputChannel, long requestedBufferSize, double requestedSampleRate)
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

    // Validate and adjust buffer size
    long actualBufferSize = preferredBufferSize;  // Default to driver's preferred size

    if (requestedBufferSize > 0) {
        if (requestedBufferSize < minBufferSize) {
            printf("Warning: Requested buffer size %ld too small, using minimum %ld\n",
                   requestedBufferSize, minBufferSize);
            actualBufferSize = minBufferSize;
        } else if (requestedBufferSize > maxBufferSize) {
            printf("Warning: Requested buffer size %ld too large, using maximum %ld\n",
                   requestedBufferSize, maxBufferSize);
            actualBufferSize = maxBufferSize;
        } else {
            // Adjust to granularity if needed
            if (bufferGranularity > 0) {
                long remainder = (requestedBufferSize - minBufferSize) % bufferGranularity;
                if (remainder != 0) {
                    actualBufferSize = requestedBufferSize - remainder;
                    printf("Warning: Adjusted buffer size from %ld to %ld to match granularity %ld\n",
                           requestedBufferSize, actualBufferSize, bufferGranularity);
                } else {
                    actualBufferSize = requestedBufferSize;
                }
            } else {
                actualBufferSize = requestedBufferSize;
            }
        }
    }

    preferredBufferSize = actualBufferSize;
    printf("Using buffer size: %ld samples\n", preferredBufferSize);

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

    // Create buffers with validated size
    ASIOError err = ASIOCreateBuffers(bufferInfos, 2, preferredBufferSize, &asioCallbacks);
    if (err != ASE_OK) {
        printf("ASIOCreateBuffers failed with error: %ld\n", err);
        return false;
    }

    // Get and cache channel info (retrieved once, used in every callback)
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

    // Allocate temporary input conversion buffer (used in real-time callback)
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

    // Clean up pre-converted sweep signal
    if (sweepSignalASIO) {
        free(sweepSignalASIO);
        sweepSignalASIO = nullptr;
        sweepSignalASIOSize = 0;
    }
}

//------------------------------------------------------------------------------
//	Name:		preconvertSweepSignal
//
//	Returns:	true on success, false on failure
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Pre-converts the sweep signal from float to ASIO output format
//	- Called after sweep generation but before starting audio stream
//	- Eliminates conversion overhead in real-time callback
//------------------------------------------------------------------------------
static bool preconvertSweepSignal()
{
    if (!audioData.sweep_signal || audioData.sweep_length <= 0) {
        printf("Error: Sweep signal not initialized\n");
        return false;
    }

    // Calculate and cache sample size for output format (used in callback)
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

    // Allocate buffer for pre-converted sweep
    sweepSignalASIOSize = audioData.sweep_length * outputSampleSize;
    sweepSignalASIO = malloc(sweepSignalASIOSize);

    if (!sweepSignalASIO) {
        printf("Error: Failed to allocate ASIO sweep signal buffer (%zu bytes)\n", sweepSignalASIOSize);
        return false;
    }

    // Convert entire sweep signal to ASIO format
    convertFloatToASIO(audioData.sweep_signal, sweepSignalASIO, audioData.sweep_length, outputChannelInfo.type);

    printf("Pre-converted sweep signal to ASIO format: %d samples, %zu bytes (sample size: %zu)\n",
           audioData.sweep_length, sweepSignalASIOSize, outputSampleSize);

    return true;
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
//	Name:		calculate_frequency_response
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Calculates frequency response using FFT
//	- Compares recorded signal to sweep signal
//	- Outputs magnitude and phase response to CSV file
//	- Frequency range limited to START_FREQ to END_FREQ
//------------------------------------------------------------------------------
void calculate_frequency_response(float *input_signal, float *output_signal,
                                  int length, double sample_rate, const char* output_filename)
{
    int fft_size = length;

//------------------------------------------------------------------------------
//	Allocate FFTW arrays
//------------------------------------------------------------------------------
    double *in_sweep = static_cast<double*>(fftw_malloc(sizeof(double) * fft_size));
    double *in_recorded = static_cast<double*>(fftw_malloc(sizeof(double) * fft_size));
    fftw_complex *out_sweep = static_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * (fft_size/2 + 1)));
    fftw_complex *out_recorded = static_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * (fft_size/2 + 1)));

//------------------------------------------------------------------------------
//	Create FFTW plans
//------------------------------------------------------------------------------
    fftw_plan plan_sweep = fftw_plan_dft_r2c_1d(fft_size, in_sweep, out_sweep, FFTW_ESTIMATE);
    fftw_plan plan_recorded = fftw_plan_dft_r2c_1d(fft_size, in_recorded, out_recorded, FFTW_ESTIMATE);

//------------------------------------------------------------------------------
//	Copy input data
//------------------------------------------------------------------------------
    for (int i = 0; i < length; i++) {
        in_sweep[i] = static_cast<double>(input_signal[i]);
        in_recorded[i] = static_cast<double>(output_signal[i]);
    }

//------------------------------------------------------------------------------
//	Zero pad if necessary
//------------------------------------------------------------------------------
    for (int i = length; i < fft_size; i++) {
        in_sweep[i] = 0.0;
        in_recorded[i] = 0.0;
    }

//------------------------------------------------------------------------------
//	Execute FFTs
//------------------------------------------------------------------------------
    fftw_execute(plan_sweep);
    fftw_execute(plan_recorded);

//------------------------------------------------------------------------------
//	Calculate frequency response and write to CSV file
//------------------------------------------------------------------------------
    FILE *fp = fopen(output_filename, "w");
    if (fp) {
        fprintf(fp, "Frequency (Hz),Magnitude (dB),Phase (degrees)\n");

        for (int i = 1; i < fft_size/2 + 1; i++) {
            double freq = static_cast<double>(i) * sample_rate / fft_size;

//------------------------------------------------------------------------------
//	Skip DC and frequencies outside our range
//------------------------------------------------------------------------------
            if (freq < START_FREQ || freq > END_FREQ) continue;

//------------------------------------------------------------------------------
//	Calculate complex division: recorded / sweep
//------------------------------------------------------------------------------
            double sweep_real = out_sweep[i][0];
            double sweep_imag = out_sweep[i][1];
            double rec_real = out_recorded[i][0];
            double rec_imag = out_recorded[i][1];

            double sweep_mag_sq = sweep_real * sweep_real + sweep_imag * sweep_imag;

            if (sweep_mag_sq > 1e-10) {									//	Avoid division by zero
//------------------------------------------------------------------------------
//	Complex division
//------------------------------------------------------------------------------
                double h_real = (rec_real * sweep_real + rec_imag * sweep_imag) / sweep_mag_sq;
                double h_imag = (rec_imag * sweep_real - rec_real * sweep_imag) / sweep_mag_sq;

//------------------------------------------------------------------------------
//	Magnitude in dB
//------------------------------------------------------------------------------
                double magnitude = sqrt(h_real * h_real + h_imag * h_imag);
                double magnitude_db = 20.0 * log10(magnitude + 1e-10);

//------------------------------------------------------------------------------
//	Phase in degrees
//------------------------------------------------------------------------------
                double phase = atan2(h_imag, h_real) * 180.0 / M_PI;

                fprintf(fp, "%.2f,%.2f,%.2f\n", freq, magnitude_db, phase);
            }
        }

        fclose(fp);
        printf("Frequency response saved to %s\n", output_filename);
    }

//------------------------------------------------------------------------------
//	Cleanup
//------------------------------------------------------------------------------
    fftw_destroy_plan(plan_sweep);
    fftw_destroy_plan(plan_recorded);
    fftw_free(in_sweep);
    fftw_free(in_recorded);
    fftw_free(out_sweep);
    fftw_free(out_recorded);
}

//------------------------------------------------------------------------------
//	Main application
//
//	This application:
//	- Generates logarithmic sine sweep
//	- Plays sweep through ASIO audio output
//	- Records ASIO audio input
//	- Calculates frequency response
//	- Saves results to CSV file
//
//	Libraries:
//	- ASIO SDK: Audio playback and recording
//	- FFTW3: FFT calculations
//	- libpopt: Command-line parsing
//------------------------------------------------------------------------------
int main(int argc, const char **argv)
{
    CoInitialize(nullptr);

//------------------------------------------------------------------------------
//	Command-line options
//------------------------------------------------------------------------------
    int version_flag = 0;
    int list_flag = 0;
    char* driverName = nullptr;
    char* outputFilename = nullptr;
    long inputChannel = 0;
    long outputChannel = 0;
    long requestedBufferSize = 0;
    double requestedSampleRate = 48000.0;

    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0, "Show version information", nullptr},
        {"list", 'l', POPT_ARG_NONE, &list_flag, 0, "List available ASIO drivers", nullptr},
        {"driver", 'd', POPT_ARG_STRING, &driverName, 0, "ASIO driver name", "NAME"},
        {"file", 'f', POPT_ARG_STRING, &outputFilename, 0, "Output CSV filename (default: frequency_response.csv)", "FILE"},
        {"input", 'i', POPT_ARG_LONG, &inputChannel, 0, "Input channel (default: 0)", "N"},
        {"output", 'o', POPT_ARG_LONG, &outputChannel, 0, "Output channel (default: 0)", "N"},
        {"buffer", 'b', POPT_ARG_LONG, &requestedBufferSize, 0, "ASIO buffer size in samples (default: driver preferred, larger = more stable)", "N"},
        {"rate", 'r', POPT_ARG_DOUBLE, &requestedSampleRate, 0, "Sample rate (default: 48000)", "HZ"},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(nullptr, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "ASIO Frequency Response Measurement Tool for audio-bench.\n\n"
        "This tool generates a logarithmic sine sweep, plays it through\n"
        "the ASIO audio interface, records the response, and calculates the\n"
        "frequency response.\n\n"
        "Examples:\n"
        "  ab_freq_response_asio --list                        # List ASIO drivers\n"
        "  ab_freq_response_asio -d \"Driver Name\"             # Run measurement\n"
        "  ab_freq_response_asio -d \"Driver\" -i 0 -o 0       # Specify channels\n"
        "  ab_freq_response_asio -d \"Driver\" -f output.csv   # Custom output file\n"
        "  ab_freq_response_asio -d \"Driver\" -b 2048         # Larger buffer (more stable)\n");

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
//	Handle version mode
//------------------------------------------------------------------------------
    if (version_flag) {
        printf("ab_freq_response_asio version 1.0.0\n");
        printf("ASIO Frequency Response Measurement Tool for audio-bench\n");
        printf("Copyright (c) 2025 A.C. Verbeck\n");
        printf("Built: %s %s\n", __DATE__, __TIME__);
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

//------------------------------------------------------------------------------
//	Handle list mode
//------------------------------------------------------------------------------
    if (list_flag) {
        listASIODrivers();
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

//------------------------------------------------------------------------------
//	Validate driver selection
//------------------------------------------------------------------------------
    if (!driverName) {
        fprintf(stderr, "Error: ASIO driver name is required (use --list to see available drivers)\n");
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    poptFreeContext(popt_ctx);

//------------------------------------------------------------------------------
//	Set default output filename if not specified
//------------------------------------------------------------------------------
    if (!outputFilename) {
        outputFilename = const_cast<char*>("frequency_response.csv");
    }

    printf("ASIO Frequency Response Measurement Tool\n");
    printf("==========================================\n\n");

//------------------------------------------------------------------------------
//	Calculate power-of-2 sweep length
//------------------------------------------------------------------------------
    audioData.sweep_only_length = calculate_power_of_2_length(DESIRED_SWEEP_DURATION, static_cast<int>(requestedSampleRate));
    audioData.lead_in_samples = static_cast<int>(LEAD_IN_DURATION * requestedSampleRate);
    audioData.sweep_length = audioData.lead_in_samples + audioData.sweep_only_length;

    double actual_duration = static_cast<double>(audioData.sweep_only_length) / requestedSampleRate;
    double total_duration = static_cast<double>(audioData.sweep_length) / requestedSampleRate;

    audioData.current_frame = 0;

    printf("Lead-in: %.3f seconds (%d samples)\n", LEAD_IN_DURATION, audioData.lead_in_samples);
    printf("Sweep length: %d samples (power of 2: 2^%d)\n",
           audioData.sweep_only_length, static_cast<int>(log2(audioData.sweep_only_length)));
    printf("Sweep duration: %.3f seconds\n", actual_duration);
    printf("Total duration: %.3f seconds\n", total_duration);
    printf("FFT frequency resolution: %.3f Hz\n\n",
           requestedSampleRate / audioData.sweep_only_length);

//------------------------------------------------------------------------------
//	Allocate buffers (includes lead-in silence)
//------------------------------------------------------------------------------
    audioData.sweep_signal = static_cast<float*>(calloc(audioData.sweep_length, sizeof(float)));  // calloc zeros memory
    audioData.recorded_signal = static_cast<float*>(calloc(audioData.sweep_length, sizeof(float)));

    if (!audioData.sweep_signal || !audioData.recorded_signal) {
        fprintf(stderr, "Failed to allocate memory\n");
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//	Generate sweep signal (after lead-in silence)
//------------------------------------------------------------------------------
    printf("Generating %d Hz to %d Hz logarithmic sweep (%.3f seconds)...\n",
           static_cast<int>(START_FREQ), static_cast<int>(END_FREQ), actual_duration);
    generate_log_sweep(audioData.sweep_signal + audioData.lead_in_samples, audioData.sweep_only_length,
                      static_cast<float>(requestedSampleRate), START_FREQ, END_FREQ);

//------------------------------------------------------------------------------
//	Apply output level adjustment (prevent DAC clipping) - only to sweep portion
//------------------------------------------------------------------------------
    float level_linear = powf(10.0f, SWEEP_LEVEL_DB / 20.0f);
    printf("Applying output level: %.1f dB (gain: %.3f)\n", SWEEP_LEVEL_DB, level_linear);
    for (int i = audioData.lead_in_samples; i < audioData.sweep_length; i++) {
        audioData.sweep_signal[i] *= level_linear;
    }

//------------------------------------------------------------------------------
//	Validate and save sweep signal for inspection
//------------------------------------------------------------------------------
    printf("Validating sweep signal...\n");

    // Check for NaN, Inf, and discontinuities
    int nan_count = 0, inf_count = 0;
    float max_val = -1.0f, min_val = 1.0f;
    float max_delta = 0.0f;

    for (int i = 0; i < audioData.sweep_length; i++) {
        float sample = audioData.sweep_signal[i];

        if (std::isnan(sample)) nan_count++;
        if (std::isinf(sample)) inf_count++;

        if (sample > max_val) max_val = sample;
        if (sample < min_val) min_val = sample;

        if (i > 0) {
            float delta = std::fabs(sample - audioData.sweep_signal[i-1]);
            if (delta > max_delta) max_delta = delta;
        }
    }

    printf("  Range: %.6f to %.6f\n", min_val, max_val);
    printf("  Max sample-to-sample delta: %.6f\n", max_delta);
    printf("  Start samples: %.6f, %.6f, %.6f\n",
           audioData.sweep_signal[0], audioData.sweep_signal[1], audioData.sweep_signal[2]);
    printf("  End samples: %.6f, %.6f, %.6f\n",
           audioData.sweep_signal[audioData.sweep_length-3],
           audioData.sweep_signal[audioData.sweep_length-2],
           audioData.sweep_signal[audioData.sweep_length-1]);

    if (nan_count > 0) printf("  WARNING: %d NaN values detected!\n", nan_count);
    if (inf_count > 0) printf("  WARNING: %d Inf values detected!\n", inf_count);
    if (max_delta > 0.5f) printf("  WARNING: Large discontinuity detected (%.6f)!\n", max_delta);

    // Save sweep to WAV file for inspection
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    sfinfo.samplerate = static_cast<int>(requestedSampleRate);
    sfinfo.channels = 1;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

    SNDFILE* outfile = sf_open("sweep_debug.wav", SFM_WRITE, &sfinfo);
    if (outfile) {
        sf_write_float(outfile, audioData.sweep_signal, audioData.sweep_length);
        sf_close(outfile);
        printf("  Saved sweep to sweep_debug.wav for inspection\n");
    } else {
        printf("  Warning: Could not save sweep debug file\n");
    }

//------------------------------------------------------------------------------
//	Initialize ASIO
//------------------------------------------------------------------------------
    if (!initASIO(driverName)) {
        fprintf(stderr, "Failed to initialize ASIO driver\n");
        free(audioData.sweep_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//	Validate channel selections
//------------------------------------------------------------------------------
    if (inputChannel >= numInputChannels) {
        fprintf(stderr, "Error: Input channel %ld out of range (0-%ld)\n", inputChannel, numInputChannels - 1);
        shutdownASIO();
        free(audioData.sweep_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

    if (outputChannel >= numOutputChannels) {
        fprintf(stderr, "Error: Output channel %ld out of range (0-%ld)\n", outputChannel, numOutputChannels - 1);
        shutdownASIO();
        free(audioData.sweep_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//	Setup ASIO buffers
//------------------------------------------------------------------------------
    printf("\n");
    if (!setupASIOBuffers(inputChannel, outputChannel, requestedBufferSize, requestedSampleRate)) {
        fprintf(stderr, "Failed to setup ASIO buffers\n");
        shutdownASIO();
        free(audioData.sweep_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//	Pre-convert sweep signal to ASIO format (eliminates conversion in callback)
//------------------------------------------------------------------------------
    if (!preconvertSweepSignal()) {
        fprintf(stderr, "Failed to pre-convert sweep signal\n");
        shutdownASIO();
        free(audioData.sweep_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//	Start measurement
//------------------------------------------------------------------------------
    printf("\nStarting measurement...\n");
    printf("Make sure your audio interface input is connected to the output!\n\n");

    measurementActive = true;

    ASIOError err = ASIOStart();
    if (err != ASE_OK) {
        fprintf(stderr, "ASIOStart failed with error: %ld\n", err);
        shutdownASIO();
        free(audioData.sweep_signal);
        free(audioData.recorded_signal);
        CoUninitialize();
        return 1;
    }

//------------------------------------------------------------------------------
//	Wait for measurement to complete
//------------------------------------------------------------------------------
    while (measurementActive) {
        Sleep(100);
        printf("\rProgress: %d / %d frames", audioData.current_frame, audioData.sweep_length);
        fflush(stdout);
    }
    printf("\n\n");

//------------------------------------------------------------------------------
//	Stop ASIO
//------------------------------------------------------------------------------
    shutdownASIO();

    printf("Recording complete. Analyzing...\n");

//------------------------------------------------------------------------------
//	Calculate frequency response (skip lead-in, analyze only sweep portion)
//------------------------------------------------------------------------------
    calculate_frequency_response(audioData.sweep_signal + audioData.lead_in_samples,
                                 audioData.recorded_signal + audioData.lead_in_samples,
                                 audioData.sweep_only_length, currentSampleRate, outputFilename);

//------------------------------------------------------------------------------
//	Cleanup
//------------------------------------------------------------------------------
    free(audioData.sweep_signal);
    free(audioData.recorded_signal);

    printf("\nDone! Check %s for results.\n", outputFilename);
    printf("You can plot this data with gnuplot, Python, or Excel.\n");

    CoUninitialize();
    return 0;
}
