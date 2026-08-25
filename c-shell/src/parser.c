#include "parser.h"

static bool parse_ARG(const TokenList *tokens, size_t *pos);
static bool parse_CMD(const TokenList *tokens, size_t *pos);
static bool parse_TGT(const TokenList *tokens, size_t *pos);
static bool parse_BG(const TokenList *tokens, size_t *pos);

static bool parse_CMD(const TokenList *tokens, size_t *pos) {
    if (*pos >= tokens->count || tokens->items[*pos].type != TOKEN_WORD) {
        return false;
    }
    (*pos)++;
    return parse_ARG(tokens, pos);
}

static bool parse_TGT(const TokenList *tokens, size_t *pos) {
    if (*pos >= tokens->count || tokens->items[*pos].type != TOKEN_WORD) {
        return false;
    }
    (*pos)++;
    return parse_ARG(tokens, pos);
}

static bool parse_BG(const TokenList *tokens, size_t *pos) {
    if (*pos >= tokens->count) {
        return true;
    }
    if (tokens->items[*pos].type != TOKEN_WORD) {
        return false;
    }
    (*pos)++;
    return parse_ARG(tokens, pos);
}

static bool parse_ARG(const TokenList *tokens, size_t *pos) {
    if (*pos >= tokens->count) {
        return true;
    }

    TokenType t = tokens->items[*pos].type;

    switch (t) {
        case TOKEN_WORD:
            (*pos)++;
            return parse_ARG(tokens, pos);
        case TOKEN_OP_LT:
            (*pos)++;
            return parse_TGT(tokens, pos);
        case TOKEN_OP_GT:
            (*pos)++;
            return parse_TGT(tokens, pos);
        case TOKEN_OP_GTGT:
            (*pos)++;
            return parse_TGT(tokens, pos);
        case TOKEN_OP_PIPE:
            (*pos)++;
            return parse_CMD(tokens, pos);
        case TOKEN_OP_SEMI:
            (*pos)++;
            return parse_CMD(tokens, pos);
        case TOKEN_OP_AMP:
            (*pos)++;
            return parse_BG(tokens, pos);
        default:
            return false;
    }
}

bool parse_line(const TokenList *tokens) {
    size_t pos = 0;

    if (tokens->count == 0) {
        return true;
    }

    if (tokens->items[pos].type != TOKEN_WORD) {
        return false;
    }
    pos++;

    return parse_ARG(tokens, &pos);
}
