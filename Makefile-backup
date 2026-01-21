# Compiler settings
CC = gcc
CFLAGS = -Wall -Wpedantic -g -std=c11 -I./include

# Directories
SUBMOD_DIR = submodules
SRC_DIR = src
BIN_DIR = bin

# Include flags
CFLAGS += -I$(SUBMOD_DIR)/zoner/include

# Link flags
LDFLAGS = $(SUBMOD_DIR)/zoner/build/libzoner.a

# Source files
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

# Target executable
TARGET = game

# Build the game and Zoner
all: build run

build:
	mkdir -p $(BIN_DIR)/
	$(CC) $(CFLAGS) $(SRC_FILES) $(LDFLAGS) -o $(BIN_DIR)/$(TARGET)

# Clean up
clean:
	rm -rf $(BIN_DIR)/*

# Run the game
run:
	cd $(BIN_DIR)/ && ./$(TARGET)

.PHONY: build clean run
