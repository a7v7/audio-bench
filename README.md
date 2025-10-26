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
- Multiple audio metrics measurement:
  - Frequency response
  - Total Harmonic Distortion (THD)
  - Signal-to-Noise Ratio (SNR)
  - Dynamic range
  - Phase response
  - Latency measurement
- Automated report generation with graphs
- Flexible usage for both device testing and file analysis

## Dependencies

### C Programs
- libsndfile (for WAV file handling)
- FFTW3 (for FFT operations)
- Standard C library (math.h, etc.)

### Python Scripts
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
./bin/audio-analyze input.wav
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

See [docs/INSTALL.md](docs/INSTALL.md) for detailed installation instructions.

## Contributing

Contributions are welcome! Please see [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines.

## License

[License information to be added]

## Authors

[Author information to be added]
