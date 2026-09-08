#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_TRACKED_PIDS 16
#define MAX_NAME_LEN 32
#define MAX_CMDLINE_LEN 128

typedef enum {
    PROC_RUNNING,
    PROC_STOPPED
} ProcState;

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED
} JobOverallState;

typedef struct {
    int job_number;
    pid_t pgid;

    pid_t pids[MAX_TRACKED_PIDS];
    char pid_names[MAX_TRACKED_PIDS][MAX_NAME_LEN];
    ProcState pid_states[MAX_TRACKED_PIDS];
    int pid_count;
    int pids_alive;

    char command_name[MAX_NAME_LEN];
    char command_line[MAX_CMDLINE_LEN];

    JobOverallState state;
    int any_abnormal;
} Job;

void jobs_init(void);
void jobs_install_signal_handlers(void);

int jobs_add(pid_t pgid, const pid_t *pids, const char pid_names[][MAX_NAME_LEN],
             int pid_count, const char *command_name, const char *command_line);

void jobs_mark_stopped(pid_t pgid);
void jobs_mark_running(pid_t pgid);
void jobs_remove_by_pgid(pid_t pgid);

Job *jobs_find_by_number(int job_number);
Job *jobs_find_by_pgid(pid_t pgid);
Job *jobs_find_by_pid(pid_t pid);

int jobs_has_stopped(void);
void jobs_hangup_all(void);

int jobs_alarm_fired_and_clear(void);

void jobs_drain_notifications(void);

int jobs_count(void);
Job *jobs_get(int index);

#endif
