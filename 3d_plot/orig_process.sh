#!/usr/bin/env bash
#-------------------------------------------------------------------------------
#	Post processing for all tests
#-------------------------------------------------------------------------------

	ab_wav_fft.exe --input=test_1.wav --output=test_1.csv --average=20
	gnuplot -c $AUDIO_BENCH/gnuplot/fft_display_v2.gp test_1.csv test_1.png "Bare Gain Coin / Silence / 28dB" "Frequency" "Level (dBFS)"
