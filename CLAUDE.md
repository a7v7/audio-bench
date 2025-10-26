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

**Note**: Several new C programs have been created but may not yet be fully integrated into the build system or Python orchestration layer. Current source files include:
- `ab_audio_analyze.c` - Basic analysis (likely working with current Makefile)
- `ab_acq.c` - Audio acquisition (requires PortAudio/libpopt - may need custom build)
- `ab_freq_response.c` - Frequency response (check build requirements)
- `ab_wav_fft.c` - FFT analysis (requires FFTW3)

**Python Scripts**:
- `ab_project_create.py` - Creates organized project directory structures (✓ implemented)
- `generate_report.py` - Report generation orchestration (partially implemented)

## Build System

**Building:** Use `make` to compile all C programs. Binaries are placed in `bin/` directory, object files in `obj/`.

```bash
make              # Build all programs
make clean        # Remove build artifacts
make install      # Install binaries to /usr/local/bin (requires sudo)
make uninstall    # Remove installed binaries
make help         # Show available make targets
```

**Compiler flags:** The Makefile uses `-Wall -O2 -std=c11` with linking to `-lm -lsndfile -lfftw3`. Additional libraries may be needed for specific programs (e.g., `-lportaudio -lpopt` for ab_acq).

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
```
See docs/INSTALL.md for complete dependency installation instructions for all platforms.

## Architecture

### Three-Layer Design

1. **C Analysis Programs** (src/): Low-level audio processing
   - Read WAV files using libsndfile
   - Perform real-time analysis (peak detection, RMS calculation, FFT)
   - Output results in parseable text format
   - Available programs (all prefixed with `ab_`):
     - `ab_audio_analyze.c` - Basic peak/RMS analysis with AudioStats structure
     - `ab_acq.c` - Audio acquisition/recording from sound card devices
     - `ab_freq_response.c` - Frequency response analysis
     - `ab_wav_fft.c` - FFT-based frequency domain analysis

2. **Python Orchestration** (scripts/): High-level workflow coordination
   - `generate_report.py`: Main entry point for report generation
   - Coordinates execution of C programs
   - Processes multiple files
   - Aggregates results into structured output

3. **Gnuplot Visualization** (gnuplot/): Graph generation
   - Pre-configured plotting scripts for different metrics
   - Uses pngcairo terminal for high-quality output
   - Templates: `frequency_response.gp`, `thd.gp`
   - Expects data files in specific format

### Data Flow
```
WAV file → C analysis programs → .dat files → gnuplot scripts → .png graphs
                                            ↓
                                    Python aggregation → report.md
```

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
# Analyze single WAV file (basic peak/RMS analysis)
./bin/ab_audio_analyze input.wav

# With options
./bin/ab_audio_analyze input.wav -o output.txt -v

# Run frequency response analysis
./bin/ab_freq_response input.wav

# Run FFT analysis
./bin/ab_wav_fft input.wav

# Record audio from sound card (list devices first)
./bin/ab_acq --list-devices
./bin/ab_acq --device 0 --output recording.wav --duration 5
```

### Creating Graphs
```bash
# Generate individual graphs (from gnuplot/ directory or specify path)
gnuplot gnuplot/frequency_response.gp
gnuplot gnuplot/thd.gp
```

### Testing with Sample Data
```bash
# Process multiple test files (batch analysis)
for file in tests/*.wav; do
    ./bin/ab_audio_analyze "$file" -o "results/$(basename $file .wav).txt"
done

# Generate test signals with SoX (if available)
sox -n -r 48000 -c 2 test_1khz.wav synth 5 sine 1000
sox -n -r 48000 -c 2 sweep.wav synth 10 sine 20-20000
```

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

2. **Update Makefile**:
   - New source files are auto-detected by `$(wildcard $(SRC_DIR)/*.c)`
   - Ensure linking includes necessary libraries
   - Note: Current Makefile may need modification if your program requires additional libraries beyond the default `-lm -lsndfile -lfftw3`

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

- **src/**: C source files for audio analysis engines
- **scripts/**: Python automation and report generation
- **gnuplot/**: Visualization templates with .gp extension
- **bin/**: Compiled binaries (created by make, not in git)
- **obj/**: Object files (created by make, not in git)
- **data/**: Sample data and test files
- **tests/**: Test files and test data
- **examples/**: Usage examples and sample WAV files
- **docs/**: Installation and contribution guidelines

## Important Notes

- The project is designed for cross-platform use (Linux, macOS, Windows/MSYS2)
- All audio I/O must go through libsndfile for format compatibility
- **Binary naming convention**: All C programs use the `ab_` prefix (e.g., ab_audio_analyze, ab_acq)
- **Makefile limitation**: Current Makefile links all programs with the same libraries (`-lm -lsndfile -lfftw3`). Programs requiring additional libraries (like ab_acq which needs `-lportaudio -lpopt`) may need custom Makefile rules or manual compilation
- Gnuplot scripts expect specific data file formats - maintain consistency
- Python scripts check for tool dependencies before execution
- C programs should handle mono and stereo audio files appropriately (see AudioStats structure in ab_audio_analyze.c)
