#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer* lexer;
    Token* current_token;
    Token* peek_token;
    char* current_namespace;
    const char* filename;
    const char* source_code;
    int error_count;
    int panic_mode;
    int max_errors;
} Parser;

Parser* init_parser(Lexer* lexer);
void parser_set_context(Parser* parser, const char* filename, const char* source_code);
Token* parser_peek_token(Parser* parser);
ASTNode* parser_parse(Parser* parser);
void parser_synchronize(Parser* parser);
int parser_get_error_count(Parser* parser);

int find_enum_constant(const char* name, int* out_val);
int find_constant(const char* name, int* out_val);
void register_constant(const char* name, int value);
int eval_const_expr(ASTNode* node, int* out_val);
const char* resolve_typedef(const char* alias);
void add_known_struct(const char* name);
int is_known_struct(const char* name);

void parser_reset(void);
void add_discovered_source(const char* path);
int get_discovered_source_count(void);
const char* get_discovered_source(int index);
void clear_discovered_sources(void);

#endif // PARSER_H
