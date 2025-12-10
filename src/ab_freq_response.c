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
#include <complex.h>
#include <sndfile.h>
#include <fftw3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MIN_MAG_DB -120.0

typedef struct {
    double *data;
    size_t length;
    int sample_rate;
    int channels;
} AudioBuffer;

// Load audio file into buffer
int load_audio_file(const char *filename, AudioBuffer *buf) {
    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(SF_INFO));
    
    SNDFILE *sf = sf_open(filename, SFM_READ, &sf_info);
    if (!sf) {
        fprintf(stderr, "Error opening %s: %s\n", filename, sf_strerror(NULL));
        return -1;
    }
    
    buf->length = sf_info.frames;
    buf->sample_rate = sf_info.samplerate;
    buf->channels = sf_info.channels;
    
    // Allocate buffer for mono (we'll mix down if stereo)
    buf->data = (double *)malloc(buf->length * sizeof(double));
    if (!buf->data) {
        fprintf(stderr, "Memory allocation failed\n");
        sf_close(sf);
        return -1;
    }
    
    if (sf_info.channels == 1) {
        // Mono - read directly
        sf_read_double(sf, buf->data, buf->length);
    } else {
        // Multi-channel - mix to mono
        double *temp = (double *)malloc(buf->length * sf_info.channels * sizeof(double));
        sf_read_double(sf, temp, buf->length * sf_info.channels);
        
        for (size_t i = 0; i < buf->length; i++) {
            buf->data[i] = 0.0;
            for (int ch = 0; ch < sf_info.channels; ch++) {
                buf->data[i] += temp[i * sf_info.channels + ch];
            }
            buf->data[i] /= sf_info.channels;
        }
        free(temp);
    }
    
    sf_close(sf);
    printf("Loaded %s: %zu samples, %d Hz, %d channel(s)\n", 
           filename, buf->length, buf->sample_rate, sf_info.channels);
    
    return 0;
}

// Free audio buffer
void free_audio_buffer(AudioBuffer *buf) {
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
}

// Calculate RMS level of a signal
double calculate_rms(double *data, size_t length) {
    double sum_sq = 0.0;
    for (size_t i = 0; i < length; i++) {
        sum_sq += data[i] * data[i];
    }
    return sqrt(sum_sq / length);
}

