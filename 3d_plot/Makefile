.PHONY:	help clean all plot plot3d
help:
	@printf "usage:\n"
	@printf "\tall     - Run clean, process, and plot3d.\n"
	@printf "\tclean   - Delete all artifacts.\n"
	@printf "\tprocess - Process the wave file into 10 different time offset FFT csv files.\n"
	@printf "\tplot    - Plot each csv file as a 2D graph.\n"
	@printf "\tplot3d  - Plot the 3D data into a 3D graph.\n"

all:	clean process plot3d

clean:
	rm -rf *.csv *.png

process:
	ab_wav_fft --input=test_1.wav --output=test_1.csv --average=20 --interval=1000

plot:
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_0000ms.csv test_1_0000ms.png "0000ms 28dB" "Frequency" "Level (dBFS)"
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_1000ms.csv test_1_1000ms.png "1000ms 28dB" "Frequency" "Level (dBFS)"
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_2000ms.csv test_1_2000ms.png "2000ms 28dB" "Frequency" "Level (dBFS)"
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_3000ms.csv test_1_3000ms.png "3000ms 28dB" "Frequency" "Level (dBFS)"
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_4000ms.csv test_1_4000ms.png "4000ms 28dB" "Frequency" "Level (dBFS)"
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_5000ms.csv test_1_5000ms.png "5000ms 28dB" "Frequency" "Level (dBFS)"
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_6000ms.csv test_1_6000ms.png "6000ms 28dB" "Frequency" "Level (dBFS)"
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_7000ms.csv test_1_7000ms.png "7000ms 28dB" "Frequency" "Level (dBFS)"
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_8000ms.csv test_1_8000ms.png "8000ms 28dB" "Frequency" "Level (dBFS)"
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1_9000ms.csv test_1_9000ms.png "9000ms 28dB" "Frequency" "Level (dBFS)"

plot3d:
	./combine_data.sh
	gnuplot -c plot_fft_combined.gp