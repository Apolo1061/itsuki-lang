#ifndef EMBEDDED_LIB_H
#define EMBEDDED_LIB_H

#include <stddef.h>

const char* get_embedded_file(const char* name, size_t* out_len);
int has_embedded_file(const char* name);
int extract_embedded_runtime(char* out_path, size_t max_len);

#endif
