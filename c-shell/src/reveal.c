#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#include "reveal.h"

static int compare_names(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static void reveal_list_dir(const char *dir_path, const char *prefix,
                             int show_all, int recursive) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    char **names = NULL;
    size_t count = 0, capacity = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        if (!show_all && name[0] == '.') continue;

        if (count == capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            names = realloc(names, capacity * sizeof(char *));
        }
        names[count++] = strdup(name);
    }
    closedir(dir);

    qsort(names, count, sizeof(char *), compare_names);

    for (size_t i = 0; i < count; i++) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, names[i]);

        struct stat st;
        int is_dir = (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode));

        printf("%s%s%s\n", prefix, names[i], (recursive && is_dir) ? "/" : "");

        if (recursive && is_dir) {
            char child_prefix[PATH_MAX];
            snprintf(child_prefix, sizeof(child_prefix), "%s%s/", prefix, names[i]);
            reveal_list_dir(full_path, child_prefix, show_all, recursive);
        }

        free(names[i]);
    }
    free(names);
}

void cmd_reveal(ShellState *state, int argc, char **argv) {
    int show_all = 0, recursive = 0;
    const char *target = NULL;

    for (int i = 0; i < argc; i++) {
        const char *tok = argv[i];
        size_t len = strlen(tok);

        int is_flag_group = (len > 1 && tok[0] == '-');

        if (is_flag_group) {
            for (size_t j = 1; j < len; j++) {
                if (tok[j] == 'a') show_all = 1;
                else if (tok[j] == 't') recursive = 1;
                else {
                    printf("reveal: invalid syntax\n");
                    return;
                }
            }
        } else {
            if (target != NULL) {
                printf("reveal: invalid syntax\n");
                return;
            }
            target = tok;
        }
    }

    char dir_path[PATH_MAX + 8];

    if (target == NULL || strcmp(target, ".") == 0) {
        if (getcwd(dir_path, sizeof(dir_path)) == NULL) {
            printf("reveal: no such directory\n");
            return;
        }
    } else if (strcmp(target, "~") == 0) {
        snprintf(dir_path, sizeof(dir_path), "%s", state->home);
    } else if (strcmp(target, "..") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            printf("reveal: no such directory\n");
            return;
        }
        snprintf(dir_path, sizeof(dir_path), "%s/..", cwd);
    } else if (strcmp(target, "-") == 0) {
        if (!state->has_prev) {
            printf("reveal: no such directory\n");
            return;
        }
        snprintf(dir_path, sizeof(dir_path), "%s", state->prev_cwd);
    } else {

        snprintf(dir_path, sizeof(dir_path), "%s", target);
    }

    struct stat st;
    if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("reveal: no such directory\n");
        return;
    }

    reveal_list_dir(dir_path, "", show_all, recursive);
}
