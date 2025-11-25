# FFT Measurement Dynamic Range with the Focusrite Scarlett 18i8 (3rd Gen)

## Overview

When performing FFT-based audio analysis with the Scarlett 18i8 3rd Gen interface at 96kHz/24-bit, measurements consistently reach -130dBFS—significantly better than the ADC's specified 111dB dynamic range might suggest. This application note explains why the measured performance exceeds the raw ADC specifications.

## ADC Hardware Specifications

The Scarlett 18i8 3rd Gen provides the following performance at 96kHz/24-bit:

- **Dynamic Range:** 111dB (A-weighted) for mic inputs, 110.5dB for line inputs
- **THD+N:** <0.0012% (mic), <0.002% (line)
- **Noise Floor (EIN):** -128dBu (A-weighted)

The practical noise floor is approximately **-110dBFS**, significantly higher than the theoretical 24-bit limit of -144dBFS.

## FFT Processing Gains

FFT analysis provides substantial processing gain that effectively lowers the measured noise floor:

### FFT Size Processing Gain

Processing gain from FFT bin resolution:

**Processing Gain = 10 × log₁₀(N/2)**

For an 8192-point FFT:
- Gain = 10 × log₁₀(4096) = **36.1 dB**

### Averaging Gain

Additional gain from averaging multiple FFT frames:

**Averaging Gain = 10 × log₁₀(M)**

For 20 averages:
- Gain = 10 × log₁₀(20) = **13.0 dB**

### Window Function Effects

A Hann window (standard for audio analysis) introduces:
- **ENBW penalty:** ~10 × log₁₀(1.5) = **-1.76 dB**
- Coherent gain loss: -6 dB (normalized in most FFT implementations)
- Scalloping loss: up to -1.4 dB (frequency-dependent)

## Total System Performance

**Net Processing Gain:**
36.1 dB (FFT) + 13.0 dB (averaging) - 1.76 dB (ENBW) = **47.3 dB**

**Theoretical Noise Floor Per FFT Bin:**
-110 dBFS (ADC) - 47.3 dB (processing) = **-157.3 dBFS**

**Measured Performance:**
Consistent measurements at **-130dBFS** represent the practical limit for distinguishing real signal components (harmonics, artifacts, distortion products) from the processed noise floor.

## Conclusion

The measured -130dBFS floor is entirely consistent with the Scarlett 18i8's 111dB ADC dynamic range specification. The FFT processing gains explain why measurements reach far below the raw ADC noise floor. This represents excellent performance for a mid-range audio interface, providing sufficient dynamic range for:

- Frequency response analysis
- THD and distortion measurement
- Spectral characterization
- Wireless microphone system analysis

## Measurement Parameters

- **Sample Rate:** 96kHz (48kHz bandwidth)
- **Bit Depth:** 24-bit
- **FFT Size:** 8192 points
- **Window Function:** Hann
- **Averaging:** 20 frames
- **Effective Measurement Floor:** -130dBFS per bin

---

*Research assistance provided by Claude (Anthropic)*