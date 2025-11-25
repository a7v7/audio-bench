//------------------------------------------------------------------------------
//	The MIT License (MIT)
//
//	Copyright (c) 2023 A.C. Verbeck
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
#include <math.h>
#include <sndfile.h>
#include <popt.h>

#define BUFFER_SIZE 4096

//------------------------------------------------------------------------------
//	Name:		calculate_rms
//
//	Returns:	0 on success, -1 on error
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Calculates RMS value for specified duration of an audio file
//	- Opens audio file using libsndfile
//	- Processes specified duration of audio
//	- Sums all channels together for overall RMS
//	- Stores calculated RMS value in output parameter
//------------------------------------------------------------------------------
int calculate_rms(const char *filename, double duration, double *rms_out)
{
    SF_INFO info;
    SNDFILE *file;
    double buffer[BUFFER_SIZE];
    sf_count_t frames_read;
    double sum_squares = 0.0;
    long total_samples = 0;

    memset(&info, 0, sizeof(info));

//------------------------------------------------------------------------------
//	Open the audio file
//------------------------------------------------------------------------------
    file = sf_open(filename, SFM_READ, &info);
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        fprintf(stderr, "%s\n", sf_strerror(NULL));
        return -1;
    }

//------------------------------------------------------------------------------
//	Calculate how many frames to read based on duration
//------------------------------------------------------------------------------
    sf_count_t frames_to_read = (sf_count_t)(duration * info.samplerate);

//------------------------------------------------------------------------------
//	Ensure we don't try to read more frames than available
//------------------------------------------------------------------------------
    if (frames_to_read > info.frames) {
        frames_to_read = info.frames;
        fprintf(stderr, "Warning: File '%s' is shorter than %.2f seconds (%.2f seconds available)\n",
                filename, duration, (double)info.frames / info.samplerate);
    }

    sf_count_t frames_remaining = frames_to_read;

//------------------------------------------------------------------------------
//	Process audio in chunks
//------------------------------------------------------------------------------
    while (frames_remaining > 0 &&
           (frames_read = sf_readf_double(file, buffer,
                                         (frames_remaining < BUFFER_SIZE / info.channels) ?
                                         frames_remaining : BUFFER_SIZE / info.channels)) > 0) {

        for (sf_count_t i = 0; i < frames_read; i++) {
//------------------------------------------------------------------------------
//	Sum all channels together for overall RMS
//------------------------------------------------------------------------------
            for (int ch = 0; ch < info.channels; ch++) {
                double sample = buffer[i * info.channels + ch];
                sum_squares += sample * sample;
            }
        }

        total_samples += frames_read * info.channels;
        frames_remaining -= frames_read;
    }

//------------------------------------------------------------------------------
//	Calculate RMS
//------------------------------------------------------------------------------
    if (total_samples > 0) {
        *rms_out = sqrt(sum_squares / total_samples);
    } else {
        *rms_out = 0.0;
    }

    sf_close(file);
    return 0;
}

//------------------------------------------------------------------------------
//	Main application
//
//	This application:
//	- Parses command-line options
//	- Calculates RMS for both input files
//	- Computes gain difference in dB
//	- Displays results
//
//	Libraries:
//	- libsndfile: Used for audio file I/O
//	- libpopt: Used for command-line parsing
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
//------------------------------------------------------------------------------
//	Command-line options
//------------------------------------------------------------------------------
    char *file1 = NULL;
    char *file2 = NULL;
    double duration = 1.0;												//	Default: 1 second
    int verbose = 0;
    int version_flag = 0;

    struct poptOption options[] = {
        {"time", 't', POPT_ARG_DOUBLE, &duration, 0,
         "Duration in seconds to analyze (default: 1.0)", "SECONDS"},
        {"verbose", 'V', POPT_ARG_NONE, &verbose, 0,
         "Verbose output", NULL},
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0,
         "Show version information", NULL},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(NULL, argc, (const char **)argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "<file1.wav> <file2.wav> [OPTIONS]\n\n"
        "Calculate gain difference between two audio files.\n"
        "Compares RMS levels for a specified duration and reports the difference in dB.\n\n"
        "Examples:\n"
        "  ab_gain_calc input1.wav input2.wav           # Compare first second\n"
        "  ab_gain_calc input1.wav input2.wav -t 2.5    # Compare first 2.5 seconds\n"
        "  ab_gain_calc input1.wav input2.wav -V        # Verbose output\n");

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
        printf("ab_gain_calc version 1.0.0\n");
        printf("Gain difference calculator for audio-bench\n");
        printf("Copyright (c) 2023 A.C. Verbeck\n");
        poptFreeContext(popt_ctx);
        return 0;
    }

