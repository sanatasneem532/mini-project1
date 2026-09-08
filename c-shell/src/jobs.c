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
static volatile sig_atomic_t alarm_fired = 0;

int jobs_alarm_fired_and_clear(void) {
    int v = alarm_fired;
    alarm_fired = 0;
    return v;
}

void jobs_init(void) {
    job_count = 0;
    next_job_number = 1;
    pending_count = 0;
}

int jobs_count(void) { return job_count; }
Job *jobs_get(int index) { return &jobs[index]; }

static void empty_handler(int signo) {
    (void)signo;
}

static void sigchld_handler(int signo) {
    (void)signo;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (int i = 0; i < job_count; i++) {
            for (int j = 0; j < jobs[i].pid_count; j++) {
                if (jobs[i].pids[j] != pid) continue;

                if (WIFSTOPPED(status)) {
                    jobs[i].pid_states[j] = PROC_STOPPED;
                } else if (WIFCONTINUED(status)) {
                    jobs[i].pid_states[j] = PROC_RUNNING;
                } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    jobs[i].pids_alive--;
                    if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
                        jobs[i].any_abnormal = 1;
                    }
                    if (jobs[i].pids_alive == 0 && jobs[i].state != JOB_STOPPED) {
                        if (pending_count < MAX_JOBS) {
                            pending_jobs[pending_count] = jobs[i];
                            pending_count++;
                        }
                        jobs[i].pids_alive = -1;
                    }
                }
                goto next_pid;
            }
        }
        next_pid:;
    }
}

static void alarm_handler(int signo) {
    (void)signo;
    alarm_fired = 1;
}

void jobs_install_signal_handlers(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = empty_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

int jobs_add(pid_t pgid, const pid_t *pids, const char pid_names[][MAX_NAME_LEN],
             int pid_count, const char *command_name, const char *command_line) {
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
        j->pid_states[i] = PROC_RUNNING;
        strncpy(j->pid_names[i], pid_names[i], MAX_NAME_LEN - 1);
        j->pid_names[i][MAX_NAME_LEN - 1] = '\0';
    }
    j->pids_alive = pid_count;
    strncpy(j->command_name, command_name, MAX_NAME_LEN - 1);
    j->command_name[MAX_NAME_LEN - 1] = '\0';
    strncpy(j->command_line, command_line, MAX_CMDLINE_LEN - 1);
    j->command_line[MAX_CMDLINE_LEN - 1] = '\0';
    j->state = JOB_RUNNING;
    j->any_abnormal = 0;

    sigprocmask(SIG_SETMASK, &old, NULL);
    return assigned;
}

Job *jobs_find_by_number(int job_number) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_number == job_number) return &jobs[i];
    }
    return NULL;
}

Job *jobs_find_by_pgid(pid_t pgid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pgid == pgid) return &jobs[i];
    }
    return NULL;
}

Job *jobs_find_by_pid(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        for (int j = 0; j < jobs[i].pid_count; j++) {
            if (jobs[i].pids[j] == pid) return &jobs[i];
        }
    }
    return NULL;
}

void jobs_mark_stopped(pid_t pgid) {
    Job *j = jobs_find_by_pgid(pgid);
    if (!j) return;
    j->state = JOB_STOPPED;
    for (int i = 0; i < j->pid_count; i++) {
        j->pid_states[i] = PROC_STOPPED;
    }
}

void jobs_mark_running(pid_t pgid) {
    Job *j = jobs_find_by_pgid(pgid);
    if (!j) return;
    j->state = JOB_RUNNING;
    for (int i = 0; i < j->pid_count; i++) {
        j->pid_states[i] = PROC_RUNNING;
    }
}

void jobs_remove_by_pgid(pid_t pgid) {
    sigset_t block, old;
    sigemptyset(&block);
    sigaddset(&block, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block, &old);

    int w = 0;
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pgid != pgid) {
            jobs[w++] = jobs[i];
        }
    }
    job_count = w;

    sigprocmask(SIG_SETMASK, &old, NULL);
}

int jobs_has_stopped(void) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].state == JOB_STOPPED) return 1;
    }
    return 0;
}

void jobs_hangup_all(void) {
    for (int i = 0; i < job_count; i++) {
        kill(-jobs[i].pgid, SIGHUP);
    }
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
