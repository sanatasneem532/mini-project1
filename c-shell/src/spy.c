#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>

#include "spy.h"

static const char *get_t(mode_t m) {
    if (S_ISREG(m)) return "REG";
    if (S_ISDIR(m)) return "DIR";
    if (S_ISCHR(m)) return "CHR";
    if (S_ISBLK(m)) return "BLK";
    if (S_ISFIFO(m)) return "FIFO";
    if (S_ISSOCK(m)) return "SOCK";
    if (S_ISLNK(m)) return "LNK";
    return "UNKNOWN";
}

static int is_num(const char *s) {
    if (!s || !*s) return 0;
    for (const char *x = s; *x; x++) {
        if (!isdigit((unsigned char)*x)) return 0;
    }
    return 1;
}

void cmd_spy(int argc, char **argv) {
    if (argc > 1) {
        printf("spy: invalid syntax\n");
        return;
    }

    int p;
    if (argc == 0) {
        p = (int)getpid();
    } else {
        if (!is_num(argv[0])) {
            printf("spy: no such process\n");
            return;
        }
        p = atoi(argv[0]);
    }

    char b[PATH_MAX + 32];
    snprintf(b, sizeof(b), "/proc/%d", p);
    if (access(b, F_OK) != 0) {
        printf("spy: no such process\n");
        return;
    }

    printf("PID FD TYPE PATH\n");

    char pbuf[PATH_MAX];
    struct stat st;
    ssize_t sz;

    snprintf(b, sizeof(b), "/proc/%d/cwd", p);
    sz = readlink(b, pbuf, sizeof(pbuf) - 1);
    if (sz > 0) {
        pbuf[sz] = '\0';
        const char *t = "DIR";
        if (stat(b, &st) == 0) t = get_t(st.st_mode);
        printf("%d cwd %s %s\n", p, t, pbuf);
    }

    snprintf(b, sizeof(b), "/proc/%d/exe", p);
    sz = readlink(b, pbuf, sizeof(pbuf) - 1);
    if (sz > 0) {
        pbuf[sz] = '\0';
        const char *t = "REG";
        if (stat(b, &st) == 0) t = get_t(st.st_mode);
        printf("%d txt %s %s\n", p, t, pbuf);
    }

    snprintf(b, sizeof(b), "/proc/%d/maps", p);
    FILE *f = fopen(b, "r");
    if (f) {
        char l[1024];
        char mems[512][PATH_MAX];
        int mcnt = 0;
        while (fgets(l, sizeof(l), f)) {
            char *sp = strchr(l, '/');
            if (!sp) continue;
            char *nl = strchr(sp, '\n');
            if (nl) *nl = '\0';
            int dup = 0;
            for (int i = 0; i < mcnt; i++) {
                if (strcmp(mems[i], sp) == 0) {
                    dup = 1;
                    break;
                }
            }
            if (!dup && mcnt < 512) {
                strncpy(mems[mcnt], sp, PATH_MAX - 1);
                mems[mcnt][PATH_MAX - 1] = '\0';
                mcnt++;
                const char *t = "REG";
                if (stat(sp, &st) == 0) t = get_t(st.st_mode);
                printf("%d mem %s %s\n", p, t, sp);
            }
        }
        fclose(f);
    }

    snprintf(b, sizeof(b), "/proc/%d/fd", p);
    DIR *d = opendir(b);
    if (d) {
        struct dirent *e;
        int fds[1024];
        int fdc = 0;
        while ((e = readdir(d))) {
            if (is_num(e->d_name) && fdc < 1024) {
                fds[fdc++] = atoi(e->d_name);
            }
        }
        closedir(d);

        for (int i = 0; i < fdc; i++) {
            for (int j = i + 1; j < fdc; j++) {
                if (fds[i] > fds[j]) {
                    int tmp = fds[i];
                    fds[i] = fds[j];
                    fds[j] = tmp;
                }
            }
        }

        for (int i = 0; i < fdc; i++) {
            snprintf(b, sizeof(b), "/proc/%d/fd/%d", p, fds[i]);
            sz = readlink(b, pbuf, sizeof(pbuf) - 1);
            if (sz > 0) {
                pbuf[sz] = '\0';
                const char *t = "UNKNOWN";
                if (stat(b, &st) == 0) {
                    t = get_t(st.st_mode);
                } else if (strncmp(pbuf, "socket:", 7) == 0) {
                    t = "SOCK";
                } else if (strncmp(pbuf, "pipe:", 5) == 0) {
                    t = "FIFO";
                }
                printf("%d %d %s %s\n", p, fds[i], t, pbuf);
            }
        }
    }
}
