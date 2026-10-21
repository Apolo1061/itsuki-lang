#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "ast.h"

// Optimiza el AST en memoria antes de la generación de código:
// - Plegado de constantes (Constant Folding)
// - Simplificaciones algebraicas neutras (x * 1, x + 0, etc.)
// - Eliminación de código muerto en condicionales (Dead Code Elimination)
ASTNode* optimize_ast(ASTNode* root);

int optimizer_get_folded_constants_count(void);
int optimizer_get_dead_code_eliminations_count(void);
void optimizer_reset_stats(void);

#endif // OPTIMIZER_H
