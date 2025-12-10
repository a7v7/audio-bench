# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

audio-bench is a comprehensive audio performance measurement toolkit consisting of C programs, Python scripts, and gnuplot visualization tools. It analyzes audio performance metrics including frequency response, harmonic distortion, signal-to-noise ratio, and other key audio characteristics.

### Two Usage Modes

audio-bench can be used in two distinct ways:

1. **Project-Driven Mode**: For organized, multi-file analysis workflows
   - Run `python scripts/ab_project_create.py <project_type> <project_directory>` to create a project structure
   - Creates a project directory with subdirectories: `data/`, `scripts/`, and `reports/`
   - Currently supports "device_report" project type (more types planned)
   - Ideal for comprehensive device testing with multiple measurements and organized results

2. **Direct Analysis Mode**: For quick, ad-hoc audio analysis
   - Call `ab_*` programs directly with parameters
   - Example: `./bin/ab_audio_analyze input.wav`
   - Ideal for one-off analysis tasks or scripting custom workflows

## Project Status

**Note**: All C programs successfully build with the current Makefile. Python orchestration layer (`generate_report.py`) is partially implemented. Current source files include:
- `ab_acq.c` - Audio acquisition/recording from sound card using PortAudio
- `ab_audio_visualizer.c` - Real-time audio waveform visualizer (Windows GUI only, uses Windows GDI)
- `ab_check_levels.c` - Utility to measure and compare levels of two audio files
- `ab_freq_response.c` - Frequency response analysis using deconvolution
- `ab_gain_calc.c` - Gain calculator for comparing two 1kHz wave files
- `ab_list_dev.c` - Lists audio devices (input/output) with filtering options using PortAudio
- `ab_list_wav.c` - Lists WAV files in directory with properties
- `ab_thd_calc.c` - Total Harmonic Distortion (THD) calculator for sine waves
- `ab_wav_fft.c` - FFT-based frequency domain analysis with interval snapshot support

**Python Scripts**:
- `ab_project_create.py` - Creates organized project directory structures (✓ implemented)
- `generate_report.py` - Report generation orchestration (partially implemented)
- `mv2db.py` - Utility for converting measurements to dB

## Build System

**Building:** Use `make` to compile all C programs. Binaries are placed in `bin/` directory.

```bash
make              # Build all programs
make clean        # Remove build artifacts
make install      # Install to /c/msys64/opt/audio-bench (Windows/MSYS2)
                  # Copies binaries, gnuplot scripts, Python scripts, and test waves
make uninstall    # Remove installed files
make help         # Show available make targets
```

**Compiler flags:** The Makefile uses `-Wall -O2 -std=c11` with linking to `-lm -lsndfile -lfftw3 -lpopt -lportaudio`. All required libraries are linked by default.

**Platform-specific notes:**
- `ab_audio_visualizer` only builds on Windows (requires Windows GDI and uses `-mwindows -lgdi32 -lcomctl32` flags)
- The `make install` target also runs `make -C waves clean all` to generate test signals (requires `dlab_chirp` utility)

## Key Dependencies

### Required Libraries
- **libsndfile**: WAV file I/O operations (all C programs depend on this)
- **FFTW3**: Fast Fourier Transform operations for frequency analysis
- **PortAudio**: Audio acquisition/recording from sound cards (used by ab_acq)
- **libpopt**: Command-line option parsing (used by ab_acq)
- **gnuplot 5.0+**: Graph visualization
- **Python 3**: NumPy, SciPy, matplotlib for data processing

