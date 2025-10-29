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

#include <stdio.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <portaudio.h>
#include <fftw3.h>
#include <popt.h>

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 512
#define DESIRED_SWEEP_DURATION 5.0  // seconds (will be adjusted to power of 2)
#define START_FREQ 20.0
#define END_FREQ 20000.0

// Calculate power-of-2 sweep length close to desired duration
static int calculate_power_of_2_length(double desired_duration, int sample_rate) {
    int desired_samples = (int)(desired_duration * sample_rate);
    
    // Find nearest power of 2
    int power = 1;
    while (power < desired_samples) {
        power *= 2;
    }
    
    // Check if previous power of 2 is closer
    int prev_power = power / 2;
    if (abs(desired_samples - prev_power) < abs(desired_samples - power)) {
        power = prev_power;
    }
    
    return power;
}

typedef struct {
    float *sweep_signal;
    float *recorded_signal;
    int sweep_length;
    int current_frame;
    int is_recording;
} AudioData;

// Generate logarithmic sine sweep
void generate_log_sweep(float *buffer, int length, float fs, float f1, float f2) {
    double duration = (double)length / fs;
    double k = duration * f1;
    double L = duration / log(f2 / f1);
    
    for (int i = 0; i < length; i++) {
        double t = (double)i / fs;
        double phase = 2.0 * M_PI * k * (exp(t / L) - 1.0);
        buffer[i] = (float)sin(phase);
    }
}

// PortAudio callback function
static int audioCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData) {
    AudioData *data = (AudioData*)userData;
    float *out = (float*)outputBuffer;
    const float *in = (const float*)inputBuffer;
    
    (void) timeInfo;
    (void) statusFlags;
    
    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        // Play sweep signal
        if (data->current_frame < data->sweep_length) {
            *out++ = data->sweep_signal[data->current_frame];
            
            // Record input signal
            if (in) {
                data->recorded_signal[data->current_frame] = *in++;
            }
            
            data->current_frame++;
        } else {
            *out++ = 0.0f;
            if (in) in++;
        }
    }
    
    if (data->current_frame >= data->sweep_length) {
        return paComplete;
    }
    
    return paContinue;
}

// Calculate frequency response using FFT
void calculate_frequency_response(float *input_signal, float *output_signal, 
                                  int length, double sample_rate) {
    int fft_size = length;
    
    // Allocate FFTW arrays
    double *in_sweep = fftw_malloc(sizeof(double) * fft_size);
    double *in_recorded = fftw_malloc(sizeof(double) * fft_size);
    fftw_complex *out_sweep = fftw_malloc(sizeof(fftw_complex) * (fft_size/2 + 1));
    fftw_complex *out_recorded = fftw_malloc(sizeof(fftw_complex) * (fft_size/2 + 1));
    
    // Create FFTW plans
    fftw_plan plan_sweep = fftw_plan_dft_r2c_1d(fft_size, in_sweep, out_sweep, FFTW_ESTIMATE);
    fftw_plan plan_recorded = fftw_plan_dft_r2c_1d(fft_size, in_recorded, out_recorded, FFTW_ESTIMATE);
    
    // Copy input data
    for (int i = 0; i < length; i++) {
        in_sweep[i] = (double)input_signal[i];
        in_recorded[i] = (double)output_signal[i];
    }
    
    // Zero pad if necessary
    for (int i = length; i < fft_size; i++) {
        in_sweep[i] = 0.0;
        in_recorded[i] = 0.0;
    }
    
    // Execute FFTs
    fftw_execute(plan_sweep);
    fftw_execute(plan_recorded);
    
    // Calculate frequency response and write to CSV file
    FILE *fp = fopen("frequency_response.csv", "w");
    if (fp) {
        fprintf(fp, "Frequency (Hz),Magnitude (dB),Phase (degrees)\n");
        
        for (int i = 1; i < fft_size/2 + 1; i++) {
            double freq = (double)i * sample_rate / fft_size;
            
            // Skip DC and frequencies outside our range
            if (freq < START_FREQ || freq > END_FREQ) continue;
            
            // Calculate complex division: recorded / sweep
            double sweep_real = out_sweep[i][0];
            double sweep_imag = out_sweep[i][1];
            double rec_real = out_recorded[i][0];
            double rec_imag = out_recorded[i][1];
            
            double sweep_mag_sq = sweep_real * sweep_real + sweep_imag * sweep_imag;
            
            if (sweep_mag_sq > 1e-10) {  // Avoid division by zero
                // Complex division
                double h_real = (rec_real * sweep_real + rec_imag * sweep_imag) / sweep_mag_sq;
                double h_imag = (rec_imag * sweep_real - rec_real * sweep_imag) / sweep_mag_sq;
                
                // Magnitude in dB
                double magnitude = sqrt(h_real * h_real + h_imag * h_imag);
                double magnitude_db = 20.0 * log10(magnitude + 1e-10);
                
                // Phase in degrees
                double phase = atan2(h_imag, h_real) * 180.0 / M_PI;
                
                fprintf(fp, "%.2f,%.2f,%.2f\n", freq, magnitude_db, phase);
            }
        }
        
        fclose(fp);
        printf("Frequency response saved to frequency_response.csv\n");
    }
    
    // Cleanup
    fftw_destroy_plan(plan_sweep);
    fftw_destroy_plan(plan_recorded);
    fftw_free(in_sweep);
    fftw_free(in_recorded);
    fftw_free(out_sweep);
    fftw_free(out_recorded);
}

