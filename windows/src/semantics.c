#include "semantics.h"
#include "arena.h"
#include "diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SemanticSymbol {
    char* name;
    char* type_name;
    int is_const;
    int is_func;
    int is_struct;
    int is_param;
    int param_count;
    char** param_types;
    int line;
    int col;
    struct SemanticSymbol* next;
} SemanticSymbol;

typedef struct SemanticScope {
    struct SemanticScope* parent;
    SemanticSymbol* symbols;
    char* name;
} SemanticScope;

typedef struct SemanticContext {
    SemanticScope* current_scope;
    SemanticScope* global_scope;
    const char* current_func_name;
    const char* current_func_return_type;
    int error_count;
    const char* filename;
    const char* source_code;
} SemanticContext;

static SemanticScope* scope_create(SemanticScope* parent, const char* name) {
    SemanticScope* s = c_calloc(1, sizeof(SemanticScope));
    s->parent = parent;
    s->name = name ? c_strdup(name) : NULL;
    return s;
}

static void scope_add_symbol(SemanticScope* scope, SemanticSymbol* sym) {
    if (!scope || !sym) return;
    sym->next = scope->symbols;
    scope->symbols = sym;
}

static SemanticSymbol* scope_lookup_current(SemanticScope* scope, const char* name) {
    if (!scope || !name) return NULL;
    SemanticSymbol* curr = scope->symbols;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

static SemanticSymbol* scope_lookup(SemanticScope* scope, const char* name) {
    SemanticScope* curr_scope = scope;
    while (curr_scope) {
        SemanticSymbol* sym = scope_lookup_current(curr_scope, name);
        if (sym) return sym;
        curr_scope = curr_scope->parent;
    }
    return NULL;
}

static void sem_error(SemanticContext* ctx, int line, int col, int len, const char* msg, const char* suggestion) {
    ctx->error_count++;
    diag_error(ctx->filename, ctx->source_code, line, col > 0 ? col : 1, len > 0 ? len : 1, msg, suggestion);
}

static void register_builtin_functions(SemanticContext* ctx) {
    const char* builtins[] = {
        "printf", "puts", "putchar", "getchar", "print", "println",
        "fundamental::printf", "fundamental::puts", "fundamental::putchar",
        "fundamental::getchar", "fundamental::print", "fundamental::println",
        "malloc", "free", "calloc", "realloc", "memset", "memcpy", "memmove",
        "fundamental::malloc", "fundamental::free", "fundamental::calloc",
        "fundamental::realloc", "fundamental::memset", "fundamental::memcpy",
        "exit", "time", "sleep", "rand", "srand", "sqrt", "pow", "abs",
        "itsuki_setjmp", "itsuki_longjmp", "setjmp", "longjmp",
        "result_ok", "result_err", "result_is_ok", "result_is_err", "result_unwrap", "result_unwrap_or",
        "fundamental::result_ok", "fundamental::result_err", "fundamental::result_is_ok",
        "fundamental::result_is_err", "fundamental::result_unwrap", "fundamental::result_unwrap_or",
        NULL
    };

    for (int i = 0; builtins[i] != NULL; i++) {
        SemanticSymbol* sym = c_calloc(1, sizeof(SemanticSymbol));
        sym->name = c_strdup(builtins[i]);
        sym->type_name = "void";
        sym->is_func = 1;
        sym->param_count = -1; // -1 indica variádico o firma flexible
        scope_add_symbol(ctx->global_scope, sym);
    }
}

static int is_numeric_type(const char* t) {
    if (!t) return 0;
    return (strcmp(t, "int") == 0 || strcmp(t, "int64") == 0 ||
            strcmp(t, "uint32") == 0 || strcmp(t, "uint64") == 0 ||
            strcmp(t, "int16") == 0 || strcmp(t, "uint16") == 0 ||
            strcmp(t, "int8") == 0 || strcmp(t, "uint8") == 0 ||
            strcmp(t, "char") == 0 || strcmp(t, "bool") == 0 ||
            strcmp(t, "float") == 0 || strcmp(t, "double") == 0);
}

static int is_float_type(const char* t) {
    if (!t) return 0;
    return (strcmp(t, "float") == 0 || strcmp(t, "double") == 0);
}

static int types_are_compatible(const char* expected, const char* actual) {
    if (!expected || !actual) return 1;
    if (strcmp(expected, "auto") == 0 || strcmp(actual, "auto") == 0) return 1;
    if (strcmp(expected, actual) == 0) return 1;

    // Numéricos enteros son compatibles entre sí con conversión implícita
    if (is_numeric_type(expected) && is_numeric_type(actual)) return 1;

    // Punteros son compatibles entre sí (int*, void*, struct Result*, char*, etc.)
    if (strstr(expected, "*") != NULL && strstr(actual, "*") != NULL) {
        return 1;
    }

    // Punteros con null / 0 / void*
    if (strstr(expected, "*") != NULL && (strcmp(actual, "null") == 0 || strcmp(actual, "nullptr") == 0 || strcmp(actual, "void*") == 0 || strcmp(actual, "int") == 0)) {
        return 1;
    }
    if (strstr(actual, "*") != NULL && (strcmp(expected, "void*") == 0 || strcmp(expected, "int") == 0)) {
        return 1;
    }

    return 0;
}

static const char* check_expression(ASTNode* expr, SemanticContext* ctx);
static void check_statement(ASTNode* stmt, SemanticContext* ctx);

static const char* check_expression(ASTNode* expr, SemanticContext* ctx) {
    if (!expr) return "void";

    switch (expr->type) {
        case AST_NUMBER:
            expr->type_name = "int";
            return "int";

        case AST_FLOAT_LITERAL:
            expr->type_name = expr->is_double ? "double" : "float";
            expr->is_float = 1;
            return expr->type_name;

        case AST_BOOLEAN:
            expr->type_name = "bool";
            return "bool";

        case AST_STRING:
            expr->type_name = "char*";
            return "char*";

        case AST_INTERPOLATED_STRING: {
            ASTNode* part = expr->children;
            while (part) {
                check_expression(part, ctx);
                part = part->next;
            }
            expr->type_name = "char*";
            return "char*";
        }

        case AST_NULL:
            expr->type_name = "void*";
            return "void*";

        case AST_IDENTIFIER: {
            SemanticSymbol* sym = scope_lookup(ctx->current_scope, expr->name);
            if (!sym) {
                char msg[256];
                snprintf(msg, sizeof(msg), "el identificador '%s' no ha sido declarado en este ambito", expr->name);
                sem_error(ctx, expr->line, expr->column, (int)strlen(expr->name), msg,
                          "asegurate de declarar la variable con 'var' o 'const' antes de usarla");
                expr->type_name = "auto";
                return "auto";
            }
            expr->type_name = sym->type_name ? sym->type_name : "int";
            expr->is_const = sym->is_const;
            if (is_float_type(expr->type_name)) {
                expr->is_float = 1;
                expr->is_double = (strcmp(expr->type_name, "double") == 0);
            }
            return expr->type_name;
        }

        case AST_SCOPED_IDENTIFIER: {
            SemanticSymbol* sym = scope_lookup(ctx->global_scope, expr->name);
            if (sym) {
                expr->type_name = sym->type_name ? sym->type_name : "int";
                return expr->type_name;
            }
            expr->type_name = "auto";
            return "auto";
        }

        case AST_UNARY_EXPR: {
            const char* inner_type = check_expression(expr->left, ctx);
            if (expr->op == '&') {
                char* ptr_type = c_alloc(strlen(inner_type) + 2);
                sprintf(ptr_type, "%s*", inner_type);
                expr->type_name = ptr_type;
                return ptr_type;
            } else if (expr->op == '*') {
                if (strstr(inner_type, "*") != NULL) {
                    char* base_type = c_strdup(inner_type);
                    base_type[strlen(base_type) - 1] = '\0';
                    expr->type_name = base_type;
                    return base_type;
                }
            }
            expr->type_name = (char*)inner_type;
            return inner_type;
        }

        case AST_BINARY_EXPR: {
            const char* left_t = check_expression(expr->left, ctx);
            const char* right_t = check_expression(expr->right, ctx);

            // Comparaciones y lógicos retornan bool
            if (expr->op_str[0] != '\0') {
                if (strcmp(expr->op_str, "==") == 0 || strcmp(expr->op_str, "!=") == 0 ||
                    strcmp(expr->op_str, "<=") == 0 || strcmp(expr->op_str, ">=") == 0 ||
                    strcmp(expr->op_str, "&&") == 0 || strcmp(expr->op_str, "||") == 0) {
                    expr->type_name = "bool";
                    return "bool";
                }
            }
            if (expr->op == '<' || expr->op == '>') {
                expr->type_name = "bool";
                return "bool";
            }

            // Si uno es double o float, el resultado asciende a punto flotante
            if (strcmp(left_t, "double") == 0 || strcmp(right_t, "double") == 0) {
                expr->type_name = "double";
                expr->is_float = 1;
                expr->is_double = 1;
                return "double";
            }
            if (strcmp(left_t, "float") == 0 || strcmp(right_t, "float") == 0) {
                expr->type_name = "float";
                expr->is_float = 1;
                return "float";
            }

            expr->type_name = (char*)left_t;
            return left_t;
        }

        case AST_ASSIGNMENT: {
            const char* target_t = check_expression(expr->left, ctx);
            const char* val_t = check_expression(expr->init_value, ctx);

            // Validar constness
            if (expr->left && expr->left->is_const) {
                char msg[256];
                snprintf(msg, sizeof(msg), "no se puede reasignar a la variable constante '%s'", expr->left->name ? expr->left->name : "");
                sem_error(ctx, expr->line, expr->column, 1, msg, "las variables declaradas con 'const' son inmutables");
            }

            if (!types_are_compatible(target_t, val_t)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "asignacion de tipos incompatibles: se esperaba '%s' pero se recibio '%s'", target_t, val_t);
                sem_error(ctx, expr->line, expr->column, 1, msg, "asegurate de que los tipos de datos coincidan o castea explicitamente");
            }

            expr->type_name = (char*)target_t;
            return target_t;
        }

        case AST_CALL_EXPR: {
            const char* fname = expr->callee ? expr->callee->name : NULL;
            if (fname) {
                SemanticSymbol* sym = scope_lookup(ctx->global_scope, fname);
                if (!sym) sym = scope_lookup(ctx->current_scope, fname);

                // Validar número de argumentos si no es variádico
                if (sym && sym->param_count >= 0) {
                    int arg_count = 0;
                    ASTNode* a = expr->args;
                    while (a) { arg_count++; a = a->next; }
                    if (arg_count != sym->param_count) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "la funcion '%s' esperaba %d argumentos pero se pasaron %d",
                                 fname, sym->param_count, arg_count);
                        sem_error(ctx, expr->line, expr->column, (int)strlen(fname), msg,
                                  "verifica la definicion de la funcion y pasa la cantidad exacta de parametros");
                    }
                }

                // Chequear expresiones de argumentos
                ASTNode* arg = expr->args;
                while (arg) {
                    check_expression(arg, ctx);
                    arg = arg->next;
                }

                if (sym && sym->type_name) {
                    expr->type_name = sym->type_name;
                    return sym->type_name;
                }
            }
            expr->type_name = "auto";
            return "auto";
        }

        case AST_MEMBER_EXPR: {
            check_expression(expr->left, ctx);
            expr->type_name = "auto";
            return "auto";
        }

        case AST_INDEX_EXPR: {
            const char* base_t = check_expression(expr->left, ctx);
            check_expression(expr->expr, ctx);
            if (base_t && strstr(base_t, "*") != NULL) {
                char* elem_t = c_strdup(base_t);
                elem_t[strlen(elem_t) - 1] = '\0';
                expr->type_name = elem_t;
                return elem_t;
            }
            expr->type_name = "int";
            return "int";
        }

        case AST_SIZEOF:
            if (expr->expr) {
                check_expression(expr->expr, ctx);
            }
            return "int";

        default:
            return "auto";
    }
}

