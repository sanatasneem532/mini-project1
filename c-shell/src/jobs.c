#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#include "jobs.h"

#define MAX_JOBS 256

static Job jobs[MAX_JOBS];
static int job_count = 0;
static int next_job_number = 1;

static Job pending_jobs[MAX_JOBS];
static volatile sig_atomic_t pending_count = 0;

void jobs_init(void) {
    job_count = 0;
    next_job_number = 1;
    pending_count = 0;
}

int jobs_count(void) {
    return job_count;
}

Job *jobs_get(int index) {
    return &jobs[index];
}

static void sigchld_handler(int signo) {
    (void)signo;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].state != JOB_RUNNING) continue;
            for (int j = 0; j < jobs[i].pid_count; j++) {
                if (jobs[i].pids[j] == pid) {
                    jobs[i].pids_alive--;
                    if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
                        jobs[i].any_abnormal = 1;
                    }
                    if (jobs[i].pids_alive == 0) {
                        if (pending_count < MAX_JOBS) {
                            pending_jobs[pending_count] = jobs[i];
                            pending_count++;
                        }
                        jobs[i].state = JOB_STOPPED;
                        jobs[i].pids_alive = -1;
                    }
                    break;
                }
            }
        }
    }
}

void jobs_install_sigchld_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);
}

int jobs_add(pid_t pgid, const pid_t *pids, int pid_count, const char *command_name) {
    sigset_t block, old;
    sigemptyset(&block);
    sigaddset(&block, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block, &old);

    int assigned = next_job_number++;
    Job *j = &jobs[job_count++];
    j->job_number = assigned;
    j->pgid = pgid;
    j->pid_count = pid_count;
    for (int i = 0; i < pid_count; i++) {
        j->pids[i] = pids[i];
    }
    j->pids_alive = pid_count;
    strncpy(j->command_name, command_name, sizeof(j->command_name) - 1);
    j->command_name[sizeof(j->command_name) - 1] = '\0';
    j->state = JOB_RUNNING;
    j->any_abnormal = 0;

    sigprocmask(SIG_SETMASK, &old, NULL);
    return assigned;
}

void jobs_drain_notifications(void) {
    sigset_t block, old;
    sigemptyset(&block);
    sigaddset(&block, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block, &old);

    int n = pending_count;
    Job local[MAX_JOBS];
    for (int i = 0; i < n; i++) {
        local[i] = pending_jobs[i];
    }
    pending_count = 0;

    int w = 0;
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pids_alive != -1) {
            jobs[w++] = jobs[i];
        }
    }
    job_count = w;

    sigprocmask(SIG_SETMASK, &old, NULL);

    for (int i = 0; i < n; i++) {
        printf("%s with pid %d exited %s\n",
               local[i].command_name,
               (int)local[i].pgid,
               local[i].any_abnormal ? "abnormally" : "normally");
    }
}
