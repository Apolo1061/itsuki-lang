#include "parser.h"
#include "embedded_lib.h"
#include "package_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

ASTNode* create_ast_node(ASTNodeType type, int line) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    return node;
}

static void add_known_namespace(const char* name);
void add_known_struct(const char* name);

Parser* init_parser(Lexer* lexer) {
    Parser* parser = malloc(sizeof(Parser));
    parser->lexer = lexer;
    parser->current_token = lexer_get_next_token(lexer);
    parser->peek_token = NULL;
    parser->current_namespace = NULL;
    add_known_namespace("fundamental");
    add_known_struct("fundamental::string");
    add_known_struct("string");
    return parser;
}

Token* parser_peek_token(Parser* parser) {
    if (parser->peek_token == NULL) {
        parser->peek_token = lexer_get_next_token(parser->lexer);
    }
    return parser->peek_token;
}

void parser_eat(Parser* parser, TokenType type) {
    if (parser->current_token->type == type) {
        free(parser->current_token->value);
        free(parser->current_token);
        if (parser->peek_token != NULL) {
            parser->current_token = parser->peek_token;
            parser->peek_token = NULL;
        } else {
            parser->current_token = lexer_get_next_token(parser->lexer);
        }
    } else {
        printf("Error de sintaxis: se esperaba token %d pero se encontro '%s' (tipo %d) en la linea %d\n",
               type, parser->current_token->value, parser->current_token->type, parser->current_token->line);
        exit(1);
    }
}

// Prototipos de funciones recursivas del parser
static ASTNode* parser_parse_declaration(Parser* parser);
static ASTNode* parser_parse_statement(Parser* parser);
static ASTNode* parser_parse_expression(Parser* parser);
static void reset_templates(void);

char* parser_parse_scoped_identifier(Parser* parser) {
    char* id = strdup(parser->current_token->value);
    parser_eat(parser, TOKEN_IDENTIFIER);
    while (parser->current_token->type == TOKEN_COLON_COLON) {
        parser_eat(parser, TOKEN_COLON_COLON);
        char* next_id = NULL;
        if (parser->current_token->type == TOKEN_IDENTIFIER ||
            parser->current_token->type == TOKEN_STRING_TYPE ||
            parser->current_token->type == TOKEN_INT ||
            parser->current_token->type == TOKEN_VOID ||
            parser->current_token->type == TOKEN_CHAR) {
            next_id = strdup(parser->current_token->value);
            parser_eat(parser, parser->current_token->type);
        } else {
            printf("Error de sintaxis: se esperaba identificador despues de '::' en la linea %d\n", parser->current_token->line);
            exit(1);
        }
        char* combined = malloc(strlen(id) + 2 + strlen(next_id) + 1);
        sprintf(combined, "%s::%s", id, next_id);
        free(id);
        free(next_id);
        id = combined;
    }
    return id;
}

#define MAX_IMPORTED_FILES 256
static char* imported_files[MAX_IMPORTED_FILES];
static int imported_files_count = 0;

#define MAX_DISCOVERED_SOURCES 64
static char* discovered_sources[MAX_DISCOVERED_SOURCES];
static int discovered_source_count = 0;

void add_discovered_source(const char* path) {
    if (path == NULL) return;
    for (int i = 0; i < discovered_source_count; i++) {
        if (strcmp(discovered_sources[i], path) == 0) return;
    }
    if (discovered_source_count < MAX_DISCOVERED_SOURCES) {
        discovered_sources[discovered_source_count++] = strdup(path);
    }
}

int get_discovered_source_count(void) {
    return discovered_source_count;
}

const char* get_discovered_source(int index) {
    if (index >= 0 && index < discovered_source_count) return discovered_sources[index];
    return NULL;
}

void clear_discovered_sources(void) {
    for (int i = 0; i < discovered_source_count; i++) {
        free(discovered_sources[i]);
    }
    discovered_source_count = 0;
}

#define MAX_KNOWN_STRUCTS 512
static char* known_struct_names[MAX_KNOWN_STRUCTS];
static int known_struct_count = 0;

#define MAX_KNOWN_NAMESPACES 256
static char* known_namespaces[MAX_KNOWN_NAMESPACES];
static int known_namespaces_count = 0;

#define MAX_KNOWN_ENUMS 512
static char* known_enum_names[MAX_KNOWN_ENUMS];
static int known_enum_count = 0;

#define MAX_ENUM_CONSTANTS 1024
typedef struct {
    char* name;
    int value;
} EnumConstant;
static EnumConstant enum_constants[MAX_ENUM_CONSTANTS];
static int enum_constant_count = 0;

#define MAX_TYPEDEFS 512
typedef struct {
    char* alias;
    char* target;
} TypedefDef;
static TypedefDef typedef_table[MAX_TYPEDEFS];
static int typedef_count = 0;

static void add_known_namespace(const char* name) {
    if (name == NULL) return;
    for (int i = 0; i < known_namespaces_count; i++) {
        if (strcmp(known_namespaces[i], name) == 0) return;
    }
    if (known_namespaces_count < MAX_KNOWN_NAMESPACES) {
        known_namespaces[known_namespaces_count++] = strdup(name);
    }
}

static int is_known_namespace(const char* name) {
    if (name == NULL) return 0;
    for (int i = 0; i < known_namespaces_count; i++) {
        if (strcmp(known_namespaces[i], name) == 0) return 1;
    }
    return 0;
}

void add_known_struct(const char* name) {
    if (name == NULL) return;
    for (int i = 0; i < known_struct_count; i++) {
        if (strcmp(known_struct_names[i], name) == 0) return;
    }
    if (known_struct_count < MAX_KNOWN_STRUCTS) {
        known_struct_names[known_struct_count++] = strdup(name);
    }
}

int is_known_struct(const char* name) {
    if (name == NULL) return 0;
    for (int i = 0; i < known_struct_count; i++) {
        if (strcmp(known_struct_names[i], name) == 0) return 1;
    }
    return 0;
}

static void add_known_enum(const char* name) {
    if (name == NULL) return;
    for (int i = 0; i < known_enum_count; i++) {
        if (strcmp(known_enum_names[i], name) == 0) return;
    }
    if (known_enum_count < MAX_KNOWN_ENUMS) {
        known_enum_names[known_enum_count++] = strdup(name);
    }
}

static int is_known_enum(const char* name) {
    if (name == NULL) return 0;
    for (int i = 0; i < known_enum_count; i++) {
        if (strcmp(known_enum_names[i], name) == 0) return 1;
    }
    return 0;
}

void register_constant(const char* name, int value) {
    if (name == NULL) return;
    for (int i = 0; i < enum_constant_count; i++) {
        if (strcmp(enum_constants[i].name, name) == 0) {
            enum_constants[i].value = value;
            return;
        }
    }
    if (enum_constant_count < MAX_ENUM_CONSTANTS) {
        enum_constants[enum_constant_count].name = strdup(name);
        enum_constants[enum_constant_count].value = value;
        enum_constant_count++;
    }
}

static void register_enum_constant(const char* name, int value) {
    register_constant(name, value);
}

int find_constant(const char* name, int* out_val) {
    if (name == NULL) return 0;
    for (int i = 0; i < enum_constant_count; i++) {
        if (strcmp(enum_constants[i].name, name) == 0) {
            if (out_val) *out_val = enum_constants[i].value;
            return 1;
        }
    }
    return 0;
}

void parser_reset(void) {
    for (int i = 0; i < imported_files_count; i++) {
        free(imported_files[i]);
    }
    imported_files_count = 0;
    
    for (int i = 0; i < typedef_count; i++) {
        free(typedef_table[i].alias);
        free(typedef_table[i].target);
    }
    typedef_count = 0;
    
    for (int i = 0; i < known_struct_count; i++) {
        free(known_struct_names[i]);
    }
    known_struct_count = 0;
    
    for (int i = 0; i < known_namespaces_count; i++) {
        free(known_namespaces[i]);
    }
    known_namespaces_count = 0;
    
    for (int i = 0; i < known_enum_count; i++) {
        free(known_enum_names[i]);
    }
    known_enum_count = 0;
    
    for (int i = 0; i < enum_constant_count; i++) {
        free(enum_constants[i].name);
    }
    enum_constant_count = 0;

    add_known_namespace("fundamental");
    add_known_struct("fundamental::string");
    add_known_struct("string");
    reset_templates();
}

typedef struct TemplateDef {
    char name[128];
    int is_struct_or_class; // 0 = función, 1 = struct, 2 = class
    char param_names[8][64];
    int param_count;
    ASTNode* ast_template;
    struct TemplateDef* next;
} TemplateDef;

static TemplateDef* template_table = NULL;

typedef struct InstantiationRecord {
    char name[128];
    struct InstantiationRecord* next;
} InstantiationRecord;

static InstantiationRecord* instantiated_records = NULL;
static ASTNode* pending_instantiations_head = NULL;
static ASTNode* pending_instantiations_tail = NULL;

static void reset_templates(void) {
    TemplateDef* curr = template_table;
    while (curr != NULL) {
        TemplateDef* next = curr->next;
        free(curr);
        curr = next;
    }
    template_table = NULL;

    InstantiationRecord* rcurr = instantiated_records;
    while (rcurr != NULL) {
        InstantiationRecord* rnext = rcurr->next;
        free(rcurr);
        rcurr = rnext;
    }
    instantiated_records = NULL;
    pending_instantiations_head = NULL;
    pending_instantiations_tail = NULL;
}

static TemplateDef* find_template(const char* name) {
    if (!name) return NULL;
    TemplateDef* curr = template_table;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

static int is_template_instantiated(const char* name) {
    if (!name) return 0;
    InstantiationRecord* curr = instantiated_records;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) return 1;
        curr = curr->next;
    }
    return 0;
}

static void mark_template_instantiated(const char* name) {
    InstantiationRecord* rec = malloc(sizeof(InstantiationRecord));
    strncpy(rec->name, name, 127);
    rec->name[127] = '\0';
    rec->next = instantiated_records;
    instantiated_records = rec;
}

static char* substitute_type_name(const char* orig_type, int param_count, char param_names[][64], char* type_args[]) {
    if (orig_type == NULL) return NULL;
    for (int i = 0; i < param_count; i++) {
        const char* p = param_names[i];
        size_t plen = strlen(p);
        
        // Coincidencia exacta: "T" -> "int"
        if (strcmp(orig_type, p) == 0) {
            return strdup(type_args[i]);
        }
        
        // Punteros: "T*" -> "int*", "T**" -> "int**"
        if (strncmp(orig_type, p, plen) == 0 && orig_type[plen] == '*') {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s%s", type_args[i], orig_type + plen);
            return strdup(buf);
        }

        // Const: "const T" -> "const int"
        if (strncmp(orig_type, "const ", 6) == 0 && strcmp(orig_type + 6, p) == 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "const %s", type_args[i]);
            return strdup(buf);
        }
        if (strncmp(orig_type, "const ", 6) == 0 && strncmp(orig_type + 6, p, plen) == 0 && (orig_type + 6)[plen] == '*') {
            char buf[256];
            snprintf(buf, sizeof(buf), "const %s%s", type_args[i], (orig_type + 6) + plen);
            return strdup(buf);
        }

        // Arreglo: "T[10]" -> "int[10]"
        if (strncmp(orig_type, p, plen) == 0 && orig_type[plen] == '[') {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s%s", type_args[i], orig_type + plen);
            return strdup(buf);
        }
    }
    return strdup(orig_type);
}

static ASTNode* clone_and_substitute_ast(ASTNode* node, int param_count, char param_names[][64], char* type_args[], const char* old_name, const char* new_name) {
    if (node == NULL) return NULL;
    
    ASTNode* c = create_ast_node(node->type, node->line);
    c->access_level = node->access_level;
    c->is_constructor = node->is_constructor;
    c->is_destructor = node->is_destructor;
    c->op = node->op;
    strcpy(c->op_str, node->op_str);
    c->int_value = node->int_value;
    c->is_array = node->is_array;
    c->array_size = node->array_size;
    c->is_inline = node->is_inline;
    c->is_const = node->is_const;
    c->is_extern = node->is_extern;
    c->is_static = node->is_static;

    // Sustitución de tipo
    if (node->type_name != NULL) {
        char* subst = substitute_type_name(node->type_name, param_count, param_names, type_args);
        if (old_name != NULL && subst != NULL) {
            size_t olen = strlen(old_name);
            if (strcmp(subst, old_name) == 0) {
                free(subst);
                subst = strdup(new_name);
            } else if (strncmp(subst, old_name, olen) == 0 && subst[olen] == '*') {
                char buf[256];
                snprintf(buf, sizeof(buf), "%s%s", new_name, subst + olen);
                free(subst);
                subst = strdup(buf);
            }
        }
        c->type_name = subst;
    }
    
    // Sustitución de nombre de identificador/función/clase
    if (node->name != NULL) {
        if (old_name != NULL && strcmp(node->name, old_name) == 0) {
            c->name = strdup(new_name);
        } else if (old_name != NULL && node->name[0] == '~' && strcmp(node->name + 1, old_name) == 0) {
            char dname[256];
            snprintf(dname, sizeof(dname), "~%s", new_name);
            c->name = strdup(dname);
        } else {
            c->name = strdup(node->name);
        }
    }

    if (node->namespace_prefix != NULL) {
        if (old_name != NULL && strcmp(node->namespace_prefix, old_name) == 0) {
            c->namespace_prefix = strdup(new_name);
        } else {
            c->namespace_prefix = strdup(node->namespace_prefix);
        }
    }

    if (node->base_class != NULL) {
        c->base_class = substitute_type_name(node->base_class, param_count, param_names, type_args);
    }

    if (node->string_value != NULL) {
        c->string_value = strdup(node->string_value);
    }

    // Recursión sobre hijos y listas
    c->children = clone_and_substitute_ast(node->children, param_count, param_names, type_args, old_name, new_name);
    c->fields = clone_and_substitute_ast(node->fields, param_count, param_names, type_args, old_name, new_name);
    c->params = clone_and_substitute_ast(node->params, param_count, param_names, type_args, old_name, new_name);
    c->body = clone_and_substitute_ast(node->body, param_count, param_names, type_args, old_name, new_name);
    c->else_branch = clone_and_substitute_ast(node->else_branch, param_count, param_names, type_args, old_name, new_name);
    c->statements = clone_and_substitute_ast(node->statements, param_count, param_names, type_args, old_name, new_name);
    c->init_value = clone_and_substitute_ast(node->init_value, param_count, param_names, type_args, old_name, new_name);
    c->update_expr = clone_and_substitute_ast(node->update_expr, param_count, param_names, type_args, old_name, new_name);
    c->return_value = clone_and_substitute_ast(node->return_value, param_count, param_names, type_args, old_name, new_name);
    c->expr = clone_and_substitute_ast(node->expr, param_count, param_names, type_args, old_name, new_name);
    c->callee = clone_and_substitute_ast(node->callee, param_count, param_names, type_args, old_name, new_name);
    c->args = clone_and_substitute_ast(node->args, param_count, param_names, type_args, old_name, new_name);
    c->left = clone_and_substitute_ast(node->left, param_count, param_names, type_args, old_name, new_name);
    c->right = clone_and_substitute_ast(node->right, param_count, param_names, type_args, old_name, new_name);
    c->next = clone_and_substitute_ast(node->next, param_count, param_names, type_args, old_name, new_name);

    return c;
}

