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
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <portaudio.h>

//------------------------------------------------------------------------------
// Application constants
//------------------------------------------------------------------------------
#define APP_NAME            "Audio Visualizer"
#define APP_VERSION         "1.0.0"
#define WINDOW_WIDTH        1000
#define WINDOW_HEIGHT       700
#define GRAPH_HEIGHT        400
#define GRAPH_MARGIN        20
#define DEFAULT_SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER   512
#define DEFAULT_TIME_WINDOW 0.5     // seconds

//------------------------------------------------------------------------------
// Control IDs
//------------------------------------------------------------------------------
#define ID_START_STOP       1001
#define ID_INPUT_DEVICE     1002
#define ID_OUTPUT_DEVICE    1003
#define ID_CHANNEL_SELECT   1004
#define ID_TIME_WINDOW      1005
#define ID_TIMER            1006

//------------------------------------------------------------------------------
// Channel display modes
//------------------------------------------------------------------------------
typedef enum {
    CHANNEL_LEFT,
    CHANNEL_RIGHT,
    CHANNEL_STEREO,
    CHANNEL_COMBINED
} ChannelMode;

//------------------------------------------------------------------------------
// Circular buffer for audio samples
//------------------------------------------------------------------------------
typedef struct {
    float *data;
    size_t size;
    size_t write_pos;
    size_t read_pos;
    CRITICAL_SECTION lock;
} CircularBuffer;

//------------------------------------------------------------------------------
// Audio capture state
//------------------------------------------------------------------------------
typedef struct {
    PaStream *stream;
    CircularBuffer buffer;
    int is_recording;
    int sample_rate;
    int channels;
    int input_device_index;
} AudioCapture;

//------------------------------------------------------------------------------
// Global application state
//------------------------------------------------------------------------------
static struct {
    HWND main_window;
    HWND start_button;
    HWND input_combo;
    HWND output_combo;
    HWND channel_combo;
    HWND time_window_edit;
    HWND graph_area;
    AudioCapture audio;
    ChannelMode channel_mode;
    float time_window;
    int num_devices;
} g_app = {0};

//------------------------------------------------------------------------------
//	Name:		CircularBuffer_Init
//
//	Returns:	1 on success, 0 on failure
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Initializes circular buffer with specified size
//	- Allocates memory for buffer data
//	- Initializes critical section for thread safety
//------------------------------------------------------------------------------
static int CircularBuffer_Init(CircularBuffer *buf, size_t size)
{
    buf->data = (float*)calloc(size, sizeof(float));
    if (!buf->data) {
        return 0;
    }
    buf->size = size;
    buf->write_pos = 0;
    buf->read_pos = 0;
    InitializeCriticalSection(&buf->lock);
    return 1;
}

//------------------------------------------------------------------------------
//	Name:		CircularBuffer_Destroy
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Frees circular buffer resources
//	- Deletes critical section
//------------------------------------------------------------------------------
static void CircularBuffer_Destroy(CircularBuffer *buf)
{
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    DeleteCriticalSection(&buf->lock);
}

//------------------------------------------------------------------------------
//	Name:		CircularBuffer_Write
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Writes samples to circular buffer
//	- Thread-safe with critical section
//	- Overwrites old data when buffer is full
//------------------------------------------------------------------------------
static void CircularBuffer_Write(CircularBuffer *buf, const float *samples, size_t count)
{
    EnterCriticalSection(&buf->lock);

    for (size_t i = 0; i < count; i++) {
        buf->data[buf->write_pos] = samples[i];
        buf->write_pos = (buf->write_pos + 1) % buf->size;
    }

    LeaveCriticalSection(&buf->lock);
}

//------------------------------------------------------------------------------
//	Name:		CircularBuffer_Read
//
//	Returns:	Number of samples actually read
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Reads samples from circular buffer
//	- Thread-safe with critical section
//	- Returns most recent samples
//------------------------------------------------------------------------------
static size_t CircularBuffer_Read(CircularBuffer *buf, float *output, size_t count)
{
    EnterCriticalSection(&buf->lock);

    size_t available = buf->size;
    size_t to_read = (count < available) ? count : available;

    // Calculate starting position (most recent samples)
    size_t start_pos = (buf->write_pos >= to_read) ?
                       (buf->write_pos - to_read) :
                       (buf->size - (to_read - buf->write_pos));

    // Read samples
    for (size_t i = 0; i < to_read; i++) {
        output[i] = buf->data[(start_pos + i) % buf->size];
    }

    LeaveCriticalSection(&buf->lock);
    return to_read;
}

