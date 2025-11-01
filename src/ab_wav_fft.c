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
#include <string.h>
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
//	Extract the root filename without extension
//------------------------------------------------------------------------------
void get_root_filename(const char *filename, char *root, size_t root_size)
{
    const char *last_slash = strrchr(filename, '/');
    if (!last_slash) {
        last_slash = strrchr(filename, '\\');
    }
    const char *base = last_slash ? last_slash + 1 : filename;

    const char *last_dot = strrchr(base, '.');
    if (last_dot) {
        size_t len = last_dot - base;
        if (len >= root_size) len = root_size - 1;
        strncpy(root, base, len);
        root[len] = '\0';
    } else {
        strncpy(root, base, root_size - 1);
        root[root_size - 1] = '\0';
    }
}

//------------------------------------------------------------------------------
//	Generate output filename with timestamp
//------------------------------------------------------------------------------
void generate_output_filename(const char *root, int time_ms, char *output, size_t output_size)
{
    snprintf(output, output_size, "%s_%04dms.csv", root, time_ms);
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
    int interval_ms = 0;  // Interval in milliseconds for snapshots (0 = single FFT mode)
    int version_flag = 0;

    // Define popt options table
    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0,
            "Show version information", NULL},
        {"input", 'i', POPT_ARG_STRING, &input_file, 0,
            "Input WAV file", "FILE"},
        {"output", 'o', POPT_ARG_STRING, &output_file, 0,
            "Output CSV file or root name for interval mode", "FILE"},
        {"fft-size", 'f', POPT_ARG_INT, &fft_size, 0,
            "FFT size (default: 8192)", "SIZE"},
        {"sample-rate", 's', POPT_ARG_INT, &sample_rate, 0,
            "Sample rate in Hz (default: use file's native rate)", "RATE"},
        {"average", 'a', POPT_ARG_INT, &avg_count, 0,
            "Number of overlapping FFTs to average (default: 1)", "COUNT"},
        {"interval", 't', POPT_ARG_INT, &interval_ms, 0,
            "Take FFT every N milliseconds (creates multiple files)", "MS"},
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

    // Validate interval
    if (interval_ms < 0) {
        fprintf(stderr, "Error: Interval must be non-negative\n");
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

    // Print file information (to stderr in interval mode, to output file otherwise)
    if (!quiet) {
        FILE *info_out = (interval_ms > 0) ? stderr : stdout;
        fprintf(info_out, "File: %s\n", input_file);
        fprintf(info_out, "Sample rate: %d Hz", sfinfo.samplerate);
        if (sample_rate > 0 && sample_rate != sfinfo.samplerate) {
            fprintf(info_out, " (overridden to %d Hz)", effective_sample_rate);
        }
        fprintf(info_out, "\n");
        fprintf(info_out, "Channels: %d\n", sfinfo.channels);
        fprintf(info_out, "Bit depth: %d\n", bit_depth);
        fprintf(info_out, "Frames: %ld\n", (long)sfinfo.frames);
        fprintf(info_out, "Duration: %.2f seconds\n", (double)sfinfo.frames / sfinfo.samplerate);
        fprintf(info_out, "FFT size: %d\n", fft_size);
        if (interval_ms > 0) {
            fprintf(info_out, "Interval mode: FFT every %d ms\n", interval_ms);
        } else if (avg_count > 1) {
            fprintf(info_out, "FFT averaging: %d windows (50%% overlap)\n", avg_count);
        }
        fprintf(info_out, "\n");
    }

    // Determine output root name for interval mode
    char output_root[512] = "";
    if (interval_ms > 0) {
        if (output_file) {
            get_root_filename(output_file, output_root, sizeof(output_root));
        } else {
            get_root_filename(input_file, output_root, sizeof(output_root));
        }
    }

    // Allocate buffers
    double *audio_buffer = (double *)malloc(fft_size * sizeof(double));
    fftw_complex *fft_output = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * (fft_size / 2 + 1));
    double *power_spectrum = (double *)calloc(fft_size / 2 + 1, sizeof(double));  // Accumulator for averaged power

    if (!audio_buffer || !fft_output || !power_spectrum) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        sf_close(infile);
        return 1;
    }

    // Create FFTW plan
    fftw_plan plan = fftw_plan_dft_r2c_1d(fft_size, audio_buffer, fft_output, FFTW_ESTIMATE);

    // Calculate frequency resolution
    double freq_resolution = (double)effective_sample_rate / fft_size;

    // Determine snapshot times and loop structure
    int num_snapshots = 1;
    int *snapshot_times_ms = NULL;

    if (interval_ms > 0) {
        // Interval mode: calculate snapshot times
        double duration_sec = (double)sfinfo.frames / sfinfo.samplerate;
        double duration_ms = duration_sec * 1000.0;
        num_snapshots = (int)(duration_ms / interval_ms) + 1;

        snapshot_times_ms = (int *)malloc(num_snapshots * sizeof(int));
        for (int i = 0; i < num_snapshots; i++) {
            snapshot_times_ms[i] = i * interval_ms;
        }

        if (!quiet) {
            fprintf(stderr, "Generating %d snapshots...\n", num_snapshots);
        }
    } else {
        // Single FFT mode
        snapshot_times_ms = (int *)malloc(sizeof(int));
        snapshot_times_ms[0] = 0;
        num_snapshots = 1;
    }

    // Process each snapshot
    for (int snapshot = 0; snapshot < num_snapshots; snapshot++) {
        int time_ms = snapshot_times_ms[snapshot];

        // Calculate starting frame for this snapshot
        sf_count_t start_frame = (sf_count_t)((double)time_ms / 1000.0 * sfinfo.samplerate);

        // Check if we're past the end of the file
        if (start_frame >= sfinfo.frames) {
            break;
        }

        // Open output file for this snapshot
        FILE *outfile = stdout;
        char snapshot_filename[1024];

        if (interval_ms > 0) {
            // Interval mode: create timestamped file
            generate_output_filename(output_root, time_ms, snapshot_filename, sizeof(snapshot_filename));
            outfile = fopen(snapshot_filename, "w");
            if (!outfile) {
                fprintf(stderr, "Error: Could not open output file '%s'\n", snapshot_filename);
                continue;
            }
            if (!quiet) {
                fprintf(stderr, "Processing snapshot %d/%d at %d ms -> %s\n",
                        snapshot + 1, num_snapshots, time_ms, snapshot_filename);
            }
        } else if (output_file) {
            // Single FFT mode with output file
            outfile = fopen(output_file, "w");
            if (!outfile) {
                fprintf(stderr, "Error: Could not open output file '%s'\n", output_file);
                free(snapshot_times_ms);
                free(power_spectrum);
                fftw_destroy_plan(plan);
                fftw_free(fft_output);
                free(audio_buffer);
                sf_close(infile);
                return 1;
            }
        }

        // Clear power spectrum accumulator
        memset(power_spectrum, 0, (fft_size / 2 + 1) * sizeof(double));

        // Calculate hop size (50% overlap) for averaging
        int hop_size = fft_size / 2;

        // Perform sliding window FFT averaging
        int windows_to_average = avg_count;

        for (int window = 0; window < windows_to_average; window++) {
            // Seek to the start position for this window
            sf_count_t window_start_frame = start_frame + (window * hop_size);
            sf_seek(infile, window_start_frame, SEEK_SET);

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
                    if (outfile != stdout) {
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

        // Write averaged magnitude spectrum for this snapshot
        fprintf(outfile, "\"Frequency (Hz)\",\"Magnitude (dBFS)\"\n");

        for (int i = 0; i < fft_size / 2 + 1; i++) {
            // Average the power spectrum
            double avg_power = power_spectrum[i] / windows_to_average;
            // Convert back to magnitude
            double magnitude = sqrt(avg_power);
            // Normalize for FFT size and Hann window (coherent gain = 0.5)
            // Division by (fft_size / 4.0) = (fft_size / 2.0) / 0.5
            double magnitude_db = 20.0 * log10(magnitude / (fft_size / 4.0) + epsilon);
            double frequency = i * freq_resolution;

            fprintf(outfile, "%10.2f,%10.2f\n", frequency, magnitude_db);
        }

        // Close output file for this snapshot (if not stdout)
        if (outfile != stdout) {
            fclose(outfile);
        }
    }

    // Cleanup
    if (!quiet && interval_ms > 0) {
        fprintf(stderr, "Completed %d snapshots\n", num_snapshots);
    }

    free(snapshot_times_ms);
    free(power_spectrum);
    fftw_destroy_plan(plan);
    fftw_free(fft_output);
    free(audio_buffer);
    sf_close(infile);

    return 0;
}