static char* instantiate_template(const char* tpl_name, char* type_args[], int type_arg_count) {
    TemplateDef* tpl = find_template(tpl_name);
    if (tpl == NULL) {
        printf("Error: plantilla '%s' no encontrada\n", tpl_name);
        exit(1);
    }
    if (type_arg_count != tpl->param_count) {
        printf("Error: la plantilla '%s' espera %d argumentos de tipo, pero se pasaron %d\n",
               tpl_name, tpl->param_count, type_arg_count);
        exit(1);
    }

    // Construir nombre especializado: tplName__T_arg1_arg2
    char spec_name[256];
    int pos = snprintf(spec_name, sizeof(spec_name), "%s__T", tpl_name);
    for (int i = 0; i < type_arg_count; i++) {
        char clean_arg[64];
        int c_idx = 0;
        for (int k = 0; type_args[i][k] != '\0' && c_idx < 60; k++) {
            if (type_args[i][k] == '*') {
                clean_arg[c_idx++] = '_';
                clean_arg[c_idx++] = 'p';
                clean_arg[c_idx++] = 't';
                clean_arg[c_idx++] = 'r';
            } else if (type_args[i][k] == ':' && type_args[i][k+1] == ':') {
                clean_arg[c_idx++] = '_';
                k++;
            } else if (isalnum((unsigned char)type_args[i][k])) {
                clean_arg[c_idx++] = type_args[i][k];
            } else {
                clean_arg[c_idx++] = '_';
            }
        }
        clean_arg[c_idx] = '\0';
        pos += snprintf(spec_name + pos, sizeof(spec_name) - pos, "_%s", clean_arg);
    }

    if (is_template_instantiated(spec_name)) {
        return strdup(spec_name);
    }
    mark_template_instantiated(spec_name);

    ASTNode* orig_next = tpl->ast_template->next;
    tpl->ast_template->next = NULL;
    ASTNode* clone = clone_and_substitute_ast(tpl->ast_template, tpl->param_count, tpl->param_names, type_args, tpl->name, spec_name);
    tpl->ast_template->next = orig_next;
    if (clone) clone->next = NULL;

    if (tpl->is_struct_or_class) {
        add_known_struct(spec_name);
    }

    // Agregar a la lista de declaraciones pendientes de instanciación
    if (pending_instantiations_head == NULL) {
        pending_instantiations_head = clone;
        pending_instantiations_tail = clone;
    } else {
        pending_instantiations_tail->next = clone;
        pending_instantiations_tail = clone;
    }

    return strdup(spec_name);
}

int find_enum_constant(const char* name, int* out_val) {
    return find_constant(name, out_val);
}

int eval_const_expr(ASTNode* node, int* out_val) {
    if (node == NULL) return 0;
    if (node->type == AST_NUMBER) {
        if (out_val) *out_val = node->int_value;
        return 1;
    }
    if (node->type == AST_BOOLEAN) {
        if (out_val) *out_val = node->int_value;
        return 1;
    }
    if (node->type == AST_NULL) {
        if (out_val) *out_val = 0;
        return 1;
    }
    if (node->type == AST_IDENTIFIER || node->type == AST_SCOPED_IDENTIFIER) {
        return find_constant(node->name, out_val);
    }
    if (node->type == AST_UNARY_EXPR) {
        int v = 0;
        if (!eval_const_expr(node->left, &v)) return 0;
        if (node->op == '-') { if (out_val) *out_val = -v; return 1; }
        else if (node->op == '+') { if (out_val) *out_val = +v; return 1; }
        else if (node->op == '!') { if (out_val) *out_val = !v; return 1; }
        else if (node->op == '~') { if (out_val) *out_val = ~v; return 1; }
        return 0;
    }
    if (node->type == AST_BINARY_EXPR) {
        if (strcmp(node->op_str, "&&") == 0) {
            int l = 0;
            if (!eval_const_expr(node->left, &l)) return 0;
            if (!l) { if (out_val) *out_val = 0; return 1; }
            int r = 0;
            if (!eval_const_expr(node->right, &r)) return 0;
            if (out_val) *out_val = (r != 0);
            return 1;
        }
        if (strcmp(node->op_str, "||") == 0) {
            int l = 0;
            if (!eval_const_expr(node->left, &l)) return 0;
            if (l) { if (out_val) *out_val = 1; return 1; }
            int r = 0;
            if (!eval_const_expr(node->right, &r)) return 0;
            if (out_val) *out_val = (r != 0);
            return 1;
        }
        int l = 0, r = 0;
        if (!eval_const_expr(node->left, &l)) return 0;
        if (!eval_const_expr(node->right, &r)) return 0;
        if (strcmp(node->op_str, "+") == 0) { if (out_val) *out_val = l + r; return 1; }
        else if (strcmp(node->op_str, "-") == 0) { if (out_val) *out_val = l - r; return 1; }
        else if (strcmp(node->op_str, "*") == 0) { if (out_val) *out_val = l * r; return 1; }
        else if (strcmp(node->op_str, "/") == 0) {
            if (r == 0) return 0;
            if (out_val) *out_val = l / r;
            return 1;
        }
        else if (strcmp(node->op_str, "%") == 0) {
            if (r == 0) return 0;
            if (out_val) *out_val = l % r;
            return 1;
        }
        else if (strcmp(node->op_str, "<<") == 0) { if (out_val) *out_val = l << r; return 1; }
        else if (strcmp(node->op_str, ">>") == 0) { if (out_val) *out_val = l >> r; return 1; }
        else if (strcmp(node->op_str, "&") == 0) { if (out_val) *out_val = l & r; return 1; }
        else if (strcmp(node->op_str, "|") == 0) { if (out_val) *out_val = l | r; return 1; }
        else if (strcmp(node->op_str, "^") == 0) { if (out_val) *out_val = l ^ r; return 1; }
        else if (strcmp(node->op_str, "==") == 0) { if (out_val) *out_val = (l == r); return 1; }
        else if (strcmp(node->op_str, "!=") == 0) { if (out_val) *out_val = (l != r); return 1; }
        else if (strcmp(node->op_str, "<") == 0) { if (out_val) *out_val = (l < r); return 1; }
        else if (strcmp(node->op_str, "<=") == 0) { if (out_val) *out_val = (l <= r); return 1; }
        else if (strcmp(node->op_str, ">") == 0) { if (out_val) *out_val = (l > r); return 1; }
        else if (strcmp(node->op_str, ">=") == 0) { if (out_val) *out_val = (l >= r); return 1; }
        return 0;
    }
    return 0;
}

static void register_typedef(const char* alias, const char* target) {
    if (alias == NULL || target == NULL) return;
    for (int i = 0; i < typedef_count; i++) {
        if (strcmp(typedef_table[i].alias, alias) == 0) {
            free(typedef_table[i].target);
            typedef_table[i].target = strdup(target);
            return;
        }
    }
    if (typedef_count < MAX_TYPEDEFS) {
        typedef_table[typedef_count].alias = strdup(alias);
        typedef_table[typedef_count].target = strdup(target);
        typedef_count++;
    }
}

const char* resolve_typedef(const char* alias) {
    if (alias == NULL) return NULL;
    const char* curr = alias;
    for (int depth = 0; depth < 10; depth++) {
        int found = 0;
        for (int i = 0; i < typedef_count; i++) {
            if (strcmp(typedef_table[i].alias, curr) == 0) {
                curr = typedef_table[i].target;
                found = 1;
                break;
            }
        }
        if (!found) break;
    }
    return curr;
}

static int is_known_typedef(const char* alias) {
    if (alias == NULL) return 0;
    for (int i = 0; i < typedef_count; i++) {
        if (strcmp(typedef_table[i].alias, alias) == 0) return 1;
    }
    return 0;
}

char* parser_parse_type(Parser* parser) {
    int has_const = 0;
    if (parser->current_token->type == TOKEN_CONST) {
        parser_eat(parser, TOKEN_CONST);
        has_const = 1;
    }
    char* tname = NULL;
    if (parser->current_token->type == TOKEN_STRUCT ||
        parser->current_token->type == TOKEN_UNION ||
        parser->current_token->type == TOKEN_ENUM ||
        parser->current_token->type == TOKEN_CLASS) {
        TokenType ptok = parser->current_token->type;
        parser_eat(parser, ptok);
        if (parser->current_token->type == TOKEN_IDENTIFIER) {
            char* raw_name = parser->current_token->value;
            TemplateDef* tpl = find_template(raw_name);
            if (tpl != NULL && tpl->is_struct_or_class && parser_peek_token(parser)->type == TOKEN_LANGLE) {
                char* tpl_name = strdup(raw_name);
                parser_eat(parser, TOKEN_IDENTIFIER);
                parser_eat(parser, TOKEN_LANGLE);
                char* type_args[8];
                int arg_count = 0;
                while (parser->current_token->type != TOKEN_RANGLE && parser->current_token->type != TOKEN_EOF) {
                    type_args[arg_count++] = parser_parse_type(parser);
                    if (parser->current_token->type == TOKEN_COMMA) {
                        parser_eat(parser, TOKEN_COMMA);
                    }
                }
                parser_eat(parser, TOKEN_RANGLE);
                tname = instantiate_template(tpl_name, type_args, arg_count);
                for (int i = 0; i < arg_count; i++) free(type_args[i]);
                free(tpl_name);
            } else {
                tname = parser_parse_scoped_identifier(parser);
            }
        } else {
            tname = parser_parse_scoped_identifier(parser);
        }
    } else if (parser->current_token->type == TOKEN_INT ||
        parser->current_token->type == TOKEN_VOID ||
        parser->current_token->type == TOKEN_CHAR ||
        parser->current_token->type == TOKEN_BOOL ||
        parser->current_token->type == TOKEN_STRING_TYPE) {
        tname = strdup(parser->current_token->value);
        parser_eat(parser, parser->current_token->type);
    } else if (parser->current_token->type == TOKEN_IDENTIFIER) {
        char* raw_name = parser->current_token->value;
        TemplateDef* tpl = find_template(raw_name);
        if (tpl != NULL && tpl->is_struct_or_class && parser_peek_token(parser)->type == TOKEN_LANGLE) {
            char* tpl_name = strdup(raw_name);
            parser_eat(parser, TOKEN_IDENTIFIER);
            parser_eat(parser, TOKEN_LANGLE);
            char* type_args[8];
            int arg_count = 0;
            while (parser->current_token->type != TOKEN_RANGLE && parser->current_token->type != TOKEN_EOF) {
                type_args[arg_count++] = parser_parse_type(parser);
                if (parser->current_token->type == TOKEN_COMMA) {
                    parser_eat(parser, TOKEN_COMMA);
                }
            }
            parser_eat(parser, TOKEN_RANGLE);
            tname = instantiate_template(tpl_name, type_args, arg_count);
            for (int i = 0; i < arg_count; i++) free(type_args[i]);
            free(tpl_name);
        } else {
            tname = parser_parse_scoped_identifier(parser);
        }
    } else {
        printf("Error de sintaxis: se esperaba un tipo en la linea %d (se encontro '%s')\n",
               parser->current_token->line, parser->current_token->value);
        exit(1);
    }
    
    // Resolver alias de typedef si aplica (por ejemplo: entero -> int)
    const char* resolved = resolve_typedef(tname);
    if (resolved != NULL && strcmp(resolved, tname) != 0) {
        char* new_tname = strdup(resolved);
        free(tname);
        tname = new_tname;
    }
    
    // Soporte para tipos de punteros: T*, T**, etc.
    while (parser->current_token->type == TOKEN_STAR) {
        parser_eat(parser, TOKEN_STAR);
        char* with_star = malloc(strlen(tname) + 2);
        sprintf(with_star, "%s*", tname);
        free(tname);
        tname = with_star;
    }

    // Soporte para tipos de arreglo: T[N]
    if (parser->current_token->type == TOKEN_LBRACKET) {
        parser_eat(parser, TOKEN_LBRACKET);
        int sz = 0;
        if (parser->current_token->type == TOKEN_NUMBER) {
            sz = atoi(parser->current_token->value);
            parser_eat(parser, TOKEN_NUMBER);
        } else if (parser->current_token->type == TOKEN_IDENTIFIER) {
            int cval = 0;
            if (find_constant(parser->current_token->value, &cval)) {
                sz = cval;
            }
            parser_eat(parser, TOKEN_IDENTIFIER);
        }
        parser_eat(parser, TOKEN_RBRACKET);
        char buf[32];
        sprintf(buf, "[%d]", sz);
        char* with_bracket = malloc(strlen(tname) + strlen(buf) + 1);
        sprintf(with_bracket, "%s%s", tname, buf);
        free(tname);
        tname = with_bracket;
    }

    if (has_const) {
        char* with_const = malloc(strlen(tname) + 7);
        sprintf(with_const, "const %s", tname);
        free(tname);
        tname = with_const;
    }
    return tname;
}

static int is_type_start(Parser* parser) {
    TokenType t = parser->current_token->type;
    if (t == TOKEN_CONST) {
        Lexer saved_l = *(parser->lexer);
        Token* saved_t = init_token(parser->current_token->type, strdup(parser->current_token->value), parser->current_token->line);
        Token* next_t = lexer_get_next_token(parser->lexer);
        int is_type = 0;
        if (next_t->type == TOKEN_INT || next_t->type == TOKEN_CHAR || next_t->type == TOKEN_VOID ||
            next_t->type == TOKEN_BOOL || next_t->type == TOKEN_STRING_TYPE ||
            next_t->type == TOKEN_STRUCT || next_t->type == TOKEN_UNION || next_t->type == TOKEN_ENUM ||
            next_t->type == TOKEN_CLASS) {
            is_type = 1;
        } else if (next_t->type == TOKEN_IDENTIFIER) {
            if (is_known_struct(next_t->value) || is_known_enum(next_t->value) || is_known_typedef(next_t->value)) {
                is_type = 1;
            }
        }
        free(next_t->value); free(next_t);
        if (parser->peek_token != NULL) {
            free(parser->peek_token->value);
            free(parser->peek_token);
            parser->peek_token = NULL;
        }
        *(parser->lexer) = saved_l;
        free(parser->current_token->value); free(parser->current_token);
        parser->current_token = saved_t;
        return is_type;
    }
    if (t == TOKEN_INT || t == TOKEN_CHAR || t == TOKEN_VOID || t == TOKEN_BOOL || t == TOKEN_STRING_TYPE) {
        return 1;
    }
    if (t == TOKEN_STRUCT || t == TOKEN_UNION || t == TOKEN_ENUM || t == TOKEN_CLASS) {
        return 1;
    }
    if (t == TOKEN_IDENTIFIER) {
        if (is_known_struct(parser->current_token->value) ||
            is_known_enum(parser->current_token->value) ||
            is_known_typedef(parser->current_token->value)) {
            return 1;
        }
        if (is_known_namespace(parser->current_token->value)) {
            Lexer saved_lexer = *(parser->lexer);
            Token* saved_token = init_token(parser->current_token->type, strdup(parser->current_token->value), parser->current_token->line);
            
            Token* next_tok = lexer_get_next_token(parser->lexer);
            int is_type = 0;
            if (next_tok->type == TOKEN_COLON_COLON) {
                free(next_tok->value);
                free(next_tok);
                Token* struct_tok = lexer_get_next_token(parser->lexer);
                char full[256];
                sprintf(full, "%s::%s", saved_token->value, struct_tok->value);
                if (is_known_struct(full) || is_known_struct(struct_tok->value) ||
                    is_known_enum(full) || is_known_enum(struct_tok->value) ||
                    is_known_typedef(full) || is_known_typedef(struct_tok->value)) {
                    is_type = 1;
                }
                free(struct_tok->value);
                free(struct_tok);
            } else {
                free(next_tok->value);
                free(next_tok);
            }
            
            *(parser->lexer) = saved_lexer;
            free(parser->current_token->value);
            free(parser->current_token);
            parser->current_token = saved_token;
            
            return is_type;
        }
    }
    return 0;
}

