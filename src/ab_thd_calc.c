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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <sndfile.h>
#include <fftw3.h>
#include <popt.h>

//------------------------------------------------------------------------------
// Default analysis parameters
//------------------------------------------------------------------------------
#define DEFAULT_FFT_SIZE			8192
#define DEFAULT_HARMONICS			10
#define DEFAULT_FUNDAMENTAL_FREQ	1000.0										//	1kHz

//------------------------------------------------------------------------------
//	Name:		apply_hann_window
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Applies Hann window to reduce spectral leakage
//	- Window function: 0.5 * (1 - cos(2*pi*n/(N-1)))
//	- Modifies data in-place
//------------------------------------------------------------------------------
void apply_hann_window(double *data, int size)
{
    for (int i = 0; i < size; i++) {
        double window = 0.5 * (1.0 - cos(2.0 * M_PI * i / (size - 1)));
        data[i] *= window;
    }
}

//------------------------------------------------------------------------------
//	Name:		find_peak_bin
//
//	Returns:	Bin index with maximum magnitude near target frequency
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Searches for peak within specified range around target frequency
//	- Uses frequency resolution to convert Hz to bin numbers
//	- Returns bin with maximum magnitude in search range
//------------------------------------------------------------------------------
int find_peak_bin(fftw_complex *fft_output, int fft_size, double sample_rate,
                  double target_freq, double search_range_hz)
{
    double freq_resolution = (double)sample_rate / fft_size;
    int target_bin = (int)(target_freq / freq_resolution + 0.5);
    int search_bins = (int)(search_range_hz / freq_resolution + 0.5);

    int start_bin = target_bin - search_bins;
    int end_bin = target_bin + search_bins;

//------------------------------------------------------------------------------
//	Clamp to valid range
//------------------------------------------------------------------------------
    if (start_bin < 0) start_bin = 0;
    if (end_bin > fft_size / 2) end_bin = fft_size / 2;

//------------------------------------------------------------------------------
//	Find peak
//------------------------------------------------------------------------------
    int peak_bin = target_bin;
    double peak_magnitude = 0.0;

    for (int i = start_bin; i <= end_bin; i++) {
        double real = fft_output[i][0];
        double imag = fft_output[i][1];
        double magnitude = sqrt(real * real + imag * imag);

        if (magnitude > peak_magnitude) {
            peak_magnitude = magnitude;
            peak_bin = i;
        }
    }

    return peak_bin;
}

//------------------------------------------------------------------------------
//	Name:		get_bin_magnitude
//
//	Returns:	Magnitude at specified bin
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Calculates magnitude from complex FFT output
//	- Magnitude = sqrt(real^2 + imag^2)
//------------------------------------------------------------------------------
double get_bin_magnitude(fftw_complex *fft_output, int bin)
{
    double real = fft_output[bin][0];
    double imag = fft_output[bin][1];
    return sqrt(real * real + imag * imag);
}

//------------------------------------------------------------------------------
//	Main application
//
//	This application:
//	- Reads audio file containing sine wave
//	- Performs FFT analysis
//	- Identifies fundamental frequency and harmonics
//	- Calculates Total Harmonic Distortion (THD)
//	- Displays results in table format
//
//	Libraries:
//	- libsndfile: Audio file I/O
//	- FFTW3: Fast Fourier Transform
//	- libpopt: Command-line parsing
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
//------------------------------------------------------------------------------
//	Command-line option variables
//------------------------------------------------------------------------------
    char *input_file = NULL;
    int fft_size = DEFAULT_FFT_SIZE;
    int harmonic_range = DEFAULT_HARMONICS;
    double fundamental_freq = DEFAULT_FUNDAMENTAL_FREQ;
    int verbose = 0;
    int version_flag = 0;

