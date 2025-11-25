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
#include <math.h>
#include <sndfile.h>
#include <popt.h>

#define BUFFER_SIZE 4096

//------------------------------------------------------------------------------
// Audio statistics structure
//------------------------------------------------------------------------------
typedef struct {
    double peak_left;												//	Peak level left channel
    double peak_right;												//	Peak level right channel
    double rms_left;												//	RMS level left channel
    double rms_right;												//	RMS level right channel
    long total_frames;												//	Total frames analyzed
    int sample_rate;												//	Sample rate
    int channels;													//	Number of channels
} AudioStats;

//------------------------------------------------------------------------------
//	Name:		print_file_info
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Displays audio file format information
//	- Shows sample rate, channels, frames, and duration
//	- Displays file format type (WAV, AIFF, FLAC)
//------------------------------------------------------------------------------
void print_file_info(SF_INFO *info)
{
    printf("Audio File Information:\n");
    printf("  Sample Rate: %d Hz\n", info->samplerate);
    printf("  Channels: %d\n", info->channels);
    printf("  Frames: %lld\n", info->frames);
    printf("  Duration: %.2f seconds\n", (double)info->frames / info->samplerate);

    const char *format_str = "Unknown";
    switch (info->format & SF_FORMAT_TYPEMASK) {
        case SF_FORMAT_WAV: format_str = "WAV"; break;
        case SF_FORMAT_AIFF: format_str = "AIFF"; break;
        case SF_FORMAT_FLAC: format_str = "FLAC"; break;
    }
    printf("  Format: %s\n", format_str);
}

//------------------------------------------------------------------------------
//	Name:		analyze_audio
//
//	Returns:	0 on success, -1 on error
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Opens audio file and reads format information
//	- Processes audio in chunks
//	- Calculates peak and RMS levels for each channel
//	- Stores results in AudioStats structure
//------------------------------------------------------------------------------
int analyze_audio(const char *filename, AudioStats *stats)
{
    SF_INFO info;
    SNDFILE *file;
    double buffer[BUFFER_SIZE];
    sf_count_t frames_read;

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

    print_file_info(&info);

//------------------------------------------------------------------------------
//	Initialize stats
//------------------------------------------------------------------------------
    stats->peak_left = 0.0;
    stats->peak_right = 0.0;
    stats->rms_left = 0.0;
    stats->rms_right = 0.0;
    stats->total_frames = info.frames;
    stats->sample_rate = info.samplerate;
    stats->channels = info.channels;

//------------------------------------------------------------------------------
//	Process audio in chunks
//------------------------------------------------------------------------------
    printf("\nAnalyzing audio...\n");
    long total_samples = 0;

    while ((frames_read = sf_readf_double(file, buffer, BUFFER_SIZE / info.channels)) > 0) {
        for (sf_count_t i = 0; i < frames_read; i++) {
            if (info.channels >= 1) {
                double sample = fabs(buffer[i * info.channels]);
                if (sample > stats->peak_left) {
                    stats->peak_left = sample;
                }
                stats->rms_left += sample * sample;
            }

            if (info.channels >= 2) {
                double sample = fabs(buffer[i * info.channels + 1]);
                if (sample > stats->peak_right) {
                    stats->peak_right = sample;
                }
                stats->rms_right += sample * sample;
            }
        }
        total_samples += frames_read;
    }

//------------------------------------------------------------------------------
//	Calculate RMS
//------------------------------------------------------------------------------
    if (total_samples > 0) {
        stats->rms_left = sqrt(stats->rms_left / total_samples);
        if (info.channels >= 2) {
            stats->rms_right = sqrt(stats->rms_right / total_samples);
        }
    }

    sf_close(file);
    return 0;
}

//------------------------------------------------------------------------------
//	Name:		print_stats
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Displays analysis results for each channel
//	- Converts peak and RMS values to dB
//	- Shows both linear and dB values
//------------------------------------------------------------------------------
void print_stats(AudioStats *stats)
{
    printf("\nAnalysis Results:\n");

//------------------------------------------------------------------------------
//	Convert to dB
//------------------------------------------------------------------------------
    double peak_left_db = 20.0 * log10(stats->peak_left);
    double rms_left_db = 20.0 * log10(stats->rms_left);

    printf("  Left Channel:\n");
    printf("    Peak Level: %.2f dB (%.4f)\n", peak_left_db, stats->peak_left);
    printf("    RMS Level:  %.2f dB (%.4f)\n", rms_left_db, stats->rms_left);

    if (stats->channels >= 2) {
        double peak_right_db = 20.0 * log10(stats->peak_right);
        double rms_right_db = 20.0 * log10(stats->rms_right);

        printf("  Right Channel:\n");
        printf("    Peak Level: %.2f dB (%.4f)\n", peak_right_db, stats->peak_right);
        printf("    RMS Level:  %.2f dB (%.4f)\n", rms_right_db, stats->rms_right);
    }
}

//------------------------------------------------------------------------------
//	Main application
//
//	This application:
//	- Parses command-line options
//	- Opens and analyzes WAV file
//	- Calculates peak and RMS levels
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
    char *input_file = NULL;
    char *output_file = NULL;
    int verbose = 0;
    int version_flag = 0;

    struct poptOption options[] = {
        {"version",		'v',	POPT_ARG_NONE,		&version_flag,	0,	"Show version information",	NULL	},
        {"output",		'o',	POPT_ARG_STRING,	&output_file,	0,	"Output results to file",	"FILE"	},
        {"verbose",		'V',	POPT_ARG_NONE,		&verbose,		0,	"Verbose output",			NULL	},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(NULL, argc, (const char **)argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "<input.wav> [OPTIONS]\n\n"
        "Basic WAV file analyzer for audio-bench.\n\n"
        "Examples:\n"
        "  ab_audio_analyze input.wav           # Analyze audio file\n"
        "  ab_audio_analyze input.wav -o out.txt # Save results to file\n"
        "  ab_audio_analyze input.wav -V        # Verbose output\n");

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
        printf("ab_audio_analyze version 1.0.0\n");
        printf("Basic WAV file analyzer for audio-bench\n");
        printf("Copyright (c) 2025 A.C. Verbeck\n");
        poptFreeContext(popt_ctx);
        return 0;
    }

//------------------------------------------------------------------------------
//	Get remaining argument (input file)
//------------------------------------------------------------------------------
    const char *remaining = poptGetArg(popt_ctx);
    if (remaining) {
        input_file = strdup(remaining);
    }

    if (!input_file) {
        fprintf(stderr, "Error: Input file required\n");
        poptPrintUsage(popt_ctx, stderr, 0);
        poptFreeContext(popt_ctx);
        return 1;
    }

    poptFreeContext(popt_ctx);

//------------------------------------------------------------------------------
//	Perform analysis
//------------------------------------------------------------------------------
    AudioStats stats;
    if (analyze_audio(input_file, &stats) != 0) {
        free(input_file);
        return 1;
    }

    print_stats(&stats);

    printf("\nAnalysis complete.\n");

    free(input_file);
    return 0;																	//	Exit: status 0 (no error)
}
