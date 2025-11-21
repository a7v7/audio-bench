# Installation Guide

## Prerequisites

### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential libsndfile1-dev libfftw3-dev gnuplot
sudo apt-get install portaudio19-dev libpopt-dev
sudo apt-get install python3 python3-pip
pip3 install numpy scipy matplotlib
```

### Linux (Fedora/RHEL)
```bash
sudo dnf install gcc make libsndfile-devel fftw3-devel gnuplot
sudo dnf install portaudio-devel popt-devel
sudo dnf install python3 python3-pip
pip3 install numpy scipy matplotlib
```

### macOS
```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install libsndfile fftw gnuplot python3 portaudio popt
pip3 install numpy scipy matplotlib
```

### Windows
For Windows, we recommend using WSL (Windows Subsystem for Linux) or MSYS2:

#### Using WSL
1. Install WSL2 and Ubuntu from the Microsoft Store
2. Follow the Linux (Ubuntu/Debian) instructions above

#### Using MSYS2
1. Download and install MSYS2 from https://www.msys2.org/
2. Open MSYS2 MINGW64 terminal
3. Run:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-libsndfile mingw-w64-x86_64-fftw
pacman -S mingw-w64-x86_64-portaudio mingw-w64-x86_64-popt
pacman -S mingw-w64-x86_64-gnuplot mingw-w64-x86_64-python mingw-w64-x86_64-python-pip
pip install numpy scipy matplotlib
```

## Building audio-bench

1. Clone or download the audio-bench repository
2. Navigate to the project directory:
```bash
cd audio-bench
```

3. Build all programs:
```bash
make
```

4. Install system-wide:
```bash
sudo make install  # Linux/macOS
make install       # Windows/MSYS2 (no sudo needed)
```

## Installation Paths

After running `make install`, files are installed to the following locations:

### Linux/macOS
```
/opt/audio-bench/
├── bin/              # Compiled C programs (ab_*)
│   ├── ab_audio_analyze
│   ├── ab_acq
│   ├── ab_freq_response
│   ├── ab_wav_fft
│   ├── ab_gain_calc
│   ├── ab_thd_calc
│   ├── ab_list_wav
│   └── ab_list_dev
├── scripts/          # Python scripts
│   ├── ab_project_create.py
│   └── generate_report.py
└── gnuplot/          # Gnuplot visualization templates
    ├── frequency_response.gp
    └── thd.gp
```

### Windows/MSYS2
```
/c/msys64/opt/audio-bench/
├── bin/              # Compiled C programs (*.exe)
│   ├── ab_audio_analyze.exe
│   ├── ab_acq.exe
│   ├── ab_freq_response.exe
│   ├── ab_wav_fft.exe
│   ├── ab_gain_calc.exe
│   ├── ab_thd_calc.exe
│   ├── ab_list_wav.exe
│   └── ab_list_dev.exe
├── scripts/          # Python scripts
└── gnuplot/          # Gnuplot visualization templates
```

## Environment Setup

To use audio-bench tools from any directory, add the following to your shell configuration file:

### Linux/macOS
Add to `~/.bashrc`, `~/.zshrc`, or `~/.profile`:
```bash
export AUDIO_BENCH=/opt/audio-bench
export PATH=$AUDIO_BENCH/bin:$PATH
```

Then reload your shell configuration:
```bash
source ~/.bashrc  # or ~/.zshrc
```

### Windows/MSYS2
Add to `~/.bashrc`:
```bash
export AUDIO_BENCH=/c/msys64/opt/audio-bench
export PATH=$AUDIO_BENCH/bin:$PATH
```

Then reload:
```bash
source ~/.bashrc
```

After setting up the environment, you can run audio-bench tools from anywhere:
```bash
ab_audio_analyze file.wav
ab_list_dev
ab_gain_calc ref.wav test.wav
```

## Verification

### Verify Build
Test that the build was successful:
```bash
# Check that binaries were created in local bin/ directory
ls bin/

# Test a program
./bin/ab_audio_analyze --help
```

### Verify Installation
After running `make install` and setting up environment variables:
```bash
# Verify AUDIO_BENCH variable is set
echo $AUDIO_BENCH

# Verify programs are in PATH
which ab_audio_analyze

# Test installed programs
ab_audio_analyze --help
ab_list_dev --help
ab_gain_calc --help
ab_thd_calc --help
```

### Verify Dependencies
```bash
# Test gnuplot
gnuplot --version

# Test Python dependencies
python3 -c "import numpy, scipy; print('Python dependencies OK')"

# Verify library dependencies (Linux)
ldd bin/ab_audio_analyze  # Should show libsndfile, libfftw3, libportaudio, etc.
```

## Troubleshooting

### libsndfile not found
If you get errors about libsndfile, ensure the development headers are installed:
- Ubuntu/Debian: `sudo apt-get install libsndfile1-dev`
- Fedora: `sudo dnf install libsndfile-devel`
- macOS: `brew install libsndfile`
- MSYS2: `pacman -S mingw-w64-x86_64-libsndfile`

### FFTW not found
Install FFTW3 development libraries:
- Ubuntu/Debian: `sudo apt-get install libfftw3-dev`
- Fedora: `sudo dnf install fftw3-devel`
- macOS: `brew install fftw`
- MSYS2: `pacman -S mingw-w64-x86_64-fftw`

### PortAudio not found
Install PortAudio development libraries:
- Ubuntu/Debian: `sudo apt-get install portaudio19-dev`
- Fedora: `sudo dnf install portaudio-devel`
- macOS: `brew install portaudio`
- MSYS2: `pacman -S mingw-w64-x86_64-portaudio`

### popt not found
Install popt development libraries:
- Ubuntu/Debian: `sudo apt-get install libpopt-dev`
- Fedora: `sudo dnf install popt-devel`
- macOS: `brew install popt`
- MSYS2: `pacman -S mingw-w64-x86_64-popt`

### Python module errors
Ensure you're using Python 3 and pip3:
```bash
python3 --version
pip3 install --upgrade numpy scipy matplotlib
```

### Environment variable not set
If commands like `ab_audio_analyze` are not found:
1. Verify `AUDIO_BENCH` is set: `echo $AUDIO_BENCH`
2. Verify PATH includes the bin directory: `echo $PATH | grep audio-bench`
3. Reload your shell configuration: `source ~/.bashrc`
4. Or use full path: `$AUDIO_BENCH/bin/ab_audio_analyze`

### Permission denied on installation
- Linux/macOS: Use `sudo make install`
- Windows/MSYS2: Run MSYS2 terminal as administrator if needed, or install to user directory

## Uninstall

To remove audio-bench from your system:

```bash
sudo make uninstall  # Linux/macOS
make uninstall       # Windows/MSYS2
```

This will remove all installed files from `/opt/audio-bench` (or `/c/msys64/opt/audio-bench` on Windows/MSYS2).

To also remove the environment variables, edit your shell configuration file (`~/.bashrc`, `~/.zshrc`, etc.) and remove the following lines:
```bash
export AUDIO_BENCH=/opt/audio-bench
export PATH=$AUDIO_BENCH/bin:$PATH
```

Then reload your shell:
```bash
source ~/.bashrc  # or ~/.zshrc
```

## Development Setup

For development, you may want additional tools:
```bash
# Install development tools (Linux/Ubuntu)
sudo apt-get install gdb valgrind  # For debugging
sudo apt-get install doxygen       # For documentation

# macOS
brew install gdb doxygen

# MSYS2
pacman -S mingw-w64-x86_64-gdb mingw-w64-x86_64-doxygen
```

## Next Steps

After installation, see the main [README.md](../README.md) for usage instructions and examples.
