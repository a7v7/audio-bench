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
 * ab_list_dev.c - Audio device listing utility
 *
 * Features:
 * - List all audio devices (input and output)
 * - Filter by input devices only
 * - Filter by output devices only
 * - Display device properties (channels, sample rate, host API)
 *
 * Dependencies: PortAudio, libpopt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>
#include <popt.h>

/* Device filter modes */
typedef enum {
    FILTER_ALL,      /* Show all devices */
    FILTER_INPUT,    /* Show only input devices */
    FILTER_OUTPUT    /* Show only output devices */
} DeviceFilter;

/*
 * List audio devices based on filter mode
 *
 * @param filter: Device filter (FILTER_ALL, FILTER_INPUT, FILTER_OUTPUT)
 * @return: 0 on success, -1 on error
 */
static int list_devices(DeviceFilter filter)
{
    PaError err;
    int num_devices;
    const PaDeviceInfo *device_info;
    const PaHostApiInfo *host_info;
    int device_count = 0;

    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Error: Failed to initialize PortAudio: %s\n",
                Pa_GetErrorText(err));
        return -1;
    }

    num_devices = Pa_GetDeviceCount();
    if (num_devices < 0) {
        fprintf(stderr, "Error: Pa_GetDeviceCount returned %d\n", num_devices);
        Pa_Terminate();
        return -1;
    }

    if (num_devices == 0) {
        printf("No audio devices found.\n");
        Pa_Terminate();
        return 0;
    }

    /* Print header based on filter mode */
    switch (filter) {
        case FILTER_INPUT:
            printf("Available audio INPUT devices:\n");
            break;
        case FILTER_OUTPUT:
            printf("Available audio OUTPUT devices:\n");
            break;
        case FILTER_ALL:
        default:
            printf("Available audio devices:\n");
            break;
    }

    printf("%-4s %-40s %-15s %-8s %-8s %s\n",
           "ID", "Device Name", "Host API", "In Ch", "Out Ch", "Default Rate");
    printf("--------------------------------------------------------------------------------\n");

    for (int i = 0; i < num_devices; i++) {
        device_info = Pa_GetDeviceInfo(i);
        host_info = Pa_GetHostApiInfo(device_info->hostApi);

        /* Apply filter */
        int show_device = 0;
        switch (filter) {
            case FILTER_INPUT:
                show_device = (device_info->maxInputChannels > 0);
                break;
            case FILTER_OUTPUT:
                show_device = (device_info->maxOutputChannels > 0);
                break;
            case FILTER_ALL:
            default:
                show_device = 1;
                break;
        }

        if (show_device) {
            /* Format input channels display */
            char in_ch_str[16];
            if (device_info->maxInputChannels > 0) {
                snprintf(in_ch_str, sizeof(in_ch_str), "%d",
                         device_info->maxInputChannels);
            } else {
                snprintf(in_ch_str, sizeof(in_ch_str), "-");
            }

            /* Format output channels display */
            char out_ch_str[16];
            if (device_info->maxOutputChannels > 0) {
                snprintf(out_ch_str, sizeof(out_ch_str), "%d",
                         device_info->maxOutputChannels);
            } else {
                snprintf(out_ch_str, sizeof(out_ch_str), "-");
            }

            printf("%-4d %-40s %-15s %-8s %-8s %.0f Hz\n",
                   i,
                   device_info->name,
                   host_info->name,
                   in_ch_str,
                   out_ch_str,
                   device_info->defaultSampleRate);

            device_count++;
        }
    }

    printf("--------------------------------------------------------------------------------\n");
    printf("Total devices found: %d\n", device_count);

    Pa_Terminate();
    return 0;
}

/*
 * Show detailed information about a specific device
 *
 * @param device_index: Index of device to examine
 * @return: 0 on success, -1 on error
 */
static int show_device_info(int device_index)
{
    PaError err;
    const PaDeviceInfo *device_info;
    const PaHostApiInfo *host_info;
    int num_devices;
    int is_default_input = 0;
    int is_default_output = 0;

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
        Pa_Terminate();
        return -1;
    }

    device_info = Pa_GetDeviceInfo(device_index);
    host_info = Pa_GetHostApiInfo(device_info->hostApi);

    /* Check if this is the default device */
    if (device_index == Pa_GetDefaultInputDevice()) {
        is_default_input = 1;
    }
    if (device_index == Pa_GetDefaultOutputDevice()) {
        is_default_output = 1;
    }

    /* Display device information */
    printf("Device %d: %s\n", device_index, device_info->name);
    printf("================================================================================\n");
    printf("Host API:                %s\n", host_info->name);
    printf("Max input channels:      %d", device_info->maxInputChannels);
    if (is_default_input) {
        printf(" (DEFAULT INPUT)");
    }
    printf("\n");
    printf("Max output channels:     %d", device_info->maxOutputChannels);
    if (is_default_output) {
        printf(" (DEFAULT OUTPUT)");
    }
    printf("\n");
    printf("Default sample rate:     %.0f Hz\n", device_info->defaultSampleRate);

    if (device_info->maxInputChannels > 0) {
        printf("Default low input latency:   %.3f ms\n",
               device_info->defaultLowInputLatency * 1000);
        printf("Default high input latency:  %.3f ms\n",
               device_info->defaultHighInputLatency * 1000);
    }

    if (device_info->maxOutputChannels > 0) {
        printf("Default low output latency:  %.3f ms\n",
               device_info->defaultLowOutputLatency * 1000);
        printf("Default high output latency: %.3f ms\n",
               device_info->defaultHighOutputLatency * 1000);
    }

    printf("\n");

    Pa_Terminate();
    return 0;
}

int main(int argc, const char **argv)
{
    /* Command-line options */
    int version_flag = 0;
    int input_flag = 0;
    int output_flag = 0;
    int info_device_index = -1;

    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &version_flag, 0,
         "Show version information", NULL},
        {"input", 'i', POPT_ARG_NONE, &input_flag, 0,
         "List only input devices", NULL},
        {"output", 'o', POPT_ARG_NONE, &output_flag, 0,
         "List only output devices", NULL},
        {"info", 'I', POPT_ARG_INT, &info_device_index, 0,
         "Show detailed information for specific device", "INDEX"},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(NULL, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "Audio device listing utility for audio-bench.\n"
        "Lists all available audio devices with their properties.\n\n"
        "Examples:\n"
        "  ab_list_dev                # List all audio devices\n"
        "  ab_list_dev --input        # List only input devices\n"
        "  ab_list_dev --output       # List only output devices\n"
        "  ab_list_dev --info 0       # Show detailed info for device 0\n");

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
        printf("ab_list_dev version 1.0.0\n");
        printf("Audio device listing utility for audio-bench\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        poptFreeContext(popt_ctx);
        return 0;
    }

    /* Handle device info mode */
    if (info_device_index >= 0) {
        int result = show_device_info(info_device_index);
        poptFreeContext(popt_ctx);
        return result;
    }

    /* Check for conflicting flags */
    if (input_flag && output_flag) {
        fprintf(stderr, "Error: Cannot specify both --input and --output\n");
        fprintf(stderr, "Use --help for usage information.\n");
        poptFreeContext(popt_ctx);
        return 1;
    }

    /* Determine filter mode */
    DeviceFilter filter = FILTER_ALL;
    if (input_flag) {
        filter = FILTER_INPUT;
    } else if (output_flag) {
        filter = FILTER_OUTPUT;
    }

    /* List devices with appropriate filter */
    int result = list_devices(filter);

    poptFreeContext(popt_ctx);
    return result;
}