static int is_struct_or_union_def(Parser* parser) {
    if (parser->current_token->type != TOKEN_STRUCT &&
        parser->current_token->type != TOKEN_UNION &&
        parser->current_token->type != TOKEN_CLASS) return 0;
    Lexer saved_l = *(parser->lexer);
    Token* saved_t = init_token(parser->current_token->type, strdup(parser->current_token->value), parser->current_token->line);
    Token* t1 = lexer_get_next_token(parser->lexer);
    int is_def = 0;
    if (t1->type == TOKEN_LBRACE) {
        is_def = 1;
    } else if (t1->type == TOKEN_IDENTIFIER || t1->type == TOKEN_STRING_TYPE) {
        Token* t2 = lexer_get_next_token(parser->lexer);
        if (t2->type == TOKEN_LBRACE) is_def = 1;
        free(t2->value); free(t2);
    }
    free(t1->value); free(t1);
    *(parser->lexer) = saved_l;
    free(parser->current_token->value); free(parser->current_token);
    parser->current_token = saved_t;
    return is_def;
}

static int is_enum_def(Parser* parser) {
    if (parser->current_token->type != TOKEN_ENUM) return 0;
    Lexer saved_l = *(parser->lexer);
    Token* saved_t = init_token(parser->current_token->type, strdup(parser->current_token->value), parser->current_token->line);
    Token* t1 = lexer_get_next_token(parser->lexer);
    int is_def = 0;
    if (t1->type == TOKEN_LBRACE) {
        is_def = 1;
    } else if (t1->type == TOKEN_IDENTIFIER) {
        Token* t2 = lexer_get_next_token(parser->lexer);
        if (t2->type == TOKEN_LBRACE) is_def = 1;
        free(t2->value); free(t2);
    }
    free(t1->value); free(t1);
    *(parser->lexer) = saved_l;
    free(parser->current_token->value); free(parser->current_token);
    parser->current_token = saved_t;
    return is_def;
}

static int is_imported(const char* name) {
    for (int i = 0; i < imported_files_count; i++) {
        if (strcmp(imported_files[i], name) == 0) return 1;
    }
    return 0;
}

static void add_imported(const char* name) {
    if (imported_files_count < MAX_IMPORTED_FILES) {
        imported_files[imported_files_count++] = strdup(name);
    }
}

