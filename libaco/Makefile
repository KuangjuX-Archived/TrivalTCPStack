CC = gcc
CFLAGS = -g -O2 -Wall -Werror
OUTPUT_DIR = ../build

PHONY: build
build:
	@$(CC) -c acosw.S -o $(OUTPUT_DIR)/acosw.o $(CFLAGS)
	@$(CC) -c aco.c -o $(OUTPUT_DIR)/aco.o $(CFLAGS)

