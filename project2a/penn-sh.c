/* penn-sh.c -- Project 2A: a shell with input/output redirection.
 *
 * Based on the author's own Project 1 (penn-shredder) code.
 *
 * Only the system calls / library functions listed in the Project 2 handout
 * are used, plus the course-provided tokenizer (tokenizer.c / tokenizer.h,
 * which are unmodified).
 *
 * Behavior:
 *   - prompts with "penn-sh> " and reads a line of input (read(2));
 *   - tokenizes the line with the course tokenizer;
 *   - supports <in and >out redirection;
 *   - never exits on SIGINT; children run with default SIGINT disposition;
 *   - exits on EOF (Ctrl-D).
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "tokenizer.h"

/* Max bytes of user input, including the null terminator. */
#define INPUT_SIZE 1024

/* The input is at most INPUT_SIZE bytes, so it can never produce more than
 * INPUT_SIZE tokens (each token consumes at least one byte of input).
 */
#define MAX_TOKENS INPUT_SIZE

/* Global variable (the only global in the program), mirroring Project 1.
 * It lets the SIGINT handler find the running child.  It is reset to zero
 * once the child has been waited on.
 */
pid_t childPid = 0;

/* Function prototypes. */
void registerSignalHandlers();
void sigintHandler(int sig);
void executeShell();
char **getCommandFromInput();
void writeToStdout(char *text);
int countTokens(char **tokens);
int parseCommand(char **tokens, int start, int end, char ***args,
                 char **inFile, char **outFile);
void redirectionsSTDINtoFile(char *fileName);
void redirectionsSTDOUTtoFile(char *fileName);
void waitForChild(pid_t *childPid);
void freeCommandArray(char **commandArray);

/* Kills the given child process with SIGKILL. */
void killChildProcess(pid_t childPid) {
    if (kill(childPid, SIGKILL) == -1) {
        perror("Invalid kill");
        exit(EXIT_FAILURE);
    }
}

/* SIGINT handler for the shell itself.
 *
 * The shell must never exit because of Ctrl-C.  If a child is running, the
 * shell kills it; the child also responds to the terminal's SIGINT with its
 * default behavior (the child resets SIGINT to SIG_DFL before execvp).
 */
void sigintHandler(int sig) {
    if (childPid != 0) {
        killChildProcess(childPid);
    }
}

/* Registers the SIGINT handler.  No alarm is used in Project 2. */
void registerSignalHandlers() {
    if (signal(SIGINT, sigintHandler) == SIG_ERR) {
        perror("Invalid signal");
        exit(EXIT_FAILURE);
    }
}

/* Writes text to standard output, exiting on failure. */
void writeToStdout(char *text) {
    if (write(STDOUT_FILENO, text, strlen(text)) == -1) {
        perror("Invalid write");
        exit(EXIT_FAILURE);
    }
}

/* Reads one line of input from standard input and tokenizes it.
 *
 * Returns a NULL-terminated array of malloc'd token strings, or NULL if the
 * line contained no tokens (empty line / Ctrl-C at the prompt).
 *
 * On EOF (Ctrl-D) the shell exits.
 */
char **getCommandFromInput() {
    char buffer[INPUT_SIZE];
    ssize_t bytesRead;
    TOKENIZER *tokenizer;
    char **commandArray;
    char *tok;
    int count = 0;

    bytesRead = read(STDIN_FILENO, buffer, INPUT_SIZE - 1);

    if (bytesRead < 0) {
        perror("Invalid read");
        exit(EXIT_FAILURE);
    }

    if (bytesRead == 0) {
        /* Ctrl-D (EOF): exit the shell. */
        writeToStdout("\n");
        exit(EXIT_SUCCESS);
    }

    buffer[bytesRead] = '\0';

    tokenizer = init_tokenizer(buffer);
    commandArray = calloc(MAX_TOKENS + 1, sizeof(char *));
    if (commandArray == NULL) {
        perror("Invalid malloc");
        exit(EXIT_FAILURE);
    }

    while ((tok = get_next_token(tokenizer)) != NULL) {
        /* Safety valve; cannot trigger with INPUT_SIZE-byte input. */
        if (count >= MAX_TOKENS) {
            free(tok);
            break;
        }
        commandArray[count++] = tok;
    }
    commandArray[count] = NULL;

    free_tokenizer(tokenizer);

    if (count == 0) {
        free(commandArray);
        return NULL;
    }

    return commandArray;
}

/* Frees every token string and the array itself. */
void freeCommandArray(char **commandArray) {
    int i;

    for (i = 0; commandArray[i] != NULL; i++) {
        free(commandArray[i]);
    }
    free(commandArray);
}

