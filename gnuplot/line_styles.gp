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

