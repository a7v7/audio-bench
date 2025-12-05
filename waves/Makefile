#-------------------------------------------------------------------------------
# Makefile for audio-bench
#
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
#	Commands
#-------------------------------------------------------------------------------
CC		= gcc
MV		= mv
RM		= rm -rf
WAVE	= dlab_chirp --log --f_start=20 --f_end=20000 --db=-12 --time=10

#-------------------------------------------------------------------------------
# Export temp directory for Windows g++ compilation
#-------------------------------------------------------------------------------
export TMPDIR=/tmp
export TMP=/tmp
export TEMP=/tmp

#-------------------------------------------------------------------------------
# Start of targets
#-------------------------------------------------------------------------------
.PHONY: help all clean install

#-------------------------------------------------------------------------------
# Help
#-------------------------------------------------------------------------------
help:
	@echo "audio-bench waves Makefile"
	@echo "Available targets:"
	@echo "  help      - Show this help message"
	@echo "  clean     - Remove all waves"
	@echo "  all       - Build waves used with ab_asio_loopback"

#-------------------------------------------------------------------------------
# Clean build artifacts
#-------------------------------------------------------------------------------
clean:
	$(RM) *.wav

#-------------------------------------------------------------------------------
# All build artifacts
#-------------------------------------------------------------------------------
all:
	$(WAVE) --rate=96000 --bits=24 --out=chirp_96k24b_10sec.wav
	$(WAVE) --rate=48000 --bits=24 --out=chirp_48k24b_10sec.wav