/* Returns the number of tokens (excluding the NULL terminator). */
int countTokens(char **tokens) {
    int count = 0;

    while (tokens[count] != NULL) {
        count++;
    }
    return count;
}

/* Parses the tokens in the range [start, end) into an argument list and
 * (optionally) an input file and an output file.
 *
 * On success *args is a NULL-terminated array of pointers into `tokens`
 * (the strings themselves are owned by the caller) and the function
 * returns 0.  On invalid input it prints an error containing the keyword
 * "invalid" and returns -1 (no command is executed).
 */
int parseCommand(char **tokens, int start, int end, char ***args,
                 char **inFile, char **outFile) {
    int argc = 0;
    int inCount = 0;
    int outCount = 0;
    int i;

    *inFile = NULL;
    *outFile = NULL;

    /* end - start + 1 is a safe upper bound on the number of arguments. */
    *args = calloc(end - start + 1, sizeof(char *));
    if (*args == NULL) {
        perror("Invalid malloc");
        exit(EXIT_FAILURE);
    }

    for (i = start; i < end; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            if (inCount >= 1) {
                writeToStdout("Invalid: Multiple standard input redirects or redirect in invalid location\n");
                return -1;
            }
            if (i + 1 >= end) {
                writeToStdout("Invalid: redirect in invalid location\n");
                return -1;
            }
            *inFile = tokens[i + 1];
            inCount++;
            i++; /* skip the file name */
        } else if (strcmp(tokens[i], ">") == 0) {
            if (outCount >= 1) {
                writeToStdout("Invalid: Multiple standard output redirects\n");
                return -1;
            }
            if (i + 1 >= end) {
                writeToStdout("Invalid: redirect in invalid location\n");
                return -1;
            }
            *outFile = tokens[i + 1];
            outCount++;
            i++; /* skip the file name */
        } else {
            (*args)[argc++] = tokens[i];
        }
    }

    if (argc == 0) {
        writeToStdout("Invalid: missing command\n");
        return -1;
    }

    (*args)[argc] = NULL;
    return 0;
}

/* Redirects standard input to the named file (child process only). */
void redirectionsSTDINtoFile(char *fileName) {
    int fd = open(fileName, O_RDONLY);

    if (fd < 0) {
        perror("Invalid standard input redirect");
        exit(EXIT_FAILURE);
    }
    if (dup2(fd, STDIN_FILENO) == -1) {
        perror("Invalid dup2");
        exit(EXIT_FAILURE);
    }
    close(fd);
}

/* Redirects standard output to the named file, creating or truncating it
 * (child process only).
 */
void redirectionsSTDOUTtoFile(char *fileName) {
    int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        perror("Invalid standard output redirect");
        exit(EXIT_FAILURE);
    }
    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("Invalid dup2");
        exit(EXIT_FAILURE);
    }
    close(fd);
}

/* Waits for the child identified by *childPid to terminate and resets the
 * global to zero once it has been reaped (so it is never a zombie).
 */
void waitForChild(pid_t *childPid) {
    int status;

    do {
        if (waitpid(*childPid, &status, 0) == -1) {
            perror("Invalid waitpid");
            exit(EXIT_FAILURE);
        }
        /* Loop while the child was stopped but not yet terminated. */
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    *childPid = 0;
}

/* Runs a single command with optional < and > redirection. */
void executeShell() {
    char **commandArray = getCommandFromInput();
    char **args;
    char *inFile;
    char *outFile;
    int n;

    if (commandArray == NULL) {
        return; /* empty line or Ctrl-C at the prompt: re-prompt */
    }

    n = countTokens(commandArray);

    if (parseCommand(commandArray, 0, n, &args, &inFile, &outFile) == -1) {
        free(args);
        freeCommandArray(commandArray);
        return;
    }

    childPid = fork();

    if (childPid < 0) {
        perror("Invalid fork");
        exit(EXIT_FAILURE);
    }

    if (childPid == 0) {
        /* Child: restore the default SIGINT behavior (after execvp the
         * child's disposition is default anyway, so Ctrl-C terminates it).
         */
        signal(SIGINT, SIG_DFL);

        if (inFile != NULL) {
            redirectionsSTDINtoFile(inFile);
        }
        if (outFile != NULL) {
            redirectionsSTDOUTtoFile(outFile);
        }

        execvp(args[0], args);
        perror("Invalid execvp");
        exit(EXIT_FAILURE);
    }

    /* Parent: wait for the child and clean up. */
    waitForChild(&childPid);
    free(args);
    freeCommandArray(commandArray);
}

/* The shell never exits on its own except on EOF (Ctrl-D). */
int main() {
    registerSignalHandlers();

    while (1) {
        writeToStdout("penn-sh> ");
        executeShell();
    }

    return 0;
}
