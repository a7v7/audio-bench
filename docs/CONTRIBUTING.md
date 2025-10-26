# Contributing to audio-bench

Thank you for your interest in contributing to audio-bench! This document provides guidelines for contributing to the project.

## How to Contribute

### Reporting Bugs
- Use the GitHub issue tracker
- Describe the bug clearly with steps to reproduce
- Include your system information (OS, compiler version, library versions)
- Attach sample files if relevant

### Suggesting Enhancements
- Check existing issues first to avoid duplicates
- Clearly describe the feature and its use case
- Explain why this would be useful to other users

### Code Contributions

1. **Fork the repository**
2. **Create a feature branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **Make your changes**
   - Follow the coding style (see below)
   - Add tests if applicable
   - Update documentation

4. **Test your changes**
   ```bash
   make clean
   make
   # Run your tests
   ```

5. **Commit your changes**
   ```bash
   git commit -m "Add feature: description of feature"
   ```

6. **Push to your fork**
   ```bash
   git push origin feature/your-feature-name
   ```

7. **Create a Pull Request**

## Coding Style

### C Code
- Use K&R style indentation (4 spaces, no tabs)
- Maximum line length: 100 characters
- Use meaningful variable names
- Comment complex algorithms
- Include function documentation

Example:
```c
/*
 * Calculate the RMS value of an audio buffer
 * 
 * @param buffer: Input audio samples
 * @param size: Number of samples in buffer
 * @return: RMS value
 */
double calculate_rms(const double *buffer, size_t size) {
    double sum = 0.0;
    for (size_t i = 0; i < size; i++) {
        sum += buffer[i] * buffer[i];
    }
    return sqrt(sum / size);
}
```

### Python Code
- Follow PEP 8 style guide
- Use meaningful variable names
- Add docstrings to functions
- Use type hints where appropriate

### Gnuplot Scripts
- Comment each section
- Use consistent naming conventions
- Make parameters configurable

## Adding New Analysis Tools

When adding a new analysis tool:

1. **C Program** (in `src/`):
   - Follow the existing structure
   - Use libsndfile for audio I/O
   - Output results in a parseable format

2. **Python Script** (in `scripts/`):
   - Add integration to `generate_report.py`
   - Include command-line interface

3. **Gnuplot Script** (in `gnuplot/`):
   - Create visualization for the new metric
   - Use consistent styling with existing graphs

4. **Documentation**:
   - Update README.md
   - Add examples to `examples/`
   - Document any new dependencies

## Testing

- Test on multiple platforms if possible (Linux, macOS, Windows)
- Test with various WAV file formats
- Verify memory leaks with valgrind (Linux)
- Check for buffer overflows

## Documentation

- Keep README.md up to date
- Document new features in appropriate places
- Include usage examples
- Update INSTALL.md if dependencies change

## Questions?

Feel free to open an issue for any questions about contributing!
