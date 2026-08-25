#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>

#include "executor.h"

#define COPY_CHUNK 4096

static void stage_init(Stage *s) {
    s->argv = NULL;
    s->argc = 0;
    s->input_files = NULL;
    s->input_count = 0;
    s->outputs = NULL;
    s->output_count = 0;
    s->prepared_stdin_fd = -1;
    s->prepared_output_fds = NULL;
}

static void stage_add_word(Stage *s, char *word) {

    s->argv = realloc(s->argv, sizeof(char *) * (size_t)(s->argc + 2));
    s->argv[s->argc] = word;
    s->argc++;
    s->argv[s->argc] = NULL;
}

static void stage_add_input(Stage *s, char *filename) {
    s->input_files = realloc(s->input_files, sizeof(char *) * (size_t)(s->input_count + 1));
    s->input_files[s->input_count++] = filename;
}

static void stage_add_output(Stage *s, char *filename, int append) {
    s->outputs = realloc(s->outputs, sizeof(OutputRedir) * (size_t)(s->output_count + 1));
    s->outputs[s->output_count].filename = filename;
    s->outputs[s->output_count].append = append;
    s->output_count++;
}

static void pipeline_add_stage(Pipeline *p, Stage stage) {
    p->stages = realloc(p->stages, sizeof(Stage) * (size_t)(p->stage_count + 1));
    p->stages[p->stage_count] = stage;
    p->stage_count++;
}

void build_pipeline(const TokenList *tokens, Pipeline *out) {
    out->stages = NULL;
    out->stage_count = 0;

    Stage current;
    stage_init(&current);

    size_t i = 0;
    while (i < tokens->count) {
        TokenType t = tokens->items[i].type;

        if (t == TOKEN_WORD) {
            stage_add_word(&current, tokens->items[i].value);
            i++;

        } else if (t == TOKEN_OP_LT) {
            i++;
            stage_add_input(&current, tokens->items[i].value);
            i++;

        } else if (t == TOKEN_OP_GT || t == TOKEN_OP_GTGT) {
            int append = (t == TOKEN_OP_GTGT);
            i++;
            stage_add_output(&current, tokens->items[i].value, append);
            i++;

        } else if (t == TOKEN_OP_PIPE) {
            pipeline_add_stage(out, current);
            stage_init(&current);
            i++;

        } else {

            break;
        }
    }

    pipeline_add_stage(out, current);
}

static int build_concatenated_stdin(char **files, int count) {
    char template[] = "/tmp/cshell_stdin_XXXXXX";
    int tmp_fd = mkstemp(template);
    if (tmp_fd < 0) {
        return -1;
    }

    unlink(template);

    for (int i = 0; i < count; i++) {
        int in_fd = open(files[i], O_RDONLY);
        if (in_fd < 0) {
            close(tmp_fd);
            return -1;
        }
        char buf[COPY_CHUNK];
        ssize_t n;
        while ((n = read(in_fd, buf, sizeof(buf))) > 0) {
            if (write(tmp_fd, buf, (size_t)n) < 0) {
                close(in_fd);
                close(tmp_fd);
                return -1;
            }
        }
        close(in_fd);
    }

    lseek(tmp_fd, 0, SEEK_SET);
    return tmp_fd;
}

int prepare_pipeline_redirections(Pipeline *pipeline) {
    for (int i = 0; i < pipeline->stage_count; i++) {
        Stage *s = &pipeline->stages[i];

        if (s->input_count > 0) {
            int fd = build_concatenated_stdin(s->input_files, s->input_count);
            if (fd < 0) {
                fprintf(stderr, "cshell: no such file or directory\n");
                return -1;
            }
            s->prepared_stdin_fd = fd;
        }

        if (s->output_count > 0) {
            s->prepared_output_fds = malloc(sizeof(int) * (size_t)s->output_count);
            for (int j = 0; j < s->output_count; j++) {
                int flags = O_WRONLY | O_CREAT | (s->outputs[j].append ? O_APPEND : O_TRUNC);
                int fd = open(s->outputs[j].filename, flags, 0644);
                if (fd < 0) {
                    fprintf(stderr, "cshell: unable to create file for writing\n");

                    for (int k = 0; k < j; k++) close(s->prepared_output_fds[k]);
                    free(s->prepared_output_fds);
                    s->prepared_output_fds = NULL;
                    return -1;
                }
                s->prepared_output_fds[j] = fd;
            }
        }
    }
    return 0;
}

static char *resolve_command_path(const char *raw_name) {
    int skip_cwd = 0;
    const char *name = raw_name;
    if (name[0] == '%') {
        skip_cwd = 1;
        name++;
    }

    struct stat st;

    if (strchr(name, '/') != NULL) {
        if (stat(name, &st) == 0 && S_ISREG(st.st_mode) && access(name, X_OK) == 0) {
            return strdup(name);
        }
        return NULL;
    }

    if (!skip_cwd) {
        char candidate[PATH_MAX * 2];
        snprintf(candidate, sizeof(candidate), "./%s", name);
        if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode) && access(candidate, X_OK) == 0) {
            return strdup(candidate);
        }
    }

    const char *path_env = getenv("PATH");
    if (path_env != NULL) {
        char *copy = strdup(path_env);
        char *saveptr = NULL;
        char *dir = strtok_r(copy, ":", &saveptr);
        while (dir != NULL) {
            char candidate[PATH_MAX * 2];
            snprintf(candidate, sizeof(candidate), "%s/%s", dir, name);
            if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode) && access(candidate, X_OK) == 0) {
                char *result = strdup(candidate);
                free(copy);
                return result;
            }
            dir = strtok_r(NULL, ":", &saveptr);
        }
        free(copy);
    }
    return NULL;
}

