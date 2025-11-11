# Gnuplot script to plot FFT data as 3D surface with logarithmic frequency axis

set terminal pngcairo size 1600,1200 enhanced font 'Verdana,10'
set output 'fft_combined.png'

# Set title and labels
set title 'FFT Analysis - 3D Surface' font 'Verdana,14'
set xlabel 'Frequency (Hz)' offset 0,-1
set ylabel 'Time (ms)' offset 0,-1
set zlabel 'Magnitude (dBFS)' offset -2,0

# Set logarithmic x-axis
set logscale x

# Set x-axis range to start at 15Hz
set xrange [15:*]

# Set z-axis range
set zrange [-140:0]

# Set colorbox range to match z-axis
set cbrange [-140:0]

# Enable PM3D for surface plotting
set pm3d
set hidden3d

# Set viewing angle (can be adjusted)
set view 60,30

# Set color palette (blue to red gradient)
set palette defined (0 "blue", 1 "cyan", 2 "green", 3 "yellow", 4 "red")

# Enable color box legend
set colorbox

# 3D Surface plot from combined data file
# Data format: frequency time magnitude
splot 'fft_combined_data.dat' using 1:2:3 with pm3d notitle
