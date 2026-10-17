#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct {
    char* source;
    int current_pos;
    char current_char;
    int line;
} Lexer;

Lexer* init_lexer(char* source);
void lexer_advance(Lexer* lexer);
char lexer_peek(Lexer* lexer);
void lexer_skip_whitespace(Lexer* lexer);
Token* init_token(TokenType type, char* value, int line);
Token* lexer_get_next_token(Lexer* lexer);

#endif // LEXER_H
