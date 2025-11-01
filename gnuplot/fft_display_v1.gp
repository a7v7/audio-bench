#-------------------------------------------------------------------------------
# MIT License
# 
# Copyright (c) 2025 Anthony Verbeck
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
#	set color styles
#-------------------------------------------------------------------------------
	set style line 1 lc rgb 'red' lt 1 lw 1
	set style line 2 lc rgb 'green' lt 1 lw 1
	set style line 3 lc rgb 'blue' lt 1 lw 1
	set style line 4 lc rgb 'cyan' lt 1 lw 1
	set style line 5 lc rgb 'magenta' lt 1 lw 1
	set style line 6 lc rgb 'yellow' lt 1 lw 1

	set style line 11 lc rgb 'dark-red' lt 1 lw 1
	set style line 12 lc rgb 'dark-green' lt 1 lw 1
	set style line 13 lc rgb 'dark-blue' lt 1 lw 1
	set style line 14 lc rgb 'dark-cyan' lt 1 lw 1
	set style line 15 lc rgb 'dark-magenta' lt 1 lw 1
	set style line 16 lc rgb 'dark-yellow' lt 1 lw 1

	set style line 101 lc rgb '#808080' lt 2 lw 1  								# Major grid lines
	set style line 102 lc rgb '#c0c0c0' lt 2 lw 1  								# Minor grid lines

	set grid xtics ytics ls 101													# Apply major grid line style
	set grid mxtics mytics ls 102  												# Apply minor grid line style

#-------------------------------------------------------------------------------
# Set input parameters
#-------------------------------------------------------------------------------
	if (ARGC > 1) {
		file_in = ARG1
		file_out = ARG2
		title_string = ARG3
		label_x_axis = ARG4
		label_y_axis = ARG5
	} else {
		print "usage: <file in> <file out> <title> <x axis> <y axis>"
		quit
	}

#-------------------------------------------------------------------------------
# Set the terminal type and output file
#-------------------------------------------------------------------------------
	hres = 2000
	vres = hres/2
	set terminal pngcairo size hres,vres
	set output file_out
	set datafile separator ","
	set logscale x
	set xrange [10:48000]
	set yrange [-150:10]

#-------------------------------------------------------------------------------
#	Plot data
#-------------------------------------------------------------------------------
	set title title_string font "Arial,24"
	set xlabel label_x_axis font "Arial,24"
	set ylabel label_y_axis font "Arial,24"

	plot file_in skip 11 using 1:2 with lines ls 1 notitle
