# Gnuplot script for frequency response graph
# Usage: gnuplot frequency_response.gp

# Set output format
set terminal pngcairo enhanced font 'Arial,12' size 1200,800
set output 'frequency_response.png'

# Graph styling
set title "Frequency Response" font 'Arial,16'
set xlabel "Frequency (Hz)" font 'Arial,14'
set ylabel "Magnitude (dB)" font 'Arial,14'
set grid
set key top right

# Logarithmic x-axis for frequency
set logscale x
set xrange [20:20000]
set yrange [-60:10]

# X-axis formatting
set format x "10^{%L}"

# Plot the data
# Uncomment and adjust the following line when you have actual data:
# plot 'data/frequency_response.dat' using 1:2 with lines lw 2 title 'Measured Response'

# Placeholder for demonstration
set label 1 "Data file not found.\nGenerate frequency_response.dat to see actual graph." \
    at graph 0.5, graph 0.5 center font 'Arial,14'

plot NaN notitle

# For PDF output, use:
# set terminal pdfcairo enhanced font 'Arial,12' size 10,6
# set output 'frequency_response.pdf'
# replot