//------------------------------------------------------------------------------
//	Define popt options table
//------------------------------------------------------------------------------
    struct poptOption options[] = {
        {"version",		'v',	POPT_ARG_NONE,		&version_flag,		0,	"Show version information",						NULL	},
        {"file",		'f',	POPT_ARG_STRING,	&input_file,		0,	"Input WAV file containing sine wave",			"FILE"	},
        {"freq",		'F',	POPT_ARG_DOUBLE,	&fundamental_freq,	0,	"Fundamental frequency in Hz (default: 1000)",	"FREQ"	},
        {"fft-size",	's',	POPT_ARG_INT,		&fft_size,			0,	"FFT size (default: 8192)",						"SIZE"	},
        {"harmonics",	'n',	POPT_ARG_INT,		&harmonic_range,	0,	"Number of harmonics to analyze (default: 10)",	"COUNT"	},
        {"verbose",		'V',	POPT_ARG_NONE,		&verbose,			0,	"Verbose output",								NULL	},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

//------------------------------------------------------------------------------
//	Parse command-line options
//------------------------------------------------------------------------------
    poptContext popt_ctx = poptGetContext(NULL, argc, (const char **)argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "Calculate Total Harmonic Distortion (THD) for a sine wave.\n\n"
        "Examples:\n"
        "  ab_thd_calc -f test_1khz.wav                      # 1kHz sine wave\n"
        "  ab_thd_calc -f test_10khz.wav -F 10000            # 10kHz sine wave\n"
        "  ab_thd_calc -f test_1khz.wav -s 16384 -n 15       # Custom FFT size and harmonics\n"
        "  ab_thd_calc -f test_1khz.wav --verbose            # Verbose output\n");

    int rc = poptGetNextOpt(popt_ctx);
    if (rc < -1) {
        fprintf(stderr, "Error: %s: %s\n",
                poptBadOption(popt_ctx, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
        poptFreeContext(popt_ctx);
        return 1;
    }

//------------------------------------------------------------------------------
//	Handle version mode
//------------------------------------------------------------------------------
    if (version_flag) {
        printf("ab_thd_calc version 1.0.0\n");
        printf("THD calculator for audio-bench\n");
        printf("Copyright (c) 2025 A.C. Verbeck\n");
        poptFreeContext(popt_ctx);
        return 0;
    }

//------------------------------------------------------------------------------
//	Validate required parameters
//------------------------------------------------------------------------------
    if (!input_file) {
        fprintf(stderr, "Error: Input file is required (use -f or --file)\n");
        poptPrintUsage(popt_ctx, stderr, 0);
        poptFreeContext(popt_ctx);
        return 1;
    }

//------------------------------------------------------------------------------
//	Validate FFT size
//------------------------------------------------------------------------------
    if (fft_size <= 0) {
        fprintf(stderr, "Error: FFT size must be positive\n");
        poptFreeContext(popt_ctx);
        return 1;
    }

//------------------------------------------------------------------------------
//	Validate harmonic range
//------------------------------------------------------------------------------
    if (harmonic_range < 1) {
        fprintf(stderr, "Error: Harmonic range must be at least 1\n");
        poptFreeContext(popt_ctx);
        return 1;
    }

//------------------------------------------------------------------------------
//	Validate fundamental frequency
//------------------------------------------------------------------------------
    if (fundamental_freq <= 0.0) {
        fprintf(stderr, "Error: Fundamental frequency must be positive\n");
        poptFreeContext(popt_ctx);
        return 1;
    }

    poptFreeContext(popt_ctx);

//------------------------------------------------------------------------------
//	Open the audio file
//------------------------------------------------------------------------------
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    SNDFILE *infile = sf_open(input_file, SFM_READ, &sfinfo);

    if (!infile) {
        fprintf(stderr, "Error: Could not open file '%s'\n", input_file);
        fprintf(stderr, "%s\n", sf_strerror(NULL));
        return 1;
    }

//------------------------------------------------------------------------------
//	Display file information
//------------------------------------------------------------------------------
    if (verbose) {
        printf("File Information:\n");
        printf("  File: %s\n", input_file);
        printf("  Sample rate: %d Hz\n", sfinfo.samplerate);
        printf("  Channels: %d\n", sfinfo.channels);
        printf("  Frames: %ld\n", (long)sfinfo.frames);
        printf("  Duration: %.2f seconds\n", (double)sfinfo.frames / sfinfo.samplerate);
        printf("\nAnalysis Parameters:\n");
        printf("  Fundamental frequency: %.0f Hz\n", fundamental_freq);
        printf("  FFT size: %d\n", fft_size);
        printf("  Frequency resolution: %.2f Hz\n", (double)sfinfo.samplerate / fft_size);
        printf("  Harmonics to analyze: %d\n", harmonic_range);
        printf("\n");
    }

//------------------------------------------------------------------------------
//	Check if we have enough samples
//------------------------------------------------------------------------------
    if (sfinfo.frames < fft_size) {
        fprintf(stderr, "Warning: File has fewer samples (%ld) than FFT size (%d)\n",
                (long)sfinfo.frames, fft_size);
        fprintf(stderr, "         Results may be unreliable. Consider using a smaller FFT size.\n");
    }

//------------------------------------------------------------------------------
//	Allocate buffers
//------------------------------------------------------------------------------
    double *audio_buffer = (double *)malloc(fft_size * sizeof(double));
    fftw_complex *fft_output = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * (fft_size / 2 + 1));

    if (!audio_buffer || !fft_output) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        sf_close(infile);
        return 1;
    }

//------------------------------------------------------------------------------
//	Read audio data (if stereo, convert to mono by averaging channels)
//------------------------------------------------------------------------------
    sf_count_t frames_read;
    if (sfinfo.channels == 1) {
        frames_read = sf_read_double(infile, audio_buffer, fft_size);
    } else {
//------------------------------------------------------------------------------
//	Read interleaved multi-channel data and average to mono
//------------------------------------------------------------------------------
        double *temp_buffer = (double *)malloc(fft_size * sfinfo.channels * sizeof(double));
        if (!temp_buffer) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(audio_buffer);
            fftw_free(fft_output);
            sf_close(infile);
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

//------------------------------------------------------------------------------
//	Zero-pad if we didn't read enough samples
//------------------------------------------------------------------------------
    for (sf_count_t i = frames_read; i < fft_size; i++) {
        audio_buffer[i] = 0.0;
    }

//------------------------------------------------------------------------------
//	Apply Hann window
//------------------------------------------------------------------------------
    apply_hann_window(audio_buffer, fft_size);

//------------------------------------------------------------------------------
//	Create FFTW plan and execute FFT
//------------------------------------------------------------------------------
    fftw_plan plan = fftw_plan_dft_r2c_1d(fft_size, audio_buffer, fft_output, FFTW_ESTIMATE);
    fftw_execute(plan);

//------------------------------------------------------------------------------
//	Calculate frequency resolution
//------------------------------------------------------------------------------
    double freq_resolution = (double)sfinfo.samplerate / fft_size;

//------------------------------------------------------------------------------
//	Find fundamental - search within +/- 50 Hz
//------------------------------------------------------------------------------
    int fundamental_bin = find_peak_bin(fft_output, fft_size, sfinfo.samplerate,
                                        fundamental_freq, 50.0);
    double measured_fundamental_freq = fundamental_bin * freq_resolution;
    double fundamental_mag = get_bin_magnitude(fft_output, fundamental_bin);

//------------------------------------------------------------------------------
//	Normalize for FFT size and Hann window (coherent gain = 0.5)
//------------------------------------------------------------------------------
    double normalization_factor = fft_size / 4.0;
    fundamental_mag /= normalization_factor;

//------------------------------------------------------------------------------
//	Convert to dBFS (relative to full scale)
//------------------------------------------------------------------------------
    double fundamental_db = 20.0 * log10(fundamental_mag + 1e-10);

//------------------------------------------------------------------------------
//	Print results header
//------------------------------------------------------------------------------
    printf("THD Analysis Results for %.0f Hz Sine Wave\n", fundamental_freq);
    printf("========================================\n\n");

    printf("Fundamental Frequency (H1):\n");
    printf("  Expected: %.0f Hz\n", fundamental_freq);
    printf("  Measured: %.2f Hz (bin %d)\n", measured_fundamental_freq, fundamental_bin);
    printf("  Level: %.2f dBFS\n\n", fundamental_db);

//------------------------------------------------------------------------------
//	Analyze harmonics
//------------------------------------------------------------------------------
    printf("Harmonic Analysis:\n");
    printf("  Harmonic  Frequency (Hz)  Level (dBFS)  Level (dB rel. to H1)\n");
    printf("  --------  --------------  ------------  ---------------------\n");

    double *harmonic_magnitudes = (double *)malloc((harmonic_range + 1) * sizeof(double));
    harmonic_magnitudes[0] = fundamental_mag;						//	Store fundamental for THD calculation

    for (int h = 1; h <= harmonic_range; h++) {
        double harmonic_freq = fundamental_freq * (h + 1);

//------------------------------------------------------------------------------
//	Check if harmonic is within Nyquist frequency
//------------------------------------------------------------------------------
        if (harmonic_freq >= sfinfo.samplerate / 2.0) {
            if (verbose) {
                printf("  H%-7d  %.2f  (above Nyquist frequency)\n", h + 1, harmonic_freq);
            }
            harmonic_magnitudes[h] = 0.0;
            continue;
        }

        int harmonic_bin = find_peak_bin(fft_output, fft_size, sfinfo.samplerate,
                                         harmonic_freq, 50.0);
        double measured_freq = harmonic_bin * freq_resolution;
        double harmonic_mag = get_bin_magnitude(fft_output, harmonic_bin);
        harmonic_mag /= normalization_factor;
        harmonic_magnitudes[h] = harmonic_mag;

        double harmonic_db = 20.0 * log10(harmonic_mag + 1e-10);
        double relative_db = harmonic_db - fundamental_db;

        printf("  H%-7d  %10.2f  %12.2f  %21.2f\n",
               h + 1, measured_freq, harmonic_db, relative_db);
    }

//------------------------------------------------------------------------------
//	Calculate THD
//	THD = sqrt(H2^2 + H3^2 + ... + Hn^2) / H1
//------------------------------------------------------------------------------
    double harmonic_sum_squares = 0.0;
    for (int h = 1; h <= harmonic_range; h++) {
        if (harmonic_magnitudes[h] > 0.0) {
            harmonic_sum_squares += harmonic_magnitudes[h] * harmonic_magnitudes[h];
        }
    }

    double thd_ratio = sqrt(harmonic_sum_squares) / (fundamental_mag + 1e-10);
    double thd_percent = thd_ratio * 100.0;
    double thd_db = 20.0 * log10(thd_ratio + 1e-10);

    printf("\nTotal Harmonic Distortion (THD):\n");
    printf("  THD: %.4f%% (%.2f dB)\n", thd_percent, thd_db);
    printf("  Based on %d harmonics (H2-H%d)\n", harmonic_range, harmonic_range + 1);

//------------------------------------------------------------------------------
//	Cleanup
//------------------------------------------------------------------------------
    free(harmonic_magnitudes);
    fftw_destroy_plan(plan);
    fftw_free(fft_output);
    free(audio_buffer);
    sf_close(infile);

    return 0;																//	Exit: status 0 (no error)
}
