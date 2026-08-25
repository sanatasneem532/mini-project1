#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>

#include "peek.h"

#define CHUNK_SIZE 4096

static void peek_forward_stream(int fd, int numbered) {
    long counter = 1;
    char *line = malloc(256);
    size_t line_cap = 256, line_len = 0;
    char buf[CHUNK_SIZE];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                if (line_len > 0) {
                    if (numbered) printf("%ld ", counter);
                    counter++;
                }
                fwrite(line, 1, line_len, stdout);
                printf("\n");
                line_len = 0;
            } else {
                if (line_len + 1 >= line_cap) {
                    line_cap *= 2;
                    line = realloc(line, line_cap);
                }
                line[line_len++] = c;
            }
        }
    }
    if (line_len > 0) {
        if (numbered) printf("%ld ", counter);
        fwrite(line, 1, line_len, stdout);
        printf("\n");
    }
    free(line);
}

typedef struct { size_t start, len; } LineSpan;

static void peek_buffered(int fd, int numbered, int reversed) {
    size_t cap = CHUNK_SIZE, len = 0;
    char *data = malloc(cap);
    char buf[CHUNK_SIZE];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        while (len + (size_t)n > cap) cap *= 2;
        data = realloc(data, cap);
        memcpy(data + len, buf, (size_t)n);
        len += (size_t)n;
    }

    LineSpan *lines = NULL;
    size_t lcount = 0, lcap = 0, start = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            if (lcount == lcap) { lcap = lcap ? lcap * 2 : 16; lines = realloc(lines, lcap * sizeof(LineSpan)); }
            lines[lcount].start = start;
            lines[lcount].len = i - start;
            lcount++;
            start = i + 1;
        }
    }
    if (start < len) {
        if (lcount == lcap) { lcap = lcap ? lcap * 2 : 16; lines = realloc(lines, lcap * sizeof(LineSpan)); }
        lines[lcount].start = start;
        lines[lcount].len = len - start;
        lcount++;
    }

    long *numbers = malloc(sizeof(long) * (lcount ? lcount : 1));
    long counter = 1;
    for (size_t i = 0; i < lcount; i++) {
        numbers[i] = lines[i].len > 0 ? counter++ : -1;
    }

    for (size_t step = 0; step < lcount; step++) {
        size_t i = reversed ? (lcount - 1 - step) : step;
        if (lines[i].len > 0 && numbered) printf("%ld ", numbers[i]);
        fwrite(data + lines[i].start, 1, lines[i].len, stdout);
        printf("\n");
    }

    free(data);
    free(lines);
    free(numbers);
}

static long count_nonempty_lines(int fd, off_t file_size) {
    long count = 0;
    long cur_len = 0;
    char chunk[CHUNK_SIZE];
    off_t offset = 0;

    while (offset < file_size) {
        size_t to_read = (size_t)((file_size - offset) > CHUNK_SIZE
                                       ? CHUNK_SIZE
                                       : (file_size - offset));
        pread(fd, chunk, to_read, offset);
        for (size_t i = 0; i < to_read; i++) {
            if (chunk[i] == '\n') {
                if (cur_len > 0) count++;
                cur_len = 0;
            } else {
                cur_len++;
            }
        }
        offset += (off_t)to_read;
    }
    if (cur_len > 0) count++;
    return count;
}

typedef struct { char *data; size_t len; size_t cap; } RevBuf;

static void revbuf_init(RevBuf *b) { b->cap = 64; b->len = 0; b->data = malloc(b->cap); }
static void revbuf_reset(RevBuf *b) { b->len = 0; }
static void revbuf_free(RevBuf *b) { free(b->data); }

static void revbuf_prepend(RevBuf *b, char c) {
    if (b->len + 1 >= b->cap) { b->cap *= 2; b->data = realloc(b->data, b->cap); }
    memmove(b->data + 1, b->data, b->len);
    b->data[0] = c;
    b->len++;
}

static void emit_line(const RevBuf *b, int numbered, long *counter) {
    if (b->len > 0) {
        if (numbered) {
            printf("%ld ", *counter);
        }
        (*counter)--;
        fwrite(b->data, 1, b->len, stdout);
        printf("\n");
    } else {
        printf("\n");
    }
}

static void peek_backward_chunks(int fd, off_t file_size, int numbered) {
    long counter = count_nonempty_lines(fd, file_size);

    off_t end = file_size;
    if (end > 0) {
        char last_byte;
        if (pread(fd, &last_byte, 1, end - 1) == 1 && last_byte == '\n') {
            end--;
        }
    }

    RevBuf buf;
    revbuf_init(&buf);
    char chunk[CHUNK_SIZE];
    off_t pos = end;

    while (pos > 0) {
        size_t to_read = (size_t)(pos > CHUNK_SIZE ? CHUNK_SIZE : pos);
        pos -= (off_t)to_read;
        pread(fd, chunk, to_read, pos);

        for (ssize_t i = (ssize_t)to_read - 1; i >= 0; i--) {
            char c = chunk[i];
            if (c == '\n') {
                emit_line(&buf, numbered, &counter);
                revbuf_reset(&buf);
            } else {
                revbuf_prepend(&buf, c);
            }
        }
    }
    if (end > 0) {
        emit_line(&buf, numbered, &counter);
    }
    revbuf_free(&buf);
}

static void peek_stream(const char *filename, int numbered, int reversed) {
    int is_stdin = (filename == NULL);
    int fd;
    struct stat st;

    if (is_stdin) {
        fd = STDIN_FILENO;
    } else {
        if (stat(filename, &st) != 0) {
            printf("peek: no such file or directory\n");
            return;
        }
        if (S_ISDIR(st.st_mode)) {
            printf("peek: is a directory\n");
            return;
        }
        fd = open(filename, O_RDONLY);
        if (fd < 0) {
            printf("peek: no such file or directory\n");
            return;
        }
    }

    int seekable = (!is_stdin) && S_ISREG(st.st_mode);

    if (!reversed) {
        peek_forward_stream(fd, numbered);
    } else if (seekable) {
        peek_backward_chunks(fd, st.st_size, numbered);
    } else {
        peek_buffered(fd, numbered, reversed);
    }

    if (!is_stdin) close(fd);
}

void cmd_peek(int argc, char **argv) {
    int numbered = 0, reversed = 0;
    char **files = malloc(sizeof(char *) * (argc > 0 ? (size_t)argc : 1));
    int file_count = 0;

    for (int i = 0; i < argc; i++) {
        const char *tok = argv[i];
        size_t len = strlen(tok);
        int is_flag_group = (len > 1 && tok[0] == '-');

        if (is_flag_group) {
            for (size_t j = 1; j < len; j++) {
                if (tok[j] == 'n') numbered = 1;
                else if (tok[j] == 'r') reversed = 1;
                else {
                    printf("peek: invalid syntax\n");
                    free(files);
                    return;
                }
            }
        } else {
            files[file_count++] = argv[i];
        }
    }

    if (file_count == 0) {
        peek_stream(NULL, numbered, reversed);
    } else {
        for (int i = 0; i < file_count; i++) {
            const char *fname = files[i];
            if (strcmp(fname, "-") == 0) {
                peek_stream(NULL, numbered, reversed);
            } else {
                peek_stream(fname, numbered, reversed);
            }
        }
    }

    free(files);
}
