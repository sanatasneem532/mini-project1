#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <sys/types.h>
#include "lexer.h"

typedef struct {
    char *filename;
    int append;
} OutputRedir;

typedef struct {
    char **argv;
    int argc;

    char **input_files;
    int input_count;

    OutputRedir *outputs;
    int output_count;

    int prepared_stdin_fd;
    int *prepared_output_fds;
} Stage;

typedef struct {
    Stage *stages;
    int stage_count;
} Pipeline;

typedef struct {
    Pipeline pipeline;
    int background;
} CommandSegment;

typedef struct {
    CommandSegment *segments;
    int segment_count;
} CommandLine;

void build_pipeline(const TokenList *tokens, Pipeline *out);

void build_command_line(const TokenList *tokens, CommandLine *out);
void free_command_line(CommandLine *cl);

int prepare_pipeline_redirections(Pipeline *pipeline);

void run_pipeline(Pipeline *pipeline);

int launch_pipeline(Pipeline *pipeline, int background, pid_t *out_pgid, pid_t *out_pids);

int wait_for_pipeline(const pid_t *pids, int count);

int wait_foreground(pid_t pgid, const pid_t *pids, int count, int *out_stopped);

void free_pipeline(Pipeline *pipeline);

#endif

