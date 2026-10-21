#include "optimizer.h"
#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int folded_constants = 0;
static int dead_code_eliminations = 0;

int optimizer_get_folded_constants_count(void) {
    return folded_constants;
}

int optimizer_get_dead_code_eliminations_count(void) {
    return dead_code_eliminations;
}

void optimizer_reset_stats(void) {
    folded_constants = 0;
    dead_code_eliminations = 0;
}

static ASTNode* make_number_node(int val, int line) {
    ASTNode* n = create_ast_node(AST_NUMBER, line);
    n->int_value = val;
    n->type_name = "int";
    return n;
}

static ASTNode* make_float_node(double val, int is_double, int line) {
    ASTNode* n = create_ast_node(AST_FLOAT_LITERAL, line);
    n->double_value = val;
    n->is_float = 1;
    n->is_double = is_double;
    n->type_name = is_double ? "double" : "float";
    return n;
}

static int is_pure_expression(ASTNode* node) {
    if (!node) return 1;
    switch (node->type) {
        case AST_NUMBER:
        case AST_FLOAT_LITERAL:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_NULL:
        case AST_IDENTIFIER:
        case AST_SIZEOF:
            return 1;
        case AST_UNARY_EXPR:
            if (node->op == '+' || node->op == '-' || node->op == '!' || node->op == '~') {
                return is_pure_expression(node->left);
            }
            return 0;
        case AST_BINARY_EXPR:
            return is_pure_expression(node->left) && is_pure_expression(node->right);
        case AST_MEMBER_EXPR:
            return is_pure_expression(node->left);
        default:
            return 0;
    }
}

