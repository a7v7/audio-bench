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
 * ab_acq.c - Audio acquisition tool for sound card recording
 *
 * Features:
 * - Record audio from specified device
 * - Configurable sample rate, bit depth, and channels
 * - Output to WAV format using libsndfile
 *
 * Note: Use 'ab_list_dev' to enumerate available audio devices
 *
 * Dependencies: PortAudio, libsndfile, libpopt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>
#include <sndfile.h>
#include <popt.h>

#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_BIT_DEPTH 16
#define DEFAULT_CHANNELS 2
#define DEFAULT_DURATION 5.0
#define FRAMES_PER_BUFFER 512

/* Recording state structure */
typedef struct {
    float *buffer;
    size_t buffer_size;
    size_t buffer_index;
    int channels;
    int finished;
} RecordingData;

/* PortAudio callback for recording */
static int recordCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo *timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData)
{
    RecordingData *data = (RecordingData*)userData;
    const float *input = (const float*)inputBuffer;
    size_t samples_to_copy = framesPerBuffer * data->channels;
    size_t remaining_space = data->buffer_size - data->buffer_index;

    (void) outputBuffer; /* Prevent unused variable warning */
    (void) timeInfo;
    (void) statusFlags;

    if (input == NULL) {
        fprintf(stderr, "Warning: Input buffer is NULL\n");
        return paContinue;
    }

    /* Limit samples to available buffer space */
    if (samples_to_copy > remaining_space) {
        samples_to_copy = remaining_space;
    }

    /* Copy audio data to our buffer */
    memcpy(&data->buffer[data->buffer_index], input,
           samples_to_copy * sizeof(float));
    data->buffer_index += samples_to_copy;

    /* Check if buffer is full */
    if (data->buffer_index >= data->buffer_size) {
        data->finished = 1;
        return paComplete;
    }

    return paContinue;
}


/* Record audio from specified device */
static int record_audio(int device_index, const char *output_file,
                       int sample_rate, int bit_depth, int channels,
                       double duration)
{
    PaError err;
    PaStream *stream;
    PaStreamParameters input_params;
    const PaDeviceInfo *device_info;
    RecordingData recording_data;
    SNDFILE *sndfile;
    SF_INFO sf_info;
    int num_devices;

    /* Validate device index */
    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Error: Failed to initialize PortAudio: %s\n",
                Pa_GetErrorText(err));
        return -1;
    }

    num_devices = Pa_GetDeviceCount();
    if (device_index < 0 || device_index >= num_devices) {
        fprintf(stderr, "Error: Invalid device index %d (valid range: 0-%d)\n",
                device_index, num_devices - 1);
        fprintf(stderr, "Use 'ab_list_dev' to see available devices.\n");
        Pa_Terminate();
        return -1;
    }

    device_info = Pa_GetDeviceInfo(device_index);
    if (device_info->maxInputChannels == 0) {
        fprintf(stderr, "Error: Device %d has no input channels\n", device_index);
        Pa_Terminate();
        return -1;
    }

    /* Limit channels to device capability */
    if (channels > device_info->maxInputChannels) {
        fprintf(stderr, "Warning: Requested %d channels, but device only supports %d. "
                "Using %d channels.\n",
                channels, device_info->maxInputChannels,
                device_info->maxInputChannels);
        channels = device_info->maxInputChannels;
    }

    printf("Recording from device %d: %s\n", device_index, device_info->name);
    printf("Sample rate: %d Hz, Bit depth: %d, Channels: %d, Duration: %.1f seconds\n",
           sample_rate, bit_depth, channels, duration);

    /* Allocate recording buffer */
    size_t total_frames = (size_t)(sample_rate * duration);
    size_t buffer_size = total_frames * channels;
    recording_data.buffer = (float*)malloc(buffer_size * sizeof(float));
    if (recording_data.buffer == NULL) {
        fprintf(stderr, "Error: Failed to allocate recording buffer\n");
        Pa_Terminate();
        return -1;
    }
    recording_data.buffer_size = buffer_size;
    recording_data.buffer_index = 0;
    recording_data.channels = channels;
    recording_data.finished = 0;

    /* Configure input parameters */
    memset(&input_params, 0, sizeof(input_params));
    input_params.device = device_index;
    input_params.channelCount = channels;
    input_params.sampleFormat = paFloat32;
    input_params.suggestedLatency = device_info->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = NULL;

    /* Open audio stream */
    err = Pa_OpenStream(&stream,
                       &input_params,
                       NULL, /* No output */
                       sample_rate,
                       FRAMES_PER_BUFFER,
                       paClipOff,
                       recordCallback,
                       &recording_data);

    if (err != paNoError) {
        fprintf(stderr, "Error: Failed to open stream: %s\n",
                Pa_GetErrorText(err));
        free(recording_data.buffer);
        Pa_Terminate();
        return -1;
    }

    /* Get actual stream sample rate (may differ from requested) */
    const PaStreamInfo *stream_info = Pa_GetStreamInfo(stream);
    if (stream_info == NULL) {
        fprintf(stderr, "Error: Failed to get stream info\n");
        Pa_CloseStream(stream);
        free(recording_data.buffer);
        Pa_Terminate();
        return -1;
    }

    int actual_sample_rate = (int)stream_info->sampleRate;
    if (actual_sample_rate != sample_rate) {
        fprintf(stderr, "Warning: Requested sample rate %d Hz, but device is using %d Hz\n",
                sample_rate, actual_sample_rate);
        printf("Actual recording rate: %d Hz\n", actual_sample_rate);
    }

    /* Start recording */
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "Error: Failed to start stream: %s\n",
                Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        free(recording_data.buffer);
        Pa_Terminate();
        return -1;
    }

    printf("Recording... ");
    fflush(stdout);

    /* Wait for recording to complete */
    while (Pa_IsStreamActive(stream) == 1 && !recording_data.finished) {
        Pa_Sleep(100);
    }

    printf("Done.\n");

    /* Stop and close stream */
    err = Pa_StopStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "Warning: Error stopping stream: %s\n",
                Pa_GetErrorText(err));
    }

    Pa_CloseStream(stream);
    Pa_Terminate();

    /* Write to WAV file using libsndfile */
    memset(&sf_info, 0, sizeof(sf_info));
    sf_info.samplerate = actual_sample_rate;
    sf_info.channels = channels;

    /* Set format based on bit depth */
    switch (bit_depth) {
        case 16:
            sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
            break;
        case 24:
            sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
            break;
        case 32:
            sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_32;
            break;
        default:
            fprintf(stderr, "Error: Unsupported bit depth %d (use 16, 24, or 32)\n",
                    bit_depth);
            free(recording_data.buffer);
            return -1;
    }

    sndfile = sf_open(output_file, SFM_WRITE, &sf_info);
    if (sndfile == NULL) {
        fprintf(stderr, "Error: Failed to open output file '%s': %s\n",
                output_file, sf_strerror(NULL));
        free(recording_data.buffer);
        return -1;
    }

    /* Write audio data */
    sf_count_t frames_written = sf_writef_float(sndfile, recording_data.buffer,
                                                total_frames);
    if (frames_written != (sf_count_t)total_frames) {
        fprintf(stderr, "Warning: Only wrote %lld of %zu frames\n",
                (long long)frames_written, total_frames);
    }

    sf_close(sndfile);
    free(recording_data.buffer);

    printf("Saved %lld frames to '%s'\n", (long long)frames_written, output_file);
    return 0;
}

