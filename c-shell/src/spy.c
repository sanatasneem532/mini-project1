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
#include <signal.h>

#include "spy.h"

#if defined(__linux__)
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
#endif

static int is_num(const char *s) {
    if (!s || !*s) return 0;
    for (const char *x = s; *x; x++) {
        if (!isdigit((unsigned char)*x)) return 0;
    }
    return 1;
}

static int pid_exists(int p) {
#if defined(__linux__)
    char b[64];
    snprintf(b, sizeof(b), "/proc/%d", p);
    return access(b, F_OK) == 0;
#else
    if (p <= 0) return 0;
    errno = 0;
    int r = kill((pid_t)p, 0);
    if (r == 0) return 1;
    if (errno == EPERM) return 1;
    return 0;
#endif
}

#if defined(__linux__)
static void spy_linux(int p) {
    char b[PATH_MAX + 32];
    char pbuf[PATH_MAX];
    struct stat st;
    ssize_t sz;

    printf("PID FD TYPE PATH\n");

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
                if (strcmp(mems[i], sp) == 0) { dup = 1; break; }
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
                if (fds[i] > fds[j]) { int tmp = fds[i]; fds[i] = fds[j]; fds[j] = tmp; }
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
#else
static void spy_macos(int p) {
    printf("PID FD TYPE PATH\n");

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "lsof -p %d 2>/dev/null", p);
    FILE *f = popen(cmd, "r");
    if (!f) return;

    char line[PATH_MAX + 256];
    int hdr = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!hdr) { hdr = 1; continue; }
        char fd_s[32], type_s[32], path_s[PATH_MAX];
        fd_s[0] = type_s[0] = path_s[0] = '\0';
        if (sscanf(line, "%*s %*d %*s %31s %31s %*s %*s %*s %[^\n]", fd_s, type_s, path_s) < 2) continue;
        if (path_s[0] == '\0') continue;

        const char *t = "REG";
        if (strcmp(type_s, "DIR") == 0) t = "DIR";
        else if (strcmp(type_s, "CHR") == 0) t = "CHR";
        else if (strcmp(type_s, "FIFO") == 0) t = "FIFO";
        else if (strcmp(type_s, "unix") == 0) t = "SOCK";

        printf("%d %s %s %s\n", p, fd_s, t, path_s);
    }
    pclose(f);
}
#endif

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

    if (!pid_exists(p)) {
        printf("spy: no such process\n");
        return;
    }

#if defined(__linux__)
    spy_linux(p);
#else
    spy_macos(p);
#endif
}
