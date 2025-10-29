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
# Load line styles
#-------------------------------------------------------------------------------
	load './gnuplot_scripts/line_styles.gp'

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
	set xrange [10:22000]
	set yrange [-100:10]

#-------------------------------------------------------------------------------
#	Plot data
#-------------------------------------------------------------------------------
	set title title_string font "Arial,24"
	set xlabel label_x_axis font "Arial,24"
	set ylabel label_y_axis font "Arial,24"

	plot file_in skip 1 using 1:2 with lines ls 1 notitle
