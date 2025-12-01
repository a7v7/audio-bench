# Audio Visualizer - Real-Time Waveform Display

## Overview

`ab_audio_visualizer` is a Windows GUI application for real-time audio visualization. It captures audio from a selected input device and displays the waveform as a scrolling graph (oscilloscope-style).

## Features

### Stage 1 (Current Implementation)
- **Real-time audio capture** from any input device
- **Waveform visualization** - amplitude (Y-axis) vs time (X-axis), scrolling left-to-right
- **Device selection** - dropdown menus for input and output devices
- **Channel modes**:
  - **Left** - Display left channel only
  - **Right** - Display right channel only
  - **Stereo** - Display both channels (currently shows left)
  - **Combined** - Display averaged/mixed signal
- **Adjustable time window** - Configure how many seconds of history to display (default: 0.5 seconds)
- **Start/Stop control** - Button to start and stop audio capture
- **Visual grid** - Background grid for amplitude and time reference

### Planned Features (Stage 2)
- FFT display for frequency domain analysis
- Dual-channel stereo visualization
- Frequency spectrum analyzer
- Configurable color schemes

## Building

### Requirements
- **Platform**: Windows (MSYS2/MinGW64 environment)
- **Dependencies**:
  - GCC compiler with MinGW
  - PortAudio library
  - Windows SDK (for Win32 API and GDI)

### Compilation

On Windows/MSYS2:

```bash
# Build only the audio visualizer
make ab_audio_visualizer

# Or build all tools (includes visualizer on Windows)
make all
```

The application will be compiled to `bin/ab_audio_visualizer.exe`.

### Platform Notes

The audio visualizer is **Windows-only** and uses:
- **Win32 API** for GUI window and controls
- **GDI (Graphics Device Interface)** for waveform rendering
- **PortAudio** for cross-platform audio I/O

On Linux/macOS, the visualizer will be skipped during `make all` since it requires Windows-specific libraries.

## Usage

### Starting the Application

```bash
# From the bin directory
./ab_audio_visualizer.exe

# Or if installed
ab_audio_visualizer
```

### User Interface

The application window contains:

1. **Input Device** dropdown - Select audio input device
2. **Output Device** dropdown - Select audio output device (reserved for future use)
3. **Channel** dropdown - Select channel display mode:
   - Left
   - Right
   - Stereo
   - Combined
4. **Time Window** text box - Enter time window in seconds (0.1 to 10.0)
5. **Start/Stop** button - Begin or end audio capture
6. **Waveform Display** - Real-time scrolling graph

### Workflow

1. Launch `ab_audio_visualizer.exe`
2. Select your input device from the dropdown (e.g., microphone, line-in)
3. Choose the channel mode you want to visualize
4. Adjust the time window if desired (default 0.5 seconds works well)
5. Click **Start** to begin capturing and visualizing audio
6. The waveform will scroll left-to-right showing amplitude over time
7. Click **Stop** when finished

### Time Window Guidelines

- **0.1 - 0.5 sec**: Good for viewing individual waveforms and transients
- **0.5 - 2.0 sec**: General purpose, balance between detail and history
- **2.0 - 10.0 sec**: Longer-term patterns, slower-moving signals

## Technical Details

### Architecture

**Circular Buffer**
- Thread-safe ring buffer stores recent audio samples
- Size dynamically adjusts based on time window setting
- Supports both mono and stereo audio

**Audio Capture**
- Uses PortAudio callback mechanism for low-latency capture
- Configurable sample rate (default: 44100 Hz)
- Supports up to 2 channels (stereo)

**Visualization**
- Double-buffered GDI rendering for smooth animation
- Updates at ~30 FPS via WM_TIMER events
- Grid overlay for amplitude reference
- Left-to-right scrolling display

### Code Structure

```
ab_audio_visualizer.c
├── Circular buffer management
│   ├── CircularBuffer_Init()
│   ├── CircularBuffer_Write()
│   ├── CircularBuffer_Read()
│   └── CircularBuffer_Destroy()
├── Audio capture (PortAudio)
│   ├── AudioCallback()
│   ├── StartAudioCapture()
│   └── StopAudioCapture()
├── Device enumeration
│   └── PopulateDeviceList()
├── Visualization (GDI)
│   ├── DrawWaveform()
│   └── UpdateTimeWindow()
└── GUI (Win32 API)
    └── WindowProc()
```

### Sample Rates

The application currently uses a fixed sample rate of 44100 Hz. Most audio devices support this rate. Future versions may allow sample rate selection.

## Troubleshooting

**"Failed to initialize PortAudio"**
- Ensure PortAudio is properly installed
- Check that audio drivers are working

**"Selected device has no input channels"**
- The chosen device doesn't support audio input
- Select a different device with input capability

**"Failed to open audio stream"**
- Device may be in use by another application
- Try a different sample rate or channel count
- Check device permissions

**No waveform visible**
- Ensure audio source is active (e.g., speak into microphone)
- Check that correct input device is selected
- Verify channel selection matches your device (mono vs stereo)

## Future Enhancements

### Stage 2: FFT Display
- Real-time frequency spectrum analyzer
- Configurable FFT size (512, 1024, 2048, 4096 samples)
- Waterfall display option
- Peak frequency indicators

### Additional Ideas
- Waveform export to PNG
- Audio recording to WAV file
- Trigger/hold functionality
- Multiple visualization modes (XY, Lissajous)
- VU meter display
- Configurable color themes

## See Also

- `ab_acq` - Command-line audio recording tool
- `ab_list_dev` - List available audio devices
- `ab_wav_fft` - FFT analysis of WAV files