// Compute frequency response via deconvolution: H(f) = Y(f) / X(f)
int compute_frequency_response(AudioBuffer *reference, AudioBuffer *recorded,
                               double **freq_axis, double **magnitude_db, 
                               double **phase_deg, size_t *num_bins,
                               int normalize_levels) {
    
    // Verify compatibility
    if (reference->sample_rate != recorded->sample_rate) {
        fprintf(stderr, "Sample rate mismatch: ref=%d, rec=%d\n",
                reference->sample_rate, recorded->sample_rate);
        return -1;
    }
    
    // Calculate and report RMS levels
    double ref_rms = calculate_rms(reference->data, reference->length);
    double rec_rms = calculate_rms(recorded->data, recorded->length);
    
    double ref_db = 20.0 * log10(ref_rms);
    double rec_db = 20.0 * log10(rec_rms);
    double level_diff = rec_db - ref_db;
    
    printf("\n=== Signal Levels ===\n");
    printf("Reference RMS: %.6f (%.2f dBFS)\n", ref_rms, ref_db);
    printf("Recorded RMS:  %.6f (%.2f dBFS)\n", rec_rms, rec_db);
    printf("Level difference: %.2f dB\n", level_diff);
    
    if (normalize_levels) {
        // Normalize recorded signal to match reference level
        double gain_factor = ref_rms / rec_rms;
        printf("Applying gain compensation: %.2f dB\n", 20.0 * log10(gain_factor));
        for (size_t i = 0; i < recorded->length; i++) {
            recorded->data[i] *= gain_factor;
        }
    } else {
        printf("No gain compensation applied (frequency response includes %.2f dB offset)\n", level_diff);
    }
    
    // Use the longer length and zero-pad both to same size
    size_t fft_size = (reference->length > recorded->length) ? 
                      reference->length : recorded->length;
    
    // Make FFT size a power of 2 for efficiency
    size_t fft_size_pow2 = 1;
    while (fft_size_pow2 < fft_size) {
        fft_size_pow2 <<= 1;
    }
    fft_size = fft_size_pow2;
    
    printf("FFT size: %zu\n", fft_size);
    
    // Allocate padded buffers
    double *ref_padded = (double *)fftw_malloc(fft_size * sizeof(double));
    double *rec_padded = (double *)fftw_malloc(fft_size * sizeof(double));
    fftw_complex *ref_fft = (fftw_complex *)fftw_malloc((fft_size/2 + 1) * sizeof(fftw_complex));
    fftw_complex *rec_fft = (fftw_complex *)fftw_malloc((fft_size/2 + 1) * sizeof(fftw_complex));
    
    if (!ref_padded || !rec_padded || !ref_fft || !rec_fft) {
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }
    
    // Copy and zero-pad
    memcpy(ref_padded, reference->data, reference->length * sizeof(double));
    memset(ref_padded + reference->length, 0, (fft_size - reference->length) * sizeof(double));
    
    memcpy(rec_padded, recorded->data, recorded->length * sizeof(double));
    memset(rec_padded + recorded->length, 0, (fft_size - recorded->length) * sizeof(double));
    
    // Create FFT plans
    fftw_plan ref_plan = fftw_plan_dft_r2c_1d(fft_size, ref_padded, ref_fft, FFTW_ESTIMATE);
    fftw_plan rec_plan = fftw_plan_dft_r2c_1d(fft_size, rec_padded, rec_fft, FFTW_ESTIMATE);
    
    // Execute FFTs
    printf("Computing FFTs...\n");
    fftw_execute(ref_plan);
    fftw_execute(rec_plan);
    
    // Compute frequency response H(f) = Y(f) / X(f)
    *num_bins = fft_size / 2 + 1;
    *freq_axis = (double *)malloc(*num_bins * sizeof(double));
    *magnitude_db = (double *)malloc(*num_bins * sizeof(double));
    *phase_deg = (double *)malloc(*num_bins * sizeof(double));
    
    if (!*freq_axis || !*magnitude_db || !*phase_deg) {
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }
    
    double freq_resolution = (double)reference->sample_rate / fft_size;
    
    printf("Computing frequency response...\n");
    for (size_t i = 0; i < *num_bins; i++) {
        (*freq_axis)[i] = i * freq_resolution;

        // Get complex values (accessing FFTW complex as [real, imag] pairs)
        double X_real = ((double*)ref_fft)[2*i];
        double X_imag = ((double*)ref_fft)[2*i + 1];
        double Y_real = ((double*)rec_fft)[2*i];
        double Y_imag = ((double*)rec_fft)[2*i + 1];

        double complex X = X_real + I * X_imag;
        double complex Y = Y_real + I * Y_imag;

        // Compute H = Y / X with regularization to avoid division by near-zero
        double X_mag = cabs(X);
        double DC_mag = sqrt(((double*)rec_fft)[0] * ((double*)rec_fft)[0] +
                            ((double*)rec_fft)[1] * ((double*)rec_fft)[1]);
        double regularization = 1e-10 * DC_mag; // Small fraction of DC
        
        double complex H;
        if (X_mag > regularization) {
            H = Y / X;
        } else {
            // Below noise floor - use zero response
            H = 0.0;
        }
        
        // Magnitude in dB
        double mag = cabs(H);
        if (mag > 0.0) {
            (*magnitude_db)[i] = 20.0 * log10(mag);
        } else {
            (*magnitude_db)[i] = MIN_MAG_DB;
        }
        
        // Limit minimum magnitude
        if ((*magnitude_db)[i] < MIN_MAG_DB) {
            (*magnitude_db)[i] = MIN_MAG_DB;
        }
        
        // Phase in degrees
        (*phase_deg)[i] = carg(H) * 180.0 / M_PI;
    }
    
    // Cleanup
    fftw_destroy_plan(ref_plan);
    fftw_destroy_plan(rec_plan);
    fftw_free(ref_padded);
    fftw_free(rec_padded);
    fftw_free(ref_fft);
    fftw_free(rec_fft);
    
    return 0;
}

// Write results to CSV file
int write_csv(const char *filename, double *freq, double *mag_db, 
              double *phase_deg, size_t num_bins) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error opening output file: %s\n", filename);
        return -1;
    }
    
    fprintf(fp, "Frequency_Hz,Magnitude_dB,Phase_deg\n");
    for (size_t i = 0; i < num_bins; i++) {
        fprintf(fp, "%.6f,%.6f,%.6f\n", freq[i], mag_db[i], phase_deg[i]);
    }
    
    fclose(fp);
    printf("Results written to %s\n", filename);
    return 0;
}

