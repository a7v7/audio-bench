# Examples

This directory contains example usage scenarios and sample data for audio-bench.

## Basic Usage Examples

### 1. Analyze a WAV file
```bash
../bin/audio-analyze sample.wav
```

### 2. Generate a full report
```bash
python3 ../scripts/generate_report.py --input sample.wav --output report/
```

### 3. Create individual graphs
```bash
# Generate frequency response graph
gnuplot ../gnuplot/frequency_response.gp

# Generate THD graph
gnuplot ../gnuplot/thd.gp
```

## Sample Workflows

### Device Testing Workflow
1. Record test signals through the device
2. Capture the output as WAV files
3. Run analysis on each test file
4. Generate comprehensive report

```bash
# Process multiple test files
for file in tests/*.wav; do
    ../bin/audio-analyze "$file" -o "results/$(basename $file .wav).txt"
done

# Generate report
python3 ../scripts/generate_report.py --input tests/ --output device_report/
```

### Single File Analysis
For quick analysis of a single audio file:

```bash
../bin/audio-analyze my_recording.wav -v
```

## Sample Data

Place sample WAV files in this directory for testing purposes.

### Creating Test Signals

You can generate test signals using various tools:

**Using SoX (Sound eXchange):**
```bash
# Generate 1kHz sine wave
sox -n -r 48000 -c 2 test_1khz.wav synth 5 sine 1000

# Generate sweep
sox -n -r 48000 -c 2 sweep.wav synth 10 sine 20-20000
```

**Using Python with scipy:**
```python
import numpy as np
from scipy.io import wavfile

# Generate 1kHz tone
sample_rate = 48000
duration = 5
t = np.linspace(0, duration, int(sample_rate * duration))
signal = np.sin(2 * np.pi * 1000 * t)
wavfile.write('test_1khz.wav', sample_rate, signal.astype(np.float32))
```
