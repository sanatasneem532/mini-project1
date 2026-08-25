#ifndef EXECUTOR_H
#define EXECUTOR_H

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

void build_pipeline(const TokenList *tokens, Pipeline *out);

int prepare_pipeline_redirections(Pipeline *pipeline);

void run_pipeline(Pipeline *pipeline);

void free_pipeline(Pipeline *pipeline);

#endif
