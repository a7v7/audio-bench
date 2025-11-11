# 3D FFT Audio Visualization

A tool for analyzing and visualizing audio frequency spectrums over time using FFT (Fast Fourier Transform) analysis. The project generates both 2D time-sliced plots and 3D surface visualizations of audio frequency content.

## Overview

This project processes WAV audio files to extract FFT data at regular time intervals and creates visualizations showing how the frequency spectrum changes over time. The output includes individual 2D plots for each time slice and a combined 3D surface plot.

## Requirements

- **ab_wav_fft**: FFT processing tool for audio files
- **gnuplot**: Plotting utility (for both 2D and 3D visualizations)
- **bash**: Shell environment for running scripts
- **$AUDIO_BENCH environment variable**: Must point to the audio bench directory containing gnuplot scripts

## Installation

Ensure all dependencies are installed and accessible in your PATH:

```bash
# Verify gnuplot is installed
gnuplot --version

# Set AUDIO_BENCH environment variable
export AUDIO_BENCH=/path/to/audio/bench
```

## Usage

### Quick Start

Run the complete pipeline:

```bash
make all
```

This will clean previous outputs, process the audio file, and generate the 3D visualization.

### Individual Commands

```bash
# Display available commands
make help

# Clean all generated files (CSV and PNG)
make clean

# Process WAV file into FFT CSV files
make process

# Generate 2D plots for each time slice
make plot

# Generate 3D surface plot
make plot3d
```

### Processing Parameters

The FFT processing uses the following default parameters:
- **Input**: `test_1.wav`
- **Averaging**: 20 FFT frames
- **Time interval**: 1000ms (1 second)
- **Time slices**: 10 (0-9 seconds)

To process different files or change parameters, edit the `Makefile` or run `ab_wav_fft` directly:

```bash
ab_wav_fft --input=your_file.wav --output=output.csv --average=20 --interval=1000
```

## Output Files

### CSV Files
- `test_1_0000ms.csv` through `test_1_9000ms.csv`: Individual FFT data for each time slice
- Format: frequency (Hz), magnitude (dBFS)

### Data Files
- `fft_combined_data.dat`: Combined 3D data file
- Format: frequency (Hz), time (ms), magnitude (dBFS)

### Visualizations
- `test_1_0000ms.png` through `test_1_9000ms.png`: 2D frequency spectrum plots
- `fft_combined.png`: 3D surface plot showing frequency spectrum evolution over time

## 3D Visualization Features

The 3D plot (`fft_combined.png`) includes:
- **X-axis**: Frequency (Hz) with logarithmic scale, starting at 15 Hz
- **Y-axis**: Time (ms)
- **Z-axis**: Magnitude (dBFS) ranging from -140 to 0
- **Color gradient**: Blue (low magnitude) to red (high magnitude)
- **View angle**: 60° elevation, 30° azimuth

To customize the 3D plot, edit `plot_fft_combined.gp`.

## Workflow Example

```bash
# 1. Place your WAV file in the directory
cp /path/to/audio.wav test_1.wav

# 2. Run the complete pipeline
make all

# 3. View the output
# - Individual 2D plots: test_1_*ms.png
# - 3D surface plot: fft_combined.png
```

## Project Structure

```
.
├── Makefile                 # Build automation
├── test_1.wav              # Sample input audio file
├── combine_data.sh         # Script to merge CSV files for 3D plotting
├── plot_fft_combined.gp    # Gnuplot script for 3D visualization
├── local_process.sh        # Alternative processing script
├── orig_process.sh         # Legacy processing script
└── README.md               # This file
```

## Customization

### Changing the View Angle

Edit `plot_fft_combined.gp` line 29:
```gnuplot
set view 60,30  # Change elevation and azimuth angles
```

### Adjusting Frequency Range

Edit `plot_fft_combined.gp` line 16:
```gnuplot
set xrange [15:*]  # Change minimum frequency (currently 15 Hz)
```

### Modifying Color Palette

Edit `plot_fft_combined.gp` line 32:
```gnuplot
set palette defined (0 "blue", 1 "cyan", 2 "green", 3 "yellow", 4 "red")
```

## License

No license specified.