static void check_statement(ASTNode* stmt, SemanticContext* ctx) {
    if (!stmt) return;

    switch (stmt->type) {
        case AST_BLOCK: {
            SemanticScope* block_scope = scope_create(ctx->current_scope, "block");
            ctx->current_scope = block_scope;
            ASTNode* s = stmt->statements;
            while (s) {
                check_statement(s, ctx);
                s = s->next;
            }
            ctx->current_scope = block_scope->parent;
            break;
        }

        case AST_VAR_DECLARATION: {
            // Verificar si ya existe en el ámbito actual
            SemanticSymbol* existing = scope_lookup_current(ctx->current_scope, stmt->name);
            if (existing) {
                char msg[256];
                snprintf(msg, sizeof(msg), "redefinicion de la variable '%s' en el mismo ambito", stmt->name);
                sem_error(ctx, stmt->line, stmt->column, (int)strlen(stmt->name), msg,
                          "cambia el nombre de la variable o reutiliza la existente");
            }

            const char* declared_t = stmt->type_name;
            if (stmt->init_value) {
                const char* init_t = check_expression(stmt->init_value, ctx);
                if (!declared_t || strcmp(declared_t, "auto") == 0 || strcmp(declared_t, "var") == 0) {
                    stmt->type_name = (char*)init_t;
                    declared_t = init_t;
                } else if (!types_are_compatible(declared_t, init_t)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "tipo de inicializacion incompatible para '%s': se esperaba '%s' pero se recibio '%s'",
                             stmt->name, declared_t, init_t);
                    sem_error(ctx, stmt->line, stmt->column, (int)strlen(stmt->name), msg,
                              "verifica que el valor asignado sea del tipo declarado");
                }
            }

            SemanticSymbol* sym = c_calloc(1, sizeof(SemanticSymbol));
            sym->name = c_strdup(stmt->name);
            sym->type_name = declared_t ? c_strdup(declared_t) : "int";
            sym->is_const = stmt->is_const;
            sym->line = stmt->line;
            sym->col = stmt->column;
            scope_add_symbol(ctx->current_scope, sym);
            break;
        }

        case AST_RETURN_STATEMENT: {
            const char* ret_t = "void";
            if (stmt->return_value) {
                ret_t = check_expression(stmt->return_value, ctx);
            }
            if (ctx->current_func_return_type) {
                if (strcmp(ctx->current_func_return_type, "void") == 0 && stmt->return_value) {
                    sem_error(ctx, stmt->line, stmt->column, 6,
                              "una funcion que retorna 'void' no debe retornar un valor",
                              "retira el valor de la sentencia 'return;'");
                } else if (strcmp(ctx->current_func_return_type, "void") != 0 && !stmt->return_value) {
                    sem_error(ctx, stmt->line, stmt->column, 6,
                              "la funcion debe retornar un valor pero se encontro 'return;' vacio",
                              "asegurate de devolver una expresion del tipo esperado");
                } else if (!types_are_compatible(ctx->current_func_return_type, ret_t)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "tipo de retorno incompatible: se esperaba '%s' pero se retorna '%s'",
                             ctx->current_func_return_type, ret_t);
                    sem_error(ctx, stmt->line, stmt->column, 6, msg,
                              "asegurate de que la expresion de return coincida con la firma de la funcion");
                }
            }
            break;
        }

        case AST_IF: {
            check_expression(stmt->expr, ctx);
            check_statement(stmt->body, ctx);
            if (stmt->else_branch) check_statement(stmt->else_branch, ctx);
            break;
        }

        case AST_WHILE:
        case AST_DO_WHILE: {
            check_expression(stmt->expr, ctx);
            check_statement(stmt->body, ctx);
            break;
        }

        case AST_FOR: {
            SemanticScope* for_scope = scope_create(ctx->current_scope, "for");
            ctx->current_scope = for_scope;
            if (stmt->init_value) check_statement(stmt->init_value, ctx);
            if (stmt->expr) check_expression(stmt->expr, ctx);
            if (stmt->update_expr) check_expression(stmt->update_expr, ctx);
            if (stmt->body) check_statement(stmt->body, ctx);
            ctx->current_scope = for_scope->parent;
            break;
        }

        case AST_EXPR_STATEMENT: {
            if (stmt->expr) check_expression(stmt->expr, ctx);
            break;
        }

        default:
            break;
    }
}