### Platform-specific setup
On Windows/MSYS2 environment (current platform), ensure MINGW64 packages are installed:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-libsndfile mingw-w64-x86_64-fftw
pacman -S mingw-w64-x86_64-portaudio mingw-w64-x86_64-popt
pacman -S mingw-w64-x86_64-gnuplot mingw-w64-x86_64-python mingw-w64-x86_64-python-pip
```
See docs/INSTALL.md for complete dependency installation instructions for all platforms.

## Architecture

### Three-Layer Design

1. **C Analysis Programs** (src/): Low-level audio processing
   - Read WAV files using libsndfile
   - Perform real-time analysis (peak detection, RMS calculation, FFT)
   - Output results in parseable text format
   - Available programs (all prefixed with `ab_`):
     - `ab_acq` - Audio acquisition/recording from sound card devices
     - `ab_audio_visualizer` - Real-time waveform visualizer with GUI (Windows only)
     - `ab_check_levels` - Utility to measure and compare levels of two audio files
     - `ab_freq_response` - Frequency response analysis using deconvolution
     - `ab_gain_calc` - Gain calculator for comparing two 1kHz wave files
     - `ab_list_dev` - Lists audio devices with input/output filtering
     - `ab_list_wav` - Lists WAV files in directory with their properties
     - `ab_thd_calc` - Total Harmonic Distortion (THD) calculator for sine waves
     - `ab_wav_fft` - FFT-based frequency domain analysis with interval snapshot support

2. **Python Orchestration** (scripts/): High-level workflow coordination
   - `generate_report.py`: Main entry point for report generation (partially implemented)
   - `ab_project_create.py`: Creates project directory structures for organized testing
   - `mv2db.py`: Measurement conversion utilities
   - Coordinates execution of C programs and aggregates results

3. **Gnuplot Visualization** (gnuplot/): Graph generation
   - Pre-configured plotting scripts for different sample rates and bit depths
   - Uses pngcairo terminal for high-quality output
   - Templates include: `plot1_*.gp`, `plot2_*.gp`, `plot3_*.gp` (for 48kHz/96kHz, 16/24-bit)
   - FFT display scripts: `fft_display_*.gp`
   - Expects CSV data files from `ab_wav_fft` or similar tools

### Data Flow
```
WAV file → C analysis programs → .csv/.dat files → gnuplot scripts → .png graphs
                                                 ↓
                                         Python aggregation → report.md
```

**Common data structures across C programs:**
- `AudioBuffer`: Standard structure for loaded audio data (data pointer, length, sample_rate, channels)
- `LevelStats`: Audio level measurements (peak_pos, peak_neg, peak_dbfs, rms, rms_dbfs, crest_factor)
- All programs use `libsndfile`'s SF_INFO for WAV file metadata
- Programs typically mix multi-channel files to mono for analysis

## Project Directory Structure (Project-Driven Mode)

When using `ab_project_create.py`, the following structure is created:

```
project_name/
├── README.md          # Project metadata (type, creation date)
├── data/              # Raw data storage
│   └── ...            # WAV files, measurements, recordings
├── scripts/           # Analysis scripts
│   └── ...            # Python scripts for processing
└── reports/           # Output location
    └── ...            # Generated reports and visualizations
```

**Directory purposes**:
- `data/`: Where ab_ tools store raw data (recordings, measurements)
- `scripts/`: Custom Python scripts for report generation and analysis
- `reports/`: Final output reports, graphs, and documentation

## Common Development Commands

### Project-Driven Mode
```bash
# Create a new project structure
python scripts/ab_project_create.py device_report project_name/

# List available project types
python scripts/ab_project_create.py --list-types

# Generate full report from project data
python scripts/generate_report.py --input device_test.wav --output report/

# Skip analysis, only regenerate graphs/report from existing data
python scripts/generate_report.py --input test.wav --output report/ --skip-analysis
```

### Direct Analysis Mode
```bash
# Check audio levels (compare two files)
./bin/ab_check_levels reference.wav measured.wav

# Calculate gain between two 1kHz files
./bin/ab_gain_calc reference.wav measured.wav

# Run frequency response analysis
./bin/ab_freq_response input.wav

# Run FFT analysis (single snapshot)
./bin/ab_wav_fft input.wav

# Run FFT analysis with interval snapshots (every 100ms)
./bin/ab_wav_fft -i input.wav -o output_prefix -t 100
# Creates files: output_prefix_0000ms.csv, output_prefix_0100ms.csv, etc.

# List all WAV files in current directory
./bin/ab_list_wav

# List all audio devices (input and output)
./bin/ab_list_dev

# List only input devices
./bin/ab_list_dev --input

# List only output devices
./bin/ab_list_dev --output

# Show detailed info for specific device
./bin/ab_list_dev --info 0

# Record audio from sound card (use ab_list_dev to find device index)
./bin/ab_acq -d 0 -o recording.wav -t 5
./bin/ab_acq -d 0 -o test.wav -r 48000 -c 2 -b 24

# Calculate THD for a sine wave
./bin/ab_thd_calc -f test_1khz.wav                    # 1kHz (default)
./bin/ab_thd_calc -f test_10khz.wav -F 10000          # 10kHz
./bin/ab_thd_calc -f test_1khz.wav -s 16384 -n 15     # Custom FFT size and harmonics

