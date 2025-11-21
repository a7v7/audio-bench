#-------------------------------------------------------------------------------
# Makefile for ab_acq_asio (ASIO Audio Acquisition Tool)
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
# Commands
#-------------------------------------------------------------------------------
CXX      = g++
RM       = rm -rf
MKDIR    = mkdir -p

#-------------------------------------------------------------------------------
# Directories
#-------------------------------------------------------------------------------
ASIO_SDK    = ASIOSDK
ASIO_COMMON = $(ASIO_SDK)/common
ASIO_HOST   = $(ASIO_SDK)/host
ASIO_PC     = $(ASIO_HOST)/pc
OBJ_DIR     = obj
BIN_DIR     = ../bin

#-------------------------------------------------------------------------------
# Compiler and Linker flags
#-------------------------------------------------------------------------------
CXXFLAGS = -Wall -O2 -std=c++11 \
           -I$(ASIO_COMMON) \
           -I$(ASIO_HOST) \
           -I$(ASIO_PC)

# Windows COM libraries required for ASIO
LDFLAGS  = -lm -lole32 -loleaut32 -lpopt

#-------------------------------------------------------------------------------
# Source files
#-------------------------------------------------------------------------------
MAIN_SRC = ab_acq_asio.cpp

# ASIO SDK sources needed for host application
ASIO_SOURCES = \
    $(ASIO_COMMON)/asio.cpp \
    $(ASIO_HOST)/asiodrivers.cpp \
    $(ASIO_PC)/asiolist.cpp

# Object files
MAIN_OBJ = $(OBJ_DIR)/ab_acq_asio.o
ASIO_OBJS = $(OBJ_DIR)/asio.o \
            $(OBJ_DIR)/asiodrivers.o \
            $(OBJ_DIR)/asiolist.o

ALL_OBJS = $(MAIN_OBJ) $(ASIO_OBJS)

#-------------------------------------------------------------------------------
# Target executable
#-------------------------------------------------------------------------------
TARGET = ab_acq_asio.exe

#-------------------------------------------------------------------------------
# Main target
#-------------------------------------------------------------------------------
all: $(BIN_DIR)/$(TARGET)

#-------------------------------------------------------------------------------
# Build executable
#-------------------------------------------------------------------------------
$(BIN_DIR)/$(TARGET): $(ALL_OBJS) | $(BIN_DIR)
	$(CXX) $(ALL_OBJS) $(LDFLAGS) -o $@
	@echo "Built: $@"

#-------------------------------------------------------------------------------
# Compile main source
#-------------------------------------------------------------------------------
$(OBJ_DIR)/ab_acq_asio.o: ab_acq_asio.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

#-------------------------------------------------------------------------------
# Compile ASIO SDK sources
#-------------------------------------------------------------------------------
$(OBJ_DIR)/asio.o: $(ASIO_COMMON)/asio.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/asiodrivers.o: $(ASIO_HOST)/asiodrivers.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/asiolist.o: $(ASIO_PC)/asiolist.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

#-------------------------------------------------------------------------------
# Create directories
#-------------------------------------------------------------------------------
$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

$(BIN_DIR):
	$(MKDIR) $(BIN_DIR)

#-------------------------------------------------------------------------------
# Clean build artifacts
#-------------------------------------------------------------------------------
clean:
	$(RM) $(OBJ_DIR)
	$(RM) $(BIN_DIR)/$(TARGET)
	@echo "Cleaned build artifacts"

#-------------------------------------------------------------------------------
# Install to audio-bench bin directory (already in ../bin)
#-------------------------------------------------------------------------------
install: all
	@echo "Binary already installed in $(BIN_DIR)"
	@echo "To install system-wide, run 'make install' from parent directory"

#-------------------------------------------------------------------------------
# Help
#-------------------------------------------------------------------------------
help:
	@echo "ab_acq_asio Makefile"
	@echo "Available targets:"
	@echo "  all       - Build ab_acq_asio.exe (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Note about installation"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - ASIO SDK in ASIOSDK/ directory"
	@echo "  - g++ compiler with C++11 support"
	@echo "  - Windows platform (MSYS2/MinGW)"

#-------------------------------------------------------------------------------
# Phony targets
#-------------------------------------------------------------------------------
.PHONY: all clean install help
