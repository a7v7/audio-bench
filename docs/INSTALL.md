# Installation Guide

## Prerequisites

### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential libsndfile1-dev libfftw3-dev gnuplot
sudo apt-get install python3 python3-pip
pip3 install numpy scipy matplotlib
```

### Linux (Fedora/RHEL)
```bash
sudo dnf install gcc make libsndfile-devel fftw3-devel gnuplot
sudo dnf install python3 python3-pip
pip3 install numpy scipy matplotlib
```

### macOS
```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install libsndfile fftw gnuplot python3
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

4. (Optional) Install system-wide:
```bash
sudo make install
```

## Verification

Test that the build was successful:
```bash
# Check that binaries were created
ls bin/

# Test gnuplot
gnuplot --version

# Test Python dependencies
python3 -c "import numpy, scipy; print('Python dependencies OK')"
```

## Troubleshooting

### libsndfile not found
If you get errors about libsndfile, ensure the development headers are installed:
- Ubuntu/Debian: `sudo apt-get install libsndfile1-dev`
- Fedora: `sudo dnf install libsndfile-devel`

### FFTW not found
Install FFTW3 development libraries:
- Ubuntu/Debian: `sudo apt-get install libfftw3-dev`
- Fedora: `sudo dnf install fftw3-devel`

### Python module errors
Ensure you're using Python 3 and pip3:
```bash
python3 --version
pip3 install --upgrade numpy scipy matplotlib
```

## Development Setup

For development, you may want additional tools:
```bash
# Install development tools
sudo apt-get install gdb valgrind  # For debugging
sudo apt-get install doxygen       # For documentation
```

## Next Steps

After installation, see the main [README.md](../README.md) for usage instructions.
