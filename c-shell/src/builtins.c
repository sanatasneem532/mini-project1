#include <string.h>

#include "builtins.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"

int dispatch_builtin(ShellState *state, int argc, char **argv) {
    if (argc == 0) {
        return 0;
    }

    if (strcmp(argv[0], "hop") == 0) {
        cmd_hop(state, argc - 1, argv + 1);
        return 1;
    }
    if (strcmp(argv[0], "reveal") == 0) {
        cmd_reveal(state, argc - 1, argv + 1);
        return 1;
    }
    if (strcmp(argv[0], "peek") == 0) {
        cmd_peek(argc - 1, argv + 1);
        return 1;
    }
    if (strcmp(argv[0], "locate") == 0) {
        cmd_locate(argc - 1, argv + 1);
        return 1;
    }

    return 0;
}