//------------------------------------------------------------------------------
//	Get file arguments
//------------------------------------------------------------------------------
    const char *arg1 = poptGetArg(popt_ctx);
    const char *arg2 = poptGetArg(popt_ctx);

    if (!arg1 || !arg2) {
        fprintf(stderr, "Error: Two input files required\n");
        poptPrintUsage(popt_ctx, stderr, 0);
        poptFreeContext(popt_ctx);
        return 1;
    }

    file1 = strdup(arg1);
    file2 = strdup(arg2);

//------------------------------------------------------------------------------
//	Check for extra arguments
//------------------------------------------------------------------------------
    if (poptGetArg(popt_ctx) != NULL) {
        fprintf(stderr, "Error: Too many arguments\n");
        poptPrintUsage(popt_ctx, stderr, 0);
        free(file1);
        free(file2);
        poptFreeContext(popt_ctx);
        return 1;
    }

//------------------------------------------------------------------------------
//	Validate duration
//------------------------------------------------------------------------------
    if (duration <= 0.0) {
        fprintf(stderr, "Error: Duration must be positive\n");
        free(file1);
        free(file2);
        poptFreeContext(popt_ctx);
        return 1;
    }

    poptFreeContext(popt_ctx);

    if (verbose) {
        printf("Calculating RMS for %.2f seconds of audio...\n", duration);
        printf("File 1: %s\n", file1);
        printf("File 2: %s\n", file2);
        printf("\n");
    }

//------------------------------------------------------------------------------
//	Calculate RMS for both files
//------------------------------------------------------------------------------
    double rms1, rms2;

    if (calculate_rms(file1, duration, &rms1) != 0) {
        free(file1);
        free(file2);
        return 1;
    }

    if (calculate_rms(file2, duration, &rms2) != 0) {
        free(file1);
        free(file2);
        return 1;
    }

//------------------------------------------------------------------------------
//	Calculate dB values and gain difference
//------------------------------------------------------------------------------
    double rms1_db = 20.0 * log10(rms1);
    double rms2_db = 20.0 * log10(rms2);
    double gain_diff_db = rms2_db - rms1_db;

//------------------------------------------------------------------------------
//	Output results
//------------------------------------------------------------------------------
    printf("Gain Calculation Results:\n");
    printf("  Analysis Duration: %.2f seconds\n", duration);
    printf("\n");
    printf("  File 1: %s\n", file1);
    printf("    RMS Level: %.2f dB (%.6f)\n", rms1_db, rms1);
    printf("\n");
    printf("  File 2: %s\n", file2);
    printf("    RMS Level: %.2f dB (%.6f)\n", rms2_db, rms2);
    printf("\n");
    printf("  Gain Difference: %.2f dB\n", gain_diff_db);

    if (gain_diff_db > 0) {
        printf("  (File 2 is %.2f dB louder than File 1)\n", gain_diff_db);
    } else if (gain_diff_db < 0) {
        printf("  (File 2 is %.2f dB quieter than File 1)\n", fabs(gain_diff_db));
    } else {
        printf("  (Files have equal RMS levels)\n");
    }

    free(file1);
    free(file2);
    return 0;																//	Exit: status 0 (no error)
}