//------------------------------------------------------------------------------
//	Name:		AudioCallback
//
//	Returns:	paContinue
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- PortAudio callback for capturing audio samples
//	- Writes incoming samples to circular buffer
//	- Processes based on channel mode
//------------------------------------------------------------------------------
static int AudioCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData)
{
    AudioCapture *capture = (AudioCapture*)userData;
    const float *input = (const float*)inputBuffer;

    (void)outputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    if (!input || !capture->is_recording) {
        return paContinue;
    }

    // Write samples to circular buffer (interleaved for stereo)
    CircularBuffer_Write(&capture->buffer, input, framesPerBuffer * capture->channels);

    return paContinue;
}

//------------------------------------------------------------------------------
//	Name:		StartAudioCapture
//
//	Returns:	1 on success, 0 on failure
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Starts PortAudio stream for audio capture
//	- Configures input parameters
//	- Opens and starts audio stream
//------------------------------------------------------------------------------
static int StartAudioCapture(int device_index)
{
    PaError err;
    PaStreamParameters input_params;
    const PaDeviceInfo *device_info;

    // Get device info
    device_info = Pa_GetDeviceInfo(device_index);
    if (!device_info || device_info->maxInputChannels == 0) {
        MessageBox(g_app.main_window, "Selected device has no input channels",
                   "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Configure input parameters
    g_app.audio.channels = (device_info->maxInputChannels >= 2) ? 2 : 1;
    g_app.audio.sample_rate = DEFAULT_SAMPLE_RATE;

    memset(&input_params, 0, sizeof(input_params));
    input_params.device = device_index;
    input_params.channelCount = g_app.audio.channels;
    input_params.sampleFormat = paFloat32;
    input_params.suggestedLatency = device_info->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = NULL;

    // Open audio stream
    err = Pa_OpenStream(&g_app.audio.stream,
                       &input_params,
                       NULL,  // No output
                       g_app.audio.sample_rate,
                       FRAMES_PER_BUFFER,
                       paClipOff,
                       AudioCallback,
                       &g_app.audio);

    if (err != paNoError) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to open audio stream: %s",
                Pa_GetErrorText(err));
        MessageBox(g_app.main_window, msg, "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Start stream
    err = Pa_StartStream(g_app.audio.stream);
    if (err != paNoError) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to start audio stream: %s",
                Pa_GetErrorText(err));
        MessageBox(g_app.main_window, msg, "Error", MB_OK | MB_ICONERROR);
        Pa_CloseStream(g_app.audio.stream);
        g_app.audio.stream = NULL;
        return 0;
    }

    g_app.audio.is_recording = 1;
    g_app.audio.input_device_index = device_index;

    return 1;
}

//------------------------------------------------------------------------------
//	Name:		StopAudioCapture
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Stops PortAudio stream
//	- Closes and cleans up audio resources
//------------------------------------------------------------------------------
static void StopAudioCapture(void)
{
    g_app.audio.is_recording = 0;

    if (g_app.audio.stream) {
        Pa_StopStream(g_app.audio.stream);
        Pa_CloseStream(g_app.audio.stream);
        g_app.audio.stream = NULL;
    }
}

//------------------------------------------------------------------------------
//	Name:		PopulateDeviceList
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Populates input and output device combo boxes
//	- Uses PortAudio device enumeration
//	- Filters by input/output capability
//------------------------------------------------------------------------------
static void PopulateDeviceList(void)
{
    int num_devices = Pa_GetDeviceCount();
    g_app.num_devices = num_devices;

    SendMessage(g_app.input_combo, CB_RESETCONTENT, 0, 0);
    SendMessage(g_app.output_combo, CB_RESETCONTENT, 0, 0);

    for (int i = 0; i < num_devices; i++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info) continue;

        char device_str[256];
        snprintf(device_str, sizeof(device_str), "[%d] %s", i, info->name);

        // Add to input combo if device has input channels
        if (info->maxInputChannels > 0) {
            int index = SendMessage(g_app.input_combo, CB_ADDSTRING, 0, (LPARAM)device_str);
            SendMessage(g_app.input_combo, CB_SETITEMDATA, index, (LPARAM)i);
        }

        // Add to output combo if device has output channels
        if (info->maxOutputChannels > 0) {
            int index = SendMessage(g_app.output_combo, CB_ADDSTRING, 0, (LPARAM)device_str);
            SendMessage(g_app.output_combo, CB_SETITEMDATA, index, (LPARAM)i);
        }
    }

    // Select first device by default
    SendMessage(g_app.input_combo, CB_SETCURSEL, 0, 0);
    SendMessage(g_app.output_combo, CB_SETCURSEL, 0, 0);
}