// Print summary statistics
void print_summary(double *freq, double *mag_db, size_t num_bins, int sample_rate) {
    // Find passband (20 Hz - 20 kHz)
    size_t start_idx = 0, end_idx = num_bins - 1;
    for (size_t i = 0; i < num_bins; i++) {
        if (freq[i] >= 20.0 && start_idx == 0) start_idx = i;
        if (freq[i] <= 20000.0) end_idx = i;
    }
    
    // Find min/max/avg in passband
    double min_db = mag_db[start_idx];
    double max_db = mag_db[start_idx];
    double sum_db = 0.0;
    size_t count = 0;
    
    for (size_t i = start_idx; i <= end_idx; i++) {
        if (mag_db[i] > MIN_MAG_DB + 10) { // Ignore noise floor
            if (mag_db[i] < min_db) min_db = mag_db[i];
            if (mag_db[i] > max_db) max_db = mag_db[i];
            sum_db += mag_db[i];
            count++;
        }
    }
    
    double avg_db = (count > 0) ? sum_db / count : 0.0;
    
    printf("\n=== Frequency Response Summary (20 Hz - 20 kHz) ===\n");
    printf("Average level: %.2f dB\n", avg_db);
    printf("Peak level: %.2f dB at ", max_db);
    for (size_t i = start_idx; i <= end_idx; i++) {
        if (fabs(mag_db[i] - max_db) < 0.01) {
            printf("%.1f Hz ", freq[i]);
            break;
        }
    }
    printf("\n");
    printf("Minimum level: %.2f dB\n", min_db);
    printf("Variation: %.2f dB\n", max_db - min_db);
    printf("Frequency resolution: %.2f Hz\n", freq[1] - freq[0]);
}

void print_usage(const char *prog_name) {
    printf("Usage: %s <reference.wav> <recorded.wav> [output.csv] [--no-normalize]\n\n", prog_name);
    printf("Measures frequency response by deconvolving recorded signal with reference.\n");
    printf("  reference.wav - Original stimulus signal\n");
    printf("  recorded.wav  - Recorded response (after passing through system)\n");
    printf("  output.csv    - Output file (default: freq_response.csv)\n");
    printf("  --no-normalize - Don't compensate for level differences (default: auto-compensate)\n\n");
    printf("By default, the program compensates for any level difference between reference\n");
    printf("and recorded signals, making the frequency response show only the frequency-\n");
    printf("dependent characteristics. Use --no-normalize to see the absolute gain/loss.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *ref_filename = argv[1];
    const char *rec_filename = argv[2];
    const char *out_filename = "freq_response.csv";
    int normalize_levels = 1; // Default: normalize levels
    
    // Parse remaining arguments
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--no-normalize") == 0) {
            normalize_levels = 0;
        } else {
            // Assume it's the output filename
            out_filename = argv[i];
        }
    }
    
    printf("=== Frequency Response Measurement via Deconvolution ===\n");
    printf("Level normalization: %s\n\n", normalize_levels ? "ENABLED" : "DISABLED");
    
    // Load audio files
    AudioBuffer reference = {0};
    AudioBuffer recorded = {0};
    
    if (load_audio_file(ref_filename, &reference) != 0) {
        return 1;
    }
    
    if (load_audio_file(rec_filename, &recorded) != 0) {
        free_audio_buffer(&reference);
        return 1;
    }
    
    // Compute frequency response
    double *freq_axis = NULL;
    double *magnitude_db = NULL;
    double *phase_deg = NULL;
    size_t num_bins = 0;
    
    if (compute_frequency_response(&reference, &recorded, 
                                   &freq_axis, &magnitude_db, 
                                   &phase_deg, &num_bins, normalize_levels) != 0) {
        free_audio_buffer(&reference);
        free_audio_buffer(&recorded);
        return 1;
    }
    
    // Write results
    if (write_csv(out_filename, freq_axis, magnitude_db, phase_deg, num_bins) != 0) {
        free(freq_axis);
        free(magnitude_db);
        free(phase_deg);
        free_audio_buffer(&reference);
        free_audio_buffer(&recorded);
        return 1;
    }
    
    // Print summary
    print_summary(freq_axis, magnitude_db, num_bins, reference.sample_rate);
    
    // Cleanup
    free(freq_axis);
    free(magnitude_db);
    free(phase_deg);
    free_audio_buffer(&reference);
    free_audio_buffer(&recorded);
    
    printf("\nDone!\n");
    return 0;
}