static char* read_imported_source(const char* filepath) {
    if (strncmp(filepath, "embedded:", 9) == 0) {
        const char* em = get_embedded_file(filepath + 9, NULL);
        if (em) return strdup(em);
    }
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        const char* em = get_embedded_file(filepath, NULL);
        if (em) return strdup(em);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = malloc(length + 1);
    if (!buffer) { fclose(file); return NULL; }
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

ASTNode* parser_parse_import(Parser* parser) {
    int line = parser->current_token->line;
    TokenType dir_type = parser->current_token->type;
    parser_eat(parser, dir_type);
    
    int is_system = 0;
    char* import_name = NULL;
    if (parser->current_token->type == TOKEN_LANGLE) {
        is_system = 1;
        parser_eat(parser, TOKEN_LANGLE);
        char buf[256] = {0};
        while (parser->current_token->type != TOKEN_RANGLE && parser->current_token->type != TOKEN_EOF) {
            strcat(buf, parser->current_token->value);
            parser_eat(parser, parser->current_token->type);
        }
        parser_eat(parser, TOKEN_RANGLE);
        import_name = strdup(buf);
    } else if (parser->current_token->type == TOKEN_STRING) {
        is_system = 0;
        import_name = strdup(parser->current_token->value);
        parser_eat(parser, TOKEN_STRING);
    } else if (parser->current_token->type == TOKEN_IDENTIFIER) {
        import_name = parser_parse_scoped_identifier(parser);
    } else {
        printf("Error de sintaxis en directiva de inclusion en la linea %d\n", line);
        exit(1);
    }
    
    if (parser->current_token->type == TOKEN_SEMICOLON) {
        parser_eat(parser, TOKEN_SEMICOLON);
    }
    
    ASTNode* node = create_ast_node(AST_IMPORT, line);
    node->name = import_name;
    
    char import_path[512] = {0};
    FILE* check_file = NULL;

    if (is_system) {
        // Directivas <...>: buscar en embebidos, lib/ o paquetes instalados
        if (has_embedded_file(import_name)) {
            snprintf(import_path, sizeof(import_path), "embedded:%s", import_name);
        } else {
            char tmp_chk[256];
            snprintf(tmp_chk, sizeof(tmp_chk), "%s.huki", import_name);
            if (has_embedded_file(tmp_chk)) {
                snprintf(import_path, sizeof(import_path), "embedded:%s", tmp_chk);
            } else {
                snprintf(tmp_chk, sizeof(tmp_chk), "%s.suki", import_name);
                if (has_embedded_file(tmp_chk)) {
                    snprintf(import_path, sizeof(import_path), "embedded:%s", tmp_chk);
                } else {
                    snprintf(tmp_chk, sizeof(tmp_chk), "%s.hu", import_name);
                    if (has_embedded_file(tmp_chk)) {
                        snprintf(import_path, sizeof(import_path), "embedded:%s", tmp_chk);
                    } else {
                        snprintf(tmp_chk, sizeof(tmp_chk), "%s.su", import_name);
                        if (has_embedded_file(tmp_chk)) {
                            snprintf(import_path, sizeof(import_path), "embedded:%s", tmp_chk);
                        }
                    }
                }
            }
        }

        if (import_path[0] == '\0') {
            if (pkg_resolve_include(import_name, 1, import_path, sizeof(import_path))) {
                check_file = fopen(import_path, "r");
            }
        }

        if (check_file == NULL && import_path[0] == '\0') {
            if (strchr(import_name, '.') != NULL) {
                sprintf(import_path, "lib/%s", import_name);
                check_file = fopen(import_path, "r");
            } else {
                const char* lib_exts[] = {".huki", ".suki", ".hu", ".su", NULL};
                for (int e = 0; lib_exts[e] != NULL; e++) {
                    sprintf(import_path, "lib/%s%s", import_name, lib_exts[e]);
                    check_file = fopen(import_path, "r");
                    if (check_file) break;
                }
            }
        }
    } else {
        // Directivas con comillas "...": buscar localmente
        if (strchr(import_name, '.') != NULL) {
            sprintf(import_path, "%s", import_name);
            check_file = fopen(import_path, "r");
        } else {
            const char* loc_exts[] = {".huki", ".suki", ".hu", ".su", NULL};
            for (int e = 0; loc_exts[e] != NULL; e++) {
                sprintf(import_path, "%s%s", import_name, loc_exts[e]);
                check_file = fopen(import_path, "r");
                if (check_file) break;
            }
            if (!check_file) {
                sprintf(import_path, "%s", import_name);
                check_file = fopen(import_path, "r");
            }
        }

        if (check_file == NULL) {
            if (pkg_resolve_include(import_name, 0, import_path, sizeof(import_path))) {
                check_file = fopen(import_path, "r");
            } else if (has_embedded_file(import_name)) {
                snprintf(import_path, sizeof(import_path), "embedded:%s", import_name);
            }
        }
    }
    
    if (check_file != NULL || strncmp(import_path, "embedded:", 9) == 0) {
        if (check_file != NULL) fclose(check_file);
        
        if (strstr(import_path, ".huki") != NULL || strstr(import_path, ".hu") != NULL) {
            char comp_path[512];
            if (pkg_find_companion_source(import_path, comp_path, sizeof(comp_path))) {
                add_discovered_source(comp_path);
            } else {
                strcpy(comp_path, import_path);
                char* huki_ptr = strstr(comp_path, ".huki");
                if (huki_ptr) {
                    strcpy(huki_ptr, ".suki");
                } else {
                    char* hu_ptr = strstr(comp_path, ".hu");
                    if (hu_ptr) strcpy(hu_ptr, ".suki");
                }
                FILE* cf = fopen(comp_path, "r");
                if (cf) {
                    fclose(cf);
                    add_discovered_source(comp_path);
                } else if (strncmp(comp_path, "embedded:", 9) == 0 || has_embedded_file(comp_path)) {
                    char emb_comp[512];
                    snprintf(emb_comp, sizeof(emb_comp), "embedded:%s", comp_path + (strncmp(comp_path, "embedded:", 9) == 0 ? 9 : 0));
                    add_discovered_source(emb_comp);
                }
            }
        }
        
        if (!is_imported(import_path)) {
            add_imported(import_path);
            char* file_content = read_imported_source(import_path);
            if (file_content != NULL) {
                Lexer* sub_lexer = init_lexer(file_content);
                Parser* sub_parser = init_parser(sub_lexer);
                ASTNode* sub_prog = parser_parse(sub_parser);
                node->children = sub_prog->children;
                free(sub_parser);
                free(sub_lexer);
                free(file_content);
            }
        }
    } else {
        printf("Error: no se pudo encontrar el archivo de inclusion '%s' en la linea %d\n", import_name, line);
        exit(1);
    }
    
    return node;
}

ASTNode* parser_parse_using(Parser* parser) {
    int line = parser->current_token->line;
    parser_eat(parser, TOKEN_USING);
    parser_eat(parser, TOKEN_NAMESPACE);
    char* ns_name = parser_parse_scoped_identifier(parser);
    parser_eat(parser, TOKEN_SEMICOLON);
    
    ASTNode* node = create_ast_node(AST_USING, line);
    node->name = ns_name;
    return node;
}

ASTNode* parser_parse_namespace(Parser* parser) {
    int line = parser->current_token->line;
    parser_eat(parser, TOKEN_NAMESPACE);
    char* ns_name = parser_parse_scoped_identifier(parser);
    add_known_namespace(ns_name);
    
    char* prev_ns = parser->current_namespace;
    if (prev_ns != NULL) {
        char* nested_ns = malloc(strlen(prev_ns) + 2 + strlen(ns_name) + 1);
        sprintf(nested_ns, "%s::%s", prev_ns, ns_name);
        parser->current_namespace = nested_ns;
    } else {
        parser->current_namespace = strdup(ns_name);
    }
    
    parser_eat(parser, TOKEN_LBRACE);
    ASTNode* node = create_ast_node(AST_NAMESPACE, line);
    node->name = ns_name;
    
    ASTNode* last_decl = NULL;
    while (parser->current_token->type != TOKEN_RBRACE && parser->current_token->type != TOKEN_EOF) {
        ASTNode* decl = parser_parse_declaration(parser);
        if (decl != NULL) {
            if (node->children == NULL) {
                node->children = decl;
            } else {
                last_decl->next = decl;
            }
            last_decl = decl;
        }
    }
    parser_eat(parser, TOKEN_RBRACE);
    if (parser->current_token->type == TOKEN_SEMICOLON) {
        parser_eat(parser, TOKEN_SEMICOLON);
    }
    
    if (prev_ns != NULL) {
        free(parser->current_namespace);
        parser->current_namespace = prev_ns;
    } else {
        free(parser->current_namespace);
        parser->current_namespace = NULL;
    }
    
    return node;
}

static ASTNode* parser_parse_struct_common(Parser* parser, ASTNodeType node_type, int is_typedef) {
    int line = parser->current_token->line;
    TokenType start_tok = parser->current_token->type;
    parser_eat(parser, start_tok);
    
    char* struct_name = NULL;
    if (parser->current_token->type == TOKEN_IDENTIFIER || parser->current_token->type == TOKEN_STRING_TYPE) {
        struct_name = strdup(parser->current_token->value);
        parser_eat(parser, parser->current_token->type);
    } else if (parser->current_token->type == TOKEN_LBRACE) {
        static int anon_struct_counter = 0;
        char buf[64];
        sprintf(buf, "_anon_%s_%d", (node_type == AST_UNION ? "union" : "struct"), ++anon_struct_counter);
        struct_name = strdup(buf);
    } else {
        printf("Error de sintaxis: se esperaba nombre o '{' tras '%s' en la linea %d\n",
               (node_type == AST_UNION ? "union" : "struct"), line);
        exit(1);
    }
    
    parser_eat(parser, TOKEN_LBRACE);
    
    ASTNode* node = create_ast_node(node_type, line);
    node->name = struct_name;
    add_known_struct(struct_name);
    if (parser->current_namespace) {
        node->namespace_prefix = strdup(parser->current_namespace);
        char full[256];
        sprintf(full, "%s::%s", parser->current_namespace, struct_name);
        add_known_struct(full);
    }
    
    ASTNode* last_field = NULL;
    while (parser->current_token->type != TOKEN_RBRACE && parser->current_token->type != TOKEN_EOF) {
        int is_const = 0;
        while (parser->current_token->type == TOKEN_CONST || parser->current_token->type == TOKEN_VAR) {
            if (parser->current_token->type == TOKEN_CONST) is_const = 1;
            parser_eat(parser, parser->current_token->type);
        }
        int fline = parser->current_token->line;
        char* field_name = NULL;
        char* field_type = NULL;
        
        // Estilo C: int x; o char* s; o int arr[10]; o Point pt;
        if (is_type_start(parser)) {
            field_type = parser_parse_type(parser);
            if (parser->current_token->type == TOKEN_CONST) {
                parser_eat(parser, TOKEN_CONST);
                is_const = 1;
            }
            field_name = strdup(parser->current_token->value);
            parser_eat(parser, TOKEN_IDENTIFIER);
            if (parser->current_token->type == TOKEN_LBRACKET) {
                parser_eat(parser, TOKEN_LBRACKET);
                int asz = 0;
                if (parser->current_token->type == TOKEN_NUMBER) {
                    asz = atoi(parser->current_token->value);
                    parser_eat(parser, TOKEN_NUMBER);
                } else if (parser->current_token->type == TOKEN_IDENTIFIER) {
                    int cval = 0;
                    if (find_constant(parser->current_token->value, &cval)) {
                        asz = cval;
                    }
                    parser_eat(parser, TOKEN_IDENTIFIER);
                }
                parser_eat(parser, TOKEN_RBRACKET);
                char buf[32];
                sprintf(buf, "[%d]", asz);
                char* with_b = malloc(strlen(field_type) + strlen(buf) + 1);
                sprintf(with_b, "%s%s", field_type, buf);
                free(field_type);
                field_type = with_b;
            }
            parser_eat(parser, TOKEN_SEMICOLON);
        } else {
            // Estilo Itsuki original: var x: int; o x: int;
            field_name = strdup(parser->current_token->value);
            parser_eat(parser, TOKEN_IDENTIFIER);
            parser_eat(parser, TOKEN_COLON);
            if (parser->current_token->type == TOKEN_CONST) {
                parser_eat(parser, TOKEN_CONST);
                is_const = 1;
            }
            field_type = parser_parse_type(parser);
            parser_eat(parser, TOKEN_SEMICOLON);
        }
        
        if (is_const && field_type != NULL && strstr(field_type, "*") != NULL && strstr(field_type, "const") == NULL) {
            char* with_c = malloc(strlen(field_type) + 7);
            sprintf(with_c, "const %s", field_type);
            free(field_type);
            field_type = with_c;
        }
        
        ASTNode* field = create_ast_node(AST_STRUCT_FIELD, fline);
        field->name = field_name;
        field->type_name = field_type;
        field->is_const = is_const;
        field->access_level = ACCESS_PUBLIC;
        
        if (node->fields == NULL) {
            node->fields = field;
        } else {
            last_field->next = field;
        }
        last_field = field;
    }
    
    parser_eat(parser, TOKEN_RBRACE);
    if (!is_typedef) {
        if (parser->current_token->type == TOKEN_SEMICOLON) {
            parser_eat(parser, TOKEN_SEMICOLON);
        }
    }
    return node;
}

ASTNode* parser_parse_struct(Parser* parser) {
    return parser_parse_struct_common(parser, AST_STRUCT, 0);
}

ASTNode* parser_parse_union(Parser* parser) {
    return parser_parse_struct_common(parser, AST_UNION, 0);
}

ASTNode* parser_parse_enum(Parser* parser) {
    int line = parser->current_token->line;
    parser_eat(parser, TOKEN_ENUM);
    char* enum_name = NULL;
    if (parser->current_token->type == TOKEN_IDENTIFIER) {
        enum_name = strdup(parser->current_token->value);
        parser_eat(parser, TOKEN_IDENTIFIER);
    } else if (parser->current_token->type == TOKEN_LBRACE) {
        static int anon_enum_counter = 0;
        char buf[32];
        sprintf(buf, "_anon_enum_%d", ++anon_enum_counter);
        enum_name = strdup(buf);
    } else {
        printf("Error de sintaxis: se esperaba identificador o '{' tras 'enum' en la linea %d\n", line);
        exit(1);
    }
    
    add_known_enum(enum_name);
    if (parser->current_namespace) {
        char full[256];
        sprintf(full, "%s::%s", parser->current_namespace, enum_name);
        add_known_enum(full);
    }
    
    parser_eat(parser, TOKEN_LBRACE);
    
    ASTNode* enum_node = create_ast_node(AST_ENUM, line);
    enum_node->name = enum_name;
    
    ASTNode* last_item = NULL;
    int current_val = 0;
    while (parser->current_token->type != TOKEN_RBRACE && parser->current_token->type != TOKEN_EOF) {
        int iline = parser->current_token->line;
        char* item_name = strdup(parser->current_token->value);
        parser_eat(parser, TOKEN_IDENTIFIER);
        
        if (parser->current_token->type == TOKEN_ASSIGN) {
            parser_eat(parser, TOKEN_ASSIGN);
            int sign = 1;
            if (parser->current_token->type == TOKEN_MINUS) {
                sign = -1;
                parser_eat(parser, TOKEN_MINUS);
            }
            if (parser->current_token->type == TOKEN_NUMBER) {
                current_val = sign * atoi(parser->current_token->value);
                parser_eat(parser, TOKEN_NUMBER);
            }
        }
        
        ASTNode* item = create_ast_node(AST_ENUM_ITEM, iline);
        item->name = item_name;
        item->int_value = current_val;
        
        register_enum_constant(item_name, current_val);
        if (enum_name && enum_name[0] != '_') {
            char enum_item[256];
            snprintf(enum_item, sizeof(enum_item), "%s::%s", enum_name, item_name);
            register_enum_constant(enum_item, current_val);
        }
        if (parser->current_namespace) {
            char full_item[256];
            sprintf(full_item, "%s::%s", parser->current_namespace, item_name);
            register_enum_constant(full_item, current_val);
            if (enum_name && enum_name[0] != '_') {
                char full_enum_item[256];
                sprintf(full_enum_item, "%s::%s::%s", parser->current_namespace, enum_name, item_name);
                register_enum_constant(full_enum_item, current_val);
            }
        }
        
        current_val++;
        
        if (enum_node->fields == NULL) {
            enum_node->fields = item;
        } else {
            last_item->next = item;
        }
        last_item = item;
        
        if (parser->current_token->type == TOKEN_COMMA) {
            parser_eat(parser, TOKEN_COMMA);
        } else {
            break;
        }
    }
    
    parser_eat(parser, TOKEN_RBRACE);
    if (parser->current_token->type == TOKEN_SEMICOLON) {
        parser_eat(parser, TOKEN_SEMICOLON);
    }
    return enum_node;
}

ASTNode* parser_parse_typedef(Parser* parser) {
    int line = parser->current_token->line;
    parser_eat(parser, TOKEN_TYPEDEF);
    
    if (parser->current_token->type == TOKEN_STRUCT) {
        Lexer saved_l = *(parser->lexer);
        Token* saved_t = init_token(parser->current_token->type, strdup(parser->current_token->value), parser->current_token->line);
        Token* t1 = lexer_get_next_token(parser->lexer);
        int has_brace = 0;
        if (t1->type == TOKEN_LBRACE) {
            has_brace = 1;
        } else if (t1->type == TOKEN_IDENTIFIER) {
            Token* t2 = lexer_get_next_token(parser->lexer);
            if (t2->type == TOKEN_LBRACE) has_brace = 1;
            free(t2->value); free(t2);
        }
        free(t1->value); free(t1);
        if (parser->peek_token != NULL) {
            free(parser->peek_token->value);
            free(parser->peek_token);
            parser->peek_token = NULL;
        }
        *(parser->lexer) = saved_l;
        free(parser->current_token->value); free(parser->current_token);
        parser->current_token = saved_t;
        
        if (has_brace) {
            ASTNode* s_node = parser_parse_struct_common(parser, AST_STRUCT, 1);
            char* alias_name = strdup(parser->current_token->value);
            parser_eat(parser, TOKEN_IDENTIFIER);
            parser_eat(parser, TOKEN_SEMICOLON);
            
            register_typedef(alias_name, s_node->name);
            add_known_struct(alias_name);
            
            ASTNode* td_node = create_ast_node(AST_TYPEDEF, line);
            td_node->name = alias_name;
            td_node->type_name = strdup(s_node->name);
            td_node->children = s_node;
            return td_node;
        }
    } else if (parser->current_token->type == TOKEN_UNION) {
        Lexer saved_l = *(parser->lexer);
        Token* saved_t = init_token(parser->current_token->type, strdup(parser->current_token->value), parser->current_token->line);
        Token* t1 = lexer_get_next_token(parser->lexer);
        int has_brace = 0;
        if (t1->type == TOKEN_LBRACE) {
            has_brace = 1;
        } else if (t1->type == TOKEN_IDENTIFIER) {
            Token* t2 = lexer_get_next_token(parser->lexer);
            if (t2->type == TOKEN_LBRACE) has_brace = 1;
            free(t2->value); free(t2);
        }
        free(t1->value); free(t1);
        if (parser->peek_token != NULL) {
            free(parser->peek_token->value);
            free(parser->peek_token);
            parser->peek_token = NULL;
        }
        *(parser->lexer) = saved_l;
        free(parser->current_token->value); free(parser->current_token);
        parser->current_token = saved_t;
        
        if (has_brace) {
            ASTNode* u_node = parser_parse_struct_common(parser, AST_UNION, 1);
            char* alias_name = strdup(parser->current_token->value);
            parser_eat(parser, TOKEN_IDENTIFIER);
            parser_eat(parser, TOKEN_SEMICOLON);
            
            register_typedef(alias_name, u_node->name);
            add_known_struct(alias_name);
            
            ASTNode* td_node = create_ast_node(AST_TYPEDEF, line);
            td_node->name = alias_name;
            td_node->type_name = strdup(u_node->name);
            td_node->children = u_node;
            return td_node;
        }
    } else if (parser->current_token->type == TOKEN_ENUM) {
        Lexer saved_l = *(parser->lexer);
        Token* saved_t = init_token(parser->current_token->type, strdup(parser->current_token->value), parser->current_token->line);
        Token* t1 = lexer_get_next_token(parser->lexer);
        int has_brace = 0;
        if (t1->type == TOKEN_LBRACE) {
            has_brace = 1;
        } else if (t1->type == TOKEN_IDENTIFIER) {
            Token* t2 = lexer_get_next_token(parser->lexer);
            if (t2->type == TOKEN_LBRACE) has_brace = 1;
            free(t2->value); free(t2);
        }
        free(t1->value); free(t1);
        if (parser->peek_token != NULL) {
            free(parser->peek_token->value);
            free(parser->peek_token);
            parser->peek_token = NULL;
        }
        *(parser->lexer) = saved_l;
        free(parser->current_token->value); free(parser->current_token);
        parser->current_token = saved_t;
        
        if (has_brace) {
            ASTNode* e_node = parser_parse_enum(parser);
            char* alias_name = strdup(parser->current_token->value);
            parser_eat(parser, TOKEN_IDENTIFIER);
            parser_eat(parser, TOKEN_SEMICOLON);
            
            register_typedef(alias_name, "int");
            add_known_enum(alias_name);
            
            ASTNode* td_node = create_ast_node(AST_TYPEDEF, line);
            td_node->name = alias_name;
            td_node->type_name = strdup("int");
            td_node->children = e_node;
            return td_node;
        }
    }
    
    char* target_type = parser_parse_type(parser);
    char* alias_name = strdup(parser->current_token->value);
    parser_eat(parser, TOKEN_IDENTIFIER);
    parser_eat(parser, TOKEN_SEMICOLON);
    
    register_typedef(alias_name, target_type);
    if (is_known_struct(target_type)) {
        add_known_struct(alias_name);
    } else if (is_known_enum(target_type)) {
        add_known_enum(alias_name);
    }
    
    ASTNode* td_node = create_ast_node(AST_TYPEDEF, line);
    td_node->name = alias_name;
    td_node->type_name = target_type;
    return td_node;
}

ASTNode* parser_parse_block(Parser* parser) {
    int line = parser->current_token->line;
    ASTNode* block = create_ast_node(AST_BLOCK, line);
    ASTNode* last_stmt = NULL;
    
    while (parser->current_token->type != TOKEN_RBRACE && parser->current_token->type != TOKEN_EOF) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt != NULL) {
            if (block->statements == NULL) {
                block->statements = stmt;
            } else {
                last_stmt->next = stmt;
            }
            last_stmt = stmt;
        }
    }
    return block;
}

ASTNode* parser_parse_function(Parser* parser) {
    int line = parser->current_token->line;
    parser_eat(parser, TOKEN_FN);
    int is_inline = 0;
    int is_static = 0;
    while (parser->current_token->type == TOKEN_INLINE || parser->current_token->type == TOKEN_STATIC) {
        if (parser->current_token->type == TOKEN_INLINE) {
            parser_eat(parser, TOKEN_INLINE);
            is_inline = 1;
        } else if (parser->current_token->type == TOKEN_STATIC) {
            parser_eat(parser, TOKEN_STATIC);
            is_static = 1;
        }
    }
    char* func_name = strdup(parser->current_token->value);
    parser_eat(parser, TOKEN_IDENTIFIER);
    
    ASTNode* params_head = NULL;
    ASTNode* last_param = NULL;
    
    if (parser->current_token->type == TOKEN_LPAREN) {
        parser_eat(parser, TOKEN_LPAREN);
        while (parser->current_token->type != TOKEN_RPAREN && parser->current_token->type != TOKEN_EOF) {
            int pline = parser->current_token->line;
            if (parser->current_token->type == TOKEN_ELLIPSIS) {
                parser_eat(parser, TOKEN_ELLIPSIS);
                ASTNode* param = create_ast_node(AST_PARAM, pline);
                param->name = strdup("...");
                param->type_name = strdup("...");
                if (params_head == NULL) {
                    params_head = param;
                } else {
                    last_param->next = param;
                }
                last_param = param;
                break;
            }
            int is_const = 0;
            while (parser->current_token->type == TOKEN_CONST || parser->current_token->type == TOKEN_VAR) {
                if (parser->current_token->type == TOKEN_CONST) is_const = 1;
                parser_eat(parser, parser->current_token->type);
            }
            
            char* pname = NULL;
            char* ptype = NULL;
            
            if (is_type_start(parser)) {
                ptype = parser_parse_type(parser);
                if (parser->current_token->type == TOKEN_CONST) {
                    parser_eat(parser, TOKEN_CONST);
                    is_const = 1;
                }
                pname = strdup(parser->current_token->value);
                parser_eat(parser, TOKEN_IDENTIFIER);
            } else {
                pname = strdup(parser->current_token->value);
                parser_eat(parser, TOKEN_IDENTIFIER);
                parser_eat(parser, TOKEN_COLON);
                if (parser->current_token->type == TOKEN_CONST) {
                    parser_eat(parser, TOKEN_CONST);
                    is_const = 1;
                }
                ptype = parser_parse_type(parser);
            }
            
            if (is_const && ptype != NULL && strstr(ptype, "*") != NULL && strstr(ptype, "const") == NULL) {
                char* with_c = malloc(strlen(ptype) + 7);
                sprintf(with_c, "const %s", ptype);
                free(ptype);
                ptype = with_c;
            }
            
            ASTNode* param = create_ast_node(AST_PARAM, pline);
            param->name = pname;
            param->type_name = ptype;
            param->is_const = is_const;
            
            if (params_head == NULL) {
                params_head = param;
            } else {
                last_param->next = param;
            }
            last_param = param;
            
            if (parser->current_token->type == TOKEN_COMMA) {
                parser_eat(parser, TOKEN_COMMA);
            }
        }
        parser_eat(parser, TOKEN_RPAREN);
    } else {
        printf("Error de sintaxis: se esperaba '(' en la declaracion de la funcion '%s' en la linea %d\n",
               func_name, line);
        exit(1);
    }
    
    char* return_type = NULL;
    if (parser->current_token->type == TOKEN_ARROW) {
        parser_eat(parser, TOKEN_ARROW);
        return_type = parser_parse_type(parser);
    } else {
        printf("Error de sintaxis: se esperaba '->' seguido del tipo de retorno en la funcion '%s' en la linea %d\n",
               func_name, line);
        exit(1);
    }
    
    ASTNode* body = NULL;
    if (parser->current_token->type == TOKEN_SEMICOLON) {
        parser_eat(parser, TOKEN_SEMICOLON);
    } else {
        parser_eat(parser, TOKEN_LBRACE);
        body = parser_parse_block(parser);
        parser_eat(parser, TOKEN_RBRACE);
    }
    
    ASTNode* func = create_ast_node(AST_FUNCTION_DECLARATION, line);
    func->name = func_name;
    func->type_name = return_type;
    func->params = params_head;
    func->body = body;
    func->is_inline = is_inline;
    func->is_static = is_static;
    if (parser->current_namespace) {
        func->namespace_prefix = strdup(parser->current_namespace);
    }
    return func;
}