//------------------------------------------------------------------------------
//	Name:		DrawWaveform
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Renders waveform to device context
//	- Draws grid, axes, and audio waveform
//	- Handles different channel modes
//------------------------------------------------------------------------------
static void DrawWaveform(HDC hdc, RECT *rect)
{
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;

    // Create double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBitmap = SelectObject(memDC, memBitmap);

    // Clear background
    HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 30));
    FillRect(memDC, rect, bgBrush);
    DeleteObject(bgBrush);

    // Draw grid
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 60));
    HPEN oldPen = SelectObject(memDC, gridPen);

    // Horizontal grid lines
    for (int i = 0; i <= 4; i++) {
        int y = height * i / 4;
        MoveToEx(memDC, GRAPH_MARGIN, y, NULL);
        LineTo(memDC, width - GRAPH_MARGIN, y);
    }

    // Vertical grid lines
    for (int i = 0; i <= 10; i++) {
        int x = GRAPH_MARGIN + (width - 2 * GRAPH_MARGIN) * i / 10;
        MoveToEx(memDC, x, 0, NULL);
        LineTo(memDC, x, height);
    }

    SelectObject(memDC, oldPen);
    DeleteObject(gridPen);

    // Draw center line
    HPEN centerPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 120));
    SelectObject(memDC, centerPen);
    MoveToEx(memDC, GRAPH_MARGIN, height / 2, NULL);
    LineTo(memDC, width - GRAPH_MARGIN, height / 2);
    DeleteObject(centerPen);

    // Get samples from circular buffer
    size_t samples_to_display = (size_t)(g_app.audio.sample_rate * g_app.time_window);
    float *samples = (float*)malloc(samples_to_display * g_app.audio.channels * sizeof(float));

    if (samples && g_app.audio.is_recording) {
        size_t samples_read = CircularBuffer_Read(&g_app.audio.buffer, samples,
                                                  samples_to_display * g_app.audio.channels);

        if (samples_read > 0) {
            // Draw waveform based on channel mode
            HPEN wavePen = CreatePen(PS_SOLID, 2, RGB(0, 255, 100));
            SelectObject(memDC, wavePen);

            int graph_width = width - 2 * GRAPH_MARGIN;
            int center_y = height / 2;
            int max_amplitude = height / 2 - 10;

            size_t num_frames = samples_read / g_app.audio.channels;

            for (size_t i = 0; i < num_frames; i++) {
                float value = 0.0f;

                // Extract sample based on channel mode
                if (g_app.audio.channels == 2) {
                    float left = samples[i * 2];
                    float right = samples[i * 2 + 1];

                    switch (g_app.channel_mode) {
                    case CHANNEL_LEFT:
                        value = left;
                        break;
                    case CHANNEL_RIGHT:
                        value = right;
                        break;
                    case CHANNEL_COMBINED:
                        value = (left + right) / 2.0f;
                        break;
                    case CHANNEL_STEREO:
                        // For stereo mode, we'll draw left channel
                        // (future enhancement: draw both channels)
                        value = left;
                        break;
                    }
                } else {
                    value = samples[i];
                }

                // Clamp value
                if (value > 1.0f) value = 1.0f;
                if (value < -1.0f) value = -1.0f;

                int x = GRAPH_MARGIN + (int)((float)i * graph_width / num_frames);
                int y = center_y - (int)(value * max_amplitude);

                if (i == 0) {
                    MoveToEx(memDC, x, y, NULL);
                } else {
                    LineTo(memDC, x, y);
                }
            }

            DeleteObject(wavePen);
        }
    }

    if (samples) {
        free(samples);
    }

    // Draw status text
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(200, 200, 200));

    char status[256];
    if (g_app.audio.is_recording) {
        snprintf(status, sizeof(status), "Recording: %d Hz, %d ch, %.2f sec window",
                g_app.audio.sample_rate, g_app.audio.channels, g_app.time_window);
    } else {
        snprintf(status, sizeof(status), "Stopped - Press Start to begin recording");
    }

    RECT textRect = {GRAPH_MARGIN, 10, width - GRAPH_MARGIN, 30};
    DrawText(memDC, status, -1, &textRect, DT_LEFT | DT_VCENTER);

    // Copy to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

