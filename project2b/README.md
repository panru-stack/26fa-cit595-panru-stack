# penn-sh -- Project 2B (redirection + two-stage pipeline)

This directory contains the author's own implementation of `penn-sh` for
Project 2B, built on top of the author's Project 1 code.

## Files

- `penn-sh.c` -- the shell (prompt, tokenize, redirection, pipe, signals)
- `tokenizer.c` -- course-provided tokenizer (unmodified)
- `tokenizer.h` -- course-provided tokenizer header (unmodified)
- `Makefile` -- builds the `penn-sh` executable

## Build and run

    make
    ./penn-sh

## Supported features

- Prompt `penn-sh> `, commands found via `execvp` (PATH lookup).
- Input redirection: `cat < file`
- Output redirection: `echo hi > file` (creates or truncates the file)
- Combined redirection: `cat < in > out` (order does not matter)
- Two-stage pipeline: `cmd1 | cmd2` (both processes run concurrently)
- Pipeline plus redirection: `cat < in | head > out`
- Ctrl-C never exits the shell; children keep default SIGINT behavior.
- Ctrl-D (EOF) exits the shell; text followed by Ctrl-D is executed first.

## Error handling

All invalid inputs print a message containing the keyword "invalid" and the
shell re-prompts without executing anything (no crash, no undefined
behavior):

- `ls > out1 > out2` -> Invalid: Multiple standard output redirects
- `cat < in1 < in2` -> Invalid: Multiple standard input redirects or redirect in invalid location
- `cat > out < missing` -> Invalid standard input redirect: No such file or directory
- `cat > out | head > x` -> Invalid output redirection
- `cmd | head < x` -> Invalid input redirection
- `a | b | c` -> Invalid: Multiple pipes

## Notes

- Only the functions listed in the Project 2 handout are used, plus the
  tokenizer functions from the course-provided header.
- Tested with valgrind (course VM) and/or AddressSanitizer; no leaks or
  memory errors.
