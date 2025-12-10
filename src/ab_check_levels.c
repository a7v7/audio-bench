/*
 * Audio Interface Level Checker
 * 
 * Simple utility to measure and compare levels of two audio files.
 * Useful for characterizing audio interface input/output levels.
 *
 * Compile: gcc -o check_levels check_levels.c -lsndfile -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sndfile.h>

typedef struct {
    double peak_pos;      // Maximum positive sample
    double peak_neg;      // Maximum negative sample (absolute value)
    double peak_dbfs;     // Peak level in dBFS
    double rms;           // RMS level (linear)
    double rms_dbfs;      // RMS level in dBFS
    double crest_factor;  // Peak/RMS ratio in dB
    size_t samples;
    int sample_rate;
    int channels;
} LevelStats;

// Calculate statistics for an audio file
int calculate_levels(const char *filename, LevelStats *stats) {
    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(SF_INFO));
    
    SNDFILE *sf = sf_open(filename, SFM_READ, &sf_info);
    if (!sf) {
        fprintf(stderr, "Error opening %s: %s\n", filename, sf_strerror(NULL));
        return -1;
    }
    
    stats->samples = sf_info.frames;
    stats->sample_rate = sf_info.samplerate;
    stats->channels = sf_info.channels;
    
    // Read all samples
    size_t total_samples = sf_info.frames * sf_info.channels;
    double *buffer = (double *)malloc(total_samples * sizeof(double));
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        sf_close(sf);
        return -1;
    }
    
    sf_read_double(sf, buffer, total_samples);
    sf_close(sf);
    
    // Calculate statistics
    stats->peak_pos = 0.0;
    stats->peak_neg = 0.0;
    double sum_sq = 0.0;
    
    for (size_t i = 0; i < total_samples; i++) {
        double sample = buffer[i];
        
        if (sample > stats->peak_pos) {
            stats->peak_pos = sample;
        }
        if (sample < stats->peak_neg) {
            stats->peak_neg = sample;
        }
        
        sum_sq += sample * sample;
    }
    
    free(buffer);
    
    // Convert to absolute value for negative peak
    stats->peak_neg = fabs(stats->peak_neg);
    
    // Overall peak
    double peak = (stats->peak_pos > stats->peak_neg) ? stats->peak_pos : stats->peak_neg;
    stats->peak_dbfs = 20.0 * log10(peak);
    
    // RMS
    stats->rms = sqrt(sum_sq / total_samples);
    stats->rms_dbfs = 20.0 * log10(stats->rms);
    
    // Crest factor
    stats->crest_factor = stats->peak_dbfs - stats->rms_dbfs;
    
    return 0;
}

// Print statistics for a file
void print_stats(const char *label, LevelStats *stats) {
    printf("\n=== %s ===\n", label);
    printf("Duration:      %.3f seconds\n", (double)stats->samples / stats->sample_rate);
    printf("Sample rate:   %d Hz\n", stats->sample_rate);
    printf("Channels:      %d\n", stats->channels);
    printf("\nPeak level:    %.2f dBFS (positive: %.6f, negative: %.6f)\n", 
           stats->peak_dbfs, stats->peak_pos, stats->peak_neg);
    printf("RMS level:     %.2f dBFS (%.6f linear)\n", 
           stats->rms_dbfs, stats->rms);
    printf("Crest factor:  %.2f dB\n", stats->crest_factor);
}

int main(int argc, char *argv[]) {
    printf("=== Audio Interface Level Checker ===\n");
    
    if (argc == 2) {
        // Single file mode
        LevelStats stats;
        if (calculate_levels(argv[1], &stats) != 0) {
            return 1;
        }
        print_stats(argv[1], &stats);
        
    } else if (argc == 3) {
        // Comparison mode (reference vs recorded)
        LevelStats ref_stats, rec_stats;
        
        if (calculate_levels(argv[1], &ref_stats) != 0) {
            return 1;
        }
        if (calculate_levels(argv[2], &rec_stats) != 0) {
            return 1;
        }
        
        print_stats("Reference (Output)", &ref_stats);
        print_stats("Recorded (Input)", &rec_stats);
        
        // Calculate differences
        double peak_diff = rec_stats.peak_dbfs - ref_stats.peak_dbfs;
        double rms_diff = rec_stats.rms_dbfs - ref_stats.rms_dbfs;
        
        printf("\n=== Level Change (Input vs Output) ===\n");
        printf("Peak difference: %.2f dB", peak_diff);
        if (peak_diff > 0) {
            printf(" (GAIN)\n");
        } else if (peak_diff < 0) {
            printf(" (LOSS)\n");
        } else {
            printf(" (UNITY)\n");
        }
        
        printf("RMS difference:  %.2f dB", rms_diff);
        if (rms_diff > 0) {
            printf(" (GAIN)\n");
        } else if (rms_diff < 0) {
            printf(" (LOSS)\n");
        } else {
            printf(" (UNITY)\n");
        }
        
        // Interpretation
        printf("\n=== Interpretation ===\n");
        if (fabs(peak_diff - rms_diff) < 0.5) {
            printf("Level change is consistent across peak and RMS: %.2f dB\n", rms_diff);
            printf("This suggests a simple gain/attenuation stage.\n");
        } else {
            printf("Peak and RMS differences don't match (%.2f vs %.2f dB)\n", 
                   peak_diff, rms_diff);
            printf("This could indicate compression, clipping, or noise.\n");
        }
        
        // Expected unity gain scenario
        if (fabs(rms_diff) < 0.5) {
            printf("\nNear unity gain detected - loopback appears transparent.\n");
        } else if (rms_diff < -5.0 && rms_diff > -10.0) {
            printf("\nTypical line input attenuation detected (%.2f dB).\n", rms_diff);
            printf("Many audio interfaces attenuate line inputs to prevent clipping.\n");
        } else if (rms_diff < -10.0) {
            printf("\nLarge attenuation detected (%.2f dB).\n", rms_diff);
            printf("Check input gain/trim settings or interface routing.\n");
        }
        
    } else {
        printf("\nUsage:\n");
        printf("  Single file:  %s <file.wav>\n", argv[0]);
        printf("  Compare:      %s <reference.wav> <recorded.wav>\n\n", argv[0]);
        printf("In compare mode, reference should be the output signal,\n");
        printf("and recorded should be the signal coming back through the input.\n");
        return 1;
    }
    
    printf("\n");
    return 0;
}
