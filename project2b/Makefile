# Define variable CC to be the compiler we want to use
CC = clang

# Define CFLAGS for the flags we will want to use with clang
# -g = debugging flag to enable gdb
# -Wall = show compilation warnings
CFLAGS = -g -Wall

# Define TARGETS to be the targets to be run when calling 'make all'
TARGETS = penn-sh

# Define SRCS to be the source files needed to compile 'penn-sh'
# (the header is listed as a prerequisite so rebuilds trigger when it changes,
#  but only .c files are passed to the compiler)
SRCS = tokenizer.c penn-sh.c

# Define PHONY targets to prevent make from confusing the phony target with the same file names
.PHONY: clean all

# If no arguments are passed to make, it will attempt the 'penn-sh' target
default: penn-sh

# This runs the targets listed in the TARGETS variable when 'make all' is run
all: $(TARGETS)

# List of targets
# $^ = names of all the prerequisites, with spaces between them
# $@ = complete name of the target
penn-sh: $(SRCS) tokenizer.h
	$(CC) $(CFLAGS) $(SRCS) -o $@

# $(RM) is the platform agnostic way to delete a file (here rm -f)
clean:
	$(RM) penn-sh