static void check_function_definition(ASTNode* fn, SemanticContext* ctx) {
    if (!fn || !fn->body) return;

    ctx->current_func_name = fn->name;
    ctx->current_func_return_type = fn->type_name ? fn->type_name : "void";

    SemanticScope* fn_scope = scope_create(ctx->global_scope, fn->name);
    ctx->current_scope = fn_scope;

    // Registrar parámetros en el scope de la función
    ASTNode* param = fn->params;
    while (param) {
        SemanticSymbol* psym = c_calloc(1, sizeof(SemanticSymbol));
        psym->name = c_strdup(param->name);
        psym->type_name = param->type_name ? c_strdup(param->type_name) : "int";
        psym->is_param = 1;
        psym->line = param->line;
        psym->col = param->column;
        scope_add_symbol(fn_scope, psym);
        param = param->next;
    }

    // Analizar el cuerpo de la función
    if (fn->body->statements) {
        ASTNode* s = fn->body->statements;
        while (s) {
            check_statement(s, ctx);
            s = s->next;
        }
    } else {
        check_statement(fn->body, ctx);
    }

    ctx->current_scope = ctx->global_scope;
    ctx->current_func_name = NULL;
    ctx->current_func_return_type = NULL;
}

static void register_declarations_hoisted(ASTNode* node, SemanticContext* ctx) {
    while (node) {
        if (node->type == AST_NAMESPACE) {
            register_declarations_hoisted(node->children, ctx);
        } else if (node->type == AST_FUNCTION_DECLARATION && node->name) {
            SemanticSymbol* sym = c_calloc(1, sizeof(SemanticSymbol));
            sym->name = c_strdup(node->name);
            sym->type_name = node->type_name ? c_strdup(node->type_name) : "void";
            sym->is_func = 1;
            sym->line = node->line;
            sym->col = node->column;

            int pcount = 0;
            ASTNode* p = node->params;
            while (p) { pcount++; p = p->next; }
            sym->param_count = pcount;

            if (strstr(node->name, "printf") != NULL ||
                strstr(node->name, "print") != NULL ||
                strstr(node->name, "println") != NULL) {
                sym->param_count = -1;
            }

            scope_add_symbol(ctx->global_scope, sym);
        } else if ((node->type == AST_STRUCT || node->type == AST_CLASS) && node->name) {
            SemanticSymbol* sym = c_calloc(1, sizeof(SemanticSymbol));
            sym->name = c_strdup(node->name);
            sym->type_name = c_strdup(node->name);
            sym->is_struct = 1;
            sym->line = node->line;
            sym->col = node->column;
            scope_add_symbol(ctx->global_scope, sym);
        }
        node = node->next;
    }
}

