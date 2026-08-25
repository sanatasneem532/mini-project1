#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>

#include "shell.h"

void init_shell_state(ShellState *state) {

    if (getcwd(state->home, sizeof(state->home)) == NULL) {
        snprintf(state->home, sizeof(state->home), "/");
    }

    struct passwd *pw = getpwuid(getuid());
    if (pw != NULL && pw->pw_name != NULL) {
        strncpy(state->username, pw->pw_name, sizeof(state->username) - 1);
        state->username[sizeof(state->username) - 1] = '\0';
    } else {
        snprintf(state->username, sizeof(state->username), "user");
    }

    if (gethostname(state->hostname, sizeof(state->hostname)) != 0) {
        snprintf(state->hostname, sizeof(state->hostname), "host");
    }

    state->prev_cwd[0] = '\0';
    state->has_prev = 0;
}

void print_prompt(const ShellState *state) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        snprintf(cwd, sizeof(cwd), "?");
    }

    char display[PATH_MAX + 2];
    size_t home_len = strlen(state->home);

    int cwd_is_under_home =
        strncmp(cwd, state->home, home_len) == 0 &&
        (cwd[home_len] == '\0' || cwd[home_len] == '/');

    if (cwd_is_under_home) {
        if (cwd[home_len] == '\0') {
            snprintf(display, sizeof(display), "~");
        } else {
            snprintf(display, sizeof(display), "~%s", cwd + home_len);
        }
    } else {
        snprintf(display, sizeof(display), "%s", cwd);
    }

    printf("<%s@%s:%s> ", state->username, state->hostname, display);
}
