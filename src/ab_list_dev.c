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
#include <portaudio.h>
#include <popt.h>

#ifdef _WIN32
#include <windows.h>
#endif

//------------------------------------------------------------------------------
// Device filter modes
//------------------------------------------------------------------------------
typedef enum {
    FILTER_ALL,														//	Show all devices
    FILTER_INPUT,													//	Show only input devices
    FILTER_OUTPUT													//	Show only output devices
} DeviceFilter;

//------------------------------------------------------------------------------
//	Name:		utf8_display_width
//
//	Returns:	Number of display characters
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Counts display width of UTF-8 string
//	- Returns number of visible characters
//	- Handles multi-byte UTF-8 sequences correctly
//	- Skips UTF-8 continuation bytes (10xxxxxx)
//------------------------------------------------------------------------------
static size_t utf8_display_width(const char *str)
{
    if (!str) return 0;

    size_t count = 0;
    const unsigned char *s = (const unsigned char *)str;

    while (*s) {
//------------------------------------------------------------------------------
//	Count only the start of UTF-8 sequences, not continuation bytes
//	UTF-8 continuation bytes start with bits 10xxxxxx (0x80-0xBF)
//------------------------------------------------------------------------------
        if ((*s & 0xC0) != 0x80) {
            count++;
        }
        s++;
    }

    return count;
}

//------------------------------------------------------------------------------
//	Name:		remove_empty_parens
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Removes empty parentheses from device name
//	- Copies source to destination while filtering
//	- Skips "()" patterns and trailing spaces
//------------------------------------------------------------------------------
static void remove_empty_parens(char *dest, const char *src, size_t dest_size)
{
    if (!src || !dest || dest_size == 0) return;

    const char *read = src;
    char *write = dest;
    size_t remaining = dest_size - 1;								//	Reserve space for null terminator

    while (*read && remaining > 0) {
//------------------------------------------------------------------------------
//	Check for empty parentheses: "() " or "()" at end
//------------------------------------------------------------------------------
        if (read[0] == '(' && read[1] == ')') {
//------------------------------------------------------------------------------
//	Skip the empty parens
//------------------------------------------------------------------------------
            read += 2;
//------------------------------------------------------------------------------
//	Also skip trailing space after empty parens if present
//------------------------------------------------------------------------------
            if (*read == ' ') {
                read++;
            }
            continue;
        }

//------------------------------------------------------------------------------
//	Copy character
//------------------------------------------------------------------------------
        *write++ = *read++;
        remaining--;
    }

    *write = '\0';
}

//------------------------------------------------------------------------------
//	Name:		clean_device_name
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Cleans device name for display
//	- Removes empty parentheses
//	- Trims trailing whitespace
//------------------------------------------------------------------------------
static void clean_device_name(char *dest, const char *src, size_t dest_size)
{
    if (!src || !dest || dest_size == 0) return;

//------------------------------------------------------------------------------
//	Remove empty parentheses
//------------------------------------------------------------------------------
    remove_empty_parens(dest, src, dest_size);

//------------------------------------------------------------------------------
//	Trim trailing whitespace
//------------------------------------------------------------------------------
    size_t len = strlen(dest);
    while (len > 0 && (dest[len - 1] == ' ' || dest[len - 1] == '\t')) {
        dest[--len] = '\0';
    }
}

//------------------------------------------------------------------------------
//	Name:		should_display_device
//
//	Returns:	1 if device should be displayed, 0 otherwise
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Determines if device matches filter criteria
//	- FILTER_ALL: show all devices
//	- FILTER_INPUT: show only input devices
//	- FILTER_OUTPUT: show only output devices
//------------------------------------------------------------------------------
static int should_display_device(const PaDeviceInfo *device_info, DeviceFilter filter)
{
    switch (filter) {
        case FILTER_INPUT:
            return device_info->maxInputChannels > 0;
        case FILTER_OUTPUT:
            return device_info->maxOutputChannels > 0;
        case FILTER_ALL:
        default:
            return 1;
    }
}

//------------------------------------------------------------------------------
//	Name:		get_device_type
//
//	Returns:	String describing device type
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Returns device type as string
//	- Types: "Input", "Output", "Input/Output"
//	- Based on channel counts
//------------------------------------------------------------------------------
static const char* get_device_type(const PaDeviceInfo *device_info)
{
    int has_input = device_info->maxInputChannels > 0;
    int has_output = device_info->maxOutputChannels > 0;

    if (has_input && has_output) {
        return "Input/Output";
    } else if (has_input) {
        return "Input";
    } else if (has_output) {
        return "Output";
    } else {
        return "Unknown";
    }
}

//------------------------------------------------------------------------------
//	Name:		print_device_table_header
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Prints formatted table header
//	- Columns: Index, Type, Name, Channels, Sample Rate, Host API
//------------------------------------------------------------------------------
static void print_device_table_header(void)
{
    printf("%-5s %-13s %-40s %-8s %-11s %-15s\n",
           "Index", "Type", "Name", "Channels", "Sample Rate", "Host API");
    printf("-------------------------------------------------------------------------------------"
           "-----------------------------\n");
}

