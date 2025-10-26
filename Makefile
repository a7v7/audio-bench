# Makefile for audio-bench

CC = gcc
CFLAGS = -Wall -O2 -std=c11
LDFLAGS = -lm -lsndfile -lfftw3

# Directories
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj

# Create directories if they don't exist
$(shell mkdir -p $(BIN_DIR) $(OBJ_DIR))

# Source files (add your C programs here)
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
BINARIES = $(SOURCES:$(SRC_DIR)/%.c=$(BIN_DIR)/%)

# Main target
all: $(BINARIES)

# Compile C files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files to binaries
$(BIN_DIR)/%: $(OBJ_DIR)/%.o
	$(CC) $< $(LDFLAGS) -o $@

# Clean build artifacts
clean:
	rm -rf $(BIN_DIR)/* $(OBJ_DIR)/*

# Install (copy binaries to system location)
install: all
	@echo "Installing binaries to /usr/local/bin (requires sudo)"
	sudo cp $(BIN_DIR)/* /usr/local/bin/

# Uninstall
uninstall:
	@echo "Removing binaries from /usr/local/bin"
	sudo rm -f /usr/local/bin/audio-*

# Help
help:
	@echo "audio-bench Makefile"
	@echo "Available targets:"
	@echo "  all       - Build all programs (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install binaries to /usr/local/bin"
	@echo "  uninstall - Remove installed binaries"
	@echo "  help      - Show this help message"

.PHONY: all clean install uninstall help
