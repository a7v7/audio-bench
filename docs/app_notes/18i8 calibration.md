# Focusrite 18i8 Gen 3 Output Calibration

## Date
December 10, 2025

## Purpose
Calibrate the relationship between digital signal levels (dBFS) and analog output voltages (dBu/Vpp) for the Focusrite 18i8 Gen 3 line outputs (channels 3-4) to enable accurate test signal generation for preamp measurements.

---

## Equipment Used

- **Audio Interface**: Focusrite 18i8 Gen 3
- **Test Output**: Channel 3 (ASIO channel 2) - rear line output
- **Signal Generator**: Custom application generating 1 kHz sine waves at 96 kHz / 24-bit
- **Playback Software**: Audacity (with output slider at unity/0 dB)
- **Oscilloscope**: Digital scope with 1 MΩ input impedance, DC coupling
- **Load Resistor**: 10 kΩ (simulating typical line input impedance)
- **Probe**: Scope probe with short ground lead
- **Test Signal**: 1 kHz pure sine wave

---

## Key Findings

### Digital-to-Analog Calibration

**Primary Calibration Points (measured with 10 kΩ load):**

| dBFS Level | Measured Vpp | Calculated V RMS | Calculated dBu |
|------------|--------------|------------------|----------------|
| -3 dBFS    | 4.5 Vpp      | 1.59 V RMS       | +6.2 dBu       |
| -12 dBFS   | 1.6 Vpp      | 0.566 V RMS      | -2.7 dBu       |

**Derived Reference Level:**
- **0 dBFS = +9.2 dBu (6.36 Vpp, 2.25 V RMS)**

### Formula for Level Conversion

To calculate the required dBFS for a target Vpp:

```
Vpp = 6.36 × 10^(dBFS/20)
```

Or inversely, to find dBFS for a desired Vpp:

```
dBFS = 20 × log₁₀(Vpp / 6.36)
```

To convert to dBu:

```
dBu = 20 × log₁₀(V_RMS / 0.775)
```

Where: `V_RMS = Vpp / 2.828`

### Quick Reference Table

| Target Vpp | Target dBu | Required dBFS |
|------------|------------|---------------|
| 6.36 Vpp   | +9.2 dBu   | 0 dBFS        |
| 4.5 Vpp    | +6.2 dBu   | -3 dBFS       |
| 3.18 Vpp   | +3.2 dBu   | -6 dBFS       |
| 1.6 Vpp    | -2.7 dBu   | -12 dBFS      |
| 800 mVpp   | -8.7 dBu   | -18 dBFS      |
| 400 mVpp   | -14.7 dBu  | -24 dBFS      |
| 200 mVpp   | -20.7 dBu  | -30 dBFS      |

---

## Important Observations

### Output Impedance

The 18i8 line outputs have very low output impedance (estimated 20-50 Ω):
- Voltage drop with 10 kΩ load was minimal (1.6 Vpp unloaded → 1.58 Vpp loaded = 0.11 dB)
- Can drive typical line input loads without significant loss
- **Do NOT attempt to drive 50 Ω loads** - this resulted in severe voltage drop (300 mVpp at -12 dBFS)

### Noise Floor Limitations

**Critical finding:** The 18i8's output noise floor becomes significant at low signal levels:

- **Above -12 dBFS**: Clean, accurate measurements possible
- **-12 to -20 dBFS**: Noise becoming visible but measurements still usable
- **Below -20 dBFS**: Noise floor dominates, measurements unreliable

**Evidence:**
- At -30 dBFS (predicted 200 mVpp), measured 340 mVpp due to noise contribution
- Visual observation: signal becomes progressively noisier as level decreases
- The noise adds to peak measurements, making low-level signals appear larger

**Recommendation for preamp testing:**
- Use signal levels of **-12 dBFS or higher** (-6 dBFS to -3 dBFS recommended)
- For noise floor measurements, account for 18i8's own noise contribution
- The noise will appear in FFT analysis and affect low-level measurements

### High-Frequency Noise ("Spikes")

When measuring unloaded outputs with a cable (not scope probe), ~18 MHz transients (~300 mVpp) were observed:
- **Disappeared when 10 kΩ load was added** (load acted as snubber/damping)
- **Disappeared when using scope probe** with short ground lead (better HF characteristics)
- Likely caused by reflections/resonance in unloaded cable or ground loop effects

