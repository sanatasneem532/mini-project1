#include <stdio.h>

#include "activities.h"
#include "jobs.h"

void cmd_activities(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int n = jobs_count();
    for (int i = 0; i < n; i++) {
        Job *j = jobs_get(i);
        printf("[%d] pgid %d\n", j->job_number, (int)j->pgid);
        for (int p = 0; p < j->pid_count; p++) {
            if (j->pid_states[p] == PROC_RUNNING && j->pids_alive == -1) continue;
            printf("  %d %s %s\n",
                   (int)j->pids[p],
                   j->pid_names[p],
                   j->pid_states[p] == PROC_STOPPED ? "Stopped" : "Running");
        }
    }
}