int main(int argc, const char **argv)
{
    /* Command-line options */
    int version_flag = 0;
    int device_index = -1;
    char *output_file = NULL;
    int sample_rate = DEFAULT_SAMPLE_RATE;
    int bit_depth = DEFAULT_BIT_DEPTH;
    int channels = DEFAULT_CHANNELS;
    double duration = DEFAULT_DURATION;

    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0,
         "Show version information", NULL},
        {"device", 'd', POPT_ARG_INT, &device_index, 0,
         "Device index to record from (use ab_list_dev to see devices)", "INDEX"},
        {"output", 'o', POPT_ARG_STRING, &output_file, 0,
         "Output WAV file", "FILE"},
        {"sample-rate", 'r', POPT_ARG_INT, &sample_rate, 0,
         "Sample rate in Hz (default: 44100)", "RATE"},
        {"bit-depth", 'b', POPT_ARG_INT, &bit_depth, 0,
         "Bit depth: 16, 24, or 32 (default: 16)", "DEPTH"},
        {"channels", 'c', POPT_ARG_INT, &channels, 0,
         "Number of channels: 1 (mono) or 2 (stereo) (default: 2)", "COUNT"},
        {"duration", 't', POPT_ARG_DOUBLE, &duration, 0,
         "Recording duration in seconds (default: 5.0)", "SECONDS"},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(NULL, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "Audio acquisition tool for recording from sound cards.\n"
        "Use 'ab_list_dev' to list available audio devices.\n\n"
        "Examples:\n"
        "  ab_acq -d 0 -o test.wav                 # Record 5s from device 0\n"
        "  ab_acq -d 1 -o out.wav -t 10 -r 48000   # Record 10s at 48kHz\n"
        "  ab_acq -d 0 -o mono.wav -c 1 -b 24      # Record mono 24-bit audio\n");

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
        printf("ab_acq version 1.0.0\n");
        printf("Audio acquisition tool for audio-bench\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        poptFreeContext(popt_ctx);
        return 0;
    }

    /* Validate recording parameters */
    if (device_index < 0) {
        fprintf(stderr, "Error: Device index required for recording (use -d/--device)\n");
        fprintf(stderr, "Use 'ab_list_dev' to see available devices.\n");
        poptFreeContext(popt_ctx);
        return 1;
    }

    if (output_file == NULL) {
        fprintf(stderr, "Error: Output file required for recording (use -o/--output)\n");
        poptFreeContext(popt_ctx);
        return 1;
    }

    if (channels < 1 || channels > 2) {
        fprintf(stderr, "Error: Channels must be 1 (mono) or 2 (stereo)\n");
        poptFreeContext(popt_ctx);
        return 1;
    }

    if (duration <= 0) {
        fprintf(stderr, "Error: Duration must be positive\n");
        poptFreeContext(popt_ctx);
        return 1;
    }

    /* Perform recording */
    int result = record_audio(device_index, output_file, sample_rate, bit_depth,
                             channels, duration);

    poptFreeContext(popt_ctx);
    return result;
}
