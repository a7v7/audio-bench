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

#ifdef _WIN32
#include <windows.h>
#endif

/* Device filter modes */
typedef enum {
    FILTER_ALL,      /* Show all devices */
    FILTER_INPUT,    /* Show only input devices */
    FILTER_OUTPUT    /* Show only output devices */
} DeviceFilter;

/*
 * Count display width of UTF-8 string (number of visible characters)
 *
 * @param str: UTF-8 encoded string
 * @return: Number of display characters
 */
static size_t utf8_display_width(const char *str)
{
    if (!str) return 0;

    size_t count = 0;
    const unsigned char *s = (const unsigned char *)str;

    while (*s) {
        /* Count only the start of UTF-8 sequences, not continuation bytes */
        /* UTF-8 continuation bytes start with bits 10xxxxxx (0x80-0xBF) */
        if ((*s & 0xC0) != 0x80) {
            count++;
        }
        s++;
    }

    return count;
}

/*
 * Check if a device name appears truncated
 *
 * @param name: Device name to check
 * @return: 1 if truncated, 0 otherwise
 */
static int is_name_truncated(const char *name)
{
    if (!name) return 0;

    size_t len = strlen(name);
    if (len == 0) return 0;

    /* Check for trailing space (common truncation indicator) */
    if (name[len - 1] == ' ') {
        return 1;
    }

    /* Check for unmatched parentheses */
    int paren_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (name[i] == '(') paren_count++;
        if (name[i] == ')') paren_count--;
    }

    if (paren_count != 0) {
        return 1;
    }

    return 0;
}

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
    int mme_truncation_detected = 0;
    size_t max_name_len = 11;  // Minimum: "Device Name"
    size_t max_host_len = 8;   // Minimum: "Host API"

#ifdef _WIN32
    /* Set console output to UTF-8 to handle Unicode characters */
    SetConsoleOutputCP(CP_UTF8);
#endif

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

    /* First pass: determine maximum column widths */
    for (int i = 0; i < num_devices; i++) {
        device_info = Pa_GetDeviceInfo(i);
        host_info = Pa_GetHostApiInfo(device_info->hostApi);

        /* Check if device matches filter */
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
            size_t name_len = strlen(device_info->name);
            size_t host_len = strlen(host_info->name);

            /* Add 3 chars for "..." if name appears truncated */
            if (is_name_truncated(device_info->name)) {
                name_len += 3;
            }

            if (name_len > max_name_len) {
                max_name_len = name_len;
            }
            if (host_len > max_host_len) {
                max_host_len = host_len;
            }
        }
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

    /* Print table header with dynamic widths */
    printf("%-4s %-*s %-*s %-8s %-8s %s\n",
           "ID", (int)max_name_len, "Device Name",
           (int)max_host_len, "Host API",
           "In Ch", "Out Ch", "Default Rate");

    /* Print separator line */
    int total_width = 4 + 1 + max_name_len + 1 + max_host_len + 1 + 8 + 1 + 8 + 1 + 12;
    for (int i = 0; i < total_width; i++) {
        printf("-");
    }
    printf("\n");

    /* Second pass: print device information */
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

            /* Check for truncation and format device name accordingly */
            char formatted_name[256];
            if (is_name_truncated(device_info->name)) {
                snprintf(formatted_name, sizeof(formatted_name), "%s...", device_info->name);

                /* Track if this is MME truncation */
                if (strcmp(host_info->name, "MME") == 0) {
                    mme_truncation_detected = 1;
                }
            } else {
                snprintf(formatted_name, sizeof(formatted_name), "%s", device_info->name);
            }

            printf("%-4d %-*s %-*s %-8s %-8s %.0f Hz\n",
                   i,
                   (int)max_name_len, formatted_name,
                   (int)max_host_len, host_info->name,
                   in_ch_str,
                   out_ch_str,
                   device_info->defaultSampleRate);

            device_count++;
        }
    }

    /* Print separator line */
    for (int i = 0; i < total_width; i++) {
        printf("-");
    }
    printf("\n");
    printf("Total devices found: %d\n", device_count);

    /* Add note about MME truncation if detected */
    if (mme_truncation_detected) {
        printf("\nNote: Device names ending with \"...\" are truncated by the MME (Multimedia\n");
        printf("      Extensions) API, which has a 32-character limit. The same device may\n");
        printf("      appear with its full name under other APIs (DirectSound, WASAPI, WDM-KS).\n");
    }

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
    PaStreamParameters input_params, output_params;
    int num_devices;
    int is_default_input = 0;
    int is_default_output = 0;

    /* Standard sample rates to test */
    const int test_sample_rates[] = {
        8000, 11025, 16000, 22050, 32000, 44100, 48000,
        88200, 96000, 176400, 192000
    };
    const int num_sample_rates = sizeof(test_sample_rates) / sizeof(test_sample_rates[0]);

    /* Sample formats to test */
    typedef struct {
        PaSampleFormat format;
        const char *name;
        int bit_depth;
    } FormatInfo;

    const FormatInfo test_formats[] = {
        {paInt8, "8-bit PCM", 8},
        {paInt16, "16-bit PCM", 16},
        {paInt24, "24-bit PCM", 24},
        {paInt32, "32-bit PCM", 32},
        {paFloat32, "32-bit Float", 32}
    };
    const int num_formats = sizeof(test_formats) / sizeof(test_formats[0]);

