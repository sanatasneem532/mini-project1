#ifndef SHELL_H
#define SHELL_H
#include <limits.h>
#define MAX_INPUT_LEN 1024 //want to limit how long a command entered by the user can be

typedef struct {
    char home[PATH_MAX]; //create a character array large enough to hold a path
    char username[256];
    char hostname[256];
} ShellState;

//function declaration
void init_shell_state(ShellState *state);
void print_prompt(const ShellState *state);

#endif
