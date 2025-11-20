# audio-bench

A comprehensive audio performance measurement toolkit consisting of C programs, Python scripts, and gnuplot visualization tools.

## Overview

audio-bench provides a suite of tools for analyzing audio performance metrics including frequency response, harmonic distortion, signal-to-noise ratio, and other key audio characteristics. It can be used to generate detailed performance reports for audio devices or to analyze individual WAV files.

## Project Structure

```
audio-bench/
├── src/           # C source files for audio analysis
├── scripts/       # Python scripts for automation and processing
├── gnuplot/       # Gnuplot templates and scripts for visualization
├── data/          # Sample data and test files
├── docs/          # Documentation
├── bin/           # Compiled binaries
├── tests/         # Test files and test data
└── examples/      # Example usage and sample outputs
```

## Features

- WAV file analysis and processing
- Audio device enumeration and recording
- FFT-based frequency domain analysis with interval snapshots
- Frequency response analysis
- Basic audio statistics (peak, RMS levels)
- Automated report generation with graphs
- Flexible usage for both device testing and file analysis

### Available Tools

All tools use the `ab_` prefix:
- `ab_audio_analyze` - Basic peak/RMS analysis
- `ab_acq` - Audio acquisition/recording from sound card
- `ab_freq_response` - Frequency response analysis
- `ab_wav_fft` - FFT analysis with optional interval snapshots and averaging
- `ab_gain_calc` - Gain calculator for comparing two 1kHz wave files
- `ab_thd_calc` - Total Harmonic Distortion (THD) calculator for sine waves
- `ab_list_wav` - List WAV files in directory with properties
- `ab_list_dev` - List audio input/output devices

## Dependencies

### C Programs
- libsndfile (for WAV file handling)
- FFTW3 (for FFT operations)
- PortAudio (for audio device I/O)
- libpopt (for command-line option parsing)
- Standard C library (math.h, etc.)

### Python Scripts
- Python 3
- NumPy
- SciPy
- matplotlib (optional, for additional plotting)

### Visualization
- gnuplot 5.0 or higher

## Building

```bash
make
```

This will compile all C programs and place binaries in the `bin/` directory.

## Usage

### Analyzing a WAV file
```bash
# Basic audio analysis (peak, RMS)
./bin/ab_audio_analyze input.wav

# FFT analysis
./bin/ab_wav_fft -i input.wav -o output.csv

# FFT with interval snapshots (every 100ms)
./bin/ab_wav_fft -i input.wav -o output -t 100

# FFT with averaging (4 overlapping windows)
./bin/ab_wav_fft -i input.wav -o output.csv -a 4

# Frequency response analysis
./bin/ab_freq_response input.wav

# Gain calculation (compare two 1kHz signals)
./bin/ab_gain_calc reference.wav measured.wav

# THD calculation (1kHz sine wave, default)
./bin/ab_thd_calc -f test_1khz.wav

# THD for different frequencies
./bin/ab_thd_calc -f test_10khz.wav -F 10000

# THD with custom FFT size and harmonic count
./bin/ab_thd_calc -f test_1khz.wav -s 16384 -n 15
```

### Audio device operations
```bash
# List all audio devices
./bin/ab_list_dev

# List only input devices
./bin/ab_list_dev --input

# Record audio (5 seconds from device 0)
./bin/ab_acq -d 0 -o recording.wav -t 5

# Record with specific settings (48kHz, stereo, 24-bit)
./bin/ab_acq -d 0 -o recording.wav -r 48000 -c 2 -b 24
```

### Generating a full report
```bash
python scripts/generate_report.py --input device_test.wav --output report/
```

### Creating individual graphs
```bash
gnuplot gnuplot/frequency_response.gp
```

## Installation

### Quick Install

```bash
make
make install
```

This will:
- Compile all C programs
- Install binaries to `/opt/audio-bench/bin`
- Install Python scripts to `/opt/audio-bench/scripts`
- Install gnuplot scripts to `/opt/audio-bench/gnuplot`

### Installation Paths

After installation, the following directory structure is created:

```
/opt/audio-bench/
├── bin/              # Compiled C programs (ab_*)
├── scripts/          # Python scripts
└── gnuplot/          # Gnuplot visualization templates
```

### Environment Setup

Add the following to your shell configuration file (`.bashrc`, `.zshrc`, etc.):

```bash
export AUDIO_BENCH=/opt/audio-bench
export PATH=$AUDIO_BENCH/bin:$PATH
```

This allows you to run audio-bench tools from any directory without specifying the full path.

### Platform-Specific Notes

- **Linux/macOS**: Default installation path is `/opt/audio-bench`
- **Windows/MSYS2**: Installation path is `/c/msys64/opt/audio-bench`

For detailed dependency installation instructions, see [docs/INSTALL.md](docs/INSTALL.md).

### Uninstall

```bash
make uninstall
```

## Contributing

Contributions are welcome! Please see [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines.

## License

MIT License - see source files for full license text

Copyright (c) 2025 Anthony Verbeck

## Authors

Anthony Verbeck