//------------------------------------------------------------------------------
//	Name:		print_device_row
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Prints formatted device information row
//	- Displays index, type, name, channels, sample rate, host API
//	- Handles UTF-8 names with proper alignment
//------------------------------------------------------------------------------
static void print_device_row(int device_index,
                             const PaDeviceInfo *device_info,
                             const PaHostApiInfo *host_info)
{
    char clean_name[256];
    clean_device_name(clean_name, device_info->name, sizeof(clean_name));

    const char *device_type = get_device_type(device_info);

//------------------------------------------------------------------------------
//	Calculate padding for UTF-8 name alignment
//------------------------------------------------------------------------------
    size_t display_width = utf8_display_width(clean_name);
    size_t name_col_width = 40;
    int padding = (display_width < name_col_width) ?
                  (name_col_width - display_width) : 0;

//------------------------------------------------------------------------------
//	Format channel info
//------------------------------------------------------------------------------
    char channels_str[16];
    if (device_info->maxInputChannels > 0 && device_info->maxOutputChannels > 0) {
        snprintf(channels_str, sizeof(channels_str), "%d/%d",
                device_info->maxInputChannels,
                device_info->maxOutputChannels);
    } else if (device_info->maxInputChannels > 0) {
        snprintf(channels_str, sizeof(channels_str), "%d in",
                device_info->maxInputChannels);
    } else {
        snprintf(channels_str, sizeof(channels_str), "%d out",
                device_info->maxOutputChannels);
    }

//------------------------------------------------------------------------------
//	Print device row
//------------------------------------------------------------------------------
    printf("%-5d %-13s %s%*s %-8s %6.0f Hz    %-15s\n",
           device_index,
           device_type,
           clean_name,
           padding, "",
           channels_str,
           device_info->defaultSampleRate,
           host_info->name);
}

//------------------------------------------------------------------------------
//	Name:		print_device_info_detailed
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Prints detailed device information
//	- Shows all properties including latency values
//	- Used for --info mode
//------------------------------------------------------------------------------
static void print_device_info_detailed(int device_index,
                                       const PaDeviceInfo *device_info,
                                       const PaHostApiInfo *host_info)
{
    char clean_name[256];
    clean_device_name(clean_name, device_info->name, sizeof(clean_name));

    printf("\nDevice %d:\n", device_index);
    printf("  Name: %s\n", clean_name);
    printf("  Host API: %s\n", host_info->name);
    printf("  Type: %s\n", get_device_type(device_info));

    printf("  Max Input Channels: %d\n", device_info->maxInputChannels);
    printf("  Max Output Channels: %d\n", device_info->maxOutputChannels);

    printf("  Default Sample Rate: %.0f Hz\n", device_info->defaultSampleRate);

    if (device_info->maxInputChannels > 0) {
        printf("  Default Low Input Latency: %.4f seconds\n",
               device_info->defaultLowInputLatency);
        printf("  Default High Input Latency: %.4f seconds\n",
               device_info->defaultHighInputLatency);
    }

    if (device_info->maxOutputChannels > 0) {
        printf("  Default Low Output Latency: %.4f seconds\n",
               device_info->defaultLowOutputLatency);
        printf("  Default High Output Latency: %.4f seconds\n",
               device_info->defaultHighOutputLatency);
    }
}

//------------------------------------------------------------------------------
//	Name:		list_audio_devices
//
//	Returns:	0 on success, -1 on error
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Lists all audio devices matching filter
//	- Initializes PortAudio
//	- Enumerates devices and displays information
//	- Optionally shows detailed info for specific device
//------------------------------------------------------------------------------
static int list_audio_devices(DeviceFilter filter, int info_device_index)
{
    PaError err;
    int num_devices;
    int displayed_count = 0;

//------------------------------------------------------------------------------
//	Initialize PortAudio
//------------------------------------------------------------------------------
    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Error: Failed to initialize PortAudio: %s\n",
                Pa_GetErrorText(err));
        return -1;
    }

//------------------------------------------------------------------------------
//	Get device count
//------------------------------------------------------------------------------
    num_devices = Pa_GetDeviceCount();
    if (num_devices < 0) {
        fprintf(stderr, "Error: Failed to get device count: %s\n",
                Pa_GetErrorText(num_devices));
        Pa_Terminate();
        return -1;
    }

    if (num_devices == 0) {
        printf("No audio devices found.\n");
        Pa_Terminate();
        return 0;
    }

