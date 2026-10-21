#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"

// Fase de Análisis Semántico y Verificación de Tipos (Type Checker Pass):
// - Valida resolución de ámbitos (scopes) y existencia de identificadores
// - Valida compatibilidad de tipos en asignaciones y expresiones binarias
// - Valida que las sentencias return coincidan con el tipo de retorno de la función
// - Valida que no se modifiquen variables constantes (const)
// - Comprueba cantidad y tipos de argumentos en llamadas a funciones
// - Anota en el AST los tipos deducidos (type_name, is_float, is_double, is_64bit)
int semantics_check(ASTNode* root, const char* filename, const char* source_code);

#endif // SEMANTICS_H