static void child_setup_stdin(const Stage *s, int prev_read_fd) {
    if (s->prepared_stdin_fd >= 0) {
        dup2(s->prepared_stdin_fd, STDIN_FILENO);
    } else if (prev_read_fd >= 0) {
        dup2(prev_read_fd, STDIN_FILENO);
    }

}

static void child_setup_stdout(const Stage *s, int next_write_fd, int output_write_fd) {
    if (s->output_count > 0) {
        dup2(output_write_fd, STDOUT_FILENO);
    } else if (next_write_fd >= 0) {
        dup2(next_write_fd, STDOUT_FILENO);
    }
}

void run_pipeline(Pipeline *pipeline) {
    int n = pipeline->stage_count;
    if (n == 0) return;

    int (*links)[2] = NULL;
    if (n > 1) {
        links = malloc(sizeof(int[2]) * (size_t)(n - 1));
        for (int i = 0; i < n - 1; i++) {
            if (pipe(links[i]) != 0) {
                fprintf(stderr, "cshell: unable to create pipe\n");
                free(links);
                return;
            }
        }
    }

    int output_pipe[2] = { -1, -1 };
    int last_has_output = (pipeline->stages[n - 1].output_count > 0);
    if (last_has_output) {
        if (pipe(output_pipe) != 0) {
            fprintf(stderr, "cshell: unable to create pipe\n");
        }
    }

    pid_t *pids = malloc(sizeof(pid_t) * (size_t)n);

    for (int i = 0; i < n; i++) {
        Stage *s = &pipeline->stages[i];

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "cshell: unable to fork\n");
            pids[i] = -1;
            continue;
        }

        if (pid == 0) {

            int prev_read = (i > 0) ? links[i - 1][0] : -1;
            int next_write = (i < n - 1) ? links[i][1] : -1;
            int out_write = (i == n - 1) ? output_pipe[1] : -1;

            child_setup_stdin(s, prev_read);
            child_setup_stdout(s, next_write, out_write);

            for (int k = 0; k < n - 1; k++) {
                close(links[k][0]);
                close(links[k][1]);
            }
            if (output_pipe[0] >= 0) close(output_pipe[0]);
            if (output_pipe[1] >= 0) close(output_pipe[1]);
            if (s->prepared_stdin_fd >= 0) close(s->prepared_stdin_fd);
            for (int j = 0; j < s->output_count; j++) {
                if (s->prepared_output_fds[j] >= 0) close(s->prepared_output_fds[j]);
            }

            char *resolved = resolve_command_path(s->argv[0]);
            if (!resolved) {
                const char *display = (s->argv[0][0] == '%') ? s->argv[0] + 1 : s->argv[0];
                fprintf(stderr, "cshell: command not found (%s)\n", display);
                _exit(127);
            }
            execv(resolved, s->argv);

            fprintf(stderr, "cshell: command not found (%s)\n", s->argv[0]);
            _exit(127);
        }

        pids[i] = pid;
    }

    for (int k = 0; k < n - 1; k++) {
        close(links[k][0]);
        close(links[k][1]);
    }
    for (int i = 0; i < n; i++) {
        Stage *s = &pipeline->stages[i];
        if (s->prepared_stdin_fd >= 0) { close(s->prepared_stdin_fd); s->prepared_stdin_fd = -1; }
    }

    if (last_has_output) {
        close(output_pipe[1]);

        Stage *last = &pipeline->stages[n - 1];
        char buf[COPY_CHUNK];
        ssize_t r;
        while ((r = read(output_pipe[0], buf, sizeof(buf))) > 0) {

            for (int j = 0; j < last->output_count; j++) {
                write(last->prepared_output_fds[j], buf, (size_t)r);
            }
        }
        close(output_pipe[0]);
        for (int j = 0; j < last->output_count; j++) {
            close(last->prepared_output_fds[j]);
            last->prepared_output_fds[j] = -1;
        }
    }

    for (int i = 0; i < n; i++) {
        if (pids[i] > 0) {
            int status;
            waitpid(pids[i], &status, 0);
        }
    }

    free(pids);
    free(links);
}

void free_pipeline(Pipeline *pipeline) {
    for (int i = 0; i < pipeline->stage_count; i++) {
        Stage *s = &pipeline->stages[i];

        if (s->prepared_stdin_fd >= 0) close(s->prepared_stdin_fd);
        if (s->prepared_output_fds) {
            for (int j = 0; j < s->output_count; j++) {
                if (s->prepared_output_fds[j] >= 0) close(s->prepared_output_fds[j]);
            }
            free(s->prepared_output_fds);
        }

        free(s->argv);
        free(s->input_files);
        free(s->outputs);
    }
    free(pipeline->stages);
    pipeline->stages = NULL;
    pipeline->stage_count = 0;
}
