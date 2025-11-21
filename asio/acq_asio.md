# ab_acq_asio - ASIO Audio Acquisition Tool

Part of the audio_bench suite. This tool allows you to enumerate ASIO audio devices and acquire audio samples from ASIO inputs.

## Features

- List all available ASIO drivers on the system
- Enumerate input/output channels for each driver
- Acquire audio samples from any ASIO input channel
- Output as 32-bit float raw PCM (easily convertible to WAV)
- Configurable sample rate, channel, and acquisition length
- Real-time acquisition with minimal overhead

## Prerequisites

### ASIO SDK
1. Download the ASIO SDK from Steinberg:
   https://www.steinberg.net/asiosdk

2. Extract the SDK to a folder named `asiosdk` in the same directory as this source code, or specify a custom path when building.

### Build Tools
- CMake 3.10 or later
- Visual Studio 2017 or later (with C++ build tools)
- Windows 7 or later

## Building

### Using CMake (Recommended)

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

Or if your ASIO SDK is in a different location:

```cmd
cmake .. -G "Visual Studio 16 2019" -DASIO_SDK_PATH="C:\path\to\asiosdk"
cmake --build . --config Release
```

### Using Visual Studio directly

1. Create a new C++ Console Application project
2. Add `ab_acq_asio.cpp` to the project
3. Add the following ASIO SDK files:
   - `asiosdk/common/asio.cpp`
   - `asiosdk/host/asiodrivers.cpp`
   - `asiosdk/host/pc/asiolist.cpp`

4. Add include directories:
   - `asiosdk/common`
   - `asiosdk/host`
   - `asiosdk/host/pc`

5. Link against: `ole32.lib oleaut32.lib uuid.lib`

6. Build in Release mode

## Usage

### List Available ASIO Drivers

```cmd
ab_acq_asio -list
```

This will show all installed ASIO drivers on your system.

### List Channels for a Driver

```cmd
ab_acq_asio -driver "ASIO4ALL v2" -channels
```

Replace "ASIO4ALL v2" with your driver name (from the -list command).
This shows all input and output channels with their types.

### Acquire Audio Samples

Basic acquisition (1 second at default rate):

```cmd
ab_acq_asio -driver "ASIO4ALL v2" -acquire -channel 0 -samples 48000 -output test.raw
```

Full options:

```cmd
ab_acq_asio -driver "ASIO4ALL v2" -acquire -channel 0 -samples 96000 -output mic_test.raw -rate 48000
```

Options:
- `-driver <name>`: ASIO driver name (use quotes if it contains spaces)
- `-acquire`: Enable acquisition mode
- `-channel <n>`: Input channel number (default: 0, use -channels to see available)
- `-samples <n>`: Number of samples to acquire (default: 48000)
- `-output <file>`: Output filename (default: output.raw)
- `-rate <n>`: Sample rate in Hz (optional, uses driver's current rate if not specified)

## Output Format

The acquired audio is saved as 32-bit float raw PCM data. This format:
- Sample format: IEEE 754 32-bit float
- Sample range: -1.0 to +1.0
- Channels: 1 (mono)
- Byte order: Little-endian (native)

### Converting to WAV

Use FFmpeg to convert the raw data to WAV:

```cmd
ffmpeg -f f32le -ar 48000 -ac 1 -i test.raw test.wav
```

Replace `48000` with your actual sample rate.

### Reading in Python

```python
import numpy as np

# Read the raw float32 data
samples = np.fromfile('test.raw', dtype=np.float32)

# samples is now a numpy array of floats from -1.0 to 1.0
print(f"Samples: {len(samples)}")
print(f"Duration: {len(samples) / 48000} seconds at 48kHz")
```

### Reading in MATLAB

```matlab
% Read the raw float32 data
fid = fopen('test.raw', 'rb');
samples = fread(fid, inf, 'float32');
fclose(fid);

% samples is now a column vector of floats from -1.0 to 1.0
fprintf('Samples: %d\n', length(samples));
fprintf('Duration: %.2f seconds at 48kHz\n', length(samples) / 48000);
```

## Supported ASIO Sample Types

The tool automatically handles these ASIO sample formats:
- ASIOSTInt16LSB (16-bit integer)
- ASIOSTInt24LSB (24-bit integer)
- ASIOSTInt32LSB (32-bit integer)
- ASIOSTFloat32LSB (32-bit float)
- ASIOSTFloat64LSB (64-bit float)

All formats are converted to 32-bit float for output.

## Example Workflow

1. **List available drivers:**
   ```cmd
   ab_acq_asio -list
   ```

2. **Check which channels your interface has:**
   ```cmd
   ab_acq_asio -driver "RME Fireface" -channels
   ```

3. **Acquire 5 seconds from microphone on channel 0 at 96kHz:**
   ```cmd
   ab_acq_asio -driver "RME Fireface" -acquire -channel 0 -samples 480000 -rate 96000 -output mic.raw
   ```

4. **Convert to WAV:**
   ```cmd
   ffmpeg -f f32le -ar 96000 -ac 1 -i mic.raw mic.wav
   ```

## Troubleshooting

### "Failed to load ASIO driver"
- Make sure the driver name exactly matches the name from `-list`
- Use quotes around driver names with spaces
- Ensure the ASIO driver is properly installed

### "ASIOInit failed"
- The ASIO driver may already be in use by another application
- Try closing your DAW or other audio applications
- Some drivers require the audio interface to be connected

### "Invalid input channel"
- Use `-channels` to see available channels
- Channel numbers start at 0
- Make sure you're specifying an input channel, not an output channel

### No audio captured (all zeros)
- Check that you're recording from the correct channel
- Verify your audio source is connected and active
- Some interfaces require enabling phantom power or setting gain externally
- Check the ASIO control panel for the driver (some drivers have configuration UIs)

### Buffer underruns or glitches
- ASIO is designed for low-latency real-time audio
- Close unnecessary applications during acquisition
- Consider increasing the buffer size (if driver supports it)
- Use an SSD for output file storage

## Technical Details

### Callback Architecture
The tool uses ASIO's callback-based architecture:
1. `bufferSwitch()` is called by the driver when audio data is ready
2. Audio is processed in real-time as it arrives
3. Samples are converted to float32 and written to disk
4. Minimal latency and overhead

### Threading
ASIO callbacks run on high-priority audio threads. The tool:
- Performs minimal processing in the callback
- Uses simple file I/O (buffered by OS)
- Avoids memory allocation in callbacks
- Signals completion via atomic flag

### Sample Rate
- Most ASIO drivers have a "current" sample rate setting
- Use `-rate` to request a specific rate
- Not all rates are supported by all hardware
- The tool will warn if the requested rate isn't available

## Integration with audio_bench

This tool is part of the audio_bench suite. It complements:
- `ab_list_dev` - Lists Windows audio devices (WASAPI/MMDevice)
- Other audio_bench tools for measurement and analysis

Unlike ab_list_dev which uses Windows' standard audio APIs, ab_acq_asio uses the ASIO protocol for professional audio interfaces with low latency and high channel counts.

## License

This tool uses the ASIO SDK which has its own license terms. Check the ASIO SDK license for details on redistribution and usage rights.

## References

- ASIO SDK: https://www.steinberg.net/asiosdk
- ASIO specification and documentation included in SDK
- Common ASIO drivers:
  - ASIO4ALL (universal ASIO driver for consumer hardware)
  - RME, Focusrite, Universal Audio, MOTU (professional interfaces)

## Author

Part of the audio_bench suite for Audix.
