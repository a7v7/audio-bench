# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

audio-bench is a comprehensive audio performance measurement toolkit consisting of C programs, Python scripts, and gnuplot visualization tools. It analyzes audio performance metrics including frequency response, harmonic distortion, signal-to-noise ratio, and other key audio characteristics.

## Build System

**Building:** Use `make` to compile all C programs. Binaries are placed in `bin/` directory, object files in `obj/`.

```bash
make              # Build all programs
make clean        # Remove build artifacts
make install      # Install binaries to /usr/local/bin (requires sudo)
make uninstall    # Remove installed binaries
make help         # Show available make targets
```

**Compiler flags:** The Makefile uses `-Wall -O2 -std=c11` with linking to `-lm -lsndfile -lfftw3`.

## Key Dependencies

### Required Libraries
- **libsndfile**: WAV file I/O operations (all C programs depend on this)
- **FFTW3**: Fast Fourier Transform operations for frequency analysis
- **gnuplot 5.0+**: Graph visualization
- **Python 3**: NumPy, SciPy, matplotlib for data processing

### Platform-specific setup
On Windows/MSYS2 environment (current platform), ensure MINGW64 packages are installed. See docs/INSTALL.md for complete dependency installation instructions for all platforms.

## Architecture

### Three-Layer Design

1. **C Analysis Programs** (src/): Low-level audio processing
   - Read WAV files using libsndfile
   - Perform real-time analysis (peak detection, RMS calculation, FFT)
   - Output results in parseable text format
   - Example: `audio-analyze.c` provides basic peak/RMS analysis with AudioStats structure

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

## Common Development Commands

### Running Analysis
```bash
# Analyze single WAV file
./bin/audio-analyze input.wav

# With options
./bin/audio-analyze input.wav -o output.txt -v

# Generate full report
python scripts/generate_report.py --input device_test.wav --output report/

# Skip analysis, only regenerate graphs/report from existing data
python scripts/generate_report.py --input test.wav --output report/ --skip-analysis
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
    ./bin/audio-analyze "$file" -o "results/$(basename $file .wav).txt"
done
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
   - Follow existing structure (see audio-analyze.c)
   - Use libsndfile for audio I/O
   - Output parseable data format
   - Add appropriate struct for results

2. **Update Makefile**:
   - New source files are auto-detected by `$(wildcard $(SRC_DIR)/*.c)`
   - Ensure linking includes necessary libraries

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
- Gnuplot scripts expect specific data file formats - maintain consistency
- Python scripts check for tool dependencies before execution
- C programs should handle mono and stereo audio files appropriately (see AudioStats structure)
