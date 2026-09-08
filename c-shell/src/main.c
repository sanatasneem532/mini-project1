#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "shell.h"
#include "lexer.h"
#include "parser.h"
#include "builtins.h"
#include "executor.h"
#include "jobs.h"

static void block_sigchld(sigset_t *old) {
    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block, old);
}

static void restore_sigmask(const sigset_t *old) {
    sigprocmask(SIG_SETMASK, old, NULL);
}

static void build_command_line_text(const Pipeline *p, char *out, size_t outsize) {
    out[0] = '\0';
    size_t used = 0;
    for (int i = 0; i < p->stage_count; i++) {
        if (i > 0) {
            int n = snprintf(out + used, outsize - used, " | ");
            if (n > 0) used += (size_t)n;
        }
        for (int a = 0; a < p->stages[i].argc; a++) {
            int n = snprintf(out + used, outsize - used, "%s%s",
                              a > 0 ? " " : "", p->stages[i].argv[a]);
            if (n > 0) used += (size_t)n;
            if (used >= outsize) return;
        }
    }
}

static void build_pid_names(const Pipeline *p, int pid_count, char names[][MAX_NAME_LEN]) {
    for (int i = 0; i < pid_count; i++) {
        int stage_idx = (i < p->stage_count) ? i : p->stage_count - 1;
        strncpy(names[i], p->stages[stage_idx].argv[0], MAX_NAME_LEN - 1);
        names[i][MAX_NAME_LEN - 1] = '\0';
    }
}

static void run_foreground(Pipeline *pipeline, pid_t shell_pgid, int *not_found_out) {
    pid_t pids[32];
    pid_t pgid;
    sigset_t old;

    char cmdline[MAX_CMDLINE_LEN];
    build_command_line_text(pipeline, cmdline, sizeof(cmdline));

    block_sigchld(&old);
    int count = launch_pipeline(pipeline, 0, &pgid, pids);
    int not_found = 0;

    if (count > 0) {
        tcsetpgrp(STDIN_FILENO, pgid);

        int stopped = 0;
        not_found = wait_foreground(pgid, pids, count, &stopped);

        tcsetpgrp(STDIN_FILENO, shell_pgid);

        if (stopped) {
            char names[32][MAX_NAME_LEN];
            build_pid_names(pipeline, count, names);
            int job_number = jobs_add(pgid, pids, names, count,
                                       pipeline->stages[0].argv[0], cmdline);
            jobs_mark_stopped(pgid);
            printf("[%d] + Stopped    %s\n", job_number, cmdline);
            not_found = 0;
        }
    }
    restore_sigmask(&old);

    *not_found_out = not_found;
}

static void run_background(Pipeline *pipeline) {
    pid_t pids[32];
    pid_t pgid;
    sigset_t old;

    char cmdline[MAX_CMDLINE_LEN];
    build_command_line_text(pipeline, cmdline, sizeof(cmdline));

    block_sigchld(&old);
    int count = launch_pipeline(pipeline, 1, &pgid, pids);
    if (count > 0) {
        char names[32][MAX_NAME_LEN];
        build_pid_names(pipeline, count, names);
        int job_number = jobs_add(pgid, pids, names, count,
                                   pipeline->stages[0].argv[0], cmdline);
        printf("[%d] %d\n", job_number, (int)pgid);
    }
    restore_sigmask(&old);
}

int main(void) {
    setvbuf(stdin, NULL, _IONBF, 0);

    jobs_init();
    jobs_install_signal_handlers();

    pid_t shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    ShellState state;
    init_shell_state(&state);

    char line[MAX_INPUT_LEN + 2];
    int eof_warned = 0;

    while (1) {
        jobs_drain_notifications();
        print_prompt(&state);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            if (errno == EINTR) {
                clearerr(stdin);
                jobs_drain_notifications();
                continue;
            }

            if (jobs_has_stopped() && !eof_warned) {
                printf("\ncshell: there are stopped jobs\n");
                eof_warned = 1;
                clearerr(stdin);
                continue;
            }

            printf("\n");
            break;
        }

        eof_warned = 0;

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        TokenList tokens;
        if (tokenize(line, &tokens) != 0) {
            printf("cshell: invalid syntax\n");
            continue;
        }

        if (!parse_line(&tokens)) {
            printf("cshell: invalid syntax\n");
            tokenlist_free(&tokens);
            continue;
        }

        CommandLine cmdline;
        build_command_line(&tokens, &cmdline);

        for (int i = 0; i < cmdline.segment_count; i++) {
            CommandSegment *seg = &cmdline.segments[i];
            if (seg->pipeline.stage_count == 0 || seg->pipeline.stages[0].argc == 0) {
                continue;
            }

            if (!seg->background &&
                cmdline.segment_count == 1 &&
                seg->pipeline.stage_count == 1 &&
                seg->pipeline.stages[0].input_count == 0 &&
                seg->pipeline.stages[0].output_count == 0) {
                int handled = dispatch_builtin(&state, seg->pipeline.stages[0].argc,
                                                seg->pipeline.stages[0].argv, shell_pgid);
                if (handled) continue;
            }

            if (prepare_pipeline_redirections(&seg->pipeline) != 0) {
                if (!seg->background) break;
                continue;
            }

            if (seg->background) {
                run_background(&seg->pipeline);
            } else {
                int not_found = 0;
                run_foreground(&seg->pipeline, shell_pgid, &not_found);
                if (not_found) {
                    break;
                }
            }
        }

        free_command_line(&cmdline);
        tokenlist_free(&tokens);
    }

    jobs_hangup_all();
    return 0;
}