static ASTNode* parser_parse_class(Parser* parser) {
    int line = parser->current_token->line;
    parser_eat(parser, TOKEN_CLASS);

    char* class_name = strdup(parser->current_token->value);
    parser_eat(parser, TOKEN_IDENTIFIER);

    char* base_name = NULL;
    int base_access = ACCESS_PUBLIC;

    // Herencia opcional: class Derivada : [public|private|protected] Base
    if (parser->current_token->type == TOKEN_COLON) {
        parser_eat(parser, TOKEN_COLON);
        if (parser->current_token->type == TOKEN_PUBLIC) {
            base_access = ACCESS_PUBLIC;
            parser_eat(parser, TOKEN_PUBLIC);
        } else if (parser->current_token->type == TOKEN_PRIVATE) {
            base_access = ACCESS_PRIVATE;
            parser_eat(parser, TOKEN_PRIVATE);
        } else if (parser->current_token->type == TOKEN_PROTECTED) {
            base_access = ACCESS_PROTECTED;
            parser_eat(parser, TOKEN_PROTECTED);
        }
        base_name = parser_parse_scoped_identifier(parser);
    }

    add_known_struct(class_name);
    if (parser->current_namespace) {
        char full[256];
        sprintf(full, "%s::%s", parser->current_namespace, class_name);
        add_known_struct(full);
    }

    parser_eat(parser, TOKEN_LBRACE);

    ASTNode* class_node = create_ast_node(AST_CLASS, line);
    class_node->name = class_name;
    class_node->base_class = base_name;
    class_node->access_level = base_access;
    if (parser->current_namespace) {
        class_node->namespace_prefix = strdup(parser->current_namespace);
    }

    int current_access = ACCESS_PRIVATE; // Por defecto en C++ class es private

    ASTNode* fields_head = NULL;
    ASTNode* last_field = NULL;
    ASTNode* methods_head = NULL;
    ASTNode* last_method = NULL;

    while (parser->current_token->type != TOKEN_RBRACE && parser->current_token->type != TOKEN_EOF) {
        // Especificadores de acceso
        if (parser->current_token->type == TOKEN_PUBLIC) {
            parser_eat(parser, TOKEN_PUBLIC);
            if (parser->current_token->type == TOKEN_COLON) parser_eat(parser, TOKEN_COLON);
            current_access = ACCESS_PUBLIC;
            continue;
        }
        if (parser->current_token->type == TOKEN_PRIVATE) {
            parser_eat(parser, TOKEN_PRIVATE);
            if (parser->current_token->type == TOKEN_COLON) parser_eat(parser, TOKEN_COLON);
            current_access = ACCESS_PRIVATE;
            continue;
        }
        if (parser->current_token->type == TOKEN_PROTECTED) {
            parser_eat(parser, TOKEN_PROTECTED);
            if (parser->current_token->type == TOKEN_COLON) parser_eat(parser, TOKEN_COLON);
            current_access = ACCESS_PROTECTED;
            continue;
        }

        int mline = parser->current_token->line;
        int is_method = 0;
        int is_dtor = 0;
        int is_ctor = 0;

        if (parser->current_token->type == TOKEN_FN) {
            is_method = 1;
        } else if (parser->current_token->type == TOKEN_TILDE) {
            is_method = 1;
            is_dtor = 1;
        } else if (parser->current_token->type == TOKEN_IDENTIFIER &&
                   (strcmp(parser->current_token->value, class_name) == 0 ||
                    strcmp(parser->current_token->value, "constructor") == 0)) {
            Lexer saved_l = *(parser->lexer);
            Token* saved_t = init_token(parser->current_token->type, strdup(parser->current_token->value), parser->current_token->line);
            Token* next_t = lexer_get_next_token(parser->lexer);
            if (next_t->type == TOKEN_LPAREN) {
                is_method = 1;
                is_ctor = 1;
            }
            free(next_t->value); free(next_t);
            *(parser->lexer) = saved_l;
            free(parser->current_token->value); free(parser->current_token);
            parser->current_token = saved_t;
        } else if (is_type_start(parser)) {
            Lexer saved_l = *(parser->lexer);
            Token* saved_t = init_token(parser->current_token->type, strdup(parser->current_token->value), parser->current_token->line);
            Token* t1 = lexer_get_next_token(parser->lexer);
            while (t1->type == TOKEN_STAR) {
                free(t1->value); free(t1);
                t1 = lexer_get_next_token(parser->lexer);
            }
            if (t1->type == TOKEN_IDENTIFIER) {
                Token* t2 = lexer_get_next_token(parser->lexer);
                if (t2->type == TOKEN_LPAREN) {
                    is_method = 1;
                }
                free(t2->value); free(t2);
            }
            free(t1->value); free(t1);
            *(parser->lexer) = saved_l;
            free(parser->current_token->value); free(parser->current_token);
            parser->current_token = saved_t;
        }

        if (is_method) {
            ASTNode* method = NULL;
            if (is_dtor) {
                parser_eat(parser, TOKEN_TILDE);
                char* dname = strdup(parser->current_token->value);
                parser_eat(parser, TOKEN_IDENTIFIER);
                parser_eat(parser, TOKEN_LPAREN);
                parser_eat(parser, TOKEN_RPAREN);
                parser_eat(parser, TOKEN_LBRACE);
                ASTNode* body = parser_parse_block(parser);
                parser_eat(parser, TOKEN_RBRACE);
                if (parser->current_token->type == TOKEN_SEMICOLON) parser_eat(parser, TOKEN_SEMICOLON);

                char dtor_full[256];
                sprintf(dtor_full, "~%s", dname);
                method = create_ast_node(AST_FUNCTION_DECLARATION, mline);
                method->name = strdup(dtor_full);
                method->type_name = strdup("void");
                method->body = body;
                method->is_destructor = 1;
                free(dname);
            } else if (is_ctor) {
                char* ctor_name = strdup(parser->current_token->value);
                parser_eat(parser, TOKEN_IDENTIFIER);
                parser_eat(parser, TOKEN_LPAREN);
                ASTNode* params_head = NULL;
                ASTNode* last_param = NULL;
                while (parser->current_token->type != TOKEN_RPAREN && parser->current_token->type != TOKEN_EOF) {
                    int pline = parser->current_token->line;
                    char* pname = NULL;
                    char* ptype = NULL;
                    if (is_type_start(parser)) {
                        ptype = parser_parse_type(parser);
                        pname = strdup(parser->current_token->value);
                        parser_eat(parser, TOKEN_IDENTIFIER);
                    } else {
                        if (parser->current_token->type == TOKEN_VAR) parser_eat(parser, TOKEN_VAR);
                        pname = strdup(parser->current_token->value);
                        parser_eat(parser, TOKEN_IDENTIFIER);
                        parser_eat(parser, TOKEN_COLON);
                        ptype = parser_parse_type(parser);
                    }
                    ASTNode* p = create_ast_node(AST_PARAM, pline);
                    p->name = pname;
                    p->type_name = ptype;
                    if (params_head == NULL) params_head = p;
                    else last_param->next = p;
                    last_param = p;
                    if (parser->current_token->type == TOKEN_COMMA) parser_eat(parser, TOKEN_COMMA);
                }
                parser_eat(parser, TOKEN_RPAREN);
                parser_eat(parser, TOKEN_LBRACE);
                ASTNode* body = parser_parse_block(parser);
                parser_eat(parser, TOKEN_RBRACE);
                if (parser->current_token->type == TOKEN_SEMICOLON) parser_eat(parser, TOKEN_SEMICOLON);

                method = create_ast_node(AST_FUNCTION_DECLARATION, mline);
                method->name = ctor_name;
                method->type_name = strdup("void");
                method->params = params_head;
                method->body = body;
                method->is_constructor = 1;
            } else if (parser->current_token->type == TOKEN_FN) {
                method = parser_parse_function(parser);
                if (strcmp(method->name, class_name) == 0 || strcmp(method->name, "constructor") == 0) {
                    method->is_constructor = 1;
                } else if (method->name[0] == '~' || strcmp(method->name, "destructor") == 0) {
                    method->is_destructor = 1;
                }
            } else {
                char* ret_type = parser_parse_type(parser);
                char* mname = strdup(parser->current_token->value);
                parser_eat(parser, TOKEN_IDENTIFIER);
                parser_eat(parser, TOKEN_LPAREN);
                ASTNode* params_head = NULL;
                ASTNode* last_param = NULL;
                while (parser->current_token->type != TOKEN_RPAREN && parser->current_token->type != TOKEN_EOF) {
                    int pline = parser->current_token->line;
                    char* pname = NULL;
                    char* ptype = NULL;
                    if (is_type_start(parser)) {
                        ptype = parser_parse_type(parser);
                        pname = strdup(parser->current_token->value);
                        parser_eat(parser, TOKEN_IDENTIFIER);
                    } else {
                        if (parser->current_token->type == TOKEN_VAR) parser_eat(parser, TOKEN_VAR);
                        pname = strdup(parser->current_token->value);
                        parser_eat(parser, TOKEN_IDENTIFIER);
                        parser_eat(parser, TOKEN_COLON);
                        ptype = parser_parse_type(parser);
                    }
                    ASTNode* p = create_ast_node(AST_PARAM, pline);
                    p->name = pname;
                    p->type_name = ptype;
                    if (params_head == NULL) params_head = p;
                    else last_param->next = p;
                    last_param = p;
                    if (parser->current_token->type == TOKEN_COMMA) parser_eat(parser, TOKEN_COMMA);
                }
                parser_eat(parser, TOKEN_RPAREN);
                parser_eat(parser, TOKEN_LBRACE);
                ASTNode* body = parser_parse_block(parser);
                parser_eat(parser, TOKEN_RBRACE);
                if (parser->current_token->type == TOKEN_SEMICOLON) parser_eat(parser, TOKEN_SEMICOLON);

                method = create_ast_node(AST_FUNCTION_DECLARATION, mline);
                method->name = mname;
                method->type_name = ret_type;
                method->params = params_head;
                method->body = body;
            }

            // Inyectar 'this' como primer parámetro del método
            ASTNode* this_param = create_ast_node(AST_PARAM, mline);
            this_param->name = strdup("this");
            char this_t[256];
            sprintf(this_t, "%s*", class_name);
            this_param->type_name = strdup(this_t);
            this_param->next = method->params;
            method->params = this_param;

            method->namespace_prefix = strdup(class_name);
            method->access_level = current_access;

            if (methods_head == NULL) methods_head = method;
            else last_method->next = method;
            last_method = method;

        } else {
            // Campo de clase (atributo)
            int is_const = 0;
            while (parser->current_token->type == TOKEN_CONST || parser->current_token->type == TOKEN_VAR) {
                if (parser->current_token->type == TOKEN_CONST) is_const = 1;
                parser_eat(parser, parser->current_token->type);
            }
            int fline = parser->current_token->line;
            char* field_name = NULL;
            char* field_type = NULL;

            if (is_type_start(parser)) {
                field_type = parser_parse_type(parser);
                if (parser->current_token->type == TOKEN_CONST) {
                    parser_eat(parser, TOKEN_CONST);
                    is_const = 1;
                }
                field_name = strdup(parser->current_token->value);
                parser_eat(parser, TOKEN_IDENTIFIER);
                if (parser->current_token->type == TOKEN_LBRACKET) {
                    parser_eat(parser, TOKEN_LBRACKET);
                    int asz = 0;
                    if (parser->current_token->type == TOKEN_NUMBER) {
                        asz = atoi(parser->current_token->value);
                        parser_eat(parser, TOKEN_NUMBER);
                    } else if (parser->current_token->type == TOKEN_IDENTIFIER) {
                        int cval = 0;
                        if (find_constant(parser->current_token->value, &cval)) asz = cval;
                        parser_eat(parser, TOKEN_IDENTIFIER);
                    }
                    parser_eat(parser, TOKEN_RBRACKET);
                    char buf[32];
                    sprintf(buf, "[%d]", asz);
                    char* with_b = malloc(strlen(field_type) + strlen(buf) + 1);
                    sprintf(with_b, "%s%s", field_type, buf);
                    free(field_type);
                    field_type = with_b;
                }
                parser_eat(parser, TOKEN_SEMICOLON);
            } else {
                field_name = strdup(parser->current_token->value);
                parser_eat(parser, TOKEN_IDENTIFIER);
                parser_eat(parser, TOKEN_COLON);
                if (parser->current_token->type == TOKEN_CONST) {
                    parser_eat(parser, TOKEN_CONST);
                    is_const = 1;
                }
                field_type = parser_parse_type(parser);
                parser_eat(parser, TOKEN_SEMICOLON);
            }

            ASTNode* field = create_ast_node(AST_STRUCT_FIELD, fline);
            field->name = field_name;
            field->type_name = field_type;
            field->is_const = is_const;
            field->access_level = current_access;

            if (fields_head == NULL) fields_head = field;
            else last_field->next = field;
            last_field = field;
        }
    }

    parser_eat(parser, TOKEN_RBRACE);
    if (parser->current_token->type == TOKEN_SEMICOLON) {
        parser_eat(parser, TOKEN_SEMICOLON);
    }

    class_node->fields = fields_head;
    class_node->body = methods_head;

    return class_node;
}

static ASTNode* parser_parse_statement_or_block(Parser* parser);

