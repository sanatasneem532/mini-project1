#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_TRACKED_PIDS 16

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED
} JobState;

typedef struct {
    int job_number;
    pid_t pgid;
    pid_t pids[MAX_TRACKED_PIDS];
    int pid_count;
    int pids_alive;
    char command_name[64];
    JobState state;
    int any_abnormal;
} Job;

void jobs_init(void);
void jobs_install_sigchld_handler(void);

int jobs_add(pid_t pgid, const pid_t *pids, int pid_count, const char *command_name);

void jobs_drain_notifications(void);

int jobs_count(void);
Job *jobs_get(int index);

#endif
