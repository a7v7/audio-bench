/*
 * audio-analyze.c
 * Basic WAV file analyzer for audio-bench
 * 
 * Reads a WAV file and performs basic analysis including:
 * - File format information
 * - Peak level detection
 * - RMS level calculation
 * - Basic frequency analysis (requires FFTW)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sndfile.h>

#define BUFFER_SIZE 4096

typedef struct {
    double peak_left;
    double peak_right;
    double rms_left;
    double rms_right;
    long total_frames;
    int sample_rate;
    int channels;
} AudioStats;

void print_usage(const char *progname) {
    printf("Usage: %s <input.wav> [options]\n", progname);
    printf("Options:\n");
    printf("  -o <file>    Output results to file\n");
    printf("  -v           Verbose output\n");
    printf("  -h           Show this help\n");
}

void print_file_info(SF_INFO *info) {
    printf("Audio File Information:\n");
    printf("  Sample Rate: %d Hz\n", info->samplerate);
    printf("  Channels: %d\n", info->channels);
    printf("  Frames: %ld\n", info->frames);
    printf("  Duration: %.2f seconds\n", (double)info->frames / info->samplerate);
    
    const char *format_str = "Unknown";
    switch (info->format & SF_FORMAT_TYPEMASK) {
        case SF_FORMAT_WAV: format_str = "WAV"; break;
        case SF_FORMAT_AIFF: format_str = "AIFF"; break;
        case SF_FORMAT_FLAC: format_str = "FLAC"; break;
    }
    printf("  Format: %s\n", format_str);
}

int analyze_audio(const char *filename, AudioStats *stats) {
    SF_INFO info;
    SNDFILE *file;
    double buffer[BUFFER_SIZE];
    sf_count_t frames_read;
    
    memset(&info, 0, sizeof(info));
    
    // Open the audio file
    file = sf_open(filename, SFM_READ, &info);
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        fprintf(stderr, "%s\n", sf_strerror(NULL));
        return -1;
    }
    
    print_file_info(&info);
    
    // Initialize stats
    stats->peak_left = 0.0;
    stats->peak_right = 0.0;
    stats->rms_left = 0.0;
    stats->rms_right = 0.0;
    stats->total_frames = info.frames;
    stats->sample_rate = info.samplerate;
    stats->channels = info.channels;
    
    // Process audio in chunks
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
    
    // Calculate RMS
    if (total_samples > 0) {
        stats->rms_left = sqrt(stats->rms_left / total_samples);
        if (info.channels >= 2) {
            stats->rms_right = sqrt(stats->rms_right / total_samples);
        }
    }
    
    sf_close(file);
    return 0;
}

void print_stats(AudioStats *stats) {
    printf("\nAnalysis Results:\n");
    
    // Convert to dB
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *input_file = argv[1];
    AudioStats stats;
    
    if (analyze_audio(input_file, &stats) != 0) {
        return 1;
    }
    
    print_stats(&stats);
    
    printf("\nAnalysis complete.\n");
    return 0;
}