//------------------------------------------------------------------------------
//	Name:		UpdateTimeWindow
//
//	Returns:	none
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Updates time window from edit control
//	- Validates and clamps value
//	- Reallocates circular buffer if needed
//------------------------------------------------------------------------------
static void UpdateTimeWindow(void)
{
    char text[32];
    GetWindowText(g_app.time_window_edit, text, sizeof(text));

    float new_window = (float)atof(text);
    if (new_window < 0.1f) new_window = 0.1f;
    if (new_window > 10.0f) new_window = 10.0f;

    if (fabs(new_window - g_app.time_window) > 0.01f) {
        g_app.time_window = new_window;

        // Update buffer size
        size_t new_size = (size_t)(g_app.audio.sample_rate * new_window * 2); // stereo
        if (g_app.audio.buffer.data) {
            CircularBuffer_Destroy(&g_app.audio.buffer);
        }
        CircularBuffer_Init(&g_app.audio.buffer, new_size);
    }
}

//------------------------------------------------------------------------------
//	Name:		WindowProc
//
//	Returns:	LRESULT
//
//------------------------------------------------------------------------------
//	Detailed description:
//	- Main window message handler
//	- Processes button clicks, combo box selections, paint events
//------------------------------------------------------------------------------
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        {
            // Create controls
            int y_pos = 10;

            // Input device label and combo
            CreateWindow("STATIC", "Input Device:", WS_VISIBLE | WS_CHILD,
                        10, y_pos, 100, 20, hwnd, NULL, NULL, NULL);
            g_app.input_combo = CreateWindow("COMBOBOX", "",
                                            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                            120, y_pos, 400, 200, hwnd, (HMENU)ID_INPUT_DEVICE, NULL, NULL);
            y_pos += 30;

            // Output device label and combo
            CreateWindow("STATIC", "Output Device:", WS_VISIBLE | WS_CHILD,
                        10, y_pos, 100, 20, hwnd, NULL, NULL, NULL);
            g_app.output_combo = CreateWindow("COMBOBOX", "",
                                             WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                             120, y_pos, 400, 200, hwnd, (HMENU)ID_OUTPUT_DEVICE, NULL, NULL);
            y_pos += 30;

            // Channel selection
            CreateWindow("STATIC", "Channel:", WS_VISIBLE | WS_CHILD,
                        10, y_pos, 100, 20, hwnd, NULL, NULL, NULL);
            g_app.channel_combo = CreateWindow("COMBOBOX", "",
                                              WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
                                              120, y_pos, 150, 200, hwnd, (HMENU)ID_CHANNEL_SELECT, NULL, NULL);
            SendMessage(g_app.channel_combo, CB_ADDSTRING, 0, (LPARAM)"Left");
            SendMessage(g_app.channel_combo, CB_ADDSTRING, 0, (LPARAM)"Right");
            SendMessage(g_app.channel_combo, CB_ADDSTRING, 0, (LPARAM)"Stereo");
            SendMessage(g_app.channel_combo, CB_ADDSTRING, 0, (LPARAM)"Combined");
            SendMessage(g_app.channel_combo, CB_SETCURSEL, 0, 0);

            // Time window
            CreateWindow("STATIC", "Time Window (sec):", WS_VISIBLE | WS_CHILD,
                        290, y_pos, 120, 20, hwnd, NULL, NULL, NULL);
            g_app.time_window_edit = CreateWindow("EDIT", "0.5",
                                                  WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                                                  420, y_pos, 100, 20, hwnd, (HMENU)ID_TIME_WINDOW, NULL, NULL);
            y_pos += 30;

            // Start/Stop button
            g_app.start_button = CreateWindow("BUTTON", "Start",
                                             WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                             10, y_pos, 100, 30, hwnd, (HMENU)ID_START_STOP, NULL, NULL);
            y_pos += 40;

            // Graph area (custom painting)
            g_app.graph_area = CreateWindow("STATIC", "",
                                           WS_VISIBLE | WS_CHILD | SS_OWNERDRAW,
                                           10, y_pos, WINDOW_WIDTH - 30, GRAPH_HEIGHT,
                                           hwnd, NULL, NULL, NULL);

            // Initialize PortAudio
            PaError err = Pa_Initialize();
            if (err != paNoError) {
                MessageBox(hwnd, "Failed to initialize PortAudio", "Error", MB_OK | MB_ICONERROR);
                return -1;
            }

            // Populate device lists
            PopulateDeviceList();

            // Initialize circular buffer
            g_app.audio.sample_rate = DEFAULT_SAMPLE_RATE;
            size_t buffer_size = (size_t)(DEFAULT_SAMPLE_RATE * DEFAULT_TIME_WINDOW * 2); // stereo
            CircularBuffer_Init(&g_app.audio.buffer, buffer_size);

            // Set timer for updating display
            SetTimer(hwnd, ID_TIMER, 33, NULL); // ~30 FPS
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_START_STOP:
            if (!g_app.audio.is_recording) {
                // Get selected input device
                int sel = SendMessage(g_app.input_combo, CB_GETCURSEL, 0, 0);
                if (sel != CB_ERR) {
                    int device_index = SendMessage(g_app.input_combo, CB_GETITEMDATA, sel, 0);
                    if (StartAudioCapture(device_index)) {
                        SetWindowText(g_app.start_button, "Stop");
                    }
                }
            } else {
                StopAudioCapture();
                SetWindowText(g_app.start_button, "Start");
            }
            break;

        case ID_CHANNEL_SELECT:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int sel = SendMessage(g_app.channel_combo, CB_GETCURSEL, 0, 0);
                g_app.channel_mode = (ChannelMode)sel;
            }
            break;

        case ID_TIME_WINDOW:
            if (HIWORD(wParam) == EN_CHANGE) {
                UpdateTimeWindow();
            }
            break;
        }
        return 0;

    case WM_TIMER:
        if (wParam == ID_TIMER) {
            // Redraw graph area
            InvalidateRect(g_app.graph_area, NULL, FALSE);
        }
        return 0;

    case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS->hwndItem == g_app.graph_area) {
                DrawWaveform(pDIS->hDC, &pDIS->rcItem);
            }
        }
        return TRUE;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Paint graph area
            if (g_app.graph_area) {
                RECT rect;
                GetClientRect(g_app.graph_area, &rect);
                MapWindowPoints(g_app.graph_area, hwnd, (POINT*)&rect, 2);

                HDC graphDC = GetDC(g_app.graph_area);
                DrawWaveform(graphDC, &rect);
                ReleaseDC(g_app.graph_area, graphDC);
            }

            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        StopAudioCapture();
        CircularBuffer_Destroy(&g_app.audio.buffer);
        Pa_Terminate();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

//------------------------------------------------------------------------------
//	Main application
//
//	This application:
//	- Creates Windows GUI for real-time audio visualization
//	- Captures audio from selected input device using PortAudio
//	- Displays waveform with configurable time window
//	- Supports channel selection (Left, Right, Stereo, Combined)
//
//	Libraries:
//	- PortAudio: Audio device access and recording
//	- Win32 API: GUI and graphics
//------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    // Initialize global state
    memset(&g_app, 0, sizeof(g_app));
    g_app.time_window = DEFAULT_TIME_WINDOW;
    g_app.channel_mode = CHANNEL_LEFT;

    // Register window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "AudioVisualizerClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Window registration failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Create main window
    g_app.main_window = CreateWindowEx(
        0,
        "AudioVisualizerClass",
        APP_NAME " - v" APP_VERSION,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!g_app.main_window) {
        MessageBox(NULL, "Window creation failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_app.main_window, nCmdShow);
    UpdateWindow(g_app.main_window);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