//------------------------------------------------------------------------------
//	Handle --info mode for specific device
//------------------------------------------------------------------------------
    if (info_device_index >= 0) {
        if (info_device_index >= num_devices) {
            fprintf(stderr, "Error: Device index %d out of range (0-%d)\n",
                    info_device_index, num_devices - 1);
            Pa_Terminate();
            return -1;
        }

        const PaDeviceInfo *device_info = Pa_GetDeviceInfo(info_device_index);
        if (!device_info) {
            fprintf(stderr, "Error: Could not get info for device %d\n",
                    info_device_index);
            Pa_Terminate();
            return -1;
        }

        const PaHostApiInfo *host_info = Pa_GetHostApiInfo(device_info->hostApi);
        print_device_info_detailed(info_device_index, device_info, host_info);

        Pa_Terminate();
        return 0;
    }

//------------------------------------------------------------------------------
//	Print filter information
//------------------------------------------------------------------------------
    switch (filter) {
    case FILTER_INPUT:
        printf("Audio Input Devices:\n\n");
        break;
    case FILTER_OUTPUT:
        printf("Audio Output Devices:\n\n");
        break;
    case FILTER_ALL:
    default:
        printf("Audio Devices:\n\n");
        break;
    }

//------------------------------------------------------------------------------
//	Print table header
//------------------------------------------------------------------------------
    print_device_table_header();

//------------------------------------------------------------------------------
//	Enumerate and display devices
//------------------------------------------------------------------------------
    for (int i = 0; i < num_devices; i++) {
        const PaDeviceInfo *device_info = Pa_GetDeviceInfo(i);
        if (!device_info) {
            continue;
        }

//------------------------------------------------------------------------------
//	Apply filter
//------------------------------------------------------------------------------
        if (!should_display_device(device_info, filter)) {
            continue;
        }

//------------------------------------------------------------------------------
//	Get host API info
//------------------------------------------------------------------------------
        const PaHostApiInfo *host_info = Pa_GetHostApiInfo(device_info->hostApi);
        if (!host_info) {
            continue;
        }

//------------------------------------------------------------------------------
//	Print device row
//------------------------------------------------------------------------------
        print_device_row(i, device_info, host_info);
        displayed_count++;
    }

//------------------------------------------------------------------------------
//	Print summary
//------------------------------------------------------------------------------
    printf("-------------------------------------------------------------------------------------"
           "-----------------------------\n");
    printf("Total: %d device%s\n", displayed_count,
           displayed_count == 1 ? "" : "s");

    if (displayed_count > 0) {
        printf("\nUse --info <index> to see detailed information for a specific device.\n");
        printf("Use with ab_acq to record from a specific device index.\n");
    }

//------------------------------------------------------------------------------
//	Cleanup
//------------------------------------------------------------------------------
    Pa_Terminate();
    return 0;
}

//------------------------------------------------------------------------------
//	Main application
//
//	This application:
//	- Lists all audio devices (input and output)
//	- Allows filtering by device type
//	- Shows detailed device information
//	- Used to identify devices for recording with ab_acq
//
//	Libraries:
//	- PortAudio: Audio device enumeration
//	- libpopt: Command-line parsing
//------------------------------------------------------------------------------
int main(int argc, const char **argv)
{
//------------------------------------------------------------------------------
//	Command-line options
//------------------------------------------------------------------------------
    int show_input_only = 0;
    int show_output_only = 0;
    int info_device_index = -1;
    int version_flag = 0;

    struct poptOption options[] = {
        {"version",		'v', POPT_ARG_NONE,	&version_flag,		0,	"Show version information",							NULL	},
        {"input",		'i', POPT_ARG_NONE,	&show_input_only,	0,	"Show only input devices",							NULL	},
        {"output",		'o', POPT_ARG_NONE,	&show_output_only,	0,	"Show only output devices",							NULL	},
        {"info",		'I', POPT_ARG_INT,	&info_device_index,	0,	"Show detailed info for specific device index",		"INDEX"	},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(NULL, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "List available audio devices for recording and playback.\n"
        "Use device index with ab_acq for audio recording.\n\n"
        "Examples:\n"
        "  ab_list_dev                # List all devices\n"
        "  ab_list_dev --input        # List input devices only\n"
        "  ab_list_dev --output       # List output devices only\n"
        "  ab_list_dev --info 0       # Show details for device 0\n");

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
        printf("ab_list_dev version 1.0.0\n");
        printf("Audio device listing tool for audio-bench\n");
        printf("Copyright (c) 2025 A.C. Verbeck\n");
        poptFreeContext(popt_ctx);
        return 0;
    }

//------------------------------------------------------------------------------
//	Validate options
//------------------------------------------------------------------------------
    if (show_input_only && show_output_only) {
        fprintf(stderr, "Error: Cannot specify both --input and --output\n");
        poptFreeContext(popt_ctx);
        return 1;
    }

    poptFreeContext(popt_ctx);

//------------------------------------------------------------------------------
//	Determine filter mode
//------------------------------------------------------------------------------
    DeviceFilter filter = FILTER_ALL;
    if (show_input_only) {
        filter = FILTER_INPUT;
    } else if (show_output_only) {
        filter = FILTER_OUTPUT;
    }

//------------------------------------------------------------------------------
//	List devices
//------------------------------------------------------------------------------
    return list_audio_devices(filter, info_device_index);
}
