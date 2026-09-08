#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>

#include "ping.h"
#include "jobs.h"

static int is_nonneg_int(const char *s) {
    if (*s == '\0') return 0;
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }
    return 1;
}

void cmd_ping(int argc, char **argv) {
    if (argc != 2) {
        printf("ping: invalid syntax\n");
        return;
    }

    const char *target = argv[0];
    const char *sig_str = argv[1];

    if (!is_nonneg_int(sig_str)) {
        printf("ping: invalid syntax\n");
        return;
    }

    long typed_signal = strtol(sig_str, NULL, 10);
    int actual_signal = (int)(typed_signal % 64);

    if (target[0] == '%') {
        int job_number = atoi(target + 1);
        Job *j = jobs_find_by_number(job_number);
        if (!j) {
            printf("ping: no such process found\n");
            return;
        }
        kill(-j->pgid, actual_signal);
        printf("Sent signal %ld to %s\n", typed_signal, target);
    } else {
        if (!is_nonneg_int(target)) {
            printf("ping: no such process found\n");
            return;
        }
        pid_t pid = (pid_t)atol(target);
        Job *j = jobs_find_by_pid(pid);
        if (!j) {
            printf("ping: no such process found\n");
            return;
        }
        kill(pid, actual_signal);
        printf("Sent signal %ld to %d\n", typed_signal, (int)pid);
    }
}
