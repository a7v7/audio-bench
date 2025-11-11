#!/bin/bash
# Combine all CSV files into a single data file for 3D surface plotting

output="fft_combined_data.dat"
> "$output"  # Clear the file

# Process each CSV file
for time in 0000 1000 2000 3000 4000 5000 6000 7000 8000 9000; do
    # Skip header and add time column, output: frequency time magnitude
    tail -n +2 "test_1_${time}ms.csv" | awk -F',' -v t="${time#0}" '{gsub(/^[ \t]+|[ \t]+$/, "", $1); gsub(/^[ \t]+|[ \t]+$/, "", $2); print $1, t, $2}' >> "$output"
    # Add blank line to separate time slices (required for gnuplot surface plots)
    echo "" >> "$output"
done

echo "Created $output"