static ASTNode* optimize_node(ASTNode* node) {
    if (!node) return NULL;

    // Primero optimizamos recursivamente hijos y campos
    if (node->children) node->children = optimize_ast(node->children);
    if (node->fields) node->fields = optimize_ast(node->fields);
    if (node->params) node->params = optimize_ast(node->params);
    if (node->body) node->body = optimize_node(node->body);
    if (node->else_branch) node->else_branch = optimize_node(node->else_branch);
    if (node->statements) node->statements = optimize_ast(node->statements);
    if (node->init_value) node->init_value = optimize_node(node->init_value);
    if (node->update_expr) node->update_expr = optimize_node(node->update_expr);
    if (node->return_value) node->return_value = optimize_node(node->return_value);
    if (node->expr) node->expr = optimize_node(node->expr);
    if (node->args) node->args = optimize_ast(node->args);
    if (node->left) node->left = optimize_node(node->left);
    if (node->right) node->right = optimize_node(node->right);

    // 1. Plegado de constantes en expresiones unarias
    if (node->type == AST_UNARY_EXPR && node->left) {
        if (node->left->type == AST_NUMBER || node->left->type == AST_BOOLEAN) {
            int val = node->left->int_value;
            if (node->op == '-') {
                folded_constants++;
                return make_number_node(-val, node->line);
            } else if (node->op == '!') {
                folded_constants++;
                return make_number_node(!val, node->line);
            } else if (node->op == '~') {
                folded_constants++;
                return make_number_node(~val, node->line);
            } else if (node->op == '+') {
                folded_constants++;
                return make_number_node(val, node->line);
            }
        } else if (node->left->type == AST_FLOAT_LITERAL) {
            if (node->op == '-') {
                folded_constants++;
                return make_float_node(-node->left->double_value, node->left->is_double, node->line);
            }
        }
    }

    // 2. Plegado de constantes en expresiones binarias
    if (node->type == AST_BINARY_EXPR && node->left && node->right) {
        // Constantes enteras o booleanas
        if ((node->left->type == AST_NUMBER || node->left->type == AST_BOOLEAN) &&
            (node->right->type == AST_NUMBER || node->right->type == AST_BOOLEAN)) {
            int a = node->left->int_value;
            int b = node->right->int_value;
            int res = 0;
            int valid = 1;

            if (node->op_str[0] != '\0') {
                if (strcmp(node->op_str, "==") == 0) res = (a == b);
                else if (strcmp(node->op_str, "!=") == 0) res = (a != b);
                else if (strcmp(node->op_str, "<=") == 0) res = (a <= b);
                else if (strcmp(node->op_str, ">=") == 0) res = (a >= b);
                else if (strcmp(node->op_str, "&&") == 0) res = (a && b);
                else if (strcmp(node->op_str, "||") == 0) res = (a || b);
                else if (strcmp(node->op_str, "<<") == 0) {
                    if (b >= 0 && b < 32) res = (a << b);
                    else valid = 0;
                }
                else if (strcmp(node->op_str, ">>") == 0) {
                    if (b >= 0 && b < 32) res = (a >> b);
                    else valid = 0;
                }
                else valid = 0;
            } else {
                switch (node->op) {
                    case '+': res = a + b; break;
                    case '-': res = a - b; break;
                    case '*': res = a * b; break;
                    case '/': 
                        if (b != 0) res = a / b;
                        else valid = 0;
                        break;
                    case '%': 
                        if (b != 0) res = a % b;
                        else valid = 0;
                        break;
                    case '<': res = (a < b); break;
                    case '>': res = (a > b); break;
                    case '&': res = (a & b); break;
                    case '|': res = (a | b); break;
                    case '^': res = (a ^ b); break;
                    default: valid = 0; break;
                }
            }

            if (valid) {
                folded_constants++;
                return make_number_node(res, node->line);
            }
        }

        // Constantes flotantes
        if (node->left->type == AST_FLOAT_LITERAL && node->right->type == AST_FLOAT_LITERAL) {
            double a = node->left->double_value;
            double b = node->right->double_value;
            int is_dbl = (node->left->is_double || node->right->is_double);
            double res = 0.0;
            int valid = 1;

            switch (node->op) {
                case '+': res = a + b; break;
                case '-': res = a - b; break;
                case '*': res = a * b; break;
                case '/': 
                    if (b != 0.0) res = a / b;
                    else valid = 0;
                    break;
                default: valid = 0; break;
            }

            if (valid) {
                folded_constants++;
                return make_float_node(res, is_dbl, node->line);
            }
        }

        // Simplificaciones algebraicas neutras
        // x + 0 = x
        if (node->op == '+' && node->right->type == AST_NUMBER && node->right->int_value == 0) {
            folded_constants++;
            return node->left;
        }
        // 0 + x = x
        if (node->op == '+' && node->left->type == AST_NUMBER && node->left->int_value == 0) {
            folded_constants++;
            return node->right;
        }
        // x - 0 = x
        if (node->op == '-' && node->right->type == AST_NUMBER && node->right->int_value == 0) {
            folded_constants++;
            return node->left;
        }
        // x * 1 = x
        if (node->op == '*' && node->right->type == AST_NUMBER && node->right->int_value == 1) {
            folded_constants++;
            return node->left;
        }
        // 1 * x = x
        if (node->op == '*' && node->left->type == AST_NUMBER && node->left->int_value == 1) {
            folded_constants++;
            return node->right;
        }
        // x * 0 = 0 (solo si la otra expresión no tiene efectos secundarios)
        if (node->op == '*' && node->right->type == AST_NUMBER && node->right->int_value == 0) {
            if (is_pure_expression(node->left)) {
                folded_constants++;
                return make_number_node(0, node->line);
            }
        }
        if (node->op == '*' && node->left->type == AST_NUMBER && node->left->int_value == 0) {
            if (is_pure_expression(node->right)) {
                folded_constants++;
                return make_number_node(0, node->line);
            }
        }
    }

    // 3. Eliminación de código muerto en if (condición constante)
    if (node->type == AST_IF && node->expr) {
        if (node->expr->type == AST_NUMBER || node->expr->type == AST_BOOLEAN) {
            dead_code_eliminations++;
            if (node->expr->int_value != 0) {
                // Siempre verdadero: conservamos el bloque body
                return node->body;
            } else {
                // Siempre falso: conservamos el else_branch si existe, o un bloque vacío
                if (node->else_branch) {
                    return node->else_branch;
                } else {
                    ASTNode* empty_block = create_ast_node(AST_BLOCK, node->line);
                    return empty_block;
                }
            }
        }
    }

    // 4. Eliminación de bucles while con condición siempre falsa
    if (node->type == AST_WHILE && node->expr) {
        if ((node->expr->type == AST_NUMBER || node->expr->type == AST_BOOLEAN) && node->expr->int_value == 0) {
            dead_code_eliminations++;
            ASTNode* empty_block = create_ast_node(AST_BLOCK, node->line);
            return empty_block;
        }
    }

    return node;
}

ASTNode* optimize_ast(ASTNode* root) {
    if (!root) return NULL;
    ASTNode* curr = root;
    ASTNode* prev = NULL;
    ASTNode* new_root = NULL;

    while (curr) {
        ASTNode* next = curr->next;
        ASTNode* opt = optimize_node(curr);
        if (opt) {
            opt->next = NULL;
            if (!new_root) {
                new_root = opt;
            } else if (prev) {
                prev->next = opt;
            }
            prev = opt;
        }
        curr = next;
    }
    return new_root;
}