static ASTNode* parser_parse_primary(Parser* parser) {
    int line = parser->current_token->line;
    if (parser->current_token->type == TOKEN_NUMBER) {
        ASTNode* node = create_ast_node(AST_NUMBER, line);
        node->int_value = atoi(parser->current_token->value);
        parser_eat(parser, TOKEN_NUMBER);
        return node;
    }
    if (parser->current_token->type == TOKEN_TRUE) {
        ASTNode* node = create_ast_node(AST_BOOLEAN, line);
        node->int_value = 1;
        parser_eat(parser, TOKEN_TRUE);
        return node;
    }
    if (parser->current_token->type == TOKEN_FALSE) {
        ASTNode* node = create_ast_node(AST_BOOLEAN, line);
        node->int_value = 0;
        parser_eat(parser, TOKEN_FALSE);
        return node;
    }
    if (parser->current_token->type == TOKEN_NULL || parser->current_token->type == TOKEN_NULLPTR) {
        ASTNode* node = create_ast_node(AST_NULL, line);
        node->int_value = 0;
        node->name = strdup(parser->current_token->value);
        parser_eat(parser, parser->current_token->type);
        return node;
    }
    if (parser->current_token->type == TOKEN_THIS) {
        ASTNode* node = create_ast_node(AST_THIS, line);
        node->name = strdup("this");
        parser_eat(parser, TOKEN_THIS);
        return node;
    }
    if (parser->current_token->type == TOKEN_STRING) {
        ASTNode* node = create_ast_node(AST_STRING, line);
        node->string_value = strdup(parser->current_token->value);
        parser_eat(parser, TOKEN_STRING);
        return node;
    }
    if (parser->current_token->type == TOKEN_LPAREN) {
        parser_eat(parser, TOKEN_LPAREN);
        ASTNode* expr = parser_parse_expression(parser);
        parser_eat(parser, TOKEN_RPAREN);
        return expr;
    }
    if (parser->current_token->type == TOKEN_IDENTIFIER) {
        TemplateDef* tpl = find_template(parser->current_token->value);
        if (tpl != NULL && parser_peek_token(parser)->type == TOKEN_LANGLE) {
            char* tpl_name = strdup(parser->current_token->value);
            parser_eat(parser, TOKEN_IDENTIFIER);
            parser_eat(parser, TOKEN_LANGLE);
            char* type_args[8];
            int arg_count = 0;
            while (parser->current_token->type != TOKEN_RANGLE && parser->current_token->type != TOKEN_EOF) {
                type_args[arg_count++] = parser_parse_type(parser);
                if (parser->current_token->type == TOKEN_COMMA) {
                    parser_eat(parser, TOKEN_COMMA);
                }
            }
            parser_eat(parser, TOKEN_RANGLE);
            char* spec_name = instantiate_template(tpl_name, type_args, arg_count);
            for (int i = 0; i < arg_count; i++) free(type_args[i]);
            free(tpl_name);

            ASTNode* node = create_ast_node(AST_IDENTIFIER, line);
            node->name = spec_name;
            return node;
        }

        char* name = parser_parse_scoped_identifier(parser);
        ASTNode* node;
        if (strstr(name, "::") != NULL) {
            node = create_ast_node(AST_SCOPED_IDENTIFIER, line);
        } else {
            node = create_ast_node(AST_IDENTIFIER, line);
        }
        node->name = name;
        return node;
    }
    printf("Error de sintaxis: expresion inesperada en la linea %d ('%s')\n",
           parser->current_token->line, parser->current_token->value);
    exit(1);
}

static ASTNode* parser_parse_postfix(Parser* parser) {
    ASTNode* expr = parser_parse_primary(parser);
    while (parser->current_token->type == TOKEN_LPAREN || 
           parser->current_token->type == TOKEN_DOT ||
           parser->current_token->type == TOKEN_ARROW ||
           parser->current_token->type == TOKEN_LBRACKET ||
           parser->current_token->type == TOKEN_INC ||
           parser->current_token->type == TOKEN_DEC) {
        int line = parser->current_token->line;
        if (parser->current_token->type == TOKEN_LPAREN) {
            parser_eat(parser, TOKEN_LPAREN);
            ASTNode* args_head = NULL;
            ASTNode* last_arg = NULL;
            
            while (parser->current_token->type != TOKEN_RPAREN && parser->current_token->type != TOKEN_EOF) {
                ASTNode* arg = parser_parse_expression(parser);
                if (args_head == NULL) {
                    args_head = arg;
                } else {
                    last_arg->next = arg;
                }
                last_arg = arg;
                
                if (parser->current_token->type == TOKEN_COMMA) {
                    parser_eat(parser, TOKEN_COMMA);
                }
            }
            parser_eat(parser, TOKEN_RPAREN);
            ASTNode* call_node = create_ast_node(AST_CALL_EXPR, line);
            call_node->callee = expr;
            call_node->args = args_head;
            expr = call_node;
        } else if (parser->current_token->type == TOKEN_DOT || parser->current_token->type == TOKEN_ARROW) {
            TokenType access_tok = parser->current_token->type;
            parser_eat(parser, access_tok);
            char* mname = NULL;
            if (parser->current_token->type == TOKEN_TILDE) {
                parser_eat(parser, TOKEN_TILDE);
                char dtor_buf[128];
                sprintf(dtor_buf, "~%s", parser->current_token->value);
                mname = strdup(dtor_buf);
                parser_eat(parser, TOKEN_IDENTIFIER);
            } else {
                mname = strdup(parser->current_token->value);
                parser_eat(parser, TOKEN_IDENTIFIER);
            }
            ASTNode* mem_node = create_ast_node(AST_MEMBER_EXPR, line);
            if (access_tok == TOKEN_ARROW) {
                ASTNode* deref = create_ast_node(AST_UNARY_EXPR, line);
                deref->op = '*';
                deref->left = expr;
                mem_node->left = deref;
            } else {
                mem_node->left = expr;
            }
            mem_node->name = mname;
            expr = mem_node;
        } else if (parser->current_token->type == TOKEN_LBRACKET) {
            parser_eat(parser, TOKEN_LBRACKET);
            ASTNode* index_expr = parser_parse_expression(parser);
            parser_eat(parser, TOKEN_RBRACKET);
            ASTNode* idx_node = create_ast_node(AST_INDEX_EXPR, line);
            idx_node->left = expr;
            idx_node->expr = index_expr;
            expr = idx_node;
        } else if (parser->current_token->type == TOKEN_INC || parser->current_token->type == TOKEN_DEC) {
            TokenType t = parser->current_token->type;
            parser_eat(parser, t);
            ASTNode* inc_node = create_ast_node(AST_INC_DEC, line);
            inc_node->left = expr;
            strcpy(inc_node->op_str, (t == TOKEN_INC) ? "++" : "--");
            inc_node->int_value = 1; // postfix
            expr = inc_node;
        }
    }
    return expr;
}

static ASTNode* parser_parse_unary(Parser* parser) {
    int line = parser->current_token->line;
    if (parser->current_token->type == TOKEN_AMPERSAND) {
        parser_eat(parser, TOKEN_AMPERSAND);
        ASTNode* operand = parser_parse_unary(parser);
        ASTNode* u = create_ast_node(AST_UNARY_EXPR, line);
        u->op = '&';
        u->left = operand;
        return u;
    }
    if (parser->current_token->type == TOKEN_STAR) {
        parser_eat(parser, TOKEN_STAR);
        ASTNode* operand = parser_parse_unary(parser);
        ASTNode* u = create_ast_node(AST_UNARY_EXPR, line);
        u->op = '*';
        u->left = operand;
        return u;
    }
    if (parser->current_token->type == TOKEN_MINUS) {
        parser_eat(parser, TOKEN_MINUS);
        ASTNode* operand = parser_parse_unary(parser);
        ASTNode* u = create_ast_node(AST_UNARY_EXPR, line);
        u->op = '-';
        u->left = operand;
        return u;
    }
    if (parser->current_token->type == TOKEN_BANG) {
        parser_eat(parser, TOKEN_BANG);
        ASTNode* operand = parser_parse_unary(parser);
        ASTNode* u = create_ast_node(AST_UNARY_EXPR, line);
        u->op = '!';
        strcpy(u->op_str, "!");
        u->left = operand;
        return u;
    }
    if (parser->current_token->type == TOKEN_TILDE) {
        parser_eat(parser, TOKEN_TILDE);
        ASTNode* operand = parser_parse_unary(parser);
        ASTNode* u = create_ast_node(AST_UNARY_EXPR, line);
        u->op = '~';
        strcpy(u->op_str, "~");
        u->left = operand;
        return u;
    }
    if (parser->current_token->type == TOKEN_INC || parser->current_token->type == TOKEN_DEC) {
        TokenType t = parser->current_token->type;
        parser_eat(parser, t);
        ASTNode* operand = parser_parse_unary(parser);
        ASTNode* node = create_ast_node(AST_INC_DEC, line);
        strcpy(node->op_str, (t == TOKEN_INC) ? "++" : "--");
        node->left = operand;
        node->int_value = 0; // prefix
        return node;
    }
    return parser_parse_postfix(parser);
}