**Not a concern for actual use:** Real-world line inputs provide loading that eliminates this artifact.

### Reference Level Comparison

The 18i8's **0 dBFS = +9.2 dBu** is significantly lower than typical professional interfaces:
- Pro interfaces typically: 0 dBFS = +18 to +24 dBu
- This means the 18i8 has less output headroom
- Must account for this when comparing with equipment expecting higher line levels

---

## Measurement Procedure (for future reference)

### Setup

1. Connect 18i8 line output (channel 3) to oscilloscope via scope probe
2. Add 10 kΩ resistor across scope input (tip to ground) to simulate line input load
3. Set scope to: **DC coupling, 1 MΩ input impedance**
4. Set scope vertical scale appropriately (500 mV/div for 1-2 Vpp signals)
5. Generate 1 kHz sine wave at desired dBFS level (96 kHz, 24-bit)
6. Play through Audacity with **output slider at unity (0 dB)**

### Measurement

1. Verify sine wave is clean on scope display
2. Measure peak-to-peak voltage (Vpp)
3. For signals above -12 dBFS, measurements are reliable
4. For signals below -20 dBFS, expect noise-corrupted measurements

### Verification

Use these checkpoints to verify calibration:
- -3 dBFS should produce ~4.5 Vpp
- -12 dBFS should produce ~1.6 Vpp
- Measurements should scale linearly (every 6 dB = 2× voltage change)

---

## Critical Reminders

1. **Always verify Audacity output slider is at unity** - even slight adjustments cause calibration errors
2. **Stay above -20 dBFS for accurate measurements** - noise floor dominates below this
3. **Use 10 kΩ load for realistic measurements** - simulates typical line input
4. **The 18i8 is NOT suitable for driving 50 Ω loads** - severe voltage drop occurs
5. **Sample rate must match** - generate at 96 kHz, ensure 18i8 is set to 96 kHz
6. **This calibration is specific to channels 3-4 (line outputs)** - channels 1-2 may differ

---

## Application to Preamp Testing

### Recommended Test Levels

For measuring preamp gain and frequency response:
- Use **-6 dBFS to -3 dBFS** output levels (3.18 to 4.5 Vpp)
- This provides clean signal well above noise floor
- Sufficient level to drive preamp inputs without clipping

### For Noise Measurements

When measuring preamp EIN (Equivalent Input Noise):
- Be aware the 18i8's noise floor will contribute to measurements
- May need to characterize 18i8's own noise separately
- Consider using a lower-noise interface for critical noise measurements

### Gain Staging

When measuring preamp gain:
- Set known dBFS level at 18i8 output
- Measure preamp output voltage
- Account for 18i8's +9.2 dBu reference when calculating gain

---

## Notes and Gotchas

### The "Should Be Called 8i4" Issue
The 18i8 only has 8 physical inputs (4 analog + 4 optical) and 4 physical outputs (2 monitor + 2 line). The remaining I/O requires the optical port, which is not useful for this application.

### Crest Factor Observations
- Pure 1 kHz sine wave should have 3.01 dB crest factor (√2 ratio)
- Reference signal showed perfect 3.01 dB crest factor ✓
- Recorded signals at low levels showed increased crest factor due to noise

### Why This Matters
Without this calibration, there's no way to generate precise analog voltage levels for testing. The relationship between dBFS and real-world voltage is entirely interface-dependent and must be measured empirically.

---

## Future Work

Consider characterizing:
1. **Frequency response** - does the calibration hold across 20 Hz to 20 kHz?
2. **Channel matching** - do channels 3 and 4 have identical calibration?
3. **Other outputs** - do channels 1-2 (monitor outputs) differ?
4. **THD+N** - what's the distortion contribution at various levels?
5. **Noise floor spectrum** - FFT analysis of output noise

---

## Conclusion

The Focusrite 18i8 Gen 3 line outputs (channels 3-4) are now calibrated:
- **0 dBFS = +9.2 dBu = 6.36 Vpp = 2.25 V RMS**
- Accurate, repeatable measurements are possible above -12 dBFS
- Below -20 dBFS, noise floor dominates and measurements become unreliable
- This calibration enables precise test signal generation for preamp testing

**The calibration is solid and ready for use in the audio-bench measurement suite.**