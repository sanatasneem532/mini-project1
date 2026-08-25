#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_WORD,
    TOKEN_OP_PIPE,
    TOKEN_OP_AMP,
    TOKEN_OP_SEMI,
    TOKEN_OP_LT,
    TOKEN_OP_GT,
    TOKEN_OP_GTGT
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

void tokenlist_init(TokenList *list);
void tokenlist_free(TokenList *list);

int tokenize(const char *line, TokenList *out);

#endif
