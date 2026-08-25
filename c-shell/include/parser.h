#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h"

bool parse_line(const TokenList *tokens);

#endif
