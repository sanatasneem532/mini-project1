#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

#include "locate.h"

static int is_executable_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode)) return 0;
    return access(path, X_OK) == 0;
}

static void print_match(const char *candidate) {
    char resolved[PATH_MAX];
    if (realpath(candidate, resolved) != NULL) {
        printf("%s\n", resolved);
    } else {
        printf("%s\n", candidate);
    }
}

static void locate_one(const char *name) {
    int found_any = 0;

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        char candidate[PATH_MAX * 2];
        snprintf(candidate, sizeof(candidate), "%s/%s", cwd, name);
        if (is_executable_file(candidate)) {
            print_match(candidate);
            found_any = 1;
        }
    }

    const char *path_env = getenv("PATH");
    if (path_env != NULL) {
        char *path_copy = strdup(path_env);
        char *saveptr = NULL;
        char *dir = strtok_r(path_copy, ":", &saveptr);

        while (dir != NULL) {
            char candidate[PATH_MAX * 2];
            snprintf(candidate, sizeof(candidate), "%s/%s", dir, name);
            if (is_executable_file(candidate)) {
                print_match(candidate);
                found_any = 1;
            }
            dir = strtok_r(NULL, ":", &saveptr);
        }
        free(path_copy);
    }

    if (!found_any) {
        printf("locate: command not found (%s)\n", name);
    }
}

void cmd_locate(int argc, char **argv) {
    if (argc == 0) {
        printf("locate: invalid syntax\n");
        return;
    }
    for (int i = 0; i < argc; i++) {
        locate_one(argv[i]);
    }
}
