#ifndef BUILTINS_H
#define BUILTINS_H

#include <sys/types.h>
#include "shell.h"

int dispatch_builtin(ShellState *state, int argc, char **argv, pid_t shell_pgid);

#endif