# Real-time audio visualization (Windows only)
./bin/ab_audio_visualizer.exe
# Opens GUI window with:
# - Device selection dropdowns (input/output)
# - Channel mode selector (Left/Right/Stereo/Combined)
# - Time window adjustment (0.1-10.0 seconds)
# - Start/Stop button for audio capture
# - Real-time waveform display
```

### Creating Graphs
```bash
# Generate graphs using gnuplot scripts (specify sample rate/bit depth version)
gnuplot gnuplot/plot1_48k24b.gp
gnuplot gnuplot/fft_display_48k16b.gp
```

### Test Signal Generation
The `waves/` directory contains a Makefile for generating standard test signals:
```bash
cd waves
make clean all    # Generate chirp and 1kHz test signals at various sample rates
# Creates: chirp_48k24b_10sec.wav, chirp_96k24b_10sec.wav,
#          1kHz_48k24b_10sec.wav, 1kHz_96k24b_10sec.wav
```

**Note:** Requires `dlab_chirp` utility to be installed. These test signals are used for:
- Frequency response testing (chirp signals)
- Gain calibration (1kHz steady tones)
- Loopback testing with ASIO interfaces

## Coding Standards

### C Code (from CONTRIBUTING.md)
- **Style**: K&R indentation, 4 spaces (no tabs), max 100 chars per line
- **Structure**: Use libsndfile for all audio I/O operations
- **Output**: Results must be in parseable format for Python integration
- **Documentation**: Include function documentation with parameter/return descriptions
- **Memory**: Check for leaks with valgrind on Linux

### Python Code
- Follow PEP 8 style guide
- Add docstrings to all functions
- Use type hints where appropriate
- Integration point: `generate_report.py` orchestrates all analysis tools

### Gnuplot Scripts
- Comment each section clearly
- Make parameters configurable (ranges, output format, etc.)
- Use consistent styling with existing graphs
- Support both PNG and PDF output formats

## Adding New Analysis Tools

When implementing a new audio metric analyzer:

1. **Create C program in src/**:
   - Follow existing structure (see ab_audio_analyze.c)
   - Use `ab_` prefix for program names (convention: ab_<tool_name>.c)
   - Use libsndfile for audio I/O
   - Output parseable data format
   - Add appropriate struct for results
   - Include proper command-line argument parsing (consider libpopt for complex options)

2. **Update Makefile** (if needed):
   - New source files are auto-detected by `$(wildcard $(SRC_DIR)/*.c)`
   - Current LDFLAGS include: `-lm -lsndfile -lfftw3 -lpopt -lportaudio`
   - If your program needs additional libraries, update the LDFLAGS line in the Makefile

3. **Create Python integration**:
   - Add subprocess calls in `generate_report.py`
   - Handle output parsing and aggregation

4. **Add gnuplot visualization**:
   - Create new .gp script in gnuplot/
   - Follow existing template structure
   - Include placeholder message when data file missing

5. **Update documentation**:
   - README.md for feature description
   - examples/README.md for usage examples

## File Organization

- **src/**: C source files for audio analysis engines (all ab_*.c files)
- **scripts/**: Python automation and report generation scripts
- **gnuplot/**: Visualization templates with .gp extension (organized by sample rate and bit depth)
- **waves/**: Test signal generation with Makefile (creates chirp and 1kHz tones)
- **bin/**: Compiled binaries (created by make, not in git)
- **docs/**: Installation, contribution guidelines, and application notes
  - **docs/app_notes/**: Application notes for specific hardware calibration and measurements

## Important Notes

- The project is designed for cross-platform use (Linux, macOS, Windows/MSYS2), but `ab_audio_visualizer` is Windows-only
- All audio I/O must go through libsndfile for format compatibility
- **Binary naming convention**: All C programs use the `ab_` prefix (e.g., ab_check_levels, ab_acq)
- **On Windows/MSYS2**: Binaries have `.exe` extension (e.g., `ab_check_levels.exe`)
- All programs link with the same base libraries: `-lm -lsndfile -lfftw3 -lpopt -lportaudio`
  - `ab_audio_visualizer` additionally links with `-mwindows -lgdi32 -lcomctl32` (Windows GUI)
- **Device enumeration**: Use `ab_list_dev` for listing audio devices; `ab_acq` is dedicated to recording only
- Gnuplot scripts are organized by sample rate and bit depth (e.g., `plot1_48k24b.gp`, `fft_display_96k16b.gp`)
- C programs typically mix multi-channel files to mono for analysis using simple averaging
- Common data structures: `AudioBuffer` (general audio data), `LevelStats` (measurement results)
- Install location varies by platform: Linux/macOS use `/opt/audio-bench`, Windows/MSYS2 uses `/c/msys64/opt/audio-bench`
- After installation, set `AUDIO_BENCH=/opt/audio-bench` (or `/c/msys64/opt/audio-bench`) and add to PATH