static ASTNode* parser_parse_multiplicative(Parser* parser) {
    ASTNode* left = parser_parse_unary(parser);
    while (parser->current_token->type == TOKEN_STAR ||
           parser->current_token->type == TOKEN_SLASH ||
           parser->current_token->type == TOKEN_MOD) {
        int line = parser->current_token->line;
        TokenType op_type = parser->current_token->type;
        parser_eat(parser, op_type);
        ASTNode* right = parser_parse_unary(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        if (op_type == TOKEN_STAR) { bin->op = '*'; strcpy(bin->op_str, "*"); }
        else if (op_type == TOKEN_SLASH) { bin->op = '/'; strcpy(bin->op_str, "/"); }
        else { bin->op = '%'; strcpy(bin->op_str, "%"); }
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_additive(Parser* parser) {
    ASTNode* left = parser_parse_multiplicative(parser);
    while (parser->current_token->type == TOKEN_PLUS || parser->current_token->type == TOKEN_MINUS) {
        int line = parser->current_token->line;
        TokenType op_type = parser->current_token->type;
        parser_eat(parser, op_type);
        ASTNode* right = parser_parse_multiplicative(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        bin->op = (op_type == TOKEN_PLUS) ? '+' : '-';
        strcpy(bin->op_str, (op_type == TOKEN_PLUS) ? "+" : "-");
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_shift(Parser* parser) {
    ASTNode* left = parser_parse_additive(parser);
    while (parser->current_token->type == TOKEN_SHL || parser->current_token->type == TOKEN_SHR) {
        int line = parser->current_token->line;
        TokenType op_type = parser->current_token->type;
        parser_eat(parser, op_type);
        ASTNode* right = parser_parse_additive(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        if (op_type == TOKEN_SHL) { bin->op = 'S'; strcpy(bin->op_str, "<<"); }
        else { bin->op = 'R'; strcpy(bin->op_str, ">>"); }
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_relational(Parser* parser) {
    ASTNode* left = parser_parse_shift(parser);
    while (parser->current_token->type == TOKEN_LANGLE ||
           parser->current_token->type == TOKEN_RANGLE ||
           parser->current_token->type == TOKEN_LE ||
           parser->current_token->type == TOKEN_GE) {
        int line = parser->current_token->line;
        TokenType op_type = parser->current_token->type;
        parser_eat(parser, op_type);
        ASTNode* right = parser_parse_shift(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        if (op_type == TOKEN_LANGLE) { bin->op = '<'; strcpy(bin->op_str, "<"); }
        else if (op_type == TOKEN_RANGLE) { bin->op = '>'; strcpy(bin->op_str, ">"); }
        else if (op_type == TOKEN_LE) { bin->op = 'L'; strcpy(bin->op_str, "<="); }
        else { bin->op = 'G'; strcpy(bin->op_str, ">="); }
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_equality(Parser* parser) {
    ASTNode* left = parser_parse_relational(parser);
    while (parser->current_token->type == TOKEN_EQ || parser->current_token->type == TOKEN_NEQ) {
        int line = parser->current_token->line;
        TokenType op_type = parser->current_token->type;
        parser_eat(parser, op_type);
        ASTNode* right = parser_parse_relational(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        if (op_type == TOKEN_EQ) { bin->op = 'E'; strcpy(bin->op_str, "=="); }
        else { bin->op = 'N'; strcpy(bin->op_str, "!="); }
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_bitwise_and(Parser* parser) {
    ASTNode* left = parser_parse_equality(parser);
    while (parser->current_token->type == TOKEN_AMPERSAND) {
        int line = parser->current_token->line;
        parser_eat(parser, TOKEN_AMPERSAND);
        ASTNode* right = parser_parse_equality(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        bin->op = '&';
        strcpy(bin->op_str, "&");
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_bitwise_xor(Parser* parser) {
    ASTNode* left = parser_parse_bitwise_and(parser);
    while (parser->current_token->type == TOKEN_CARET) {
        int line = parser->current_token->line;
        parser_eat(parser, TOKEN_CARET);
        ASTNode* right = parser_parse_bitwise_and(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        bin->op = '^';
        strcpy(bin->op_str, "^");
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_bitwise_or(Parser* parser) {
    ASTNode* left = parser_parse_bitwise_xor(parser);
    while (parser->current_token->type == TOKEN_PIPE) {
        int line = parser->current_token->line;
        parser_eat(parser, TOKEN_PIPE);
        ASTNode* right = parser_parse_bitwise_xor(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        bin->op = '|';
        strcpy(bin->op_str, "|");
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_logical_and(Parser* parser) {
    ASTNode* left = parser_parse_bitwise_or(parser);
    while (parser->current_token->type == TOKEN_AND) {
        int line = parser->current_token->line;
        parser_eat(parser, TOKEN_AND);
        ASTNode* right = parser_parse_bitwise_or(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        bin->op = 'A';
        strcpy(bin->op_str, "&&");
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_logical_or(Parser* parser) {
    ASTNode* left = parser_parse_logical_and(parser);
    while (parser->current_token->type == TOKEN_OR) {
        int line = parser->current_token->line;
        parser_eat(parser, TOKEN_OR);
        ASTNode* right = parser_parse_logical_and(parser);
        ASTNode* bin = create_ast_node(AST_BINARY_EXPR, line);
        bin->left = left;
        bin->right = right;
        bin->op = 'O';
        strcpy(bin->op_str, "||");
        left = bin;
    }
    return left;
}

static ASTNode* parser_parse_conditional(Parser* parser) {
    ASTNode* cond = parser_parse_logical_or(parser);
    if (parser->current_token->type == TOKEN_QUESTION) {
        int line = parser->current_token->line;
        parser_eat(parser, TOKEN_QUESTION);
        ASTNode* then_expr = parser_parse_expression(parser);
        parser_eat(parser, TOKEN_COLON);
        ASTNode* else_expr = parser_parse_conditional(parser);
        ASTNode* ternary = create_ast_node(AST_TERNARY, line);
        ternary->expr = cond;
        ternary->left = then_expr;
        ternary->right = else_expr;
        return ternary;
    }
    return cond;
}

static ASTNode* parser_parse_expression(Parser* parser) {
    ASTNode* left = parser_parse_conditional(parser);
    int is_assignable = (left->type == AST_IDENTIFIER || 
                         left->type == AST_MEMBER_EXPR ||
                         left->type == AST_INDEX_EXPR ||
                         (left->type == AST_UNARY_EXPR && left->op == '*'));
    if (is_assignable) {
        TokenType t = parser->current_token->type;
        if (t == TOKEN_ASSIGN) {
            int line = parser->current_token->line;
            parser_eat(parser, TOKEN_ASSIGN);
            ASTNode* value = parser_parse_expression(parser);
            ASTNode* assign = create_ast_node(AST_ASSIGNMENT, line);
            assign->left = left;
            assign->init_value = value;
            return assign;
        } else if (t == TOKEN_PLUS_ASSIGN || t == TOKEN_MINUS_ASSIGN ||
                   t == TOKEN_STAR_ASSIGN || t == TOKEN_SLASH_ASSIGN ||
                   t == TOKEN_MOD_ASSIGN || t == TOKEN_AMP_ASSIGN ||
                   t == TOKEN_PIPE_ASSIGN || t == TOKEN_CARET_ASSIGN ||
                   t == TOKEN_SHL_ASSIGN || t == TOKEN_SHR_ASSIGN) {
            int line = parser->current_token->line;
            parser_eat(parser, t);
            ASTNode* value = parser_parse_expression(parser);
            ASTNode* assign = create_ast_node(AST_COMPOUND_ASSIGNMENT, line);
            assign->left = left;
            assign->init_value = value;
            if (t == TOKEN_PLUS_ASSIGN) strcpy(assign->op_str, "+=");
            else if (t == TOKEN_MINUS_ASSIGN) strcpy(assign->op_str, "-=");
            else if (t == TOKEN_STAR_ASSIGN) strcpy(assign->op_str, "*=");
            else if (t == TOKEN_SLASH_ASSIGN) strcpy(assign->op_str, "/=");
            else if (t == TOKEN_MOD_ASSIGN) strcpy(assign->op_str, "%=");
            else if (t == TOKEN_AMP_ASSIGN) strcpy(assign->op_str, "&=");
            else if (t == TOKEN_PIPE_ASSIGN) strcpy(assign->op_str, "|=");
            else if (t == TOKEN_CARET_ASSIGN) strcpy(assign->op_str, "^=");
            else if (t == TOKEN_SHL_ASSIGN) strcpy(assign->op_str, "<<=");
            else strcpy(assign->op_str, ">>=");
            return assign;
        }
    }
    return left;
}

static ASTNode* parser_parse_var_declaration(Parser* parser) {
    int line = parser->current_token->line;
    int is_const = 0;
    int is_static = 0;
    
    while (parser->current_token->type == TOKEN_CONST || parser->current_token->type == TOKEN_VAR || parser->current_token->type == TOKEN_STATIC) {
        if (parser->current_token->type == TOKEN_CONST) is_const = 1;
        if (parser->current_token->type == TOKEN_STATIC) is_static = 1;
        parser_eat(parser, parser->current_token->type);
    }
    
    char* var_name = NULL;
    char* vtype = NULL;
    
    if (is_type_start(parser)) {
        vtype = parser_parse_type(parser);
        while (parser->current_token->type == TOKEN_CONST || parser->current_token->type == TOKEN_STATIC) {
            if (parser->current_token->type == TOKEN_CONST) is_const = 1;
            if (parser->current_token->type == TOKEN_STATIC) is_static = 1;
            parser_eat(parser, parser->current_token->type);
        }
        if (parser->current_token->type != TOKEN_IDENTIFIER) {
            printf("Error de sintaxis: se esperaba identificador de variable tras el tipo '%s' en la linea %d\n",
                   vtype, parser->current_token->line);
            exit(1);
        }
        var_name = strdup(parser->current_token->value);
        parser_eat(parser, TOKEN_IDENTIFIER);
    } else if (parser->current_token->type == TOKEN_IDENTIFIER) {
        var_name = strdup(parser->current_token->value);
        parser_eat(parser, TOKEN_IDENTIFIER);
        if (parser->current_token->type == TOKEN_COLON) {
            parser_eat(parser, TOKEN_COLON);
            while (parser->current_token->type == TOKEN_CONST || parser->current_token->type == TOKEN_STATIC) {
                if (parser->current_token->type == TOKEN_CONST) is_const = 1;
                if (parser->current_token->type == TOKEN_STATIC) is_static = 1;
                parser_eat(parser, parser->current_token->type);
            }
            vtype = parser_parse_type(parser);
        }
    } else {
        printf("Error de sintaxis: se esperaba nombre o tipo de variable en la linea %d (se encontro '%s')\n",
               parser->current_token->line, parser->current_token->value);
        exit(1);
    }
    
    int is_array = 0;
    int array_size = 0;
    if (parser->current_token->type == TOKEN_LBRACKET) {
        parser_eat(parser, TOKEN_LBRACKET);
        if (parser->current_token->type == TOKEN_NUMBER) {
            array_size = atoi(parser->current_token->value);
            parser_eat(parser, TOKEN_NUMBER);
        } else if (parser->current_token->type == TOKEN_IDENTIFIER) {
            int cval = 0;
            if (find_constant(parser->current_token->value, &cval)) {
                array_size = cval;
            }
            parser_eat(parser, TOKEN_IDENTIFIER);
        }
        parser_eat(parser, TOKEN_RBRACKET);
        is_array = 1;
    } else if (vtype && strstr(vtype, "[") != NULL) {
        is_array = 1;
        char* b = strchr(vtype, '[');
        array_size = atoi(b + 1);
    }
    
    ASTNode* init_expr = NULL;
    if (parser->current_token->type == TOKEN_ASSIGN) {
        parser_eat(parser, TOKEN_ASSIGN);
        init_expr = parser_parse_expression(parser);
    } else if (parser->current_token->type == TOKEN_LPAREN) {
        int pline = parser->current_token->line;
        parser_eat(parser, TOKEN_LPAREN);
        ASTNode* args_head = NULL;
        ASTNode* last_arg = NULL;
        while (parser->current_token->type != TOKEN_RPAREN && parser->current_token->type != TOKEN_EOF) {
            ASTNode* arg = parser_parse_expression(parser);
            if (args_head == NULL) args_head = arg;
            else last_arg->next = arg;
            last_arg = arg;
            if (parser->current_token->type == TOKEN_COMMA) parser_eat(parser, TOKEN_COMMA);
        }
        parser_eat(parser, TOKEN_RPAREN);
        ASTNode* ctor_call = create_ast_node(AST_CALL_EXPR, pline);
        ASTNode* callee = create_ast_node(AST_IDENTIFIER, pline);
        callee->name = strdup(vtype ? vtype : "auto");
        ctor_call->callee = callee;
        ctor_call->args = args_head;
        init_expr = ctor_call;
    }
    parser_eat(parser, TOKEN_SEMICOLON);
    
    if (is_const && init_expr == NULL) {
        printf("Error de sintaxis: la variable constante '%s' debe ser inicializada en la linea %d\n", var_name, line);
        exit(1);
    }
    
    if (vtype == NULL) {
        if (init_expr && init_expr->type == AST_STRING) {
            vtype = strdup("fundamental::string");
        } else if (init_expr && init_expr->type == AST_NULL) {
            vtype = strdup("void*");
        } else {
            vtype = strdup("int");
        }
    }
    
    int cval = 0;
    if (is_const && init_expr && eval_const_expr(init_expr, &cval)) {
        register_constant(var_name, cval);
        if (parser->current_namespace) {
            char full_ns[256];
            sprintf(full_ns, "%s::%s", parser->current_namespace, var_name);
            register_constant(full_ns, cval);
        }
    }
    
    if (is_const && vtype != NULL && strstr(vtype, "*") != NULL && strstr(vtype, "const") == NULL) {
        char* with_c = malloc(strlen(vtype) + 7);
        sprintf(with_c, "const %s", vtype);
        free(vtype);
        vtype = with_c;
    }
    
    ASTNode* vdecl = create_ast_node(AST_VAR_DECLARATION, line);
    vdecl->name = var_name;
    vdecl->type_name = vtype;
    vdecl->init_value = init_expr;
    vdecl->is_array = is_array;
    vdecl->array_size = array_size;
    vdecl->is_const = is_const;
    vdecl->is_static = is_static;
    if (is_const && init_expr && eval_const_expr(init_expr, &cval)) {
        vdecl->int_value = cval;
    }
    return vdecl;
}

static ASTNode* parser_parse_asm_or_c(Parser* parser) {
    int line = parser->current_token->line;
    TokenType tok_type = parser->current_token->type;
    parser_eat(parser, tok_type);
    
    if (parser->current_token->type == TOKEN_IDENTIFIER &&
        (strcmp(parser->current_token->value, "volatile") == 0 ||
         strcmp(parser->current_token->value, "__volatile__") == 0 ||
         strcmp(parser->current_token->value, "__volatile") == 0)) {
        parser_eat(parser, TOKEN_IDENTIFIER);
    }
    
    parser_eat(parser, TOKEN_LPAREN);
    
    size_t cap = 512;
    size_t len = 0;
    char* code_str = malloc(cap);
    code_str[0] = '\0';
    
    while (parser->current_token->type == TOKEN_STRING) {
        size_t slen = strlen(parser->current_token->value);
        if (len + slen + 2 >= cap) {
            cap = (len + slen + 2) * 2;
            code_str = realloc(code_str, cap);
        }
        strcpy(code_str + len, parser->current_token->value);
        len += slen;
        parser_eat(parser, TOKEN_STRING);
    }
    
    while (parser->current_token->type != TOKEN_RPAREN && parser->current_token->type != TOKEN_EOF) {
        parser_eat(parser, parser->current_token->type);
    }
    parser_eat(parser, TOKEN_RPAREN);
    
    if (parser->current_token->type == TOKEN_SEMICOLON) {
        parser_eat(parser, TOKEN_SEMICOLON);
    }
    
    ASTNode* node = create_ast_node((tok_type == TOKEN_C_CODE ? AST_INLINE_C : AST_INLINE_ASM), line);
    node->string_value = code_str;
    return node;
}

static ASTNode* parser_parse_template_declaration(Parser* parser) {
    int line = parser->current_token->line;
    parser_eat(parser, TOKEN_TEMPLATE);
    parser_eat(parser, TOKEN_LANGLE);
    
    char params[8][64];
    int pcount = 0;
    while (parser->current_token->type != TOKEN_RANGLE && parser->current_token->type != TOKEN_EOF) {
        if (parser->current_token->type == TOKEN_TYPENAME || parser->current_token->type == TOKEN_CLASS) {
            parser_eat(parser, parser->current_token->type);
        }
        if (parser->current_token->type == TOKEN_IDENTIFIER) {
            if (pcount < 8) {
                strncpy(params[pcount], parser->current_token->value, 63);
                params[pcount][63] = '\0';
                add_known_struct(params[pcount]);
                pcount++;
            }
            parser_eat(parser, TOKEN_IDENTIFIER);
        } else {
            printf("Error de sintaxis: se esperaba identificador de parametro de plantilla en la linea %d\n", parser->current_token->line);
            exit(1);
        }
        if (parser->current_token->type == TOKEN_COMMA) {
            parser_eat(parser, TOKEN_COMMA);
        }
    }
    parser_eat(parser, TOKEN_RANGLE);
    
    ASTNode* decl = NULL;
    int is_sc = 0;
    if (parser->current_token->type == TOKEN_STRUCT) {
        decl = parser_parse_struct(parser);
        is_sc = 1;
    } else if (parser->current_token->type == TOKEN_CLASS) {
        decl = parser_parse_class(parser);
        is_sc = 2;
    } else if (parser->current_token->type == TOKEN_UNION) {
        decl = parser_parse_union(parser);
        is_sc = 1;
    } else if (parser->current_token->type == TOKEN_FN) {
        decl = parser_parse_function(parser);
        is_sc = 0;
    } else if (is_type_start(parser)) {
        char* ret_type = parser_parse_type(parser);
        if (parser->current_token->type == TOKEN_IDENTIFIER) {
            char* fname = strdup(parser->current_token->value);
            parser_eat(parser, TOKEN_IDENTIFIER);
            if (parser->current_token->type == TOKEN_LPAREN) {
                parser_eat(parser, TOKEN_LPAREN);
                ASTNode* params_head = NULL;
                ASTNode* last_param = NULL;
                while (parser->current_token->type != TOKEN_RPAREN && parser->current_token->type != TOKEN_EOF) {
                    int pline = parser->current_token->line;
                    char* ptype = parser_parse_type(parser);
                    char* pname = strdup(parser->current_token->value);
                    parser_eat(parser, TOKEN_IDENTIFIER);
                    ASTNode* p = create_ast_node(AST_PARAM, pline);
                    p->name = pname;
                    p->type_name = ptype;
                    if (params_head == NULL) params_head = p;
                    else last_param->next = p;
                    last_param = p;
                    if (parser->current_token->type == TOKEN_COMMA) parser_eat(parser, TOKEN_COMMA);
                }
                parser_eat(parser, TOKEN_RPAREN);
                parser_eat(parser, TOKEN_LBRACE);
                ASTNode* body = parser_parse_block(parser);
                parser_eat(parser, TOKEN_RBRACE);
                
                decl = create_ast_node(AST_FUNCTION_DECLARATION, line);
                decl->name = fname;
                decl->type_name = ret_type;
                decl->params = params_head;
                decl->body = body;
                is_sc = 0;
            } else {
                printf("Error de sintaxis: se esperaba '(' tras identificador en la funcion de plantilla en linea %d\n", line);
                exit(1);
            }
        } else {
            printf("Error de sintaxis: se esperaba nombre de funcion tras tipo en plantilla en linea %d\n", line);
            exit(1);
        }
    } else {
        decl = parser_parse_declaration(parser);
        if (decl != NULL) {
            is_sc = (decl->type == AST_CLASS ? 2 : (decl->type == AST_STRUCT || decl->type == AST_UNION ? 1 : 0));
        }
    }
    
    if (decl == NULL || decl->name == NULL) {
        printf("Error de sintaxis: declaracion de plantilla invalida en la linea %d\n", line);
        exit(1);
    }
    
    TemplateDef* tpl = malloc(sizeof(TemplateDef));
    strncpy(tpl->name, decl->name, 127);
    tpl->name[127] = '\0';
    tpl->is_struct_or_class = is_sc;
    tpl->param_count = pcount;
    for (int i = 0; i < pcount; i++) {
        snprintf(tpl->param_names[i], sizeof(tpl->param_names[i]), "%.63s", params[i]);
        tpl->param_names[i][63] = '\0';
    }
    tpl->ast_template = decl;
    tpl->next = template_table;
    template_table = tpl;
    
    return NULL;
}

static ASTNode* parser_parse_statement(Parser* parser) {
    int line = parser->current_token->line;
    
    if (parser->current_token->type == TOKEN_SEMICOLON) {
        parser_eat(parser, TOKEN_SEMICOLON);
        return NULL;
    }
    if (parser->current_token->type == TOKEN_ASM || parser->current_token->type == TOKEN_C_CODE) {
        return parser_parse_asm_or_c(parser);
    }
    if (is_struct_or_union_def(parser)) {
        if (parser->current_token->type == TOKEN_STRUCT) return parser_parse_struct(parser);
        else return parser_parse_union(parser);
    }
    if (is_enum_def(parser)) {
        return parser_parse_enum(parser);
    }
    if (parser->current_token->type == TOKEN_TYPEDEF) {
        return parser_parse_typedef(parser);
    }
    
    // Bloque: { sentencias... }
    if (parser->current_token->type == TOKEN_LBRACE) {
        parser_eat(parser, TOKEN_LBRACE);
        ASTNode* blk = parser_parse_block(parser);
        parser_eat(parser, TOKEN_RBRACE);
        return blk;
    }
    
    // if (cond) then_branch [else else_branch]
    if (parser->current_token->type == TOKEN_IF) {
        parser_eat(parser, TOKEN_IF);
        parser_eat(parser, TOKEN_LPAREN);
        ASTNode* cond = parser_parse_expression(parser);
        parser_eat(parser, TOKEN_RPAREN);
        ASTNode* then_branch = parser_parse_statement_or_block(parser);
        ASTNode* else_branch = NULL;
        if (parser->current_token->type == TOKEN_ELSE) {
            parser_eat(parser, TOKEN_ELSE);
            else_branch = parser_parse_statement_or_block(parser);
        }
        ASTNode* if_node = create_ast_node(AST_IF, line);
        if_node->expr = cond;
        if_node->body = then_branch;
        if_node->else_branch = else_branch;
        return if_node;
    }
    
    // while (cond) body
    if (parser->current_token->type == TOKEN_WHILE) {
        parser_eat(parser, TOKEN_WHILE);
        parser_eat(parser, TOKEN_LPAREN);
        ASTNode* cond = parser_parse_expression(parser);
        parser_eat(parser, TOKEN_RPAREN);
        ASTNode* body = parser_parse_statement_or_block(parser);
        ASTNode* while_node = create_ast_node(AST_WHILE, line);
        while_node->expr = cond;
        while_node->body = body;
        return while_node;
    }
    
    // for (init; cond; update) body
    if (parser->current_token->type == TOKEN_FOR) {
        parser_eat(parser, TOKEN_FOR);
        parser_eat(parser, TOKEN_LPAREN);
        ASTNode* init_stmt = NULL;
        if (parser->current_token->type != TOKEN_SEMICOLON) {
            init_stmt = parser_parse_statement(parser);
        } else {
            parser_eat(parser, TOKEN_SEMICOLON);
        }
        ASTNode* cond_expr = NULL;
        if (parser->current_token->type != TOKEN_SEMICOLON) {
            cond_expr = parser_parse_expression(parser);
        }
        parser_eat(parser, TOKEN_SEMICOLON);
        ASTNode* update_expr = NULL;
        if (parser->current_token->type != TOKEN_RPAREN) {
            update_expr = parser_parse_expression(parser);
        }
        parser_eat(parser, TOKEN_RPAREN);
        ASTNode* body = parser_parse_statement_or_block(parser);
        ASTNode* for_node = create_ast_node(AST_FOR, line);
        for_node->init_value = init_stmt;
        for_node->expr = cond_expr;
        for_node->update_expr = update_expr;
        for_node->body = body;
        return for_node;
    }

    // do body while (cond);
    if (parser->current_token->type == TOKEN_DO) {
        parser_eat(parser, TOKEN_DO);
        ASTNode* body = parser_parse_statement_or_block(parser);
        parser_eat(parser, TOKEN_WHILE);
        parser_eat(parser, TOKEN_LPAREN);
        ASTNode* cond = parser_parse_expression(parser);
        parser_eat(parser, TOKEN_RPAREN);
        parser_eat(parser, TOKEN_SEMICOLON);
        ASTNode* do_node = create_ast_node(AST_DO_WHILE, line);
        do_node->body = body;
        do_node->expr = cond;
        return do_node;
    }

    // break;
    if (parser->current_token->type == TOKEN_BREAK) {
        parser_eat(parser, TOKEN_BREAK);
        parser_eat(parser, TOKEN_SEMICOLON);
        return create_ast_node(AST_BREAK, line);
    }

    // continue;
    if (parser->current_token->type == TOKEN_CONTINUE) {
        parser_eat(parser, TOKEN_CONTINUE);
        parser_eat(parser, TOKEN_SEMICOLON);
        return create_ast_node(AST_CONTINUE, line);
    }

    // switch (expr) { case V: ... default: ... }
    if (parser->current_token->type == TOKEN_SWITCH) {
        parser_eat(parser, TOKEN_SWITCH);
        parser_eat(parser, TOKEN_LPAREN);
        ASTNode* expr = parser_parse_expression(parser);
        parser_eat(parser, TOKEN_RPAREN);
        parser_eat(parser, TOKEN_LBRACE);
        
        ASTNode* cases_head = NULL;
        ASTNode* last_case = NULL;
        
        while (parser->current_token->type != TOKEN_RBRACE && parser->current_token->type != TOKEN_EOF) {
            int cline = parser->current_token->line;
            if (parser->current_token->type == TOKEN_CASE) {
                parser_eat(parser, TOKEN_CASE);
                int case_val = 0;
                int sign = 1;
                if (parser->current_token->type == TOKEN_MINUS) {
                    parser_eat(parser, TOKEN_MINUS);
                    sign = -1;
                }
                if (parser->current_token->type == TOKEN_NUMBER) {
                    case_val = sign * atoi(parser->current_token->value);
                    parser_eat(parser, TOKEN_NUMBER);
                } else if (parser->current_token->type == TOKEN_IDENTIFIER) {
                    int enum_val = 0;
                    if (find_enum_constant(parser->current_token->value, &enum_val)) {
                        case_val = sign * enum_val;
                    }
                    parser_eat(parser, TOKEN_IDENTIFIER);
                }
                parser_eat(parser, TOKEN_COLON);
                
                ASTNode* block = create_ast_node(AST_BLOCK, cline);
                ASTNode* last_stmt = NULL;
                while (parser->current_token->type != TOKEN_CASE &&
                       parser->current_token->type != TOKEN_DEFAULT &&
                       parser->current_token->type != TOKEN_RBRACE &&
                       parser->current_token->type != TOKEN_EOF) {
                    ASTNode* stmt = parser_parse_statement(parser);
                    if (stmt != NULL) {
                        if (block->statements == NULL) block->statements = stmt;
                        else last_stmt->next = stmt;
                        last_stmt = stmt;
                    }
                }
                ASTNode* case_node = create_ast_node(AST_CASE, cline);
                case_node->int_value = case_val;
                case_node->body = block;
                
                if (cases_head == NULL) cases_head = case_node;
                else last_case->next = case_node;
                last_case = case_node;
            } else if (parser->current_token->type == TOKEN_DEFAULT) {
                parser_eat(parser, TOKEN_DEFAULT);
                parser_eat(parser, TOKEN_COLON);
                
                ASTNode* block = create_ast_node(AST_BLOCK, cline);
                ASTNode* last_stmt = NULL;
                while (parser->current_token->type != TOKEN_CASE &&
                       parser->current_token->type != TOKEN_DEFAULT &&
                       parser->current_token->type != TOKEN_RBRACE &&
                       parser->current_token->type != TOKEN_EOF) {
                    ASTNode* stmt = parser_parse_statement(parser);
                    if (stmt != NULL) {
                        if (block->statements == NULL) block->statements = stmt;
                        else last_stmt->next = stmt;
                        last_stmt = stmt;
                    }
                }
                ASTNode* def_node = create_ast_node(AST_DEFAULT, cline);
                def_node->body = block;
                
                if (cases_head == NULL) cases_head = def_node;
                else last_case->next = def_node;
                last_case = def_node;
            } else {
                printf("Error de sintaxis: se esperaba 'case' o 'default' en la linea %d\n", parser->current_token->line);
                exit(1);
            }
        }
        parser_eat(parser, TOKEN_RBRACE);
        
        ASTNode* switch_node = create_ast_node(AST_SWITCH, line);
        switch_node->expr = expr;
        switch_node->body = cases_head;
        return switch_node;
    }

    if (parser->current_token->type == TOKEN_VAR ||
        parser->current_token->type == TOKEN_CONST ||
        parser->current_token->type == TOKEN_STATIC ||
        is_type_start(parser)) {
        return parser_parse_var_declaration(parser);
    }
    
    if (parser->current_token->type == TOKEN_RETURN) {
        parser_eat(parser, TOKEN_RETURN);
        ASTNode* ret_val = NULL;
        if (parser->current_token->type != TOKEN_SEMICOLON) {
            ret_val = parser_parse_expression(parser);
        }
        parser_eat(parser, TOKEN_SEMICOLON);
        
        ASTNode* rstmt = create_ast_node(AST_RETURN_STATEMENT, line);
        rstmt->return_value = ret_val;
        return rstmt;
    }
    
    ASTNode* expr = parser_parse_expression(parser);
    parser_eat(parser, TOKEN_SEMICOLON);
    
    ASTNode* estmt = create_ast_node(AST_EXPR_STATEMENT, line);
    estmt->expr = expr;
    return estmt;
}

static ASTNode* parser_parse_statement_or_block(Parser* parser) {
    if (parser->current_token->type == TOKEN_LBRACE) {
        parser_eat(parser, TOKEN_LBRACE);
        ASTNode* blk = parser_parse_block(parser);
        parser_eat(parser, TOKEN_RBRACE);
        return blk;
    }
    return parser_parse_statement(parser);
}

static ASTNode* parser_parse_declaration(Parser* parser) {
    if (parser->current_token->type == TOKEN_SEMICOLON) {
        parser_eat(parser, TOKEN_SEMICOLON);
        return NULL;
    }
    if (parser->current_token->type == TOKEN_ASM || parser->current_token->type == TOKEN_C_CODE) {
        return parser_parse_asm_or_c(parser);
    }
    if (parser->current_token->type == TOKEN_TEMPLATE) {
        return parser_parse_template_declaration(parser);
    }
    if (parser->current_token->type == TOKEN_IMPORT || parser->current_token->type == TOKEN_INCLUDE) {
        return parser_parse_import(parser);
    }
    if (parser->current_token->type == TOKEN_USING) {
        return parser_parse_using(parser);
    }
    if (parser->current_token->type == TOKEN_NAMESPACE) {
        return parser_parse_namespace(parser);
    }
    if (parser->current_token->type == TOKEN_CLASS) {
        return parser_parse_class(parser);
    }
    if (parser->current_token->type == TOKEN_STRUCT) {
        return parser_parse_struct(parser);
    }
    if (parser->current_token->type == TOKEN_UNION) {
        return parser_parse_union(parser);
    }
    if (parser->current_token->type == TOKEN_ENUM) {
        return parser_parse_enum(parser);
    }
    if (parser->current_token->type == TOKEN_TYPEDEF) {
        return parser_parse_typedef(parser);
    }
    int is_inline = 0;
    int is_extern = 0;
    int is_static = 0;
    while (parser->current_token->type == TOKEN_INLINE ||
           parser->current_token->type == TOKEN_EXTERN ||
           parser->current_token->type == TOKEN_STATIC) {
        if (parser->current_token->type == TOKEN_INLINE) {
            parser_eat(parser, TOKEN_INLINE);
            is_inline = 1;
        } else if (parser->current_token->type == TOKEN_EXTERN) {
            parser_eat(parser, TOKEN_EXTERN);
            is_extern = 1;
            if (parser->current_token->type == TOKEN_STRING) {
                // Soporte para extern "C" o extern "cdecl"
                parser_eat(parser, TOKEN_STRING);
            }
            if (parser->current_token->type == TOKEN_LBRACE) {
                parser_eat(parser, TOKEN_LBRACE);
                ASTNode* head = NULL;
                ASTNode* tail = NULL;
                while (parser->current_token->type != TOKEN_RBRACE && parser->current_token->type != TOKEN_EOF) {
                    ASTNode* inner = parser_parse_declaration(parser);
                    if (inner != NULL) {
                        ASTNode* cur = inner;
                        while (cur != NULL) {
                            cur->is_extern = 1;
                            if (head == NULL) head = cur;
                            else tail->next = cur;
                            tail = cur;
                            cur = cur->next;
                        }
                    }
                }
                parser_eat(parser, TOKEN_RBRACE);
                return head;
            }
        } else if (parser->current_token->type == TOKEN_STATIC) {
            parser_eat(parser, TOKEN_STATIC);
            is_static = 1;
        }
    }
    if (parser->current_token->type == TOKEN_FN) {
        ASTNode* func = parser_parse_function(parser);
        if (is_inline) {
            func->is_inline = 1;
        }
        if (is_extern) {
            func->is_extern = 1;
        }
        if (is_static) {
            func->is_static = 1;
        }
        return func;
    }
    
    if (parser->current_token->type == TOKEN_CONST ||
        parser->current_token->type == TOKEN_VAR ||
        parser->current_token->type == TOKEN_STATIC ||
        is_type_start(parser)) {
        ASTNode* vdecl = parser_parse_var_declaration(parser);
        if (is_extern) {
            vdecl->is_extern = 1;
        }
        if (is_static) {
            vdecl->is_static = 1;
        }
        if (parser->current_namespace) {
            vdecl->namespace_prefix = strdup(parser->current_namespace);
        }
        return vdecl;
    }
    
    printf("Error de sintaxis: declaracion inesperada '%s' en la linea %d\n",
           parser->current_token->value, parser->current_token->line);
    exit(1);
}

ASTNode* parser_parse(Parser* parser) {
    ASTNode* program = create_ast_node(AST_PROGRAM, 1);
    ASTNode* last_decl = NULL;
    
    while (parser->current_token->type != TOKEN_EOF) {
        ASTNode* decl = parser_parse_declaration(parser);
        if (decl != NULL) {
            if (program->children == NULL) {
                program->children = decl;
            } else {
                last_decl->next = decl;
            }
            while (decl->next != NULL) {
                decl = decl->next;
            }
            last_decl = decl;
        }
    }

    // Flush any pending template instantiations
    while (pending_instantiations_head != NULL) {
        ASTNode* inst = pending_instantiations_head;
        pending_instantiations_head = NULL;
        pending_instantiations_tail = NULL;
        
        while (inst != NULL) {
            ASTNode* next_inst = inst->next;
            inst->next = NULL;
            if (program->children == NULL) {
                program->children = inst;
            } else {
                last_decl->next = inst;
            }
            while (inst->next != NULL) {
                inst = inst->next;
            }
            last_decl = inst;
            inst = next_inst;
        }
    }
    
    return program;
}