int main(int argc, const char **argv) {
    /* Command-line options */
    int version_flag = 0;

    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0,
         "Show version information", NULL},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(NULL, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "Frequency Response Measurement Tool for audio-bench.\n\n"
        "This tool generates a logarithmic sine sweep, plays it through\n"
        "the audio interface, records the response, and calculates the\n"
        "frequency response.\n\n"
        "Example:\n"
        "  ab_freq_response           # Run frequency response measurement\n");

    int rc = poptGetNextOpt(popt_ctx);
    if (rc < -1) {
        fprintf(stderr, "Error: %s: %s\n",
                poptBadOption(popt_ctx, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
        poptFreeContext(popt_ctx);
        return 1;
    }

    /* Handle version mode */
    if (version_flag) {
        printf("ab_freq_response version 1.0.0\n");
        printf("Frequency Response Measurement Tool for audio-bench\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        poptFreeContext(popt_ctx);
        return 0;
    }

    poptFreeContext(popt_ctx);

    PaError err;
    PaStream *stream;
    AudioData data;

    printf("Frequency Response Measurement Tool\n");
    printf("====================================\n\n");
    
    // Calculate power-of-2 sweep length
    data.sweep_length = calculate_power_of_2_length(DESIRED_SWEEP_DURATION, SAMPLE_RATE);
    double actual_duration = (double)data.sweep_length / SAMPLE_RATE;
    
    data.current_frame = 0;
    data.is_recording = 1;
    
    printf("Sweep length: %d samples (power of 2: 2^%d)\n", 
           data.sweep_length, (int)log2(data.sweep_length));
    printf("Actual duration: %.3f seconds\n", actual_duration);
    printf("FFT frequency resolution: %.3f Hz\n\n", 
           (double)SAMPLE_RATE / data.sweep_length);
    
    // Allocate buffers
    data.sweep_signal = (float*)malloc(data.sweep_length * sizeof(float));
    data.recorded_signal = (float*)calloc(data.sweep_length, sizeof(float));
    
    if (!data.sweep_signal || !data.recorded_signal) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }
    
    // Generate sweep signal
    printf("Generating %d Hz to %d Hz logarithmic sweep (%.3f seconds)...\n", 
           (int)START_FREQ, (int)END_FREQ, actual_duration);
    generate_log_sweep(data.sweep_signal, data.sweep_length, SAMPLE_RATE, START_FREQ, END_FREQ);
    
    // Initialize PortAudio
    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return 1;
    }
    
    // Open audio stream (duplex - both input and output)
    err = Pa_OpenDefaultStream(&stream,
                               1,          // input channels
                               1,          // output channels
                               paFloat32,  // sample format
                               SAMPLE_RATE,
                               FRAMES_PER_BUFFER,
                               audioCallback,
                               &data);
    
    if (err != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return 1;
    }
    
    // Start stream
    printf("Starting measurement...\n");
    printf("Make sure your audio interface input is connected to the output!\n\n");
    
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
    }
    
    // Wait for stream to complete
    while (Pa_IsStreamActive(stream)) {
        Pa_Sleep(100);
        printf("\rProgress: %d / %d frames", data.current_frame, data.sweep_length);
        fflush(stdout);
    }
    printf("\n\n");
    
    // Stop and close stream
    err = Pa_StopStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
    }
    
    Pa_CloseStream(stream);
    Pa_Terminate();
    
    printf("Recording complete. Analyzing...\n");
    
    // Calculate frequency response
    calculate_frequency_response(data.sweep_signal, data.recorded_signal, 
                                 data.sweep_length, SAMPLE_RATE);
    
    // Cleanup
    free(data.sweep_signal);
    free(data.recorded_signal);
    
    printf("\nDone! Check frequency_response.csv for results.\n");
    printf("You can plot this data with gnuplot, Python, or Excel.\n");
    
    return 0;
}
