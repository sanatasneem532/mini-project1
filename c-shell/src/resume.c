#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "resume.h"
#include "jobs.h"

static int is_nonneg_int(const char *s) {
    if (*s == '\0') return 0;
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }
    return 1;
}

static int wait_with_timeout(pid_t pgid, int pid_count, int timeout_secs,
                              int *out_stopped, int *out_timed_out) {
    *out_stopped = 0;
    *out_timed_out = 0;

    if (timeout_secs >= 0) {
        jobs_alarm_fired_and_clear();
        alarm((unsigned)timeout_secs);
    }

    int remaining = pid_count;
    while (remaining > 0) {
        int status;
        pid_t w = waitpid(-pgid, &status, WUNTRACED);
        if (w < 0) {
            if (errno == EINTR) {
                if (timeout_secs >= 0 && jobs_alarm_fired_and_clear()) {
                    *out_timed_out = 1;
                    return 0;
                }
                continue;
            }
            break;
        }
        if (WIFSTOPPED(status)) {
            if (timeout_secs >= 0) alarm(0);
            *out_stopped = 1;
            return 0;
        }
        remaining--;
    }

    if (timeout_secs >= 0) alarm(0);
    return 0;
}

void cmd_resume(int argc, char **argv, pid_t shell_pgid) {
    if (argc < 2 || argv[0][0] != '%' || !is_nonneg_int(argv[0] + 1)) {
        printf("resume: invalid syntax\n");
        return;
    }

    int job_number = atoi(argv[0] + 1);
    int is_fg = (strcmp(argv[1], "fg") == 0);
    int is_bg = (strcmp(argv[1], "bg") == 0);

    if (!is_fg && !is_bg) {
        printf("resume: invalid syntax\n");
        return;
    }

    int timeout_secs = -1;

    if (is_bg) {
        if (argc != 2) {
            printf("resume: invalid syntax\n");
            return;
        }
    } else {
        if (argc == 2) {
            /* fg with no timeout */
        } else if (argc == 4 && strcmp(argv[2], "--timeout") == 0 && is_nonneg_int(argv[3])) {
            timeout_secs = atoi(argv[3]);
        } else {
            printf("resume: invalid syntax\n");
            return;
        }
    }

    Job *j = jobs_find_by_number(job_number);
    if (!j) {
        printf("resume: no such job\n");
        return;
    }

    pid_t pgid = j->pgid;
    int pid_count = j->pid_count;
    char command_line[MAX_CMDLINE_LEN];
    strncpy(command_line, j->command_line, sizeof(command_line) - 1);
    command_line[sizeof(command_line) - 1] = '\0';

    kill(-pgid, SIGCONT);
    jobs_mark_running(pgid);

    if (is_bg) {
        printf("[%d] + Running    %s\n", job_number, command_line);
        return;
    }

    printf("%s\n", command_line);
    tcsetpgrp(STDIN_FILENO, pgid);

    int stopped = 0, timed_out = 0;
    wait_with_timeout(pgid, pid_count, timeout_secs, &stopped, &timed_out);

    if (timed_out) {
        kill(-pgid, SIGTERM);
        printf("resume: job timed out\n");
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        jobs_remove_by_pgid(pgid);
        return;
    }

    tcsetpgrp(STDIN_FILENO, shell_pgid);

    if (stopped) {
        jobs_mark_stopped(pgid);
        printf("[%d] + Stopped    %s\n", job_number, command_line);
    } else {
        jobs_remove_by_pgid(pgid);
    }
}
