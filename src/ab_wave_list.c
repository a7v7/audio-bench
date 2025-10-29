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
 * ab_wave_list.c - List all WAV files in current directory with their properties
 *
 * Features:
 * - Scans current directory for WAV files
 * - Displays filename, sample rate, bit depth, and length
 * - Supports multiple audio formats via libsndfile
 *
 * Dependencies: libsndfile, libpopt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sndfile.h>
#include <popt.h>
#include <ctype.h>

//------------------------------------------------------------------------------
// Determine bit depth from SF_INFO format field
//------------------------------------------------------------------------------
int get_bit_depth(int format)
{
    int format_subtype = format & SF_FORMAT_SUBMASK;

    switch (format_subtype) {
        case SF_FORMAT_PCM_S8:
        case SF_FORMAT_PCM_U8:
            return 8;
        case SF_FORMAT_PCM_16:
            return 16;
        case SF_FORMAT_PCM_24:
            return 24;
        case SF_FORMAT_PCM_32:
            return 32;
        case SF_FORMAT_FLOAT:
            return 32;
        case SF_FORMAT_DOUBLE:
            return 64;
        default:
            return 0;  // Unknown
    }
}

//------------------------------------------------------------------------------
// Check if filename ends with .wav (case-insensitive)
//------------------------------------------------------------------------------
int is_wav_file(const char *filename)
{
    size_t len = strlen(filename);
    if (len < 4) {
        return 0;
    }

    const char *ext = filename + len - 4;
    return (tolower(ext[0]) == '.' &&
            tolower(ext[1]) == 'w' &&
            tolower(ext[2]) == 'a' &&
            tolower(ext[3]) == 'v');
}

//------------------------------------------------------------------------------
// List all WAV files in the current directory
//------------------------------------------------------------------------------
int list_wav_files(int verbose)
{
    DIR *dir;
    struct dirent *entry;
    int file_count = 0;

    // Open current directory
    dir = opendir(".");
    if (!dir) {
        fprintf(stderr, "Error: Cannot open current directory\n");
        return -1;
    }

    // Print header
    if (verbose) {
        printf("Scanning current directory for WAV files...\n\n");
    }
    printf("%-40s %12s %10s %12s\n", "Filename", "Sample Rate", "Bit Depth", "Duration");
    printf("--------------------------------------------------------------------------------------------\n");

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check if it's a WAV file
        if (!is_wav_file(entry->d_name)) {
            continue;
        }

        // Open the WAV file
        SF_INFO sfinfo;
        memset(&sfinfo, 0, sizeof(sfinfo));

        SNDFILE *sndfile = sf_open(entry->d_name, SFM_READ, &sfinfo);
        if (!sndfile) {
            fprintf(stderr, "Warning: Could not open '%s': %s\n",
                    entry->d_name, sf_strerror(NULL));
            continue;
        }

        // Get bit depth
        int bit_depth = get_bit_depth(sfinfo.format);

        // Calculate length in seconds
        double length_seconds = (double)sfinfo.frames / sfinfo.samplerate;

        // Print file information
        printf("%-40s %9d Hz %7d-bit %9.2f sec\n",
               entry->d_name,
               sfinfo.samplerate,
               bit_depth,
               length_seconds);

        sf_close(sndfile);
        file_count++;
    }

    closedir(dir);

    // Print summary
    printf("--------------------------------------------------------------------------------------------\n");
    if (file_count == 0) {
        printf("No WAV files found in current directory.\n");
    } else {
        printf("Total: %d WAV file%s\n", file_count, file_count == 1 ? "" : "s");
    }

    return 0;
}

//------------------------------------------------------------------------------
// Main application
//------------------------------------------------------------------------------
int main(int argc, const char **argv)
{
    /* Command-line options */
    int version_flag = 0;
    int verbose = 0;

    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0,
         "Show version information", NULL},
        {"verbose", 'V', POPT_ARG_NONE, &verbose, 0,
         "Verbose output", NULL},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(NULL, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "List all WAV files in the current directory with their properties.\n\n"
        "Examples:\n"
        "  ab_wave_list           # List all WAV files\n"
        "  ab_wave_list -V        # Verbose output\n");

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
        printf("ab_wave_list version 1.0.0\n");
        printf("WAV file listing tool for audio-bench\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        poptFreeContext(popt_ctx);
        return 0;
    }

    poptFreeContext(popt_ctx);

    /* List WAV files */
    return list_wav_files(verbose);
}
