#ifndef GENERATOR_H
#define GENERATOR_H

#include "ast.h"
#include <stdio.h>

void generate_code(ASTNode* node, const char* output_filename);
void generator_set_opt_level(int level);
int generator_get_opt_level(void);
void generator_set_target_bits(int bits);
int generator_get_target_bits(void);
const char* generator_get_inline_c_file(void);
int generator_get_peephole_optimizations_count(void);
void generator_set_context(const char* filename, const char* source_code);

#endif // GENERATOR_H
