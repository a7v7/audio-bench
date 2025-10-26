# Gnuplot script for Total Harmonic Distortion (THD) graph
# Usage: gnuplot thd.gp

# Set output format
set terminal pngcairo enhanced font 'Arial,12' size 1200,800
set output 'thd.png'

# Graph styling
set title "Total Harmonic Distortion (THD)" font 'Arial,16'
set xlabel "Frequency (Hz)" font 'Arial,14'
set ylabel "THD (%)" font 'Arial,14'
set grid
set key top right

# Logarithmic scales
set logscale x
set logscale y
set xrange [20:20000]
set yrange [0.001:10]

# Axis formatting
set format x "10^{%L}"
set format y "%.3f"

# Plot the data
# Uncomment and adjust the following line when you have actual data:
# plot 'data/thd.dat' using 1:2 with lines lw 2 title 'THD Measurement'

# Placeholder for demonstration
set label 1 "Data file not found.\nGenerate thd.dat to see actual graph." \
    at graph 0.5, graph 0.5 center font 'Arial,14'

plot NaN notitle