static void check_declarations_bodies(ASTNode* node, SemanticContext* ctx) {
    while (node) {
        if (node->type == AST_NAMESPACE) {
            check_declarations_bodies(node->children, ctx);
        } else if (node->type == AST_FUNCTION_DECLARATION) {
            check_function_definition(node, ctx);
        } else if (node->type == AST_VAR_DECLARATION) {
            check_statement(node, ctx);
        }
        node = node->next;
    }
}

int semantics_check(ASTNode* root, const char* filename, const char* source_code) {
    if (!root) return 1;

    SemanticContext ctx;
    memset(&ctx, 0, sizeof(SemanticContext));
    ctx.global_scope = scope_create(NULL, "global");
    ctx.current_scope = ctx.global_scope;
    ctx.filename = filename ? filename : "fuente";
    ctx.source_code = source_code;
    ctx.error_count = 0;

    register_builtin_functions(&ctx);

    ASTNode* top = (root->type == AST_PROGRAM) ? root->children : root;

    // PASO 1: Registro preliminar de funciones, structs y clases (Hoisting)
    register_declarations_hoisted(top, &ctx);

    // PASO 2: Verificación detallada de cuerpos de funciones y sentencias
    check_declarations_bodies(top, &ctx);

    return (ctx.error_count == 0);
}