#ifdef _WIN32
    /* Set console output to UTF-8 to handle Unicode characters */
    SetConsoleOutputCP(CP_UTF8);
#endif

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

    /* Display basic device information */
    printf("Device %d: %s", device_index, device_info->name);
    if (is_name_truncated(device_info->name)) {
        printf("...");
    }
    printf("\n");
    printf("================================================================================\n");
    printf("Host API:                %s\n", host_info->name);

    /* Add note if name is truncated by MME */
    if (is_name_truncated(device_info->name) && strcmp(host_info->name, "MME") == 0) {
        printf("\nNOTE: Device name truncated by MME API (32 character limit).\n");
        printf("      Full name may be visible under other APIs (DirectSound, WASAPI, WDM-KS).\n\n");
    }
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

    /* Test supported sample rates for INPUT devices */
    if (device_info->maxInputChannels > 0) {
        printf("Supported Input Sample Rates:\n");
        printf("--------------------------------------------------------------------------------\n");

        memset(&input_params, 0, sizeof(input_params));
        input_params.device = device_index;
        input_params.channelCount = device_info->maxInputChannels > 1 ? 2 : 1;
        input_params.sampleFormat = paInt16;
        input_params.suggestedLatency = device_info->defaultLowInputLatency;
        input_params.hostApiSpecificStreamInfo = NULL;

        for (int i = 0; i < num_sample_rates; i++) {
            err = Pa_IsFormatSupported(&input_params, NULL, test_sample_rates[i]);
            if (err == paFormatIsSupported) {
                printf("  %6d Hz  [OK]\n", test_sample_rates[i]);
            }
        }
        printf("\n");

        /* Test supported formats at default sample rate */
        printf("Supported Input Formats (at %.0f Hz, %d ch):\n",
               device_info->defaultSampleRate,
               input_params.channelCount);
        printf("--------------------------------------------------------------------------------\n");

        for (int i = 0; i < num_formats; i++) {
            input_params.sampleFormat = test_formats[i].format;
            err = Pa_IsFormatSupported(&input_params, NULL, device_info->defaultSampleRate);
            if (err == paFormatIsSupported) {
                printf("  %-20s [OK]\n", test_formats[i].name);
            }
        }
        printf("\n");

        /* Test channel configurations */
        printf("Supported Input Channel Configurations (at %.0f Hz, 16-bit):\n",
               device_info->defaultSampleRate);
        printf("--------------------------------------------------------------------------------\n");

        input_params.sampleFormat = paInt16;
        for (int ch = 1; ch <= device_info->maxInputChannels && ch <= 8; ch++) {
            input_params.channelCount = ch;
            err = Pa_IsFormatSupported(&input_params, NULL, device_info->defaultSampleRate);
            if (err == paFormatIsSupported) {
                printf("  %d channel%s  [OK]\n", ch, ch > 1 ? "s" : "");
            }
        }
        printf("\n");
    }

    /* Test supported sample rates for OUTPUT devices */
    if (device_info->maxOutputChannels > 0) {
        printf("Supported Output Sample Rates:\n");
        printf("--------------------------------------------------------------------------------\n");

        memset(&output_params, 0, sizeof(output_params));
        output_params.device = device_index;
        output_params.channelCount = device_info->maxOutputChannels > 1 ? 2 : 1;
        output_params.sampleFormat = paInt16;
        output_params.suggestedLatency = device_info->defaultLowOutputLatency;
        output_params.hostApiSpecificStreamInfo = NULL;

        for (int i = 0; i < num_sample_rates; i++) {
            err = Pa_IsFormatSupported(NULL, &output_params, test_sample_rates[i]);
            if (err == paFormatIsSupported) {
                printf("  %6d Hz  [OK]\n", test_sample_rates[i]);
            }
        }
        printf("\n");

        /* Test supported formats at default sample rate */
        printf("Supported Output Formats (at %.0f Hz, %d ch):\n",
               device_info->defaultSampleRate,
               output_params.channelCount);
        printf("--------------------------------------------------------------------------------\n");

        for (int i = 0; i < num_formats; i++) {
            output_params.sampleFormat = test_formats[i].format;
            err = Pa_IsFormatSupported(NULL, &output_params, device_info->defaultSampleRate);
            if (err == paFormatIsSupported) {
                printf("  %-20s [OK]\n", test_formats[i].name);
            }
        }
        printf("\n");

        /* Test channel configurations */
        printf("Supported Output Channel Configurations (at %.0f Hz, 16-bit):\n",
               device_info->defaultSampleRate);
        printf("--------------------------------------------------------------------------------\n");

        output_params.sampleFormat = paInt16;
        for (int ch = 1; ch <= device_info->maxOutputChannels && ch <= 8; ch++) {
            output_params.channelCount = ch;
            err = Pa_IsFormatSupported(NULL, &output_params, device_info->defaultSampleRate);
            if (err == paFormatIsSupported) {
                printf("  %d channel%s  [OK]\n", ch, ch > 1 ? "s" : "");
            }
        }
        printf("\n");
    }

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
