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
#include <sndfile.h>
#include <fftw3.h>
#include <popt.h>

//------------------------------------------------------------------------------
//	Apply Hann window to reduce spectral leakage
//------------------------------------------------------------------------------
void apply_hann_window(double *data, int size)
{
    for (int i = 0; i < size; i++) {
        double window = 0.5 * (1.0 - cos(2.0 * M_PI * i / (size - 1)));
        data[i] *= window;
    }
}

//------------------------------------------------------------------------------
//	main application
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // Command-line option variables
    char *input_file = NULL;
    char *output_file = NULL;
    int fft_size = 8192;
    int sample_rate = 0;  // 0 means use file's native sample rate
    int quiet = 0;  // 0 = show diagnostic output, 1 = quiet mode
    int avg_count = 1;  // Number of FFTs to average (default: 1 = no averaging)
    int version_flag = 0;

    // Define popt options table
    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0,
            "Show version information", NULL},
        {"input", 'i', POPT_ARG_STRING, &input_file, 0,
            "Input WAV file", "FILE"},
        {"output", 'o', POPT_ARG_STRING, &output_file, 0,
            "Output CSV file (default: stdout)", "FILE"},
        {"fft-size", 'f', POPT_ARG_INT, &fft_size, 0,
            "FFT size (default: 8192)", "SIZE"},
        {"sample-rate", 's', POPT_ARG_INT, &sample_rate, 0,
            "Sample rate in Hz (default: use file's native rate)", "RATE"},
        {"average", 'a', POPT_ARG_INT, &avg_count, 0,
            "Number of overlapping FFTs to average (default: 1)", "COUNT"},
        {"quiet", 'q', POPT_ARG_NONE, &quiet, 0,
            "Quiet mode: suppress diagnostic output", NULL},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    // Parse command-line options
    poptContext opt_context = poptGetContext(NULL, argc, (const char **)argv, options, 0);
    poptSetOtherOptionHelp(opt_context, "[OPTIONS]");

    int rc;
    if ((rc = poptGetNextOpt(opt_context)) < -1) {
        fprintf(stderr, "Error parsing options: %s: %s\n",
                poptBadOption(opt_context, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
        poptFreeContext(opt_context);
        return 1;
    }

    // Handle version mode
    if (version_flag) {
        printf("ab_wav_fft version 1.0.0\n");
        printf("WAV file FFT analyzer for audio-bench\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        poptFreeContext(opt_context);
        return 0;
    }

    // Validate required parameters
    if (!input_file) {
        fprintf(stderr, "Error: Input file is required\n");
        poptPrintUsage(opt_context, stderr, 0);
        poptFreeContext(opt_context);
        return 1;
    }

    // Validate FFT size (must be positive and power of 2 for efficiency)
    if (fft_size <= 0) {
        fprintf(stderr, "Error: FFT size must be positive\n");
        poptFreeContext(opt_context);
        return 1;
    }

    // Validate average count
    if (avg_count < 1) {
        fprintf(stderr, "Error: Average count must be at least 1\n");
        poptFreeContext(opt_context);
        return 1;
    }

    poptFreeContext(opt_context);

    // Open the audio file
    SF_INFO sfinfo;
    SNDFILE *infile = sf_open(input_file, SFM_READ, &sfinfo);

    if (!infile) {
        fprintf(stderr, "Error: Could not open file '%s'\n", input_file);
        fprintf(stderr, "%s\n", sf_strerror(NULL));
        return 1;
    }

    // Use specified sample rate if provided, otherwise use file's native rate
    int effective_sample_rate = (sample_rate > 0) ? sample_rate : sfinfo.samplerate;

    // Determine bit depth and calculate epsilon (noise floor)
    int bit_depth = 0;
    double epsilon = 1e-10;  // Default epsilon

    int format_subtype = sfinfo.format & SF_FORMAT_SUBMASK;
    switch (format_subtype) {
        case SF_FORMAT_PCM_S8:
        case SF_FORMAT_PCM_U8:
            bit_depth = 8;
            epsilon = pow(10.0, -48.0 / 20.0);  // -48 dB for 8-bit
            break;
        case SF_FORMAT_PCM_16:
            bit_depth = 16;
            epsilon = pow(10.0, -96.0 / 20.0);  // -96 dB for 16-bit
            break;
        case SF_FORMAT_PCM_24:
            bit_depth = 24;
            epsilon = pow(10.0, -144.0 / 20.0); // -144 dB for 24-bit
            break;
        case SF_FORMAT_PCM_32:
            bit_depth = 32;
            epsilon = pow(10.0, -192.0 / 20.0); // -192 dB for 32-bit
            break;
        case SF_FORMAT_FLOAT:
            bit_depth = 32;
            epsilon = 1e-10;  // Use small value for float
            break;
        case SF_FORMAT_DOUBLE:
            bit_depth = 64;
            epsilon = 1e-10;  // Use small value for double
            break;
        default:
            bit_depth = 16;  // Assume 16-bit if unknown
            epsilon = pow(10.0, -96.0 / 20.0);
            break;
    }

    // Open output file if specified, otherwise use stdout
    FILE *outfile = stdout;
    if (output_file) {
        outfile = fopen(output_file, "w");
        if (!outfile) {
            fprintf(stderr, "Error: Could not open output file '%s'\n", output_file);
            sf_close(infile);
            return 1;
        }
    }

    if (!quiet) {
        fprintf(outfile, "File: %s\n", input_file);
        fprintf(outfile, "Sample rate: %d Hz", sfinfo.samplerate);
        if (sample_rate > 0 && sample_rate != sfinfo.samplerate) {
            fprintf(outfile, " (overridden to %d Hz)", effective_sample_rate);
        }
        fprintf(outfile, "\n");
        fprintf(outfile, "Channels: %d\n", sfinfo.channels);
        fprintf(outfile, "Bit depth: %d\n", bit_depth);
        fprintf(outfile, "Frames: %ld\n", (long)sfinfo.frames);
        fprintf(outfile, "FFT size: %d\n", fft_size);
        if (avg_count > 1) {
            fprintf(outfile, "FFT averaging: %d windows (50%% overlap)\n", avg_count);
        }
        fprintf(outfile, "\n");
    }

    // Allocate buffers
    double *audio_buffer = (double *)malloc(fft_size * sizeof(double));
    fftw_complex *fft_output = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * (fft_size / 2 + 1));
    double *power_spectrum = (double *)calloc(fft_size / 2 + 1, sizeof(double));  // Accumulator for averaged power

    if (!audio_buffer || !fft_output || !power_spectrum) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        sf_close(infile);
        if (output_file && outfile != stdout) {
            fclose(outfile);
        }
        return 1;
    }

    // Create FFTW plan
    fftw_plan plan = fftw_plan_dft_r2c_1d(fft_size, audio_buffer, fft_output, FFTW_ESTIMATE);

    // Calculate hop size (50% overlap)
    int hop_size = fft_size / 2;
    sf_count_t total_frames_read = 0;

    // Perform sliding window FFT averaging
    for (int window = 0; window < avg_count; window++) {
        // Seek to the start position for this window
        sf_count_t start_frame = window * hop_size;
        sf_seek(infile, start_frame, SEEK_SET);

        // Read audio data (if stereo, convert to mono by averaging channels)
        sf_count_t frames_read;
        if (sfinfo.channels == 1) {
            frames_read = sf_read_double(infile, audio_buffer, fft_size);
        } else {
            // Read interleaved multi-channel data and average to mono
            double *temp_buffer = (double *)malloc(fft_size * sfinfo.channels * sizeof(double));
            if (!temp_buffer) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                free(audio_buffer);
                fftw_free(fft_output);
                free(power_spectrum);
                fftw_destroy_plan(plan);
                sf_close(infile);
                if (output_file && outfile != stdout) {
                    fclose(outfile);
                }
                return 1;
            }

            frames_read = sf_read_double(infile, temp_buffer, fft_size * sfinfo.channels) / sfinfo.channels;

            for (int i = 0; i < frames_read; i++) {
                audio_buffer[i] = 0.0;
                for (int ch = 0; ch < sfinfo.channels; ch++) {
                    audio_buffer[i] += temp_buffer[i * sfinfo.channels + ch];
                }
                audio_buffer[i] /= sfinfo.channels;
            }
            free(temp_buffer);
        }

        // Zero-pad if we didn't read enough samples
        for (sf_count_t i = frames_read; i < fft_size; i++) {
            audio_buffer[i] = 0.0;
        }

        // Track total frames for diagnostic output
        if (window == 0) {
            total_frames_read = frames_read + (avg_count - 1) * hop_size;
        }

        // Apply window function
        apply_hann_window(audio_buffer, fft_size);

        // Execute FFT
        fftw_execute(plan);

        // Accumulate power spectrum (magnitude squared)
        for (int i = 0; i < fft_size / 2 + 1; i++) {
            double real = fft_output[i][0];
            double imag = fft_output[i][1];
            double magnitude = sqrt(real * real + imag * imag);
            power_spectrum[i] += magnitude * magnitude;
        }
    }

    if (!quiet) {
        fprintf(outfile, "Processed %ld frames across %d windows\n\n", (long)total_frames_read, avg_count);
    }

    // Calculate and display averaged magnitude spectrum
    fprintf(outfile, "\"Frequency (Hz)\",\"Magnitude (dBFS)\"\n");

    double freq_resolution = (double)effective_sample_rate / fft_size;

    for (int i = 0; i < fft_size / 2 + 1; i++) {
        // Average the power spectrum
        double avg_power = power_spectrum[i] / avg_count;
        // Convert back to magnitude
        double magnitude = sqrt(avg_power);
        // Normalize for FFT size and Hann window (coherent gain = 0.5)
        // Division by (fft_size / 4.0) = (fft_size / 2.0) / 0.5
        double magnitude_db = 20.0 * log10(magnitude / (fft_size / 4.0) + epsilon);
        double frequency = i * freq_resolution;

        fprintf(outfile, "%10.2f,%10.2f\n", frequency, magnitude_db);
    }

    // Cleanup power spectrum buffer
    free(power_spectrum);

    // Cleanup
    fftw_destroy_plan(plan);
    fftw_free(fft_output);
    free(audio_buffer);
    sf_close(infile);

    if (output_file && outfile != stdout) {
        fclose(outfile);
    }

    return 0;
}
