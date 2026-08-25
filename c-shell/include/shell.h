#ifndef SHELL_H
#define SHELL_H

#include <limits.h>

#define MAX_INPUT_LEN 1024

typedef struct {
    char home[PATH_MAX];
    char username[256];
    char hostname[256];

    char prev_cwd[PATH_MAX];
    int has_prev;
} ShellState;

void init_shell_state(ShellState *state);

void print_prompt(const ShellState *state);

#endif
