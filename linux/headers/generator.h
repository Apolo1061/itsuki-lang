#ifndef GENERATOR_H
#define GENERATOR_H

#include "ast.h"
#include <stdio.h>

void generate_code(ASTNode* node, const char* output_filename);
void generator_set_opt_level(int level);
int generator_get_opt_level(void);
const char* generator_get_inline_c_file(void);

#endif // GENERATOR_H
