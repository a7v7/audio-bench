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
#	GCC flags
#-------------------------------------------------------------------------------
CC = gcc
CFLAGS = -Wall -O2 -std=c11
LDFLAGS = -lm -lsndfile -lfftw3 -lpopt -lportaudio

# Directories
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj

#-------------------------------------------------------------------------------
# Create directories if they don't exist
#-------------------------------------------------------------------------------
$(shell mkdir -p $(BIN_DIR) $(OBJ_DIR))

#-------------------------------------------------------------------------------
# Source files (add your C programs here)
#-------------------------------------------------------------------------------
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
BINARIES = $(SOURCES:$(SRC_DIR)/%.c=$(BIN_DIR)/%)

#-------------------------------------------------------------------------------
# Main target
#-------------------------------------------------------------------------------
all: $(BINARIES)

#-------------------------------------------------------------------------------
# Compile C files to object files
#-------------------------------------------------------------------------------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

#-------------------------------------------------------------------------------
# Link object files to binaries
#-------------------------------------------------------------------------------
$(BIN_DIR)/%: $(OBJ_DIR)/%.o
	$(CC) $< $(LDFLAGS) -o $@

#-------------------------------------------------------------------------------
# Start of targets
#-------------------------------------------------------------------------------
.PHONY: all clean install uninstall help

#-------------------------------------------------------------------------------
# Help
#-------------------------------------------------------------------------------
help:
	@echo "audio-bench Makefile"
	@echo "Available targets:"
	@echo "  all       - Build all programs (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install binaries to /c/msys64/opt and update ~/.bash_profile"
	@echo "  uninstall - Remove installed binaries"
	@echo "  help      - Show this help message"
	@echo "Make sure to set path for /opt/audio-bench/bin and /opt/audio-bench/scripts"
	@echo "Make sure to export AUDIO_BENCH to point to /opt/audio-bench"

#-------------------------------------------------------------------------------
# Clean build artifacts
#-------------------------------------------------------------------------------
clean:
	rm -rf $(BIN_DIR)/* $(OBJ_DIR)/*

#-------------------------------------------------------------------------------
# Install (copy binaries to /c/msys64/opt)
#-------------------------------------------------------------------------------
INSTALL_DIR = /c/msys64/opt/audio-bench
install: all
	@echo "Creating installation directory $(INSTALL_DIR)"
	mkdir -p $(INSTALL_DIR)
	@echo "Installing binaries to $(INSTALL_DIR)/bin"
	mkdir -p $(INSTALL_DIR)/bin
	cp $(BIN_DIR)/* $(INSTALL_DIR)/bin
	@echo "Installing gnuplot scripts to $(INSTALL_DIR)/gnuplot"
	mkdir -p $(INSTALL_DIR)/gnuplot
	cp gnuplot/* $(INSTALL_DIR)/gnuplot
	@echo "Installing python scripts to $(INSTALL_DIR)/scripts"
	mkdir -p $(INSTALL_DIR)/scripts
	cp scripts/* $(INSTALL_DIR)/scripts
	@echo "Installation complete."

#-------------------------------------------------------------------------------
# Uninstall
#-------------------------------------------------------------------------------
uninstall:
	@echo "Removing $(INSTALL_DIR)"
	rm -rf $(INSTALL_DIR)
	@echo "Note: PATH entry in ~/.bash_profile must be removed manually if desired"
