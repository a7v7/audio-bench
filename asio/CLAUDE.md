# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **ASIO extension subdirectory** of the audio-bench project. It provides Windows-only ASIO (Audio Stream Input/Output) protocol support for professional audio interfaces, complementing the main project's PortAudio-based acquisition tool.

**Key Distinction:**
- **Main project (`ab_acq`)**: Uses PortAudio/WASAPI for consumer audio devices
- **This ASIO extension**: Direct ASIO protocol for professional audio interfaces with low-latency requirements
  - `ab_acq_asio`: Audio acquisition from ASIO devices
  - `ab_list_dev_asio`: Device enumeration and status checking
- **Platform**: Windows-only (ASIO SDK is Windows-specific)

## Build System

This subdirectory has its **own independent Makefile**, separate from the parent audio-bench project.

### Building

```bash
# From asio/ directory
make              # Build all ASIO tools (ab_acq_asio.exe, ab_list_dev_asio.exe)
make clean        # Remove build artifacts
make help         # Show available targets
```

**Output Location:**
- Binaries: `../bin/ab_acq_asio.exe` and `../bin/ab_list_dev_asio.exe` (parent project's bin directory)
- Object files: `obj/` (local to asio/ subdirectory)

### Build Requirements

**Platform & Tools:**
- Windows/MSYS2 environment (required)
- g++ compiler with C++11 support
- make utility

**Dependencies:**
- ASIO SDK must be present in `ASIOSDK/` directory
- Windows COM libraries: `-lole32 -loleaut32`
- Math library: `-lm`

**Compiler Flags:**
- `-Wall -O2 -std=c++11`
- Include paths: `-I` for ASIOSDK/common, ASIOSDK/host, ASIOSDK/host/pc

### What Gets Compiled

The build compiles the following sources:
1. `ab_acq_asio.cpp` (audio acquisition application)
2. `ab_list_dev_asio.cpp` (device listing application)
2. `ASIOSDK/common/asio.cpp` (ASIO host interface)
3. `ASIOSDK/host/asiodrivers.cpp` (driver management)
4. `ASIOSDK/host/pc/asiolist.cpp` (Windows COM registry access)

## Code Architecture

### Callback-Based Real-Time Processing

`ab_acq_asio.cpp` uses ASIO's asynchronous callback model:

**Global State** (lines 17-33):
- ASIO driver instance and configuration
- Buffer management structures
- Acquisition parameters (channel, samples, output file)
- Thread-safe acquisition flag

**ASIO Callbacks** (lines 35-177) - Execute on audio driver's real-time thread:
- `bufferSwitchTimeInfo()`: Main audio callback
  - Converts incoming samples to 32-bit float
  - Writes to output file
  - Tracks progress and signals completion
- `sampleRateChanged()`: Handle sample rate changes
- `asioMessages()`: Process ASIO system messages

**Sample Format Conversion** (lines 75-122):
Automatically handles 5 ASIO sample types:
- `ASIOSTInt16LSB`, `ASIOSTInt24LSB`, `ASIOSTInt32LSB` → normalized float (-1.0 to +1.0)
- `ASIOSTFloat32LSB` → direct copy
- `ASIOSTFloat64LSB` → downconvert to float32

**Driver Management** (lines 183-288):
- `initASIO()`: Load and initialize ASIO driver, retrieve capabilities
- `setupASIOBuffers()`: Configure buffers and register callbacks
- `shutdownASIO()`: Clean shutdown with proper resource cleanup

**Threading Model:**
- **Main thread**: CLI parsing, initialization, progress monitoring (polling with `Sleep(100)`)
- **Audio thread**: Callback runs on driver's real-time thread for low-latency
- **Synchronization**: `acquisitionActive` flag signals when acquisition completes

### Three Operational Modes

```bash
# 1. List all installed ASIO drivers
ab_acq_asio.exe -list

# 2. Show channels for a specific driver
ab_acq_asio.exe -driver "ASIO4ALL v2" -channels

# 3. Acquire audio samples
ab_acq_asio.exe -driver "ASIO4ALL v2" -acquire \
    -channel 0 -samples 96000 -output recording.raw -rate 48000
```

### Output Format

All recordings are saved as:
- **Format**: 32-bit IEEE 754 float raw PCM
- **Range**: -1.0 to +1.0 (normalized)
- **Channels**: Mono (single channel per recording)
- **Byte order**: Little-endian

**Conversion to WAV:**
```bash
ffmpeg -f f32le -ar 48000 -ac 1 -i recording.raw output.wav
```

## ASIO SDK Structure

The ASIO 2.3.4 SDK is embedded in `ASIOSDK/`:

**Key Directories:**
- `common/`: Core ASIO API definitions
  - `asio.h` - Main ASIO interface (v2.3)
  - `asio.cpp` - Host interface implementation
  - `iasiodrv.h` - ASIO driver interface definition
- `host/`: Host application utilities
  - `asiodrivers.h/.cpp` - Driver enumeration and management
  - `pc/asiolist.h/.cpp` - Windows COM registry driver loading
- `driver/`: Sample driver implementations (reference only, not compiled)

**Documentation PDFs:**
- `Steinberg ASIO SDK 2.3.pdf` - Complete specification
- `Steinberg ASIO Usage Guidelines.pdf` - Branding and usage rules
- `Steinberg ASIO Licensing Agreement.pdf` - License terms

**License**: Dual license (Proprietary or GPLv3) - all code subject to ASIO SDK terms

## Integration with Parent Project

**Relationship:**
- Independent subdirectory with separate build system
- Shares output directory: binaries go to `../bin/`
- Complements main `ab_acq` tool for professional audio hardware
- Not built automatically by parent Makefile (manual build required)
- Not yet tracked in git (currently untracked)

**Use Cases:**
- Professional audio interfaces (RME, Focusrite, MOTU, etc.)
- Multi-channel recording (up to 32 channels supported in code)
- Low-latency audio acquisition
- Direct driver access bypassing Windows audio stack

## Common Development Tasks

### Device Enumeration

```bash
# Quick device status check - lists all ASIO drivers and their attachment status
./ab_list_dev_asio.exe

# Output shows:
# - Device index and name
# - Status (ATTACHED or NOT ATTACHED)
# - For attached devices: input/output channel counts, ASIO version, driver version
```

### Testing with Real Hardware

```bash
# 1. Check which devices are attached
./ab_list_dev_asio.exe

# 2. List available ASIO drivers (alternative method)
./ab_acq_asio.exe -list

# 3. Check driver capabilities
./ab_acq_asio.exe -driver "Your Driver Name" -channels

# 4. Record test sample
./ab_acq_asio.exe -driver "Your Driver Name" -acquire \
    -channel 0 -samples 48000 -output test.raw -rate 48000

# 5. Convert to WAV for analysis
ffmpeg -f f32le -ar 48000 -ac 1 -i test.raw test.wav

# 6. Use parent project tools to analyze
../bin/ab_audio_analyze test.wav
```

### Debugging ASIO Issues

**Common problems documented in `acq_asio.md`:**
- Driver not found: Check ASIO driver installation and registry entries
- Init fails: Verify driver isn't already in use by another application
- Sample rate not supported: Query driver capabilities first
- Buffer underruns: Check system load and buffer size settings

**Windows COM Initialization:**
The code properly initializes/uninitializes COM (`CoInitialize`/`CoUninitialize`) for Windows ASIO driver registry access.

## Important Notes

- **Windows-only**: ASIO is Windows-specific; code will not compile on Linux/macOS
- **Real-time constraints**: Audio callback must complete quickly (avoid file I/O delays, allocations)
- **Driver conflicts**: Only one application can use an ASIO driver at a time
- **Sample type handling**: Code converts all formats to normalized float32 for consistency
- **Multi-channel limitation**: Current implementation records single channel at a time (up to 32 channels supported in `bufferInfos` array)
- **Progress reporting**: Uses polling with `Sleep(100)` on main thread while audio thread processes callbacks
- **File format**: Raw PCM output requires post-processing (use FFmpeg to create WAV files)

## File Organization

- `ab_acq_asio.cpp` - Audio acquisition application (486 lines)
- `ab_list_dev_asio.cpp` - Device enumeration application (136 lines)
- `acq_asio.md` - Comprehensive user documentation with examples and troubleshooting
- `Makefile` - Build system (separate from parent project)
- `ASIOSDK/` - Complete Steinberg ASIO 2.3.4 SDK
- `obj/` - Object files during build (created by make, not in git)
- `../bin/` - Output directory for compiled executables

## Documentation Reference

For detailed usage instructions, troubleshooting, and technical details, see `acq_asio.md` in this directory.
