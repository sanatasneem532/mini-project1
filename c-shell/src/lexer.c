#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"

void tokenlist_init(TokenList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void tokenlist_free(TokenList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].value);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void tokenlist_append(TokenList *list, Token tok) {
    if (list->count == list->capacity) {
        size_t new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        Token *bigger = realloc(list->items, new_cap * sizeof(Token));
        if (!bigger) {
            fprintf(stderr, "cshell: out of memory\n");
            exit(1);
        }
        list->items = bigger;
        list->capacity = new_cap;
    }
    list->items[list->count++] = tok;
}

static int is_special(char c) {
    return c == '|' || c == '&' || c == ';' || c == '<' || c == '>';
}

static int is_shell_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static void buf_init(Buf *b) {
    b->cap = 32;
    b->len = 0;
    b->data = malloc(b->cap);
}

static void buf_push(Buf *b, char c) {
    if (b->len + 1 >= b->cap) {
        b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
    b->data[b->len++] = c;
}

static int read_word(const char *line, size_t len, size_t *pos, char **out) {
    Buf buf;
    buf_init(&buf);

    while (*pos < len) {
        char c = line[*pos];

        if (is_shell_space(c) || is_special(c)) {
            break;
        }

        if (c == '\\') {

            (*pos)++;
            if (*pos >= len) {
                free(buf.data);
                return -1;
            }
            buf_push(&buf, line[*pos]);
            (*pos)++;

        } else if (c == '"') {

            (*pos)++;
            int closed = 0;
            while (*pos < len) {
                char qc = line[*pos];
                if (qc == '"') {
                    (*pos)++;
                    closed = 1;
                    break;
                }
                if (qc == '\\') {
                    (*pos)++;
                    if (*pos >= len) {
                        free(buf.data);
                        return -1;
                    }
                    char nc = line[*pos];
                    (*pos)++;
                    if (nc == '"' || nc == '\\') {
                        buf_push(&buf, nc);
                    } else {
                        buf_push(&buf, '\\');
                        buf_push(&buf, nc);
                    }
                } else {
                    buf_push(&buf, qc);
                    (*pos)++;
                }
            }
            if (!closed) {
                free(buf.data);
                return -1;
            }

        } else if (c == '\'') {

            (*pos)++;
            int closed = 0;
            while (*pos < len) {
                char qc = line[*pos];
                if (qc == '\'') {
                    (*pos)++;
                    closed = 1;
                    break;
                }
                buf_push(&buf, qc);
                (*pos)++;
            }
            if (!closed) {
                free(buf.data);
                return -1;
            }

        } else {

            buf_push(&buf, c);
            (*pos)++;
        }
    }

    buf_push(&buf, '\0');
    *out = buf.data;
    return 0;
}

int tokenize(const char *line, TokenList *out) {
    tokenlist_init(out);
    size_t len = strlen(line);
    size_t pos = 0;

    while (pos < len) {
        char c = line[pos];

        if (is_shell_space(c)) {
            pos++;
            continue;
        }

        Token tok;
        tok.value = NULL;

        if (c == '|') {
            tok.type = TOKEN_OP_PIPE;
            pos++;
        } else if (c == '&') {
            tok.type = TOKEN_OP_AMP;
            pos++;
        } else if (c == ';') {
            tok.type = TOKEN_OP_SEMI;
            pos++;
        } else if (c == '<') {
            tok.type = TOKEN_OP_LT;
            pos++;
        } else if (c == '>') {

            if (pos + 1 < len && line[pos + 1] == '>') {
                tok.type = TOKEN_OP_GTGT;
                pos += 2;
            } else {
                tok.type = TOKEN_OP_GT;
                pos++;
            }
        } else {

            char *word;
            if (read_word(line, len, &pos, &word) != 0) {
                tokenlist_free(out);
                return -1;
            }
            tok.type = TOKEN_WORD;
            tok.value = word;
        }

        tokenlist_append(out, tok);
    }

    return 0;
}
