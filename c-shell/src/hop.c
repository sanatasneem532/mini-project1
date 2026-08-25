#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <limits.h>

#include "hop.h"

typedef struct {
    char path[PATH_MAX];
    double score;
    time_t last_visit;
} HopEntry;

typedef struct {
    HopEntry *items;
    size_t count;
    size_t capacity;
} HopDB;

static void db_init(HopDB *db) {
    db->items = NULL;
    db->count = 0;
    db->capacity = 0;
}

static void db_free(HopDB *db) {
    free(db->items);
    db->items = NULL;
    db->count = 0;
    db->capacity = 0;
}

static HopEntry *db_grow(HopDB *db) {
    if (db->count == db->capacity) {
        size_t new_cap = db->capacity == 0 ? 16 : db->capacity * 2;
        db->items = realloc(db->items, new_cap * sizeof(HopEntry));
        db->capacity = new_cap;
    }
    return &db->items[db->count++];
}

static void db_file_path(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, size, "%s/.cshell_hop_db", home);
}

static void db_load(HopDB *db) {
    db_init(db);
    char path[PATH_MAX];
    db_file_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[PATH_MAX + 64];
    while (fgets(line, sizeof(line), f)) {
        double score;
        long last;
        char p[PATH_MAX];

        if (sscanf(line, "%lf\t%ld\t%[^\n]", &score, &last, p) == 3) {
            HopEntry *e = db_grow(db);
            strncpy(e->path, p, sizeof(e->path) - 1);
            e->path[sizeof(e->path) - 1] = '\0';
            e->score = score;
            e->last_visit = (time_t)last;
        }
    }
    fclose(f);
}

static void db_save(const HopDB *db) {
    char path[PATH_MAX];
    db_file_path(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) return;
    for (size_t i = 0; i < db->count; i++) {
        fprintf(f, "%f\t%ld\t%s\n",
                db->items[i].score, (long)db->items[i].last_visit, db->items[i].path);
    }
    fclose(f);
}

static void db_touch(HopDB *db, const char *path) {
    time_t now = time(NULL);
    for (size_t i = 0; i < db->count; i++) {
        if (strcmp(db->items[i].path, path) == 0) {
            db->items[i].score += 1.0;
            db->items[i].last_visit = now;
            return;
        }
    }
    HopEntry *e = db_grow(db);
    strncpy(e->path, path, sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = '\0';
    e->score = 1.0;
    e->last_visit = now;
}

static double recency_weight(time_t last_visit, time_t now) {
    double elapsed = difftime(now, last_visit);
    if (elapsed < 3600.0)   return 4.0;
    if (elapsed < 86400.0)  return 2.0;
    if (elapsed < 604800.0) return 0.5;
    return 0.25;
}

static const char *db_best_match(HopDB *db, const char *substr) {
    time_t now = time(NULL);
    double best_weight = -1.0;
    const char *best_path = NULL;

    for (size_t i = 0; i < db->count; i++) {
        if (strstr(db->items[i].path, substr) == NULL) {
            continue;
        }
        struct stat st;
        if (stat(db->items[i].path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }
        double weight = db->items[i].score * recency_weight(db->items[i].last_visit, now);
        if (weight > best_weight) {
            best_weight = weight;
            best_path = db->items[i].path;
        }
    }
    return best_path;
}

static void remember_prev(ShellState *state, const char *dir_before_move) {
    strncpy(state->prev_cwd, dir_before_move, sizeof(state->prev_cwd) - 1);
    state->prev_cwd[sizeof(state->prev_cwd) - 1] = '\0';
    state->has_prev = 1;
}

static int hop_one(ShellState *state, HopDB *db, const char *tok) {
    char before[PATH_MAX];
    if (getcwd(before, sizeof(before)) == NULL) {
        before[0] = '\0';
    }

    if (strcmp(tok, ".") == 0) {
        return 0;
    }

    char target[PATH_MAX + 8];

    if (strcmp(tok, "~") == 0) {
        snprintf(target, sizeof(target), "%s", state->home);
    } else if (strcmp(tok, "..") == 0) {

        snprintf(target, sizeof(target), "%s/..", before);
    } else if (strcmp(tok, "-") == 0) {
        if (!state->has_prev) {
            return 0;
        }
        snprintf(target, sizeof(target), "%s", state->prev_cwd);
    } else {

        struct stat st;
        if (stat(tok, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(target, sizeof(target), "%s", tok);
        } else {

            const char *match = db_best_match(db, tok);
            if (!match) {
                printf("hop: no such directory\n");
                return 0;
            }
            snprintf(target, sizeof(target), "%s", match);
        }
    }

    if (chdir(target) != 0) {
        printf("hop: no such directory\n");
        return 0;
    }

    remember_prev(state, before);

    char after[PATH_MAX];
    if (getcwd(after, sizeof(after)) != NULL) {
        db_touch(db, after);
    }
    return 1;
}

void cmd_hop(ShellState *state, int argc, char **argv) {
    HopDB db;
    db_load(&db);

    if (argc == 0) {

        hop_one(state, &db, "~");
    } else {
        for (int i = 0; i < argc; i++) {
            hop_one(state, &db, argv[i]);
        }
    }

    db_save(&db);
    db_free(&db);
}
