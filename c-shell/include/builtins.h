#ifndef BUILTINS_H
#define BUILTINS_H

#include "shell.h"

int dispatch_builtin(ShellState *state, int argc, char **argv);

#endif
