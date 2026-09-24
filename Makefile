CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror

.PHONY: all test clean
all: framewatch

framewatch: src/framewatch.c
	$(CC) $(CFLAGS) -o $@ $<

test: framewatch
	python3 -m unittest discover -s tests -v

clean:
	rm -f framewatch
