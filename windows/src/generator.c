#include "generator.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char* value;
    int id;
} StringLiteral;

typedef struct Symbol {
    char* name;
    int stack_offset;
    int is_param;
    char* type_name;
    int is_string;
    int is_array;
    int array_size;
    int is_const;
    int is_static;
    char* asm_name;
    struct Symbol* next;
} Symbol;

typedef struct StaticLocalVar {
    char* asm_name;
    int int_val;
    char* string_val;
    int is_string;
    int is_array;
    int array_size;
    struct StaticLocalVar* next;
} StaticLocalVar;

static StaticLocalVar* declared_static_locals = NULL;
static int static_local_counter = 0;

typedef struct GlobalVar {
    char* name;
    char* namespace_prefix;
    char* full_name;
    char* asm_name;
    char* type_name;
    int is_const;
    int is_extern;
    int is_static;
    int is_string;
    int is_array;
    int array_size;
    int int_val;
    char* string_val;
    ASTNode* init_node;
    struct GlobalVar* next;
} GlobalVar;

static GlobalVar* declared_globals = NULL;
static GlobalVar* find_global_var(const char* name);

typedef struct UsingNS {
    char* name;
    struct UsingNS* next;
} UsingNS;

typedef struct DeclaredFunc {
    char* name;
    char* namespace_prefix;
    int is_inline;
    ASTNode* node;
    struct DeclaredFunc* next;
} DeclaredFunc;

static int current_inline_exit_label = 0;
static int inlining_depth = 0;

static int generator_opt_level = 0;

void generator_set_opt_level(int level) {
    generator_opt_level = level;
}

int generator_get_opt_level(void) {
    return generator_opt_level;
}

static char inline_c_file_path[512] = {0};
static int inline_c_count = 0;
static char* inline_c_buffer = NULL;
static size_t inline_c_cap = 0;
static size_t inline_c_len = 0;

const char* generator_get_inline_c_file(void) {
    if (inline_c_count > 0) return inline_c_file_path;
    return NULL;
}

static void append_inline_c_raw(const char* text) {
    if (!text) return;
    size_t tlen = strlen(text);
    if (inline_c_len + tlen + 2 >= inline_c_cap) {
        inline_c_cap = (inline_c_len + tlen + 2048) * 2;
        inline_c_buffer = realloc(inline_c_buffer, inline_c_cap);
    }
    strcpy(inline_c_buffer + inline_c_len, text);
    inline_c_len += tlen;
}

static void emit_inline_asm(const char* asm_str, FILE* out) {
    if (!asm_str) return;
    const char* p = asm_str;
    while (*p) {
        char line_buf[512];
        int li = 0;
        while (*p && *p != '\n' && li < 510) {
            line_buf[li++] = *p++;
        }
        if (*p == '\n') p++;
        line_buf[li] = '\0';
        char* trimmed = line_buf;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0') continue;
        if (strchr(trimmed, ':') != NULL && strchr(trimmed, ' ') == NULL) {
            fprintf(out, "%s\n", trimmed);
        } else {
            fprintf(out, "    %s\n", trimmed);
        }
    }
}

static void peephole_optimize_file(const char* filepath) {
    FILE* in = fopen(filepath, "r");
    if (!in) return;
    
    char** lines = NULL;
    int line_count = 0;
    int line_cap = 512;
    lines = malloc(line_cap * sizeof(char*));
    if (!lines) { fclose(in); return; }
    
    char buf[2048];
    while (fgets(buf, sizeof(buf), in)) {
        size_t l = strlen(buf);
        while (l > 0 && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) {
            buf[--l] = '\0';
        }
        if (line_count >= line_cap) {
            line_cap *= 2;
            lines = realloc(lines, line_cap * sizeof(char*));
        }
        lines[line_count++] = strdup(buf);
    }
    fclose(in);
    
    int changed = 1;
    int max_passes = 12;
    while (changed && max_passes-- > 0) {
        changed = 0;
        for (int i = 0; i < line_count; i++) {
            if (lines[i] == NULL) continue;
            
            char* t1 = lines[i];
            while (*t1 == ' ' || *t1 == '\t') t1++;
            if (*t1 == '\0') continue;
            
            // 1. Instrucciones identidad que no hacen nada
            if (strcmp(t1, "movl %eax, %eax") == 0 ||
                strcmp(t1, "movl %ebx, %ebx") == 0 ||
                strcmp(t1, "movl %ecx, %ecx") == 0 ||
                strcmp(t1, "movl %edx, %edx") == 0 ||
                strcmp(t1, "movl %esi, %esi") == 0 ||
                strcmp(t1, "movl %edi, %edi") == 0 ||
                strcmp(t1, "addl $0, %eax") == 0 ||
                strcmp(t1, "addl $0, %ebx") == 0 ||
                strcmp(t1, "addl $0, %ecx") == 0 ||
                strcmp(t1, "addl $0, %edx") == 0 ||
                strcmp(t1, "addl $0, %esp") == 0 ||
                strcmp(t1, "subl $0, %eax") == 0 ||
                strcmp(t1, "subl $0, %ebx") == 0 ||
                strcmp(t1, "subl $0, %ecx") == 0 ||
                strcmp(t1, "subl $0, %edx") == 0 ||
                strcmp(t1, "subl $0, %esp") == 0) {
                free(lines[i]);
                lines[i] = NULL;
                changed = 1;
                continue;
            }
            
            // 2. Optimización de tamaño de instrucción: movl $0, %reg -> xorl %reg, %reg (2 bytes vs 5 bytes)
            if (strcmp(t1, "movl $0, %eax") == 0) {
                free(lines[i]); lines[i] = strdup("    xorl %eax, %eax");
                changed = 1; t1 = lines[i] + 4;
            } else if (strcmp(t1, "movl $0, %ebx") == 0) {
                free(lines[i]); lines[i] = strdup("    xorl %ebx, %ebx");
                changed = 1; t1 = lines[i] + 4;
            } else if (strcmp(t1, "movl $0, %ecx") == 0) {
                free(lines[i]); lines[i] = strdup("    xorl %ecx, %ecx");
                changed = 1; t1 = lines[i] + 4;
            } else if (strcmp(t1, "movl $0, %edx") == 0) {
                free(lines[i]); lines[i] = strdup("    xorl %edx, %edx");
                changed = 1; t1 = lines[i] + 4;
            } else if (strcmp(t1, "movl $0, %esi") == 0) {
                free(lines[i]); lines[i] = strdup("    xorl %esi, %esi");
                changed = 1; t1 = lines[i] + 4;
            } else if (strcmp(t1, "movl $0, %edi") == 0) {
                free(lines[i]); lines[i] = strdup("    xorl %edi, %edi");
                changed = 1; t1 = lines[i] + 4;
            }
            
            // 3. Incrementos y decrementos: addl $1 / subl $1 -> incl / decl (1 byte vs 3 bytes)
            if (strcmp(t1, "addl $1, %eax") == 0) { free(lines[i]); lines[i] = strdup("    incl %eax"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "subl $1, %eax") == 0) { free(lines[i]); lines[i] = strdup("    decl %eax"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "addl $1, %edx") == 0) { free(lines[i]); lines[i] = strdup("    incl %edx"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "subl $1, %edx") == 0) { free(lines[i]); lines[i] = strdup("    decl %edx"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "addl $1, %ecx") == 0) { free(lines[i]); lines[i] = strdup("    incl %ecx"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "subl $1, %ecx") == 0) { free(lines[i]); lines[i] = strdup("    decl %ecx"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "addl $1, %ebx") == 0) { free(lines[i]); lines[i] = strdup("    incl %ebx"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "subl $1, %ebx") == 0) { free(lines[i]); lines[i] = strdup("    decl %ebx"); changed = 1; t1 = lines[i] + 4; }
            
            // 4. Comparación con cero: cmpl $0, %reg -> testl %reg, %reg (2 bytes vs 3 bytes)
            if (strcmp(t1, "cmpl $0, %eax") == 0) { free(lines[i]); lines[i] = strdup("    testl %eax, %eax"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "cmpl $0, %edx") == 0) { free(lines[i]); lines[i] = strdup("    testl %edx, %edx"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "cmpl $0, %ecx") == 0) { free(lines[i]); lines[i] = strdup("    testl %ecx, %ecx"); changed = 1; t1 = lines[i] + 4; }
            else if (strcmp(t1, "cmpl $0, %ebx") == 0) { free(lines[i]); lines[i] = strdup("    testl %ebx, %ebx"); changed = 1; t1 = lines[i] + 4; }

            // 5. Eliminar código inalcanzable tras ret o jmp incondicional
            if (strcmp(t1, "ret") == 0 || (strncmp(t1, "jmp .L", 6) == 0 && t1[strlen(t1) - 1] != ':')) {
                for (int k = i + 1; k < line_count; k++) {
                    if (lines[k] == NULL) continue;
                    char* tk = lines[k];
                    while (*tk == ' ' || *tk == '\t') tk++;
                    if (*tk == '\0') continue;
                    if (strchr(tk, ':') != NULL || *tk == '.') break; // Llegamos a una etiqueta o directiva
                    free(lines[k]);
                    lines[k] = NULL;
                    changed = 1;
                }
            }
            
            // Encontrar la siguiente instrucción no nula
            int next_idx = -1;
            for (int k = i + 1; k < line_count; k++) {
                if (lines[k] != NULL) {
                    char* tk = lines[k];
                    while (*tk == ' ' || *tk == '\t') tk++;
                    if (*tk != '\0') {
                        next_idx = k;
                        break;
                    }
                }
            }
            if (next_idx < 0) continue;
            
            char* t2 = lines[next_idx];
            while (*t2 == ' ' || *t2 == '\t') t2++;

            // 6. Fusión de ajustes a %esp: addl $X, %esp + addl $Y, %esp -> addl $(X+Y), %esp
            if (strncmp(t1, "addl $", 6) == 0 && strstr(t1, ", %esp") != NULL &&
                strncmp(t2, "addl $", 6) == 0 && strstr(t2, ", %esp") != NULL) {
                int v1 = atoi(t1 + 6);
                int v2 = atoi(t2 + 6);
                if (v1 > 0 && v2 > 0) {
                    char new_esp[64];
                    snprintf(new_esp, sizeof(new_esp), "    addl $%d, %%esp", v1 + v2);
                    free(lines[i]);
                    lines[i] = strdup(new_esp);
                    free(lines[next_idx]);
                    lines[next_idx] = NULL;
                    changed = 1;
                    continue;
                }
            }

            // 7. push %reg seguido de pop %reg -> ninguno
            if ((strcmp(t1, "pushl %eax") == 0 && strcmp(t2, "popl %eax") == 0) ||
                (strcmp(t1, "pushl %ebx") == 0 && strcmp(t2, "popl %ebx") == 0) ||
                (strcmp(t1, "pushl %ecx") == 0 && strcmp(t2, "popl %ecx") == 0) ||
                (strcmp(t1, "pushl %edx") == 0 && strcmp(t2, "popl %edx") == 0) ||
                (strcmp(t1, "pushl %esi") == 0 && strcmp(t2, "popl %esi") == 0) ||
                (strcmp(t1, "pushl %edi") == 0 && strcmp(t2, "popl %edi") == 0)) {
                free(lines[i]);
                lines[i] = NULL;
                free(lines[next_idx]);
                lines[next_idx] = NULL;
                changed = 1;
                continue;
            }

            // 8. test redundante tras xorl %reg, %reg
            if ((strcmp(t1, "xorl %eax, %eax") == 0 && strcmp(t2, "testl %eax, %eax") == 0) ||
                (strcmp(t1, "xorl %edx, %edx") == 0 && strcmp(t2, "testl %edx, %edx") == 0) ||
                (strcmp(t1, "xorl %ecx, %ecx") == 0 && strcmp(t2, "testl %ecx, %ecx") == 0) ||
                (strcmp(t1, "xorl %ebx, %ebx") == 0 && strcmp(t2, "testl %ebx, %ebx") == 0)) {
                free(lines[next_idx]);
                lines[next_idx] = NULL;
                changed = 1;
                continue;
            }
            
            // 9. Store seguido de Load redundante a la misma ubicación
            if (strncmp(t1, "movl %eax, ", 11) == 0 && strncmp(t2, "movl ", 5) == 0) {
                const char* loc1 = t1 + 11;
                const char* rest2 = t2 + 5;
                const char* comma2 = strstr(rest2, ", %eax");
                if (comma2 != NULL && comma2[6] == '\0') {
                    size_t loc2_len = (size_t)(comma2 - rest2);
                    if (strlen(loc1) == loc2_len && strncmp(loc1, rest2, loc2_len) == 0) {
                        free(lines[next_idx]);
                        lines[next_idx] = NULL;
                        changed = 1;
                        continue;
                    }
                }
            }

            // 10. Movimientos de rebote (ping-pong)
            if ((strcmp(t1, "movl %eax, %edx") == 0 && strcmp(t2, "movl %edx, %eax") == 0) ||
                (strcmp(t1, "movl %eax, %ecx") == 0 && strcmp(t2, "movl %ecx, %eax") == 0) ||
                (strcmp(t1, "movl %eax, %ebx") == 0 && strcmp(t2, "movl %ebx, %eax") == 0)) {
                free(lines[next_idx]);
                lines[next_idx] = NULL;
                changed = 1;
                continue;
            }

            // 11. Salto a la etiqueta inmediatamente siguiente
            if (strncmp(t1, "jmp .L", 6) == 0 && t2[0] == '.') {
                const char* target_lbl = t1 + 4;
                size_t tlen = strlen(target_lbl);
                if (strncmp(t2, target_lbl, tlen) == 0 && t2[tlen] == ':' && t2[tlen + 1] == '\0') {
                    free(lines[i]);
                    lines[i] = NULL;
                    changed = 1;
                    continue;
                }
            }
        }
    }
    
    FILE* out = fopen(filepath, "w");
    if (out) {
        for (int i = 0; i < line_count; i++) {
            if (lines[i] != NULL) {
                fprintf(out, "%s\n", lines[i]);
                free(lines[i]);
            }
        }
        fclose(out);
    } else {
        for (int i = 0; i < line_count; i++) {
            if (lines[i] != NULL) free(lines[i]);
        }
    }
    free(lines);
}

typedef struct LoopContext {
    int break_label;
    int continue_label;
} LoopContext;

#define MAX_LOOP_DEPTH 64
static LoopContext loop_stack[MAX_LOOP_DEPTH];
static int loop_depth = 0;
static int global_loop_counter = 0;

static void push_loop(int break_label, int continue_label) {
    if (loop_depth < MAX_LOOP_DEPTH) {
        loop_stack[loop_depth].break_label = break_label;
        loop_stack[loop_depth].continue_label = continue_label;
        loop_depth++;
    }
}

static void pop_loop() {
    if (loop_depth > 0) {
        loop_depth--;
    }
}

typedef struct StructFieldInfo {
    char* name;
    char* type_name;
    int offset;
    int size;
    int is_string;
    int is_const;
    int access_level;
} StructFieldInfo;

#define MAX_STRING_LITERALS 4096
#define MAX_STRUCT_FIELDS 64

typedef struct StructDef {
    char* name;             // "Perfil"
    char* namespace_prefix; // "usuario"
    char* full_name;        // "usuario::Perfil"
    char* base_class;       // Nombre de clase base heredada
    int is_class;           // 1 si es class, 0 si struct
    int total_size;
    int field_count;
    StructFieldInfo fields[MAX_STRUCT_FIELDS];
    struct StructDef* next;
} StructDef;

static StructDef* find_struct_def(const char* type_name);
static void register_declared_functions(ASTNode* decl);

static StringLiteral string_table[MAX_STRING_LITERALS];
static int string_count = 0;
static Symbol* current_symbols = NULL;
static UsingNS* active_usings = NULL;
static DeclaredFunc* declared_functions = NULL;
static StructDef* declared_structs = NULL;
static const char* current_func_namespace = NULL;
static const char* current_func_name = NULL;

static void register_struct_node(ASTNode* s_node) {
    if (s_node == NULL || (s_node->type != AST_STRUCT && s_node->type != AST_UNION && s_node->type != AST_CLASS)) return;
    StructDef* sd = malloc(sizeof(StructDef));
    sd->name = strdup(s_node->name);
    sd->namespace_prefix = s_node->namespace_prefix ? strdup(s_node->namespace_prefix) : NULL;
    sd->base_class = s_node->base_class ? strdup(s_node->base_class) : NULL;
    sd->is_class = (s_node->type == AST_CLASS) ? 1 : 0;
    if (s_node->namespace_prefix) {
        char full[256];
        sprintf(full, "%s::%s", s_node->namespace_prefix, s_node->name);
        sd->full_name = strdup(full);
    } else {
        sd->full_name = strdup(s_node->name);
    }
    sd->field_count = 0;
    int curr_offset = 0;
    int max_field_size = 4;

    // Herencia: heredar campos de la clase base
    if (s_node->base_class != NULL) {
        StructDef* base_sd = find_struct_def(s_node->base_class);
        if (base_sd != NULL) {
            for (int i = 0; i < base_sd->field_count && sd->field_count < MAX_STRUCT_FIELDS; i++) {
                sd->fields[sd->field_count].name = strdup(base_sd->fields[i].name);
                sd->fields[sd->field_count].type_name = strdup(base_sd->fields[i].type_name);
                sd->fields[sd->field_count].offset = base_sd->fields[i].offset;
                sd->fields[sd->field_count].size = base_sd->fields[i].size;
                sd->fields[sd->field_count].is_string = base_sd->fields[i].is_string;
                sd->fields[sd->field_count].is_const = base_sd->fields[i].is_const;
                sd->fields[sd->field_count].access_level = base_sd->fields[i].access_level;
                sd->field_count++;
            }
            curr_offset = base_sd->total_size;
        }
    }

    ASTNode* f = s_node->fields;
    while (f != NULL && sd->field_count < MAX_STRUCT_FIELDS) {
        sd->fields[sd->field_count].name = strdup(f->name);
        sd->fields[sd->field_count].type_name = f->type_name ? strdup(f->type_name) : strdup("int");
        int is_str = (f->type_name && strstr(f->type_name, "string") != NULL && strchr(f->type_name, '*') == NULL) ? 1 : 0;
        sd->fields[sd->field_count].is_string = is_str;
        sd->fields[sd->field_count].is_const = f->is_const || (f->type_name && strstr(f->type_name, "const") != NULL);
        sd->fields[sd->field_count].access_level = (s_node->type == AST_CLASS) ? f->access_level : ACCESS_PUBLIC;
        
        int fsize = 4;
        if (is_str) {
            fsize = 256;
        } else if (f->type_name && strstr(f->type_name, "[") != NULL) {
            char* b = strchr(f->type_name, '[');
            int asz = atoi(b + 1);
            if (asz <= 0) asz = 1;
            fsize = asz * 4;
        } else {
            StructDef* sub_sd = find_struct_def(f->type_name);
            if (sub_sd != NULL) fsize = sub_sd->total_size;
        }
        
        if (s_node->type == AST_UNION) {
            sd->fields[sd->field_count].offset = 0;
            if (fsize > max_field_size) max_field_size = fsize;
        } else {
            sd->fields[sd->field_count].offset = curr_offset;
            curr_offset += fsize;
        }
        sd->fields[sd->field_count].size = fsize;
        sd->field_count++;
        f = f->next;
    }
    if (s_node->type == AST_UNION) {
        sd->total_size = max_field_size;
    } else {
        sd->total_size = curr_offset > 0 ? curr_offset : 4;
    }
    sd->next = declared_structs;
    declared_structs = sd;

    if (s_node->type == AST_CLASS && s_node->body != NULL) {
        register_declared_functions(s_node->body);
    }
}

static void register_declared_structs(ASTNode* decl) {
    while (decl != NULL) {
        if (decl->type == AST_STRUCT || decl->type == AST_UNION || decl->type == AST_CLASS) {
            register_struct_node(decl);
        } else if (decl->type == AST_TYPEDEF) {
            if (decl->children != NULL && (decl->children->type == AST_STRUCT || decl->children->type == AST_UNION)) {
                register_struct_node(decl->children);
                StructDef* orig = declared_structs;
                if (orig != NULL) {
                    StructDef* alias_sd = malloc(sizeof(StructDef));
                    *alias_sd = *orig;
                    alias_sd->name = strdup(decl->name);
                    alias_sd->full_name = strdup(decl->name);
                    alias_sd->next = declared_structs;
                    declared_structs = alias_sd;
                }
            }
        } else if (decl->type == AST_NAMESPACE) {
            register_declared_structs(decl->children);
        } else if (decl->type == AST_IMPORT) {
            if (decl->children != NULL) {
                register_declared_structs(decl->children);
            }
        } else if (decl->type == AST_FUNCTION_DECLARATION) {
            if (decl->body && decl->body->statements) {
                register_declared_structs(decl->body->statements);
            }
        }
        decl = decl->next;
    }
}

static StructDef* find_struct_def(const char* type_name) {
    if (type_name == NULL) return NULL;
    
    if (strncmp(type_name, "const ", 6) == 0) type_name += 6;
    if (strncmp(type_name, "struct ", 7) == 0) type_name += 7;
    else if (strncmp(type_name, "union ", 6) == 0) type_name += 6;
    else if (strncmp(type_name, "class ", 6) == 0) type_name += 6;
    
    const char* resolved = resolve_typedef(type_name);
    if (resolved != NULL) {
        if (strncmp(resolved, "const ", 6) == 0) resolved += 6;
        if (strncmp(resolved, "struct ", 7) == 0) resolved += 7;
        else if (strncmp(resolved, "union ", 6) == 0) resolved += 6;
        else if (strncmp(resolved, "class ", 6) == 0) resolved += 6;
    }
    
    StructDef* curr = declared_structs;
    while (curr != NULL) {
        if (strcmp(curr->full_name, type_name) == 0 || strcmp(curr->name, type_name) == 0 ||
            (resolved && (strcmp(curr->full_name, resolved) == 0 || strcmp(curr->name, resolved) == 0))) {
            return curr;
        }
        curr = curr->next;
    }
    if (strstr(type_name, "::") == NULL) {
        if (current_func_namespace != NULL) {
            char qualified[256];
            sprintf(qualified, "%s::%s", current_func_namespace, type_name);
            curr = declared_structs;
            while (curr != NULL) {
                if (strcmp(curr->full_name, qualified) == 0) {
                    return curr;
                }
                curr = curr->next;
            }
        }
        UsingNS* u = active_usings;
        while (u != NULL) {
            char qualified[256];
            sprintf(qualified, "%s::%s", u->name, type_name);
            curr = declared_structs;
            while (curr != NULL) {
                if (strcmp(curr->full_name, qualified) == 0 ||
                    (resolved && strcmp(curr->name, resolved) == 0)) {
                    return curr;
                }
                curr = curr->next;
            }
            u = u->next;
        }
    }
    return NULL;
}

static StructFieldInfo* find_struct_field(StructDef* sd, const char* field_name) {
    if (sd == NULL || field_name == NULL) return NULL;
    for (int i = 0; i < sd->field_count; i++) {
        if (strcmp(sd->fields[i].name, field_name) == 0) {
            return &sd->fields[i];
        }
    }
    if (sd->base_class != NULL) {
        StructDef* base_sd = find_struct_def(sd->base_class);
        if (base_sd != NULL) {
            return find_struct_field(base_sd, field_name);
        }
    }
    return NULL;
}

static int add_string_literal(const char* value) {
    for (int i = 0; i < string_count; i++) {
        if (strcmp(string_table[i].value, value) == 0) {
            return string_table[i].id;
        }
    }
    if (string_count >= MAX_STRING_LITERALS) {
        printf("Error fatal del compilador: limite de literales de cadena excedido (%d)\n", MAX_STRING_LITERALS);
        exit(1);
    }
    int id = string_count;
    string_table[id].value = strdup(value);
    string_table[id].id = id;
    string_count++;
    return id;
}

static void collect_strings(ASTNode* node) {
    if (node == NULL) return;
    
    if (node->type == AST_STRING && node->string_value != NULL) {
        node->int_value = add_string_literal(node->string_value);
    }
    
    collect_strings(node->next);
    collect_strings(node->children);
    collect_strings(node->fields);
    collect_strings(node->params);
    collect_strings(node->body);
    collect_strings(node->else_branch);
    collect_strings(node->statements);
    collect_strings(node->init_value);
    collect_strings(node->update_expr);
    collect_strings(node->return_value);
    collect_strings(node->expr);
    collect_strings(node->callee);
    collect_strings(node->args);
    collect_strings(node->left);
    collect_strings(node->right);
}

static DeclaredFunc* find_exact_declared_function(const char* name, const char* ns) {
    DeclaredFunc* curr = declared_functions;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            if ((curr->namespace_prefix == NULL && ns == NULL) ||
                (curr->namespace_prefix && ns && strcmp(curr->namespace_prefix, ns) == 0)) {
                return curr;
            }
        }
        curr = curr->next;
    }
    return NULL;
}

static void register_declared_functions(ASTNode* decl) {
    while (decl != NULL) {
        if (decl->type == AST_FUNCTION_DECLARATION) {
            DeclaredFunc* existing = find_exact_declared_function(decl->name, decl->namespace_prefix);
            if (existing != NULL) {
                if (decl->body != NULL) {
                    existing->node = decl;
                    existing->is_inline = decl->is_inline;
                }
            } else {
                DeclaredFunc* df = malloc(sizeof(DeclaredFunc));
                df->name = strdup(decl->name);
                df->namespace_prefix = decl->namespace_prefix ? strdup(decl->namespace_prefix) : NULL;
                df->is_inline = decl->is_inline;
                df->node = decl;
                df->next = declared_functions;
                declared_functions = df;
            }
        } else if (decl->type == AST_NAMESPACE) {
            register_declared_functions(decl->children);
        } else if (decl->type == AST_CLASS) {
            register_declared_functions(decl->body);
        } else if (decl->type == AST_IMPORT) {
            if (decl->children != NULL) {
                register_declared_functions(decl->children);
            }
        }
        decl = decl->next;
    }
}

static void register_global_var_node(ASTNode* node) {
    if (node == NULL || node->type != AST_VAR_DECLARATION) return;
    GlobalVar* gv = malloc(sizeof(GlobalVar));
    gv->name = strdup(node->name);
    gv->namespace_prefix = node->namespace_prefix ? strdup(node->namespace_prefix) : NULL;
    gv->type_name = node->type_name ? strdup(node->type_name) : NULL;
    gv->is_const = node->is_const;
    gv->is_extern = node->is_extern;
    gv->is_static = node->is_static;
    gv->is_array = node->is_array;
    gv->array_size = node->array_size;
    gv->is_string = (node->type_name && strstr(node->type_name, "string") != NULL && strchr(node->type_name, '*') == NULL) ? 1 : 0;
    gv->init_node = node->init_value;
    gv->int_val = 0;
    gv->string_val = NULL;
    
    if (node->namespace_prefix != NULL && strlen(node->namespace_prefix) > 0) {
        char full[256];
        sprintf(full, "%s::%s", node->namespace_prefix, node->name);
        gv->full_name = strdup(full);
        
        char clean_ns[256];
        int c_idx = 0;
        for (int i = 0; node->namespace_prefix[i] != '\0' && c_idx < 250; i++) {
            if (node->namespace_prefix[i] == ':' && node->namespace_prefix[i+1] == ':') {
                clean_ns[c_idx++] = '_';
                clean_ns[c_idx++] = '_';
                i++;
            } else {
                clean_ns[c_idx++] = node->namespace_prefix[i];
            }
        }
        clean_ns[c_idx] = '\0';
        char asm_buf[300];
        sprintf(asm_buf, "%s__%s", clean_ns, node->name);
        gv->asm_name = strdup(asm_buf);
    } else {
        gv->full_name = strdup(node->name);
        gv->asm_name = strdup(node->name);
    }
    
    if (node->init_value != NULL) {
        int val = 0;
        if (eval_const_expr(node->init_value, &val)) {
            gv->int_val = val;
        } else if (node->init_value->type == AST_NUMBER) {
            gv->int_val = node->init_value->int_value;
        } else if (node->init_value->type == AST_NULL) {
            gv->int_val = 0;
        }
        if (node->init_value->type == AST_STRING && node->init_value->string_value != NULL) {
            gv->string_val = strdup(node->init_value->string_value);
        }
    }
    
    gv->next = declared_globals;
    declared_globals = gv;
}

static void register_declared_variables(ASTNode* decl) {
    while (decl != NULL) {
        if (decl->type == AST_VAR_DECLARATION) {
            register_global_var_node(decl);
        } else if (decl->type == AST_NAMESPACE) {
            register_declared_variables(decl->children);
        } else if (decl->type == AST_IMPORT) {
            if (decl->children != NULL) {
                register_declared_variables(decl->children);
            }
        }
        decl = decl->next;
    }
}

static GlobalVar* find_global_var(const char* name) {
    if (name == NULL) return NULL;
    
    GlobalVar* curr = declared_globals;
    while (curr != NULL) {
        if (strcmp(curr->full_name, name) == 0 || strcmp(curr->name, name) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    
    if (strstr(name, "::") == NULL) {
        if (current_func_namespace != NULL) {
            char qualified[256];
            sprintf(qualified, "%s::%s", current_func_namespace, name);
            curr = declared_globals;
            while (curr != NULL) {
                if (strcmp(curr->full_name, qualified) == 0) {
                    return curr;
                }
                curr = curr->next;
            }
        }
        UsingNS* u = active_usings;
        while (u != NULL) {
            char qualified[256];
            sprintf(qualified, "%s::%s", u->name, name);
            curr = declared_globals;
            while (curr != NULL) {
                if (strcmp(curr->full_name, qualified) == 0) {
                    return curr;
                }
                curr = curr->next;
            }
            u = u->next;
        }
    }
    return NULL;
}

static void add_symbol(const char* name, int offset, int is_param, const char* type_name, int is_string, int is_array, int array_size, int is_const) {
    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->stack_offset = offset;
    sym->is_param = is_param;
    sym->type_name = type_name ? strdup(type_name) : NULL;
    sym->is_string = is_string;
    sym->is_array = is_array;
    sym->array_size = array_size;
    sym->is_const = is_const;
    sym->is_static = 0;
    sym->asm_name = NULL;
    sym->next = current_symbols;
    current_symbols = sym;
}

static void add_static_symbol(const char* name, const char* asm_name, const char* type_name, int is_string, int is_array, int array_size, int is_const) {
    Symbol* sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->stack_offset = 0;
    sym->is_param = 0;
    sym->type_name = type_name ? strdup(type_name) : NULL;
    sym->is_string = is_string;
    sym->is_array = is_array;
    sym->array_size = array_size;
    sym->is_const = is_const;
    sym->is_static = 1;
    sym->asm_name = strdup(asm_name);
    sym->next = current_symbols;
    current_symbols = sym;
}

static Symbol* find_symbol(const char* name) {
    Symbol* curr = current_symbols;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static void free_symbols() {
    Symbol* curr = current_symbols;
    while (curr != NULL) {
        Symbol* next = curr->next;
        free(curr->name);
        if (curr->type_name) free(curr->type_name);
        if (curr->asm_name) free(curr->asm_name);
        free(curr);
        curr = next;
    }
    current_symbols = NULL;
}

static void print_assembly_string(FILE* out, const char* str) {
    fprintf(out, "\"");
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') fprintf(out, "\\n");
        else if (str[i] == '\t') fprintf(out, "\\t");
        else if (str[i] == '\r') fprintf(out, "\\r");
        else if (str[i] == '\\') fprintf(out, "\\\\");
        else if (str[i] == '\"') fprintf(out, "\\\"");
        else fputc(str[i], out);
    }
    fprintf(out, "\"");
}

static void generate_expression(ASTNode* node, FILE* out);
static void generate_statement(ASTNode* node, FILE* out);

typedef struct {
    int is_pointer;     // 1 si la base está cargada en %eax, 0 si está en el stack (%ebp)
    int stack_offset;   // si !is_pointer: offset respecto a %ebp
    int ptr_offset;     // si is_pointer: offset respecto a %eax
    int is_string;
    int is_const;
    char type_name[256];
} MemberResolution;

static int resolve_member_access(ASTNode* node, MemberResolution* res, FILE* out, int load_pointer) {
    if (node == NULL || node->type != AST_MEMBER_EXPR) return 0;
    
    // Recopilar cadena de campos de hoja a raíz
    const char* field_chain[16];
    int chain_len = 0;
    ASTNode* curr = node;
    while (curr != NULL && curr->type == AST_MEMBER_EXPR && chain_len < 16) {
        field_chain[chain_len++] = curr->name;
        curr = curr->left;
    }
    
    char curr_type[256] = {0};
    int total_offset = 0;
    int is_ptr = 0;
    int is_root_const = 0;
    
    if (curr != NULL && (curr->type == AST_IDENTIFIER || curr->type == AST_THIS)) {
        Symbol* sym = find_symbol(curr->name);
        if (sym == NULL || sym->type_name == NULL) return 0;
        strcpy(curr_type, sym->type_name);
        if (strchr(curr_type, '*') != NULL) {
            char* star = strchr(curr_type, '*');
            if (star) *star = '\0';
            while (strlen(curr_type) > 0 && curr_type[strlen(curr_type)-1] == ' ') {
                curr_type[strlen(curr_type)-1] = '\0';
            }
            if (load_pointer && out != NULL) {
                fprintf(out, "    movl %d(%%ebp), %%eax\n", sym->stack_offset);
            }
            total_offset = 0;
            is_ptr = 1;
        } else {
            total_offset = sym->stack_offset;
            is_ptr = 0;
        }
        if (sym->is_const || (sym->type_name && strstr(sym->type_name, "const") != NULL)) {
            is_root_const = 1;
        }
    } else if (curr != NULL && curr->type == AST_UNARY_EXPR && curr->op == '*') {
        if (curr->left->type == AST_IDENTIFIER || curr->left->type == AST_THIS) {
            Symbol* sym = find_symbol(curr->left->name);
            if (sym == NULL || sym->type_name == NULL) return 0;
            strcpy(curr_type, sym->type_name);
            char* star = strchr(curr_type, '*');
            if (star) *star = '\0';
            while (strlen(curr_type) > 0 && curr_type[strlen(curr_type)-1] == ' ') {
                curr_type[strlen(curr_type)-1] = '\0';
            }
            if (load_pointer && out != NULL) {
                generate_expression(curr->left, out); // Carga puntero en %eax
            }
            total_offset = 0;
            is_ptr = 1;
            if (sym->is_const || (sym->type_name && strstr(sym->type_name, "const") != NULL)) {
                is_root_const = 1;
            }
        } else {
            return 0;
        }
    } else {
        return 0;
    }
    
    int is_str = 0;
    int is_const_field = is_root_const;
    for (int i = chain_len - 1; i >= 0; i--) {
        StructDef* sd = find_struct_def(curr_type);
        if (sd == NULL) return 0;
        StructFieldInfo* f = find_struct_field(sd, field_chain[i]);
        if (f == NULL) return 0;

        // Comprobación de encapsulamiento (private / protected)
        if (sd->is_class && f->access_level == ACCESS_PRIVATE) {
            if (current_func_namespace == NULL || strcmp(current_func_namespace, sd->name) != 0) {
                printf("Error semántico: el miembro '%s' de la clase '%s' es privado en la línea %d\n", f->name, sd->name, node->line);
                exit(1);
            }
        } else if (sd->is_class && f->access_level == ACCESS_PROTECTED) {
            int allowed = 0;
            if (current_func_namespace != NULL) {
                if (strcmp(current_func_namespace, sd->name) == 0) allowed = 1;
                else {
                    StructDef* cur_sd = find_struct_def(current_func_namespace);
                    if (cur_sd && cur_sd->base_class && strcmp(cur_sd->base_class, sd->name) == 0) {
                        allowed = 1;
                    }
                }
            }
            if (!allowed) {
                printf("Error semántico: el miembro '%s' de la clase '%s' es protegido en la línea %d\n", f->name, sd->name, node->line);
                exit(1);
            }
        }

        total_offset += f->offset;
        strcpy(curr_type, f->type_name);
        is_str = f->is_string;
        if (f->is_const) is_const_field = 1;
    }
    
    res->is_pointer = is_ptr;
    if (is_ptr) {
        res->ptr_offset = total_offset;
        res->stack_offset = 0;
    } else {
        res->stack_offset = total_offset;
        res->ptr_offset = 0;
    }
    res->is_string = is_str;
    res->is_const = is_const_field;
    strcpy(res->type_name, curr_type);
    return 1;
}

static int is_string_expression(ASTNode* expr) {
    if (expr == NULL) return 0;
    if (expr->type == AST_STRING) return 1;
    if (expr->type == AST_IDENTIFIER) {
        Symbol* sym = find_symbol(expr->name);
        if (sym != NULL) {
            if (sym->is_string) return 1;
            if (sym->type_name && (strstr(sym->type_name, "string") != NULL || strstr(sym->type_name, "char*") != NULL)) return 1;
        }
    }
    if (expr->type == AST_MEMBER_EXPR) {
        MemberResolution res;
        if (resolve_member_access(expr, &res, NULL, 0)) {
            return res.is_string;
        }
        if (expr->name && strstr(expr->name, "nombre") != NULL) return 1;
    }
    return 0;
}

static int get_expression_element_size(ASTNode* node) {
    if (node == NULL) return 4;
    if (node->type == AST_STRING) return 1;
    if (node->type == AST_IDENTIFIER) {
        Symbol* sym = find_symbol(node->name);
        if (sym != NULL && sym->type_name != NULL) {
            if (strstr(sym->type_name, "char") != NULL || strstr(sym->type_name, "int8") != NULL) {
                return 1;
            }
        }
        GlobalVar* gv = find_global_var(node->name);
        if (gv != NULL && gv->type_name != NULL) {
            if (strstr(gv->type_name, "char") != NULL || strstr(gv->type_name, "int8") != NULL) {
                return 1;
            }
        }
    } else if (node->type == AST_MEMBER_EXPR) {
        MemberResolution res;
        if (resolve_member_access(node, &res, NULL, 0)) {
            if (strstr(res.type_name, "char") != NULL || strstr(res.type_name, "int8") != NULL) {
                return 1;
            }
        }
    }
    if (node->type_name != NULL) {
        if (strstr(node->type_name, "char") != NULL || strstr(node->type_name, "int8") != NULL) {
            return 1;
        }
    }
    return 4;
}

static int get_identifier_memory_ref(ASTNode* node, char* out_ref) {
    if (node == NULL || node->type != AST_IDENTIFIER) return 0;
    Symbol* sym = find_symbol(node->name);
    if (sym != NULL && !sym->is_string && !sym->is_array) {
        if (sym->is_static && sym->asm_name != NULL) {
            sprintf(out_ref, "%s", sym->asm_name);
            return 1;
        }
        sprintf(out_ref, "%d(%%ebp)", sym->stack_offset);
        return 1;
    }
    GlobalVar* gv = find_global_var(node->name);
    if (gv != NULL && !gv->is_string && !gv->is_array && !gv->is_const) {
        sprintf(out_ref, "%s", gv->asm_name);
        return 1;
    }
    return 0;
}

static void push_arg_efficiently(ASTNode* arg, FILE* out) {
    if (arg->type == AST_NUMBER) {
        fprintf(out, "    pushl $%d\n", arg->int_value);
    } else if (arg->type == AST_NULL) {
        fprintf(out, "    pushl $0\n");
    } else if (arg->type == AST_THIS) {
        Symbol* this_sym = find_symbol("this");
        int this_off = this_sym ? this_sym->stack_offset : 8;
        fprintf(out, "    pushl %d(%%ebp)\n", this_off);
    } else if (arg->type == AST_STRING) {
        fprintf(out, "    pushl $.LC%d\n", arg->int_value);
    } else if (arg->type == AST_IDENTIFIER) {
        Symbol* sym = find_symbol(arg->name);
        if (sym != NULL) {
            if (sym->is_static && sym->asm_name != NULL) {
                if (sym->is_string || sym->is_array) {
                    fprintf(out, "    pushl $%s\n", sym->asm_name);
                } else {
                    fprintf(out, "    pushl %s\n", sym->asm_name);
                }
            } else if (!(sym->is_string && !sym->is_param) && !sym->is_array) {
                fprintf(out, "    pushl %d(%%ebp)\n", sym->stack_offset);
            } else {
                generate_expression(arg, out);
                fprintf(out, "    pushl %%eax\n");
            }
        } else {
            GlobalVar* gv = find_global_var(arg->name);
            if (gv != NULL) {
                if (gv->is_string || gv->is_array) {
                    fprintf(out, "    pushl $%s\n", gv->asm_name);
                } else if (gv->is_const && gv->string_val == NULL && (gv->init_node == NULL || gv->init_node->type == AST_NUMBER)) {
                    fprintf(out, "    pushl $%d\n", gv->int_val);
                } else {
                    fprintf(out, "    pushl %s\n", gv->asm_name);
                }
            } else {
                int eval = 0;
                if (find_constant(arg->name, &eval)) {
                    fprintf(out, "    pushl $%d\n", eval);
                } else {
                    generate_expression(arg, out);
                    fprintf(out, "    pushl %%eax\n");
                }
            }
        }
    } else if (arg->type == AST_MEMBER_EXPR) {
        MemberResolution res;
        if (resolve_member_access(arg, &res, NULL, 0) && !res.is_pointer && !res.is_string) {
            fprintf(out, "    pushl %d(%%ebp)\n", res.stack_offset);
        } else {
            generate_expression(arg, out);
            fprintf(out, "    pushl %%eax\n");
        }
    } else {
        generate_expression(arg, out);
        fprintf(out, "    pushl %%eax\n");
    }
}

static int current_inline_base_slot = -1;

static DeclaredFunc* find_declared_func(const char* target_func) {
    if (target_func == NULL) return NULL;
    
    // Si tiene '::', separar namespace y nombre de función
    if (strstr(target_func, "::") != NULL) {
        char ns_buf[256];
        const char* last_colon = strstr(target_func, "::");
        int ns_len = last_colon - target_func;
        if (ns_len >= 250) ns_len = 250;
        strncpy(ns_buf, target_func, ns_len);
        ns_buf[ns_len] = '\0';
        const char* fname = last_colon + 2;
        
        DeclaredFunc* df = declared_functions;
        while (df != NULL) {
            if (df->name && strcmp(df->name, fname) == 0 &&
                df->namespace_prefix && strcmp(df->namespace_prefix, ns_buf) == 0) {
                return df;
            }
            df = df->next;
        }
        return NULL;
    }
    
    // 1. Revisar si pertenece al propio namespace de la función invocante
    if (current_func_namespace != NULL) {
        DeclaredFunc* df = declared_functions;
        while (df != NULL) {
            if (strcmp(df->name, target_func) == 0 && df->namespace_prefix != NULL &&
                strcmp(df->namespace_prefix, current_func_namespace) == 0) {
                return df;
            }
            df = df->next;
        }
    }
    
    // 2. Revisar si existe global
    DeclaredFunc* df = declared_functions;
    while (df != NULL) {
        if (strcmp(df->name, target_func) == 0 && df->namespace_prefix == NULL) {
            return df;
        }
        df = df->next;
    }
    
    // 3. Revisar en los namespaces con 'using'
    UsingNS* u = active_usings;
    while (u != NULL) {
        df = declared_functions;
        while (df != NULL) {
            if (strcmp(df->name, target_func) == 0 && df->namespace_prefix != NULL &&
                strcmp(df->namespace_prefix, u->name) == 0) {
                return df;
            }
            df = df->next;
        }
        u = u->next;
    }
    
    return NULL;
}

static int get_param_or_var_size(ASTNode* node) {
    if (node == NULL) return 4;
    StructDef* sd = find_struct_def(node->type_name);
    int var_size = 4;
    int is_arr = node->is_array;
    int arr_sz = node->array_size;
    if (is_arr && arr_sz > 0) {
        var_size = arr_sz * 4;
    } else if (sd != NULL) {
        var_size = sd->total_size;
    } else if (node->type_name && strstr(node->type_name, "string") != NULL && strchr(node->type_name, '*') == NULL) {
        if (node->type == AST_PARAM) {
            var_size = 4;
        } else {
            var_size = 256;
        }
    }
    return var_size;
}

static void scan_local_vars_sizes(ASTNode* node, int* current_offset) {
    while (node != NULL) {
        if (node->type == AST_VAR_DECLARATION) {
            if (!node->is_static) {
                *current_offset += get_param_or_var_size(node);
            }
        } else if (node->type == AST_BLOCK) {
            scan_local_vars_sizes(node->statements, current_offset);
        } else if (node->type == AST_IF) {
            scan_local_vars_sizes(node->body, current_offset);
            if (node->else_branch) scan_local_vars_sizes(node->else_branch, current_offset);
        } else if (node->type == AST_WHILE) {
            scan_local_vars_sizes(node->body, current_offset);
        } else if (node->type == AST_FOR) {
            if (node->init_value) scan_local_vars_sizes(node->init_value, current_offset);
            scan_local_vars_sizes(node->body, current_offset);
        } else if (node->type == AST_DO_WHILE) {
            scan_local_vars_sizes(node->body, current_offset);
        } else if (node->type == AST_SWITCH) {
            ASTNode* c = node->body;
            while (c != NULL) {
                scan_local_vars_sizes(c->body, current_offset);
                c = c->next;
            }
        }
        node = node->next;
    }
}

#define MAX_INLINE_STACK 32
static const char* inline_call_stack[MAX_INLINE_STACK];
static int inline_call_stack_depth = 0;

static int is_in_inline_stack(const char* name) {
    if (!name) return 0;
    if (current_func_name && strcmp(current_func_name, name) == 0) return 1;
    for (int i = 0; i < inline_call_stack_depth; i++) {
        if (inline_call_stack[i] && strcmp(inline_call_stack[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int ast_contains_loops_or_calls(ASTNode* node, const char* func_name) {
    if (node == NULL) return 0;
    if (node->type == AST_FOR || node->type == AST_WHILE || node->type == AST_DO_WHILE) return 1;
    if (node->type == AST_CALL_EXPR && node->callee && node->callee->name && func_name) {
        if (strcmp(node->callee->name, func_name) == 0) return 1;
    }
    if (ast_contains_loops_or_calls(node->left, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->right, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->expr, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->init_value, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->update_expr, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->return_value, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->body, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->else_branch, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->statements, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->args, func_name)) return 1;
    if (ast_contains_loops_or_calls(node->next, func_name)) return 1;
    return 0;
}

static int compute_inlined_size(DeclaredFunc* df, int depth);

static void scan_inlines_size(ASTNode* node, int* total, int depth) {
    if (node == NULL || depth >= 4) return;
    if (node->type == AST_CALL_EXPR && node->callee && node->callee->name) {
        DeclaredFunc* df = find_declared_func(node->callee->name);
        if (df != NULL && df->is_inline && !is_in_inline_stack(df->name) && df->node != NULL && df->node->body != NULL) {
            *total += compute_inlined_size(df, depth + 1);
        }
    }
    scan_inlines_size(node->left, total, depth);
    scan_inlines_size(node->right, total, depth);
    scan_inlines_size(node->expr, total, depth);
    scan_inlines_size(node->init_value, total, depth);
    scan_inlines_size(node->update_expr, total, depth);
    scan_inlines_size(node->return_value, total, depth);
    scan_inlines_size(node->body, total, depth);
    scan_inlines_size(node->else_branch, total, depth);
    scan_inlines_size(node->statements, total, depth);
    scan_inlines_size(node->args, total, depth);
    scan_inlines_size(node->next, total, depth);
}

static int compute_inlined_size(DeclaredFunc* df, int depth) {
    if (depth >= 4 || df == NULL || df->node == NULL || df->node->body == NULL) return 0;
    if (is_in_inline_stack(df->name)) return 0;
    if (ast_contains_loops_or_calls(df->node->body, df->name)) return 0;

    if (inline_call_stack_depth < MAX_INLINE_STACK) {
        inline_call_stack[inline_call_stack_depth++] = df->name;
    }
    int total = 0;
    ASTNode* p = df->node->params;
    while (p != NULL) {
        total += get_param_or_var_size(p);
        p = p->next;
    }
    scan_local_vars_sizes(df->node->body, &total);
    scan_inlines_size(df->node->body, &total, depth + 1);
    if (inline_call_stack_depth > 0) {
        inline_call_stack_depth--;
    }
    return total;
}

static void scan_inlines(ASTNode* node, int* current_offset, int depth) {
    if (node == NULL || depth >= 4) return;
    
    if (node->type == AST_CALL_EXPR && node->callee && node->callee->name) {
        DeclaredFunc* df = find_declared_func(node->callee->name);
        if (df != NULL && df->is_inline && !is_in_inline_stack(df->name) && df->node != NULL && df->node->body != NULL) {
            node->inline_slot_offset = *current_offset;
            int needed = compute_inlined_size(df, 0);
            *current_offset += needed;
        }
    }
    
    scan_inlines(node->left, current_offset, depth);
    scan_inlines(node->right, current_offset, depth);
    scan_inlines(node->expr, current_offset, depth);
    scan_inlines(node->init_value, current_offset, depth);
    scan_inlines(node->update_expr, current_offset, depth);
    scan_inlines(node->return_value, current_offset, depth);
    scan_inlines(node->body, current_offset, depth);
    scan_inlines(node->else_branch, current_offset, depth);
    scan_inlines(node->statements, current_offset, depth);
    scan_inlines(node->args, current_offset, depth);
    scan_inlines(node->next, current_offset, depth);
}

static void add_inlined_local_symbols(ASTNode* node, int* slot_offset) {
    while (node != NULL) {
        if (node->type == AST_VAR_DECLARATION) {
            int sz = get_param_or_var_size(node);
            *slot_offset += sz;
            int is_str = (node->type_name && strstr(node->type_name, "string") && !strchr(node->type_name, '*')) ? 1 : 0;
            add_symbol(node->name, -(*slot_offset), 0, node->type_name, is_str, node->is_array, node->array_size, node->is_const);
        } else if (node->type == AST_BLOCK) {
            add_inlined_local_symbols(node->statements, slot_offset);
        } else if (node->type == AST_IF) {
            add_inlined_local_symbols(node->body, slot_offset);
            if (node->else_branch) add_inlined_local_symbols(node->else_branch, slot_offset);
        } else if (node->type == AST_WHILE) {
            add_inlined_local_symbols(node->body, slot_offset);
        } else if (node->type == AST_FOR) {
            if (node->init_value) add_inlined_local_symbols(node->init_value, slot_offset);
            add_inlined_local_symbols(node->body, slot_offset);
        } else if (node->type == AST_DO_WHILE) {
            add_inlined_local_symbols(node->body, slot_offset);
        } else if (node->type == AST_SWITCH) {
            ASTNode* c = node->body;
            while (c != NULL) {
                add_inlined_local_symbols(c->body, slot_offset);
                c = c->next;
            }
        }
        node = node->next;
    }
}

static void generate_call(ASTNode* node, FILE* out) {
    // Recopilar argumentos
    ASTNode* arg_array[64];
    int arg_count = 0;
    ASTNode* curr_arg = node->args;
    while (curr_arg != NULL && arg_count < 64) {
        arg_array[arg_count++] = curr_arg;
        curr_arg = curr_arg->next;
    }
    
    // 0. Llamada a método de objeto: obj.metodo(args) o ptr->metodo(args)
    if (node->callee != NULL && node->callee->type == AST_MEMBER_EXPR) {
        ASTNode* obj_expr = node->callee->left;
        char* method_name = node->callee->name;
        char class_name[256] = {0};
        int is_obj_ptr = 0;
        Symbol* obj_sym = NULL;

        if (obj_expr->type == AST_IDENTIFIER || obj_expr->type == AST_THIS) {
            obj_sym = find_symbol(obj_expr->name);
            if (obj_sym != NULL && obj_sym->type_name != NULL) {
                strcpy(class_name, obj_sym->type_name);
                if (strchr(class_name, '*') != NULL) {
                    is_obj_ptr = 1;
                    char* star = strchr(class_name, '*');
                    if (star) *star = '\0';
                }
            } else {
                GlobalVar* gv = find_global_var(obj_expr->name);
                if (gv != NULL && gv->type_name != NULL) {
                    strcpy(class_name, gv->type_name);
                    if (strchr(class_name, '*') != NULL) {
                        is_obj_ptr = 1;
                        char* star = strchr(class_name, '*');
                        if (star) *star = '\0';
                    }
                }
            }
        } else if (obj_expr->type == AST_UNARY_EXPR && obj_expr->op == '*') {
            if (obj_expr->left->type == AST_IDENTIFIER || obj_expr->left->type == AST_THIS) {
                obj_sym = find_symbol(obj_expr->left->name);
                if (obj_sym != NULL && obj_sym->type_name != NULL) {
                    strcpy(class_name, obj_sym->type_name);
                    char* star = strchr(class_name, '*');
                    if (star) *star = '\0';
                    is_obj_ptr = 1;
                } else {
                    GlobalVar* gv = find_global_var(obj_expr->left->name);
                    if (gv != NULL && gv->type_name != NULL) {
                        strcpy(class_name, gv->type_name);
                        char* star = strchr(class_name, '*');
                        if (star) *star = '\0';
                        is_obj_ptr = 1;
                    }
                }
            }
        }

        while (strlen(class_name) > 0 && class_name[strlen(class_name)-1] == ' ') {
            class_name[strlen(class_name)-1] = '\0';
        }

        StructDef* sd = find_struct_def(class_name);
        DeclaredFunc* df = NULL;
        if (sd != NULL) {
            df = find_exact_declared_function(method_name, sd->name);
            StructDef* search_sd = sd;
            while (df == NULL && search_sd != NULL && search_sd->base_class != NULL) {
                search_sd = find_struct_def(search_sd->base_class);
                if (search_sd != NULL) {
                    df = find_exact_declared_function(method_name, search_sd->name);
                }
            }
        }

        if (df != NULL) {
            if (df->node != NULL) {
                if (df->node->access_level == ACCESS_PRIVATE) {
                    if (current_func_namespace == NULL || strcmp(current_func_namespace, df->namespace_prefix) != 0) {
                        printf("Error semántico: el método '%s' de la clase '%s' es privado en la línea %d\n", method_name, df->namespace_prefix, node->line);
                        exit(1);
                    }
                } else if (df->node->access_level == ACCESS_PROTECTED) {
                    int allowed = 0;
                    if (current_func_namespace != NULL) {
                        if (strcmp(current_func_namespace, df->namespace_prefix) == 0) allowed = 1;
                        else {
                            StructDef* cur_sd = find_struct_def(current_func_namespace);
                            if (cur_sd && cur_sd->base_class && strcmp(cur_sd->base_class, df->namespace_prefix) == 0) {
                                allowed = 1;
                            }
                        }
                    }
                    if (!allowed) {
                        printf("Error semántico: el método '%s' de la clase '%s' es protegido en la línea %d\n", method_name, df->namespace_prefix, node->line);
                        exit(1);
                    }
                }
            }

            for (int i = arg_count - 1; i >= 0; i--) {
                push_arg_efficiently(arg_array[i], out);
            }

            if (is_obj_ptr) {
                if (obj_sym != NULL) {
                    if (obj_sym->is_static && obj_sym->asm_name != NULL) {
                        fprintf(out, "    pushl %s\n", obj_sym->asm_name);
                    } else {
                        fprintf(out, "    pushl %d(%%ebp)\n", obj_sym->stack_offset);
                    }
                } else {
                    ASTNode* ptr_node = (obj_expr->type == AST_UNARY_EXPR && obj_expr->op == '*') ? obj_expr->left : obj_expr;
                    GlobalVar* gv = (ptr_node->type == AST_IDENTIFIER) ? find_global_var(ptr_node->name) : NULL;
                    if (gv != NULL) {
                        fprintf(out, "    pushl %s\n", gv->asm_name);
                    } else {
                        generate_expression(ptr_node, out);
                        fprintf(out, "    pushl %%eax\n");
                    }
                }
            } else {
                if (obj_sym != NULL) {
                    if (obj_sym->is_static && obj_sym->asm_name != NULL) {
                        fprintf(out, "    leal %s, %%eax\n", obj_sym->asm_name);
                    } else {
                        fprintf(out, "    leal %d(%%ebp), %%eax\n", obj_sym->stack_offset);
                    }
                    fprintf(out, "    pushl %%eax\n");
                } else {
                    GlobalVar* gv = (obj_expr->type == AST_IDENTIFIER) ? find_global_var(obj_expr->name) : NULL;
                    if (gv != NULL) {
                        fprintf(out, "    leal %s, %%eax\n", gv->asm_name);
                        fprintf(out, "    pushl %%eax\n");
                    } else {
                        generate_expression(obj_expr, out);
                        fprintf(out, "    pushl %%eax\n");
                    }
                }
            }

            char asm_call[512];
            char clean_mname[256];
            int n_idx = 0;
            for (int i = 0; df->name[i] != '\0' && n_idx < 250; i++) {
                if (df->name[i] == '~') {
                    clean_mname[n_idx++] = 'd';
                    clean_mname[n_idx++] = 't';
                    clean_mname[n_idx++] = 'o';
                    clean_mname[n_idx++] = 'r';
                    clean_mname[n_idx++] = '_';
                } else {
                    clean_mname[n_idx++] = df->name[i];
                }
            }
            clean_mname[n_idx] = '\0';
            sprintf(asm_call, "_%s_%s", df->namespace_prefix, clean_mname);
            fprintf(out, "    call %s\n", asm_call);
            fprintf(out, "    addl $%d, %%esp\n", (arg_count + 1) * 4);
            return;
        }
    }
    
    char* target_func = node->callee->name;
    
    // 1. Manejo nativo e independiente de fundamental::print y fundamental::println
    int is_print = (strcmp(target_func, "fundamental::print") == 0 || strcmp(target_func, "print") == 0);
    int is_println = (strcmp(target_func, "fundamental::println") == 0 || strcmp(target_func, "println") == 0);
    
    if (is_print || is_println) {
        if (arg_count == 0) {
            if (is_println) {
                fprintf(out, "    call _fundamental_println_empty\n");
            }
            return;
        }
        for (int i = 0; i < arg_count; i++) {
            push_arg_efficiently(arg_array[i], out);
            if (is_string_expression(arg_array[i])) {
                fprintf(out, "    call _fundamental_print_str\n");
            } else {
                fprintf(out, "    call _fundamental_print_int\n");
            }
            fprintf(out, "    addl $4, %%esp\n");
        }
        if (is_println) {
            fprintf(out, "    call _fundamental_println_empty\n");
        }
        return;
    }
    
    // 2. Manejo nativo e independiente de fundamental::scanf, fundamental::read y stdints
    if (strcmp(target_func, "fundamental::scanf") == 0 || strcmp(target_func, "scanf") == 0 ||
        strcmp(target_func, "fundamental::read") == 0 || strcmp(target_func, "read") == 0 ||
        strcmp(target_func, "fundamental::stdints") == 0 || strcmp(target_func, "stdints") == 0) {
        if (arg_count > 0) {
            ASTNode* target_arg = arg_array[0];
            if (target_arg->type == AST_UNARY_EXPR && target_arg->op == '&') {
                target_arg = target_arg->left;
            }
            int is_int = 0;
            if (target_arg->type == AST_IDENTIFIER) {
                Symbol* sym = find_symbol(target_arg->name);
                if (sym != NULL && sym->type_name != NULL && strstr(sym->type_name, "int") != NULL) {
                    is_int = 1;
                }
            }
            generate_expression(arg_array[0], out);
            fprintf(out, "    pushl %%eax\n");
            if (is_int) {
                fprintf(out, "    call _fundamental_read_int\n");
            } else {
                fprintf(out, "    call _fundamental_read_str\n");
            }
            fprintf(out, "    addl $4, %%esp\n");
        }
        return;
    }
    
    DeclaredFunc* df = find_declared_func(target_func);

    // Si no es función global, comprobar si es método implícito de la clase actual
    if (df == NULL && current_func_namespace != NULL) {
        StructDef* cur_sd = find_struct_def(current_func_namespace);
        if (cur_sd != NULL) {
            DeclaredFunc* m = find_exact_declared_function(target_func, cur_sd->name);
            StructDef* s_sd = cur_sd;
            while (m == NULL && s_sd && s_sd->base_class) {
                s_sd = find_struct_def(s_sd->base_class);
                if (s_sd) m = find_exact_declared_function(target_func, s_sd->name);
            }
            if (m != NULL) {
                for (int i = arg_count - 1; i >= 0; i--) {
                    push_arg_efficiently(arg_array[i], out);
                }
                Symbol* this_sym = find_symbol("this");
                int this_off = this_sym ? this_sym->stack_offset : 8;
                fprintf(out, "    pushl %d(%%ebp)\n", this_off);
                char asm_call[512];
                char clean_mname[256];
                int n_idx = 0;
                for (int i = 0; m->name[i] != '\0' && n_idx < 250; i++) {
                    if (m->name[i] == '~') {
                        clean_mname[n_idx++] = 'd';
                        clean_mname[n_idx++] = 't';
                        clean_mname[n_idx++] = 'o';
                        clean_mname[n_idx++] = 'r';
                        clean_mname[n_idx++] = '_';
                    } else {
                        clean_mname[n_idx++] = m->name[i];
                    }
                }
                clean_mname[n_idx] = '\0';
                sprintf(asm_call, "_%s_%s", m->namespace_prefix, clean_mname);
                fprintf(out, "    call %s\n", asm_call);
                fprintf(out, "    addl $%d, %%esp\n", (arg_count + 1) * 4);
                return;
            }
        }
    }
    
    // 3. Expansión INLINE cuando la función está marcada con inline o en optimización -O2/-O3
    int max_depth = (generator_opt_level >= 2 ? 8 : 4);
    int is_trivial = 0;
    if (generator_opt_level >= 2 && df != NULL && df->node != NULL && df->node->body != NULL) {
        ASTNode* stmts = df->node->body->statements;
        if (stmts != NULL && stmts->next == NULL &&
            (stmts->type == AST_RETURN_STATEMENT || stmts->type == AST_EXPR_STATEMENT) &&
            !ast_contains_loops_or_calls(df->node->body, df->name)) {
            is_trivial = 1;
        }
    }
    int should_inline = (df != NULL && df->node != NULL && df->node->body != NULL &&
                        inlining_depth < max_depth &&
                        !is_in_inline_stack(df->name) &&
                        (df->is_inline ? !ast_contains_loops_or_calls(df->node->body, df->name) : is_trivial));
    if (should_inline) {
        int base_slot = (current_inline_base_slot >= 0) ? current_inline_base_slot : node->inline_slot_offset;
        
        // Evaluar cada argumento en el scope del llamador y almacenarlo en la ranura del parámetro
        int curr_slot = base_slot;
        ASTNode* param = df->node->params;
        int arg_idx = 0;
        while (param != NULL && arg_idx < arg_count) {
            int psz = get_param_or_var_size(param);
            curr_slot += psz;
            int param_offset = -curr_slot;
            generate_expression(arg_array[arg_idx], out);
            fprintf(out, "    movl %%eax, %d(%%ebp)\n", param_offset);
            param = param->next;
            arg_idx++;
        }
        
        // Guardar estado de símbolos
        Symbol* saved_symbols = current_symbols;
        
        // Añadir parámetros de la función inline
        curr_slot = base_slot;
        param = df->node->params;
        while (param != NULL) {
            int psz = get_param_or_var_size(param);
            curr_slot += psz;
            int param_offset = -curr_slot;
            int is_str = (param->type_name && strstr(param->type_name, "string") && !strchr(param->type_name, '*')) ? 1 : 0;
            add_symbol(param->name, param_offset, 1, param->type_name, is_str, 0, 0, param->is_const);
            param = param->next;
        }
        
        // Añadir variables locales de la función inline
        add_inlined_local_symbols(df->node->body, &curr_slot);
        
        // Configurar salto de retorno y anidamiento
        static int inline_counter = 0;
        int inline_id = ++inline_counter;
        int saved_exit = current_inline_exit_label;
        int saved_base_slot = current_inline_base_slot;
        current_inline_exit_label = inline_id;
        current_inline_base_slot = curr_slot;
        inlining_depth++;
        
        if (inline_call_stack_depth < MAX_INLINE_STACK) {
            inline_call_stack[inline_call_stack_depth++] = df->name;
        }

        // Generar cuerpo de la función inlined
        generate_statement(df->node->body, out);

        if (inline_call_stack_depth > 0) {
            inline_call_stack_depth--;
        }
        
        // Etiqueta de salida de la llamada inlined
        fprintf(out, ".Linline_exit_%d:\n", inline_id);
        
        // Restaurar estado de retorno, inlining y símbolos
        current_inline_exit_label = saved_exit;
        current_inline_base_slot = saved_base_slot;
        inlining_depth--;
        
        while (current_symbols != saved_symbols) {
            Symbol* next = current_symbols->next;
            free(current_symbols->name);
            if (current_symbols->type_name) free(current_symbols->type_name);
            free(current_symbols);
            current_symbols = next;
        }
        return;
    }
    
    // 4. Llamada regular a función normal (no inline o límite de recursión)
    for (int i = arg_count - 1; i >= 0; i--) {
        push_arg_efficiently(arg_array[i], out);
    }
    
    char asm_call_name[512];
    if (strcmp(target_func, "fundamental::printf") == 0 || strcmp(target_func, "printf") == 0) {
        strcpy(asm_call_name, "_printf");
    } else if (strcmp(target_func, "fundamental::puts") == 0 || strcmp(target_func, "puts") == 0) {
        strcpy(asm_call_name, "_puts");
    } else if (df != NULL) {
        if (df->namespace_prefix != NULL) {
            char clean_ns[256];
            int c_idx = 0;
            for (int i = 0; df->namespace_prefix[i] != '\0' && c_idx < 250; i++) {
                if (df->namespace_prefix[i] == ':' && df->namespace_prefix[i+1] == ':') {
                    clean_ns[c_idx++] = '_';
                    i++;
                } else {
                    clean_ns[c_idx++] = df->namespace_prefix[i];
                }
            }
            clean_ns[c_idx] = '\0';
            sprintf(asm_call_name, "_%s_%s", clean_ns, df->name);
        } else {
            sprintf(asm_call_name, "_%s", df->name);
        }
    } else {
        char mangled[256];
        int m_idx = 0;
        for (int i = 0; target_func[i] != '\0' && m_idx < 250; i++) {
            if (target_func[i] == ':' && target_func[i+1] == ':') {
                mangled[m_idx++] = '_';
                i++;
            } else {
                mangled[m_idx++] = target_func[i];
            }
        }
        mangled[m_idx] = '\0';
        sprintf(asm_call_name, "_%s", mangled);
    }
    
    fprintf(out, "    call %s\n", asm_call_name);
    if (arg_count > 0) {
        fprintf(out, "    addl $%d, %%esp\n", arg_count * 4);
    }
}

static void generate_expression(ASTNode* node, FILE* out) {
    if (node == NULL) return;
    
    switch (node->type) {
        case AST_NUMBER:
            if (node->int_value == 0) {
                fprintf(out, "    xorl %%eax, %%eax\n");
            } else {
                fprintf(out, "    movl $%d, %%eax\n", node->int_value);
            }
            break;
            
        case AST_BOOLEAN:
            if (node->int_value) {
                fprintf(out, "    movl $1, %%eax\n");
            } else {
                fprintf(out, "    xorl %%eax, %%eax\n");
            }
            break;

        case AST_NULL:
            fprintf(out, "    xorl %%eax, %%eax\n");
            break;

        case AST_STRING:
            fprintf(out, "    movl $.LC%d, %%eax\n", node->int_value);
            break;
            
        case AST_THIS: {
            Symbol* this_sym = find_symbol("this");
            int this_off = this_sym ? this_sym->stack_offset : 8;
            fprintf(out, "    movl %d(%%ebp), %%eax\n", this_off);
            break;
        }

        case AST_IDENTIFIER: {
            Symbol* sym = find_symbol(node->name);
            if (sym != NULL) {
                if (sym->is_static && sym->asm_name != NULL) {
                    if (sym->is_string || sym->is_array) {
                        fprintf(out, "    leal %s, %%eax\n", sym->asm_name);
                    } else {
                        fprintf(out, "    movl %s, %%eax\n", sym->asm_name);
                    }
                } else if ((sym->is_string && !sym->is_param) || sym->is_array) {
                    fprintf(out, "    leal %d(%%ebp), %%eax\n", sym->stack_offset);
                } else {
                    fprintf(out, "    movl %d(%%ebp), %%eax\n", sym->stack_offset);
                }
            } else {
                StructDef* cur_sd = (current_func_namespace != NULL) ? find_struct_def(current_func_namespace) : NULL;
                StructFieldInfo* f = cur_sd ? find_struct_field(cur_sd, node->name) : NULL;
                if (f != NULL) {
                    Symbol* this_sym = find_symbol("this");
                    int this_off = this_sym ? this_sym->stack_offset : 8;
                    fprintf(out, "    movl %d(%%ebp), %%edx\n", this_off);
                    if (f->is_string) {
                        fprintf(out, "    leal %d(%%edx), %%eax\n", f->offset);
                    } else {
                        fprintf(out, "    movl %d(%%edx), %%eax\n", f->offset);
                    }
                } else {
                    GlobalVar* gv = find_global_var(node->name);
                    if (gv != NULL) {
                        if (gv->is_string || gv->is_array) {
                            fprintf(out, "    leal %s, %%eax\n", gv->asm_name);
                        } else if (gv->is_const && gv->string_val == NULL && (gv->init_node == NULL || gv->init_node->type == AST_NUMBER)) {
                            fprintf(out, "    movl $%d, %%eax\n", gv->int_val);
                        } else {
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        }
                    } else {
                        int eval = 0;
                        if (find_constant(node->name, &eval)) {
                            if (eval == 0) fprintf(out, "    xorl %%eax, %%eax\n");
                            else fprintf(out, "    movl $%d, %%eax\n", eval);
                        } else {
                            fprintf(out, "    xorl %%eax, %%eax\n");
                        }
                    }
                }
            }
            break;
        }

        case AST_INDEX_EXPR: {
            char idx_mem[64];
            int esz = get_expression_element_size(node->left);
            int scale = (esz == 1) ? 1 : 4;
            
            // Caso 1: Índice es constante inmediata (arr[0], arr[1], etc.)
            if (node->expr->type == AST_NUMBER) {
                int idx = node->expr->int_value;
                generate_expression(node->left, out);
                if (esz == 1) {
                    fprintf(out, "    movzbl %d(%%eax), %%eax\n", idx * scale);
                } else {
                    fprintf(out, "    movl %d(%%eax), %%eax\n", idx * scale);
                }
                break;
            }
            
            // Caso 2: Índice es una variable en memoria (arr[i], arr[j], etc.)
            if (get_identifier_memory_ref(node->expr, idx_mem)) {
                generate_expression(node->left, out);
                fprintf(out, "    movl %s, %%ecx\n", idx_mem);
                if (esz == 1) {
                    fprintf(out, "    movzbl (%%eax, %%ecx, 1), %%eax\n");
                } else {
                    fprintf(out, "    movl (%%eax, %%ecx, 4), %%eax\n");
                }
                break;
            }
            
            // Caso general
            generate_expression(node->expr, out);
            fprintf(out, "    pushl %%eax\n");
            generate_expression(node->left, out);
            fprintf(out, "    movl (%%esp), %%ecx\n");
            fprintf(out, "    addl $4, %%esp\n");
            if (esz == 1) {
                fprintf(out, "    movzbl (%%eax, %%ecx, 1), %%eax\n");
            } else {
                fprintf(out, "    movl (%%eax, %%ecx, 4), %%eax\n");
            }
            break;
        }

        case AST_INC_DEC: {
            if (node->left->type == AST_IDENTIFIER) {
                Symbol* sym = find_symbol(node->left->name);
                if (sym != NULL) {
                    if (sym->is_const) {
                        printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", sym->name, node->line);
                        exit(1);
                    }
                    char target[64];
                    if (sym->is_static && sym->asm_name != NULL) {
                        strcpy(target, sym->asm_name);
                    } else {
                        sprintf(target, "%d(%%ebp)", sym->stack_offset);
                    }
                    int is_inc = (strstr(node->op_str, "++") != NULL);
                    if (node->int_value == 1) { // Postfix
                        fprintf(out, "    movl %s, %%eax\n", target);
                        if (is_inc) fprintf(out, "    incl %s\n", target);
                        else fprintf(out, "    decl %s\n", target);
                    } else { // Prefix
                        if (is_inc) fprintf(out, "    incl %s\n", target);
                        else fprintf(out, "    decl %s\n", target);
                        fprintf(out, "    movl %s, %%eax\n", target);
                    }
                } else {
                    GlobalVar* gv = find_global_var(node->left->name);
                    if (gv != NULL) {
                        if (gv->is_const) {
                            printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", gv->name, node->line);
                            exit(1);
                        }
                        int is_inc = (strstr(node->op_str, "++") != NULL);
                        if (node->int_value == 1) { // Postfix
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                            if (is_inc) fprintf(out, "    incl %s\n", gv->asm_name);
                            else fprintf(out, "    decl %s\n", gv->asm_name);
                        } else { // Prefix
                            if (is_inc) fprintf(out, "    incl %s\n", gv->asm_name);
                            else fprintf(out, "    decl %s\n", gv->asm_name);
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        }
                    } else if (current_func_namespace != NULL) {
                        StructDef* cdef = find_struct_def(current_func_namespace);
                        if (cdef != NULL) {
                            StructFieldInfo* f_info = find_struct_field(cdef, node->left->name);
                            if (f_info != NULL) {
                                int f_offset = f_info->offset;
                                if (f_info->is_const) {
                                    printf("Error semántico: no se puede modificar el miembro constante '%s' en la línea %d\n", node->left->name, node->line);
                                    exit(1);
                                }
                                int is_inc = (strstr(node->op_str, "++") != NULL);
                                fprintf(out, "    movl 8(%%ebp), %%edx\n");
                                if (node->int_value == 1) { // Postfix
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                    if (is_inc) fprintf(out, "    incl %d(%%edx)\n", f_offset);
                                    else fprintf(out, "    decl %d(%%edx)\n", f_offset);
                                } else { // Prefix
                                    if (is_inc) fprintf(out, "    incl %d(%%edx)\n", f_offset);
                                    else fprintf(out, "    decl %d(%%edx)\n", f_offset);
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                }
                            }
                        }
                    }
                }
            } else if (node->left->type == AST_SCOPED_IDENTIFIER) {
                GlobalVar* gv = find_global_var(node->left->name);
                if (gv != NULL) {
                    if (gv->is_const) {
                        printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", gv->name, node->line);
                        exit(1);
                    }
                    int is_inc = (strstr(node->op_str, "++") != NULL);
                    if (node->int_value == 1) { // Postfix
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        if (is_inc) fprintf(out, "    incl %s\n", gv->asm_name);
                        else fprintf(out, "    decl %s\n", gv->asm_name);
                    } else { // Prefix
                        if (is_inc) fprintf(out, "    incl %s\n", gv->asm_name);
                        else fprintf(out, "    decl %s\n", gv->asm_name);
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                    }
                }
            } else if (node->left->type == AST_INDEX_EXPR) {
                generate_expression(node->left->expr, out);
                fprintf(out, "    pushl %%eax\n");
                generate_expression(node->left->left, out);
                fprintf(out, "    movl (%%esp), %%ecx\n");
                fprintf(out, "    addl $4, %%esp\n");
                fprintf(out, "    leal (%%eax, %%ecx, 4), %%edx\n");
                int is_inc = (strstr(node->op_str, "++") != NULL);
                if (node->int_value == 1) { // Postfix
                    fprintf(out, "    movl (%%edx), %%eax\n");
                    if (is_inc) fprintf(out, "    incl (%%edx)\n");
                    else fprintf(out, "    decl (%%edx)\n");
                } else { // Prefix
                    if (is_inc) fprintf(out, "    incl (%%edx)\n");
                    else fprintf(out, "    decl (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                }
            } else if (node->left->type == AST_MEMBER_EXPR) {
                MemberResolution res;
                if (resolve_member_access(node->left, &res, out, 1)) {
                    if (res.is_const) {
                        printf("Error semántico: no se puede modificar el miembro constante '%s' en la línea %d\n", node->left->name, node->line);
                        exit(1);
                    }
                    int is_inc = (strstr(node->op_str, "++") != NULL);
                    if (res.is_pointer) {
                        if (node->int_value == 1) { // Postfix
                            fprintf(out, "    movl %d(%%eax), %%edx\n", res.ptr_offset);
                            if (is_inc) fprintf(out, "    incl %d(%%eax)\n", res.ptr_offset);
                            else fprintf(out, "    decl %d(%%eax)\n", res.ptr_offset);
                            fprintf(out, "    movl %%edx, %%eax\n");
                        } else { // Prefix
                            if (is_inc) fprintf(out, "    incl %d(%%eax)\n", res.ptr_offset);
                            else fprintf(out, "    decl %d(%%eax)\n", res.ptr_offset);
                            fprintf(out, "    movl %d(%%eax), %%eax\n", res.ptr_offset);
                        }
                    } else {
                        if (node->int_value == 1) { // Postfix
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                            if (is_inc) fprintf(out, "    incl %d(%%ebp)\n", res.stack_offset);
                            else fprintf(out, "    decl %d(%%ebp)\n", res.stack_offset);
                        } else { // Prefix
                            if (is_inc) fprintf(out, "    incl %d(%%ebp)\n", res.stack_offset);
                            else fprintf(out, "    decl %d(%%ebp)\n", res.stack_offset);
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                        }
                    }
                }
            } else if (node->left->type == AST_UNARY_EXPR && node->left->op == '*') {
                if (node->left->left->type == AST_IDENTIFIER) {
                    Symbol* psym = find_symbol(node->left->left->name);
                    if (psym != NULL && psym->type_name != NULL && strstr(psym->type_name, "const") != NULL) {
                        printf("Error semántico: no se puede modificar el valor apuntado por un puntero a constante '%s' en la línea %d\n", psym->name, node->line);
                        exit(1);
                    }
                }
                generate_expression(node->left->left, out);
                int is_inc = (strstr(node->op_str, "++") != NULL);
                if (node->int_value == 1) {
                    fprintf(out, "    movl (%%eax), %%edx\n");
                    if (is_inc) fprintf(out, "    incl (%%eax)\n");
                    else fprintf(out, "    decl (%%eax)\n");
                    fprintf(out, "    movl %%edx, %%eax\n");
                } else {
                    if (is_inc) fprintf(out, "    incl (%%eax)\n");
                    else fprintf(out, "    decl (%%eax)\n");
                    fprintf(out, "    movl (%%eax), %%eax\n");
                }
            }
            break;
        }

        case AST_COMPOUND_ASSIGNMENT: {
            if (node->left->type == AST_IDENTIFIER) {
                Symbol* sym = find_symbol(node->left->name);
                if (sym != NULL) {
                    if (sym->is_const) {
                        printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", sym->name, node->line);
                        exit(1);
                    }
                    char target[64];
                    if (sym->is_static && sym->asm_name != NULL) {
                        strcpy(target, sym->asm_name);
                    } else {
                        sprintf(target, "%d(%%ebp)", sym->stack_offset);
                    }
                    generate_expression(node->init_value, out);
                    if (strcmp(node->op_str, "+=") == 0) {
                        fprintf(out, "    addl %%eax, %s\n", target);
                        fprintf(out, "    movl %s, %%eax\n", target);
                    } else if (strcmp(node->op_str, "-=") == 0) {
                        fprintf(out, "    subl %%eax, %s\n", target);
                        fprintf(out, "    movl %s, %%eax\n", target);
                    } else if (strcmp(node->op_str, "*=") == 0) {
                        fprintf(out, "    imull %s, %%eax\n", target);
                        fprintf(out, "    movl %%eax, %s\n", target);
                    } else if (strcmp(node->op_str, "/=") == 0) {
                        fprintf(out, "    movl %%eax, %%ecx\n");
                        fprintf(out, "    movl %s, %%eax\n", target);
                        fprintf(out, "    cltd\n");
                        fprintf(out, "    idivl %%ecx\n");
                        fprintf(out, "    movl %%eax, %s\n", target);
                    } else if (strcmp(node->op_str, "%=") == 0) {
                        fprintf(out, "    movl %%eax, %%ecx\n");
                        fprintf(out, "    movl %s, %%eax\n", target);
                        fprintf(out, "    cltd\n");
                        fprintf(out, "    idivl %%ecx\n");
                        fprintf(out, "    movl %%edx, %s\n", target);
                        fprintf(out, "    movl %%edx, %%eax\n");
                    } else if (strcmp(node->op_str, "&=") == 0) {
                        fprintf(out, "    andl %%eax, %s\n", target);
                        fprintf(out, "    movl %s, %%eax\n", target);
                    } else if (strcmp(node->op_str, "|=") == 0) {
                        fprintf(out, "    orl %%eax, %s\n", target);
                        fprintf(out, "    movl %s, %%eax\n", target);
                    } else if (strcmp(node->op_str, "^=") == 0) {
                        fprintf(out, "    xorl %%eax, %s\n", target);
                        fprintf(out, "    movl %s, %%eax\n", target);
                    } else if (strcmp(node->op_str, "<<=") == 0) {
                        fprintf(out, "    movl %%eax, %%ecx\n");
                        fprintf(out, "    shll %%cl, %s\n", target);
                        fprintf(out, "    movl %s, %%eax\n", target);
                    } else if (strcmp(node->op_str, ">>=") == 0) {
                        fprintf(out, "    movl %%eax, %%ecx\n");
                        fprintf(out, "    sarl %%cl, %s\n", target);
                        fprintf(out, "    movl %s, %%eax\n", target);
                    }
                } else {
                    GlobalVar* gv = find_global_var(node->left->name);
                    if (gv != NULL) {
                        if (gv->is_const) {
                            printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", gv->name, node->line);
                            exit(1);
                        }
                        generate_expression(node->init_value, out);
                        if (strcmp(node->op_str, "+=") == 0) {
                            fprintf(out, "    addl %%eax, %s\n", gv->asm_name);
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        } else if (strcmp(node->op_str, "-=") == 0) {
                            fprintf(out, "    subl %%eax, %s\n", gv->asm_name);
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        } else if (strcmp(node->op_str, "*=") == 0) {
                            fprintf(out, "    imull %s, %%eax\n", gv->asm_name);
                            fprintf(out, "    movl %%eax, %s\n", gv->asm_name);
                        } else if (strcmp(node->op_str, "/=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                            fprintf(out, "    cltd\n");
                            fprintf(out, "    idivl %%ecx\n");
                            fprintf(out, "    movl %%eax, %s\n", gv->asm_name);
                        } else if (strcmp(node->op_str, "%=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                            fprintf(out, "    cltd\n");
                            fprintf(out, "    idivl %%ecx\n");
                            fprintf(out, "    movl %%edx, %s\n", gv->asm_name);
                            fprintf(out, "    movl %%edx, %%eax\n");
                        } else if (strcmp(node->op_str, "&=") == 0) {
                            fprintf(out, "    andl %%eax, %s\n", gv->asm_name);
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        } else if (strcmp(node->op_str, "|=") == 0) {
                            fprintf(out, "    orl %%eax, %s\n", gv->asm_name);
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        } else if (strcmp(node->op_str, "^=") == 0) {
                            fprintf(out, "    xorl %%eax, %s\n", gv->asm_name);
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        } else if (strcmp(node->op_str, "<<=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    shll %%cl, %s\n", gv->asm_name);
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        } else if (strcmp(node->op_str, ">>=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    sarl %%cl, %s\n", gv->asm_name);
                            fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        }
                    } else if (current_func_namespace != NULL) {
                        StructDef* cdef = find_struct_def(current_func_namespace);
                        if (cdef != NULL) {
                            StructFieldInfo* f_info = find_struct_field(cdef, node->left->name);
                            if (f_info != NULL) {
                                int f_offset = f_info->offset;
                                if (f_info->is_const) {
                                    printf("Error semántico: no se puede modificar el miembro constante '%s' en la línea %d\n", node->left->name, node->line);
                                    exit(1);
                                }
                                generate_expression(node->init_value, out);
                                fprintf(out, "    movl 8(%%ebp), %%edx\n");
                                if (strcmp(node->op_str, "+=") == 0) {
                                    fprintf(out, "    addl %%eax, %d(%%edx)\n", f_offset);
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                } else if (strcmp(node->op_str, "-=") == 0) {
                                    fprintf(out, "    subl %%eax, %d(%%edx)\n", f_offset);
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                } else if (strcmp(node->op_str, "*=") == 0) {
                                    fprintf(out, "    imull %d(%%edx), %%eax\n", f_offset);
                                    fprintf(out, "    movl %%eax, %d(%%edx)\n", f_offset);
                                } else if (strcmp(node->op_str, "/=") == 0) {
                                    fprintf(out, "    movl %%eax, %%ecx\n");
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                    fprintf(out, "    cltd\n");
                                    fprintf(out, "    idivl %%ecx\n");
                                    fprintf(out, "    movl %%eax, %d(%%edx)\n", f_offset);
                                } else if (strcmp(node->op_str, "%=") == 0) {
                                    fprintf(out, "    movl %%eax, %%ecx\n");
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                    fprintf(out, "    cltd\n");
                                    fprintf(out, "    idivl %%ecx\n");
                                    fprintf(out, "    movl %%edx, %d(%%edx)\n", f_offset);
                                    fprintf(out, "    movl %%edx, %%eax\n");
                                } else if (strcmp(node->op_str, "&=") == 0) {
                                    fprintf(out, "    andl %%eax, %d(%%edx)\n", f_offset);
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                } else if (strcmp(node->op_str, "|=") == 0) {
                                    fprintf(out, "    orl %%eax, %d(%%edx)\n", f_offset);
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                } else if (strcmp(node->op_str, "^=") == 0) {
                                    fprintf(out, "    xorl %%eax, %d(%%edx)\n", f_offset);
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                } else if (strcmp(node->op_str, "<<=") == 0) {
                                    fprintf(out, "    movl %%eax, %%ecx\n");
                                    fprintf(out, "    shll %%cl, %d(%%edx)\n", f_offset);
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                } else if (strcmp(node->op_str, ">>=") == 0) {
                                    fprintf(out, "    movl %%eax, %%ecx\n");
                                    fprintf(out, "    sarl %%cl, %d(%%edx)\n", f_offset);
                                    fprintf(out, "    movl %d(%%edx), %%eax\n", f_offset);
                                }
                            }
                        }
                    }
                }
            } else if (node->left->type == AST_SCOPED_IDENTIFIER) {
                GlobalVar* gv = find_global_var(node->left->name);
                if (gv != NULL) {
                    if (gv->is_const) {
                        printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", gv->name, node->line);
                        exit(1);
                    }
                    generate_expression(node->init_value, out);
                    if (strcmp(node->op_str, "+=") == 0) {
                        fprintf(out, "    addl %%eax, %s\n", gv->asm_name);
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                    } else if (strcmp(node->op_str, "-=") == 0) {
                        fprintf(out, "    subl %%eax, %s\n", gv->asm_name);
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                    } else if (strcmp(node->op_str, "*=") == 0) {
                        fprintf(out, "    imull %s, %%eax\n", gv->asm_name);
                        fprintf(out, "    movl %%eax, %s\n", gv->asm_name);
                    } else if (strcmp(node->op_str, "/=") == 0) {
                        fprintf(out, "    movl %%eax, %%ecx\n");
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        fprintf(out, "    cltd\n");
                        fprintf(out, "    idivl %%ecx\n");
                        fprintf(out, "    movl %%eax, %s\n", gv->asm_name);
                    } else if (strcmp(node->op_str, "%=") == 0) {
                        fprintf(out, "    movl %%eax, %%ecx\n");
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                        fprintf(out, "    cltd\n");
                        fprintf(out, "    idivl %%ecx\n");
                        fprintf(out, "    movl %%edx, %s\n", gv->asm_name);
                        fprintf(out, "    movl %%edx, %%eax\n");
                    } else if (strcmp(node->op_str, "&=") == 0) {
                        fprintf(out, "    andl %%eax, %s\n", gv->asm_name);
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                    } else if (strcmp(node->op_str, "|=") == 0) {
                        fprintf(out, "    orl %%eax, %s\n", gv->asm_name);
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                    } else if (strcmp(node->op_str, "^=") == 0) {
                        fprintf(out, "    xorl %%eax, %s\n", gv->asm_name);
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                    } else if (strcmp(node->op_str, "<<=") == 0) {
                        fprintf(out, "    movl %%eax, %%ecx\n");
                        fprintf(out, "    shll %%cl, %s\n", gv->asm_name);
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                    } else if (strcmp(node->op_str, ">>=") == 0) {
                        fprintf(out, "    movl %%eax, %%ecx\n");
                        fprintf(out, "    sarl %%cl, %s\n", gv->asm_name);
                        fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                    }
                }
            } else if (node->left->type == AST_INDEX_EXPR) {
                generate_expression(node->left->expr, out);
                fprintf(out, "    pushl %%eax\n");
                generate_expression(node->left->left, out);
                fprintf(out, "    pushl %%eax\n");
                generate_expression(node->init_value, out);
                fprintf(out, "    movl (%%esp), %%edx\n");
                fprintf(out, "    movl 4(%%esp), %%ecx\n");
                fprintf(out, "    addl $8, %%esp\n");
                int esz = get_expression_element_size(node->left->left);
                int scale = (esz == 1) ? 1 : 4;
                fprintf(out, "    leal (%%edx, %%ecx, %d), %%edx\n", scale);
                if (strcmp(node->op_str, "+=") == 0) {
                    fprintf(out, "    addl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "-=") == 0) {
                    fprintf(out, "    subl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "*=") == 0) {
                    fprintf(out, "    imull (%%edx), %%eax\n");
                    fprintf(out, "    movl %%eax, (%%edx)\n");
                } else if (strcmp(node->op_str, "/=") == 0) {
                    fprintf(out, "    movl %%eax, %%ecx\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                    fprintf(out, "    cltd\n");
                    fprintf(out, "    idivl %%ecx\n");
                    fprintf(out, "    movl %%eax, (%%edx)\n");
                } else if (strcmp(node->op_str, "%=") == 0) {
                    fprintf(out, "    movl %%eax, %%ecx\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                    fprintf(out, "    cltd\n");
                    fprintf(out, "    idivl %%ecx\n");
                    fprintf(out, "    movl %%edx, (%%edx)\n");
                    fprintf(out, "    movl %%edx, %%eax\n");
                } else if (strcmp(node->op_str, "&=") == 0) {
                    fprintf(out, "    andl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "|=") == 0) {
                    fprintf(out, "    orl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "^=") == 0) {
                    fprintf(out, "    xorl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "<<=") == 0) {
                    fprintf(out, "    movl %%eax, %%ecx\n");
                    fprintf(out, "    shll %%cl, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, ">>=") == 0) {
                    fprintf(out, "    movl %%eax, %%ecx\n");
                    fprintf(out, "    sarl %%cl, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                }
            } else if (node->left->type == AST_MEMBER_EXPR) {
                MemberResolution res;
                if (resolve_member_access(node->left, &res, out, 1)) {
                    if (res.is_const) {
                        printf("Error semántico: no se puede modificar el miembro constante '%s' en la línea %d\n", node->left->name, node->line);
                        exit(1);
                    }
                    if (res.is_pointer) {
                        fprintf(out, "    pushl %%eax\n");
                        generate_expression(node->init_value, out);
                        fprintf(out, "    movl (%%esp), %%edx\n");
                        fprintf(out, "    addl $4, %%esp\n");
                        if (strcmp(node->op_str, "+=") == 0) {
                            fprintf(out, "    addl %%eax, %d(%%edx)\n", res.ptr_offset);
                            fprintf(out, "    movl %d(%%edx), %%eax\n", res.ptr_offset);
                        } else if (strcmp(node->op_str, "-=") == 0) {
                            fprintf(out, "    subl %%eax, %d(%%edx)\n", res.ptr_offset);
                            fprintf(out, "    movl %d(%%edx), %%eax\n", res.ptr_offset);
                        } else if (strcmp(node->op_str, "*=") == 0) {
                            fprintf(out, "    imull %d(%%edx), %%eax\n", res.ptr_offset);
                            fprintf(out, "    movl %%eax, %d(%%edx)\n", res.ptr_offset);
                        } else if (strcmp(node->op_str, "/=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    movl %d(%%edx), %%eax\n", res.ptr_offset);
                            fprintf(out, "    cltd\n");
                            fprintf(out, "    idivl %%ecx\n");
                            fprintf(out, "    movl %%eax, %d(%%edx)\n", res.ptr_offset);
                        } else if (strcmp(node->op_str, "%=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    movl %d(%%edx), %%eax\n", res.ptr_offset);
                            fprintf(out, "    cltd\n");
                            fprintf(out, "    idivl %%ecx\n");
                            fprintf(out, "    movl %%edx, %d(%%edx)\n", res.ptr_offset);
                            fprintf(out, "    movl %%edx, %%eax\n");
                        } else if (strcmp(node->op_str, "&=") == 0) {
                            fprintf(out, "    andl %%eax, %d(%%edx)\n", res.ptr_offset);
                            fprintf(out, "    movl %d(%%edx), %%eax\n", res.ptr_offset);
                        } else if (strcmp(node->op_str, "|=") == 0) {
                            fprintf(out, "    orl %%eax, %d(%%edx)\n", res.ptr_offset);
                            fprintf(out, "    movl %d(%%edx), %%eax\n", res.ptr_offset);
                        } else if (strcmp(node->op_str, "^=") == 0) {
                            fprintf(out, "    xorl %%eax, %d(%%edx)\n", res.ptr_offset);
                            fprintf(out, "    movl %d(%%edx), %%eax\n", res.ptr_offset);
                        } else if (strcmp(node->op_str, "<<=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    shll %%cl, %d(%%edx)\n", res.ptr_offset);
                            fprintf(out, "    movl %d(%%edx), %%eax\n", res.ptr_offset);
                        } else if (strcmp(node->op_str, ">>=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    sarl %%cl, %d(%%edx)\n", res.ptr_offset);
                            fprintf(out, "    movl %d(%%edx), %%eax\n", res.ptr_offset);
                        }
                    } else {
                        generate_expression(node->init_value, out);
                        if (strcmp(node->op_str, "+=") == 0) {
                            fprintf(out, "    addl %%eax, %d(%%ebp)\n", res.stack_offset);
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                        } else if (strcmp(node->op_str, "-=") == 0) {
                            fprintf(out, "    subl %%eax, %d(%%ebp)\n", res.stack_offset);
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                        } else if (strcmp(node->op_str, "*=") == 0) {
                            fprintf(out, "    imull %d(%%ebp), %%eax\n", res.stack_offset);
                            fprintf(out, "    movl %%eax, %d(%%ebp)\n", res.stack_offset);
                        } else if (strcmp(node->op_str, "/=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                            fprintf(out, "    cltd\n");
                            fprintf(out, "    idivl %%ecx\n");
                            fprintf(out, "    movl %%eax, %d(%%ebp)\n", res.stack_offset);
                        } else if (strcmp(node->op_str, "%=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                            fprintf(out, "    cltd\n");
                            fprintf(out, "    idivl %%ecx\n");
                            fprintf(out, "    movl %%edx, %d(%%ebp)\n", res.stack_offset);
                            fprintf(out, "    movl %%edx, %%eax\n");
                        } else if (strcmp(node->op_str, "&=") == 0) {
                            fprintf(out, "    andl %%eax, %d(%%ebp)\n", res.stack_offset);
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                        } else if (strcmp(node->op_str, "|=") == 0) {
                            fprintf(out, "    orl %%eax, %d(%%ebp)\n", res.stack_offset);
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                        } else if (strcmp(node->op_str, "^=") == 0) {
                            fprintf(out, "    xorl %%eax, %d(%%ebp)\n", res.stack_offset);
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                        } else if (strcmp(node->op_str, "<<=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    shll %%cl, %d(%%ebp)\n", res.stack_offset);
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                        } else if (strcmp(node->op_str, ">>=") == 0) {
                            fprintf(out, "    movl %%eax, %%ecx\n");
                            fprintf(out, "    sarl %%cl, %d(%%ebp)\n", res.stack_offset);
                            fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                        }
                    }
                }
            } else if (node->left->type == AST_UNARY_EXPR && node->left->op == '*') {
                if (node->left->left->type == AST_IDENTIFIER) {
                    Symbol* psym = find_symbol(node->left->left->name);
                    if (psym != NULL && psym->type_name != NULL && strstr(psym->type_name, "const") != NULL) {
                        printf("Error semántico: no se puede modificar el valor apuntado por un puntero a constante '%s' en la línea %d\n", psym->name, node->line);
                        exit(1);
                    }
                }
                generate_expression(node->left->left, out);
                fprintf(out, "    pushl %%eax\n");
                generate_expression(node->init_value, out);
                fprintf(out, "    movl (%%esp), %%edx\n");
                fprintf(out, "    addl $4, %%esp\n");
                if (strcmp(node->op_str, "+=") == 0) {
                    fprintf(out, "    addl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "-=") == 0) {
                    fprintf(out, "    subl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "*=") == 0) {
                    fprintf(out, "    imull (%%edx), %%eax\n");
                    fprintf(out, "    movl %%eax, (%%edx)\n");
                } else if (strcmp(node->op_str, "/=") == 0) {
                    fprintf(out, "    movl %%eax, %%ecx\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                    fprintf(out, "    cltd\n");
                    fprintf(out, "    idivl %%ecx\n");
                    fprintf(out, "    movl %%eax, (%%edx)\n");
                } else if (strcmp(node->op_str, "%=") == 0) {
                    fprintf(out, "    movl %%eax, %%ecx\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                    fprintf(out, "    cltd\n");
                    fprintf(out, "    idivl %%ecx\n");
                    fprintf(out, "    movl %%edx, (%%edx)\n");
                    fprintf(out, "    movl %%edx, %%eax\n");
                } else if (strcmp(node->op_str, "&=") == 0) {
                    fprintf(out, "    andl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "|=") == 0) {
                    fprintf(out, "    orl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "^=") == 0) {
                    fprintf(out, "    xorl %%eax, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, "<<=") == 0) {
                    fprintf(out, "    movl %%eax, %%ecx\n");
                    fprintf(out, "    shll %%cl, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                } else if (strcmp(node->op_str, ">>=") == 0) {
                    fprintf(out, "    movl %%eax, %%ecx\n");
                    fprintf(out, "    sarl %%cl, (%%edx)\n");
                    fprintf(out, "    movl (%%edx), %%eax\n");
                }
            }
            break;
        }

        case AST_UNARY_EXPR:
            if (node->op == '&') {
                if (node->left->type == AST_IDENTIFIER) {
                    Symbol* sym = find_symbol(node->left->name);
                    if (sym != NULL) {
                        if (sym->is_static && sym->asm_name != NULL) {
                            fprintf(out, "    leal %s, %%eax\n", sym->asm_name);
                        } else {
                            fprintf(out, "    leal %d(%%ebp), %%eax\n", sym->stack_offset);
                        }
                    } else {
                        GlobalVar* gv = find_global_var(node->left->name);
                        if (gv != NULL) {
                            fprintf(out, "    leal %s, %%eax\n", gv->asm_name);
                        } else {
                            fprintf(out, "    xorl %%eax, %%eax\n");
                        }
                    }
                } else if (node->left->type == AST_SCOPED_IDENTIFIER) {
                    GlobalVar* gv = find_global_var(node->left->name);
                    if (gv != NULL) {
                        fprintf(out, "    leal %s, %%eax\n", gv->asm_name);
                    } else {
                        fprintf(out, "    xorl %%eax, %%eax\n");
                    }
                } else if (node->left->type == AST_MEMBER_EXPR) {
                    MemberResolution res;
                    if (resolve_member_access(node->left, &res, out, 1)) {
                        if (res.is_pointer) {
                            if (res.ptr_offset > 0) fprintf(out, "    addl $%d, %%eax\n", res.ptr_offset);
                        } else {
                            fprintf(out, "    leal %d(%%ebp), %%eax\n", res.stack_offset);
                        }
                        break;
                    }
                    fprintf(out, "    xorl %%eax, %%eax\n");
                } else if (node->left->type == AST_UNARY_EXPR && node->left->op == '*') {
                    generate_expression(node->left->left, out);
                } else {
                    generate_expression(node->left, out);
                }
            } else if (node->op == '*') {
                generate_expression(node->left, out);
                fprintf(out, "    movl (%%eax), %%eax\n");
            } else if (node->op == '-') {
                generate_expression(node->left, out);
                fprintf(out, "    negl %%eax\n");
            } else if (node->op == '!') {
                generate_expression(node->left, out);
                fprintf(out, "    testl %%eax, %%eax\n");
                fprintf(out, "    sete %%al\n");
                fprintf(out, "    movzbl %%al, %%eax\n");
            } else if (node->op == '~') {
                generate_expression(node->left, out);
                fprintf(out, "    notl %%eax\n");
            }
            break;
            
        case AST_SCOPED_IDENTIFIER: {
            GlobalVar* gv = find_global_var(node->name);
            if (gv != NULL) {
                if (gv->is_string || gv->is_array) {
                    fprintf(out, "    leal %s, %%eax\n", gv->asm_name);
                } else if (gv->is_const && gv->string_val == NULL && (gv->init_node == NULL || gv->init_node->type == AST_NUMBER)) {
                    fprintf(out, "    movl $%d, %%eax\n", gv->int_val);
                } else {
                    fprintf(out, "    movl %s, %%eax\n", gv->asm_name);
                }
            } else {
                int eval = 0;
                if (find_constant(node->name, &eval)) {
                    if (eval == 0) fprintf(out, "    xorl %%eax, %%eax\n");
                    else fprintf(out, "    movl $%d, %%eax\n", eval);
                } else {
                    fprintf(out, "    xorl %%eax, %%eax\n");
                }
            }
            break;
        }
            
        case AST_MEMBER_EXPR: {
            MemberResolution res;
            if (resolve_member_access(node, &res, out, 1)) {
                if (res.is_pointer) {
                    if (res.is_string) {
                        if (res.ptr_offset > 0) fprintf(out, "    addl $%d, %%eax\n", res.ptr_offset);
                    } else {
                        fprintf(out, "    movl %d(%%eax), %%eax\n", res.ptr_offset);
                    }
                } else {
                    if (res.is_string) {
                        fprintf(out, "    leal %d(%%ebp), %%eax\n", res.stack_offset);
                    } else {
                        fprintf(out, "    movl %d(%%ebp), %%eax\n", res.stack_offset);
                    }
                }
                break;
            }
            generate_expression(node->left, out);
            break;
        }
            
        case AST_BINARY_EXPR: {
            // Constant Folding si ambos operandos son números o NULL
            if ((node->left->type == AST_NUMBER || node->left->type == AST_NULL) &&
                (node->right->type == AST_NUMBER || node->right->type == AST_NULL)) {
                int lv = (node->left->type == AST_NULL) ? 0 : node->left->int_value;
                int rv = (node->right->type == AST_NULL) ? 0 : node->right->int_value;
                int res = 0;
                int folded = 1;
                if (node->op == '+') res = lv + rv;
                else if (node->op == '-') res = lv - rv;
                else if (node->op == '*') res = lv * rv;
                else if (node->op == '/' && rv != 0) res = lv / rv;
                else if (node->op == '%' && rv != 0) res = lv % rv;
                else if (node->op == '&') res = lv & rv;
                else if (node->op == '|') res = lv | rv;
                else if (node->op == '^') res = lv ^ rv;
                else if (node->op == 'S') res = lv << rv;
                else if (node->op == 'R') res = lv >> rv;
                else if (node->op == '<') res = (lv < rv);
                else if (node->op == '>') res = (lv > rv);
                else if (node->op == 'L') res = (lv <= rv);
                else if (node->op == 'G') res = (lv >= rv);
                else if (node->op == 'E') res = (lv == rv);
                else if (node->op == 'N') res = (lv != rv);
                else if (node->op == 'A') res = (lv && rv);
                else if (node->op == 'O') res = (lv || rv);
                else folded = 0;
                
                if (folded) {
                    if (res == 0) fprintf(out, "    xorl %%eax, %%eax\n");
                    else fprintf(out, "    movl $%d, %%eax\n", res);
                    break;
                }
            }
            
            // Reordenar para conmutativos
            ASTNode* left_op = node->left;
            ASTNode* right_op = node->right;
            if (((left_op->type == AST_NUMBER || left_op->type == AST_NULL) && (right_op->type != AST_NUMBER && right_op->type != AST_NULL)) ||
                (left_op->type == AST_IDENTIFIER && right_op->type != AST_IDENTIFIER && right_op->type != AST_NUMBER && right_op->type != AST_NULL)) {
                if (node->op == '+' || node->op == '*' || node->op == '&' || node->op == '|' || node->op == '^') {
                    left_op = node->right;
                    right_op = node->left;
                }
            }
            
            // Operación directa con constante inmediata para aritmética simple y bitwise
            if ((right_op->type == AST_NUMBER || right_op->type == AST_NULL) && (node->op == '+' || node->op == '-' || node->op == '*' || node->op == '/' ||
                                                 node->op == '%' ||
                                                 node->op == '&' || node->op == '|' || node->op == '^' ||
                                                 node->op == 'S' || node->op == 'R')) {
                generate_expression(left_op, out);
                int imm = (right_op->type == AST_NULL) ? 0 : right_op->int_value;
                if (node->op == '+') {
                    if (imm == 1) fprintf(out, "    incl %%eax\n");
                    else if (imm == -1) fprintf(out, "    decl %%eax\n");
                    else if (imm != 0) fprintf(out, "    addl $%d, %%eax\n", imm);
                } else if (node->op == '-') {
                    if (imm == 1) fprintf(out, "    decl %%eax\n");
                    else if (imm == -1) fprintf(out, "    incl %%eax\n");
                    else if (imm != 0) fprintf(out, "    subl $%d, %%eax\n", imm);
                } else if (node->op == '*') {
                    if (imm == 0) fprintf(out, "    xorl %%eax, %%eax\n");
                    else if (imm == 1) { /* no-op */ }
                    else if (imm == -1) fprintf(out, "    negl %%eax\n");
                    else if (imm == 2) fprintf(out, "    addl %%eax, %%eax\n");
                    else if (imm == 3) fprintf(out, "    leal (%%eax, %%eax, 2), %%eax\n");
                    else if (imm == 5) fprintf(out, "    leal (%%eax, %%eax, 4), %%eax\n");
                    else if (imm == 6) {
                        fprintf(out, "    leal (%%eax, %%eax, 2), %%eax\n");
                        fprintf(out, "    addl %%eax, %%eax\n");
                    } else if (imm == 9) fprintf(out, "    leal (%%eax, %%eax, 8), %%eax\n");
                    else if (imm == 10) {
                        fprintf(out, "    leal (%%eax, %%eax, 4), %%eax\n");
                        fprintf(out, "    addl %%eax, %%eax\n");
                    } else if (imm > 0 && (imm & (imm - 1)) == 0) {
                        int shift = 0;
                        while ((1 << shift) < imm) shift++;
                        fprintf(out, "    shll $%d, %%eax\n", shift);
                    } else fprintf(out, "    imull $%d, %%eax\n", imm);
                } else if (node->op == '/') {
                    if (imm == 1) {
                        /* no-op */
                    } else if (imm == -1) {
                        fprintf(out, "    negl %%eax\n");
                    } else if (imm == 2) {
                        fprintf(out, "    movl %%eax, %%edx\n");
                        fprintf(out, "    shrl $31, %%edx\n");
                        fprintf(out, "    addl %%edx, %%eax\n");
                        fprintf(out, "    sarl $1, %%eax\n");
                    } else if (imm > 0 && (imm & (imm - 1)) == 0) {
                        int k = 0;
                        while ((1 << k) < imm) k++;
                        fprintf(out, "    movl %%eax, %%edx\n");
                        fprintf(out, "    sarl $31, %%edx\n");
                        fprintf(out, "    shrl $%d, %%edx\n", 32 - k);
                        fprintf(out, "    addl %%edx, %%eax\n");
                        fprintf(out, "    sarl $%d, %%eax\n", k);
                    } else if (imm != 0) {
                        fprintf(out, "    movl $%d, %%ecx\n", imm);
                        fprintf(out, "    cltd\n");
                        fprintf(out, "    idivl %%ecx\n");
                    }
                } else if (node->op == '%') {
                    if (imm == 1 || imm == -1) {
                        fprintf(out, "    xorl %%eax, %%eax\n");
                    } else if (imm == 2) {
                        fprintf(out, "    movl %%eax, %%edx\n");
                        fprintf(out, "    shrl $31, %%edx\n");
                        fprintf(out, "    addl %%edx, %%eax\n");
                        fprintf(out, "    andl $1, %%eax\n");
                        fprintf(out, "    subl %%edx, %%eax\n");
                    } else if (imm > 0 && (imm & (imm - 1)) == 0) {
                        int k = 0;
                        while ((1 << k) < imm) k++;
                        fprintf(out, "    movl %%eax, %%edx\n");
                        fprintf(out, "    sarl $31, %%edx\n");
                        fprintf(out, "    shrl $%d, %%edx\n", 32 - k);
                        fprintf(out, "    addl %%edx, %%eax\n");
                        fprintf(out, "    andl $%d, %%eax\n", imm - 1);
                        fprintf(out, "    subl %%edx, %%eax\n");
                    } else if (imm != 0) {
                        fprintf(out, "    movl $%d, %%ecx\n", imm);
                        fprintf(out, "    cltd\n");
                        fprintf(out, "    idivl %%ecx\n");
                        fprintf(out, "    movl %%edx, %%eax\n");
                    }
                } else if (node->op == '&') {
                    if (imm == 0) fprintf(out, "    xorl %%eax, %%eax\n");
                    else if (imm != -1) fprintf(out, "    andl $%d, %%eax\n", imm);
                } else if (node->op == '|') {
                    if (imm != 0) fprintf(out, "    orl $%d, %%eax\n", imm);
                } else if (node->op == '^') {
                    if (imm == -1) fprintf(out, "    notl %%eax\n");
                    else if (imm != 0) fprintf(out, "    xorl $%d, %%eax\n", imm);
                } else if (node->op == 'S') { // <<
                    if (imm > 0) fprintf(out, "    shll $%d, %%eax\n", imm);
                } else if (node->op == 'R') { // >>
                    if (imm > 0) fprintf(out, "    sarl $%d, %%eax\n", imm);
                }
                break;
            }

            // Operación directa con operando en memoria (variables sin stack push/pop)
            char mem_ref[64];
            if (get_identifier_memory_ref(right_op, mem_ref) &&
                (node->op == '+' || node->op == '-' || node->op == '*' ||
                 node->op == '/' || node->op == '%' ||
                 node->op == '&' || node->op == '|' || node->op == '^')) {
                generate_expression(left_op, out);
                if (node->op == '+') fprintf(out, "    addl %s, %%eax\n", mem_ref);
                else if (node->op == '-') fprintf(out, "    subl %s, %%eax\n", mem_ref);
                else if (node->op == '*') fprintf(out, "    imull %s, %%eax\n", mem_ref);
                else if (node->op == '/') {
                    fprintf(out, "    cltd\n");
                    fprintf(out, "    idivl %s\n", mem_ref);
                } else if (node->op == '%') {
                    fprintf(out, "    cltd\n");
                    fprintf(out, "    idivl %s\n", mem_ref);
                    fprintf(out, "    movl %%edx, %%eax\n");
                } else if (node->op == '&') fprintf(out, "    andl %s, %%eax\n", mem_ref);
                else if (node->op == '|') fprintf(out, "    orl %s, %%eax\n", mem_ref);
                else if (node->op == '^') fprintf(out, "    xorl %s, %%eax\n", mem_ref);
                break;
            }

            // Operación directa con constante inmediata para comparaciones
            char cmp_op = node->op;
            ASTNode* cmp_left = node->left;
            ASTNode* cmp_right = node->right;
            int is_cmp = (cmp_op == '<' || cmp_op == '>' || cmp_op == 'L' || cmp_op == 'G' || cmp_op == 'E' || cmp_op == 'N');
            if (is_cmp) {
                if (cmp_right->type == AST_NUMBER || cmp_right->type == AST_NULL) {
                    generate_expression(cmp_left, out);
                    int imm = (cmp_right->type == AST_NULL) ? 0 : cmp_right->int_value;
                    if (imm == 0 && (cmp_op == 'E' || cmp_op == 'N')) {
                        fprintf(out, "    testl %%eax, %%eax\n");
                    } else {
                        fprintf(out, "    cmpl $%d, %%eax\n", imm);
                    }
                    if (cmp_op == '<') fprintf(out, "    setl %%al\n");
                    else if (cmp_op == '>') fprintf(out, "    setg %%al\n");
                    else if (cmp_op == 'L') fprintf(out, "    setle %%al\n");
                    else if (cmp_op == 'G') fprintf(out, "    setge %%al\n");
                    else if (cmp_op == 'E') fprintf(out, "    sete %%al\n");
                    else if (cmp_op == 'N') fprintf(out, "    setne %%al\n");
                    fprintf(out, "    movzbl %%al, %%eax\n");
                    break;
                } else if (cmp_left->type == AST_NUMBER || cmp_left->type == AST_NULL) {
                    generate_expression(cmp_right, out);
                    int imm = (cmp_left->type == AST_NULL) ? 0 : cmp_left->int_value;
                    if (imm == 0 && (cmp_op == 'E' || cmp_op == 'N')) {
                        fprintf(out, "    testl %%eax, %%eax\n");
                    } else {
                        fprintf(out, "    cmpl $%d, %%eax\n", imm);
                    }
                    if (cmp_op == '<') fprintf(out, "    setg %%al\n");
                    else if (cmp_op == '>') fprintf(out, "    setl %%al\n");
                    else if (cmp_op == 'L') fprintf(out, "    setge %%al\n");
                    else if (cmp_op == 'G') fprintf(out, "    setle %%al\n");
                    else if (cmp_op == 'E') fprintf(out, "    sete %%al\n");
                    else if (cmp_op == 'N') fprintf(out, "    setne %%al\n");
                    fprintf(out, "    movzbl %%al, %%eax\n");
                    break;
                } else if (get_identifier_memory_ref(cmp_right, mem_ref)) {
                    generate_expression(cmp_left, out);
                    fprintf(out, "    cmpl %s, %%eax\n", mem_ref);
                    if (cmp_op == '<') fprintf(out, "    setl %%al\n");
                    else if (cmp_op == '>') fprintf(out, "    setg %%al\n");
                    else if (cmp_op == 'L') fprintf(out, "    setle %%al\n");
                    else if (cmp_op == 'G') fprintf(out, "    setge %%al\n");
                    else if (cmp_op == 'E') fprintf(out, "    sete %%al\n");
                    else if (cmp_op == 'N') fprintf(out, "    setne %%al\n");
                    fprintf(out, "    movzbl %%al, %%eax\n");
                    break;
                }
            }
            
            // Cortocircuito para operadores lógicos && y ||
            if (node->op == 'A') { // && (Short-circuit AND)
                static int sc_and_counter = 0;
                int id = sc_and_counter++;
                generate_expression(node->left, out);
                fprintf(out, "    testl %%eax, %%eax\n");
                fprintf(out, "    jz .Lsc_and_false_%d\n", id);
                generate_expression(node->right, out);
                fprintf(out, "    testl %%eax, %%eax\n");
                fprintf(out, "    jz .Lsc_and_false_%d\n", id);
                fprintf(out, "    movl $1, %%eax\n");
                fprintf(out, "    jmp .Lsc_and_end_%d\n", id);
                fprintf(out, ".Lsc_and_false_%d:\n", id);
                fprintf(out, "    xorl %%eax, %%eax\n");
                fprintf(out, ".Lsc_and_end_%d:\n", id);
                break;
            }
            if (node->op == 'O') { // || (Short-circuit OR)
                static int sc_or_counter = 0;
                int id = sc_or_counter++;
                generate_expression(node->left, out);
                fprintf(out, "    testl %%eax, %%eax\n");
                fprintf(out, "    jnz .Lsc_or_true_%d\n", id);
                generate_expression(node->right, out);
                fprintf(out, "    testl %%eax, %%eax\n");
                fprintf(out, "    jnz .Lsc_or_true_%d\n", id);
                fprintf(out, "    xorl %%eax, %%eax\n");
                fprintf(out, "    jmp .Lsc_or_end_%d\n", id);
                fprintf(out, ".Lsc_or_true_%d:\n", id);
                fprintf(out, "    movl $1, %%eax\n");
                fprintf(out, ".Lsc_or_end_%d:\n", id);
                break;
            }
            
            // Caso general: evaluar izquierdo en edx, derecho en eax
            generate_expression(node->left, out);
            fprintf(out, "    pushl %%eax\n");
            generate_expression(node->right, out);
            fprintf(out, "    movl (%%esp), %%edx\n");
            fprintf(out, "    addl $4, %%esp\n");
            
            if (node->op == '+') {
                fprintf(out, "    addl %%edx, %%eax\n");
            } else if (node->op == '-') {
                fprintf(out, "    subl %%eax, %%edx\n");
                fprintf(out, "    movl %%edx, %%eax\n");
            } else if (node->op == '*') {
                fprintf(out, "    imull %%edx, %%eax\n");
            } else if (node->op == '/') {
                fprintf(out, "    movl %%eax, %%ecx\n");
                fprintf(out, "    movl %%edx, %%eax\n");
                fprintf(out, "    cltd\n");
                fprintf(out, "    idivl %%ecx\n");
            } else if (node->op == '%') {
                fprintf(out, "    movl %%eax, %%ecx\n");
                fprintf(out, "    movl %%edx, %%eax\n");
                fprintf(out, "    cltd\n");
                fprintf(out, "    idivl %%ecx\n");
                fprintf(out, "    movl %%edx, %%eax\n");
            } else if (node->op == '&') {
                fprintf(out, "    andl %%edx, %%eax\n");
            } else if (node->op == '|') {
                fprintf(out, "    orl %%edx, %%eax\n");
            } else if (node->op == '^') {
                fprintf(out, "    xorl %%edx, %%eax\n");
            } else if (node->op == 'S') { // <<
                fprintf(out, "    movl %%eax, %%ecx\n");
                fprintf(out, "    shll %%cl, %%edx\n");
                fprintf(out, "    movl %%edx, %%eax\n");
            } else if (node->op == 'R') { // >>
                fprintf(out, "    movl %%eax, %%ecx\n");
                fprintf(out, "    sarl %%cl, %%edx\n");
                fprintf(out, "    movl %%edx, %%eax\n");
            } else if (node->op == '<') {
                fprintf(out, "    cmpl %%eax, %%edx\n");
                fprintf(out, "    setl %%al\n");
                fprintf(out, "    movzbl %%al, %%eax\n");
            } else if (node->op == '>') {
                fprintf(out, "    cmpl %%eax, %%edx\n");
                fprintf(out, "    setg %%al\n");
                fprintf(out, "    movzbl %%al, %%eax\n");
            } else if (node->op == 'L') { // <=
                fprintf(out, "    cmpl %%eax, %%edx\n");
                fprintf(out, "    setle %%al\n");
                fprintf(out, "    movzbl %%al, %%eax\n");
            } else if (node->op == 'G') { // >=
                fprintf(out, "    cmpl %%eax, %%edx\n");
                fprintf(out, "    setge %%al\n");
                fprintf(out, "    movzbl %%al, %%eax\n");
            } else if (node->op == 'E') { // ==
                fprintf(out, "    cmpl %%eax, %%edx\n");
                fprintf(out, "    sete %%al\n");
                fprintf(out, "    movzbl %%al, %%eax\n");
            } else if (node->op == 'N') { // !=
                fprintf(out, "    cmpl %%eax, %%edx\n");
                fprintf(out, "    setne %%al\n");
                fprintf(out, "    movzbl %%al, %%eax\n");
            }
            break;
        }

        case AST_TERNARY: {
            static int ternary_counter = 0;
            int id = ternary_counter++;
            generate_expression(node->expr, out);
            fprintf(out, "    testl %%eax, %%eax\n");
            fprintf(out, "    jz .Lternary_else_%d\n", id);
            generate_expression(node->left, out);
            fprintf(out, "    jmp .Lternary_end_%d\n", id);
            fprintf(out, ".Lternary_else_%d:\n", id);
            generate_expression(node->right, out);
            fprintf(out, ".Lternary_end_%d:\n", id);
            break;
        }
            
        case AST_CALL_EXPR:
            generate_call(node, out);
            break;
            
        case AST_ASSIGNMENT:
            generate_statement(node, out);
            break;
            
        default:
            break;
    }
}

static void generate_condition_jump(ASTNode* cond, int jump_if_true, const char* target_label, FILE* out) {
    if (cond == NULL) return;
    
    // Caso 1: Negación lógica !expr
    if (cond->type == AST_UNARY_EXPR && cond->op == '!') {
        generate_condition_jump(cond->left, !jump_if_true, target_label, out);
        return;
    }
    
    // Caso 2: Operador de comparación binario (<, >, <=, >=, ==, !=)
    if (cond->type == AST_BINARY_EXPR) {
        char cmp_op = cond->op;
        int is_cmp = (cmp_op == '<' || cmp_op == '>' || cmp_op == 'L' || cmp_op == 'G' || cmp_op == 'E' || cmp_op == 'N');
        
        if (is_cmp) {
            char mem_ref[64];
            
            // Subcaso A: Comparación con constante inmediata a la derecha (expr OP imm)
            if (cond->right->type == AST_NUMBER || cond->right->type == AST_NULL) {
                int imm = (cond->right->type == AST_NULL) ? 0 : cond->right->int_value;
                
                // Optimización crítica: expr % (potencia_de_2) == 0 ó != 0
                // En complemento a dos, X % 2^k == 0 <===> (X & (2^k - 1)) == 0 para todo X positivo o negativo.
                // Reemplaza un idivl de 30-45 ciclos por un testb/testl de 1 ciclo.
                if (imm == 0 && (cmp_op == 'E' || cmp_op == 'N') &&
                    cond->left->type == AST_BINARY_EXPR && cond->left->op == '%' &&
                    cond->left->right->type == AST_NUMBER &&
                    cond->left->right->int_value > 0 &&
                    (cond->left->right->int_value & (cond->left->right->int_value - 1)) == 0) {
                    int mod_val = cond->left->right->int_value;
                    generate_expression(cond->left->left, out);
                    if (mod_val <= 128) {
                        fprintf(out, "    testb $%d, %%al\n", mod_val - 1);
                    } else {
                        fprintf(out, "    testl $%d, %%eax\n", mod_val - 1);
                    }
                    if (cmp_op == 'E') { // == 0
                        if (jump_if_true) fprintf(out, "    jz %s\n", target_label);
                        else fprintf(out, "    jnz %s\n", target_label);
                    } else { // != 0
                        if (jump_if_true) fprintf(out, "    jnz %s\n", target_label);
                        else fprintf(out, "    jz %s\n", target_label);
                    }
                    return;
                }
                
                generate_expression(cond->left, out);
                if (imm == 0 && (cmp_op == 'E' || cmp_op == 'N')) {
                    fprintf(out, "    testl %%eax, %%eax\n");
                } else {
                    fprintf(out, "    cmpl $%d, %%eax\n", imm);
                }
            }
            // Subcaso B: Comparación con constante inmediata a la izquierda (imm OP expr)
            else if (cond->left->type == AST_NUMBER || cond->left->type == AST_NULL) {
                generate_expression(cond->right, out);
                int imm = (cond->left->type == AST_NULL) ? 0 : cond->left->int_value;
                if (imm == 0 && (cmp_op == 'E' || cmp_op == 'N')) {
                    fprintf(out, "    testl %%eax, %%eax\n");
                } else {
                    fprintf(out, "    cmpl $%d, %%eax\n", imm);
                }
                if (cmp_op == '<') cmp_op = '>';
                else if (cmp_op == '>') cmp_op = '<';
                else if (cmp_op == 'L') cmp_op = 'G';
                else if (cmp_op == 'G') cmp_op = 'L';
            }
            // Subcaso C: Comparación con operando derecho en memoria (expr OP var)
            else if (get_identifier_memory_ref(cond->right, mem_ref)) {
                generate_expression(cond->left, out);
                fprintf(out, "    cmpl %s, %%eax\n", mem_ref);
            }
            // Subcaso D: Caso general (evaluar ambos con stack)
            else {
                generate_expression(cond->left, out);
                fprintf(out, "    pushl %%eax\n");
                generate_expression(cond->right, out);
                fprintf(out, "    movl (%%esp), %%edx\n");
                fprintf(out, "    addl $4, %%esp\n");
                fprintf(out, "    cmpl %%eax, %%edx\n");
            }
            
            // Emitir el salto condicional directo sobre los flags de CPU
            if (jump_if_true) {
                if (cmp_op == '<') fprintf(out, "    jl %s\n", target_label);
                else if (cmp_op == '>') fprintf(out, "    jg %s\n", target_label);
                else if (cmp_op == 'L') fprintf(out, "    jle %s\n", target_label);
                else if (cmp_op == 'G') fprintf(out, "    jge %s\n", target_label);
                else if (cmp_op == 'E') fprintf(out, "    je %s\n", target_label);
                else if (cmp_op == 'N') fprintf(out, "    jne %s\n", target_label);
            } else {
                if (cmp_op == '<') fprintf(out, "    jge %s\n", target_label);
                else if (cmp_op == '>') fprintf(out, "    jle %s\n", target_label);
                else if (cmp_op == 'L') fprintf(out, "    jg %s\n", target_label);
                else if (cmp_op == 'G') fprintf(out, "    jl %s\n", target_label);
                else if (cmp_op == 'E') fprintf(out, "    jne %s\n", target_label);
                else if (cmp_op == 'N') fprintf(out, "    je %s\n", target_label);
            }
            return;
        }
        
        // Operador lógico && ('A') con cortocircuito
        if (cond->op == 'A') {
            static int and_counter = 0;
            int aid = and_counter++;
            char skip_lbl[64];
            sprintf(skip_lbl, ".Land_skip_%d", aid);
            
            if (!jump_if_true) {
                generate_condition_jump(cond->left, 0, target_label, out);
                generate_condition_jump(cond->right, 0, target_label, out);
            } else {
                generate_condition_jump(cond->left, 0, skip_lbl, out);
                generate_condition_jump(cond->right, 1, target_label, out);
                fprintf(out, "%s:\n", skip_lbl);
            }
            return;
        }
        
        // Operador lógico || ('O') con cortocircuito
        if (cond->op == 'O') {
            static int or_counter = 0;
            int oid = or_counter++;
            char skip_lbl[64];
            sprintf(skip_lbl, ".Lor_skip_%d", oid);
            
            if (jump_if_true) {
                generate_condition_jump(cond->left, 1, target_label, out);
                generate_condition_jump(cond->right, 1, target_label, out);
            } else {
                generate_condition_jump(cond->left, 1, skip_lbl, out);
                generate_condition_jump(cond->right, 0, target_label, out);
                fprintf(out, "%s:\n", skip_lbl);
            }
            return;
        }
    }
    
    // Caso general booleano
    generate_expression(cond, out);
    fprintf(out, "    testl %%eax, %%eax\n");
    if (jump_if_true) {
        fprintf(out, "    jnz %s\n", target_label);
    } else {
        fprintf(out, "    jz %s\n", target_label);
    }
}

static void generate_statement(ASTNode* node, FILE* out) {
    if (node == NULL) return;
    
    switch (node->type) {
        case AST_BLOCK: {
            ASTNode* stmt = node->statements;
            while (stmt != NULL) {
                generate_statement(stmt, out);
                if (generator_opt_level >= 1 && (stmt->type == AST_RETURN_STATEMENT || stmt->type == AST_BREAK || stmt->type == AST_CONTINUE)) {
                    break;
                }
                stmt = stmt->next;
            }
            break;
        }

        case AST_IF: {
            int const_cond = 0;
            if (generator_opt_level >= 1 && eval_const_expr(node->expr, &const_cond)) {
                if (const_cond != 0) {
                    generate_statement(node->body, out);
                } else if (node->else_branch != NULL) {
                    generate_statement(node->else_branch, out);
                }
                break;
            }

            static int if_counter = 0;
            int id = if_counter++;
            char target_lbl[64];
            
            if (node->else_branch != NULL) {
                sprintf(target_lbl, ".Lelse_%d", id);
                generate_condition_jump(node->expr, 0, target_lbl, out);
                generate_statement(node->body, out);
                fprintf(out, "    jmp .Lif_end_%d\n", id);
                fprintf(out, ".Lelse_%d:\n", id);
                generate_statement(node->else_branch, out);
                fprintf(out, ".Lif_end_%d:\n", id);
            } else {
                sprintf(target_lbl, ".Lif_end_%d", id);
                generate_condition_jump(node->expr, 0, target_lbl, out);
                generate_statement(node->body, out);
                fprintf(out, ".Lif_end_%d:\n", id);
            }
            break;
        }

        case AST_WHILE: {
            int const_cond = 0;
            if (generator_opt_level >= 1 && eval_const_expr(node->expr, &const_cond) && const_cond == 0) {
                // while (0) es completamente código muerto: no generar nada
                break;
            }

            int id = global_loop_counter++;
            
            push_loop(id, id);
            
            // Loop Inversion (Bottom-Tested Loop):
            fprintf(out, "    jmp .Lwhile_test_%d\n", id);
            fprintf(out, ".Lwhile_body_%d:\n", id);
            
            generate_statement(node->body, out);
            
            fprintf(out, ".Lloop_continue_%d:\n", id);
            fprintf(out, ".Lwhile_test_%d:\n", id);
            
            char body_lbl[64];
            sprintf(body_lbl, ".Lwhile_body_%d", id);
            generate_condition_jump(node->expr, 1, body_lbl, out);
            
            fprintf(out, ".Lloop_break_%d:\n", id);
            fprintf(out, ".Lwhile_end_%d:\n", id);
            
            pop_loop();
            break;
        }

        case AST_FOR: {
            int id = global_loop_counter++;
            
            if (node->init_value != NULL) {
                generate_statement(node->init_value, out);
            }
            
            push_loop(id, id);
            
            // Loop Inversion (Bottom-Tested Loop):
            fprintf(out, "    jmp .Lfor_test_%d\n", id);
            fprintf(out, ".Lfor_body_%d:\n", id);
            
            generate_statement(node->body, out);
            
            fprintf(out, ".Lloop_continue_%d:\n", id);
            if (node->update_expr != NULL) {
                generate_expression(node->update_expr, out);
            }
            
            fprintf(out, ".Lfor_test_%d:\n", id);
            if (node->expr != NULL) {
                char body_lbl[64];
                sprintf(body_lbl, ".Lfor_body_%d", id);
                generate_condition_jump(node->expr, 1, body_lbl, out);
            } else {
                fprintf(out, "    jmp .Lfor_body_%d\n", id);
            }
            
            fprintf(out, ".Lloop_break_%d:\n", id);
            fprintf(out, ".Lfor_end_%d:\n", id);
            
            pop_loop();
            break;
        }

        case AST_DO_WHILE: {
            int id = global_loop_counter++;
            
            push_loop(id, id);
            
            fprintf(out, ".Ldo_start_%d:\n", id);
            generate_statement(node->body, out);
            
            fprintf(out, ".Lloop_continue_%d:\n", id);
            char start_lbl[64];
            sprintf(start_lbl, ".Ldo_start_%d", id);
            generate_condition_jump(node->expr, 1, start_lbl, out);
            
            fprintf(out, ".Lloop_break_%d:\n", id);
            
            pop_loop();
            break;
        }

        case AST_BREAK:
            if (loop_depth > 0) {
                fprintf(out, "    jmp .Lloop_break_%d\n", loop_stack[loop_depth - 1].break_label);
            }
            break;

        case AST_CONTINUE:
            if (loop_depth > 0) {
                for (int i = loop_depth - 1; i >= 0; i--) {
                    if (loop_stack[i].continue_label >= 0) {
                        fprintf(out, "    jmp .Lloop_continue_%d\n", loop_stack[i].continue_label);
                        break;
                    }
                }
            }
            break;

        case AST_SWITCH: {
            int id = global_loop_counter++;
            
            generate_expression(node->expr, out);
            fprintf(out, "    movl %%eax, %%ecx\n");
            
            // Loop de comparación para cada case
            ASTNode* c = node->body;
            int case_idx = 0;
            int has_default = 0;
            while (c != NULL) {
                if (c->type == AST_CASE) {
                    fprintf(out, "    cmpl $%d, %%ecx\n", c->int_value);
                    fprintf(out, "    je .Lswitch_%d_case_%d\n", id, case_idx);
                } else if (c->type == AST_DEFAULT) {
                    has_default = 1;
                }
                case_idx++;
                c = c->next;
            }
            
            if (has_default) {
                fprintf(out, "    jmp .Lswitch_%d_default\n", id);
            } else {
                fprintf(out, "    jmp .Lloop_break_%d\n", id);
            }
            
            // Switch puede recibir break (continue_label = -1 para ignorar switch en continue)
            push_loop(id, -1);
            
            // Generar el código de cada rama
            c = node->body;
            case_idx = 0;
            while (c != NULL) {
                if (c->type == AST_CASE) {
                    fprintf(out, ".Lswitch_%d_case_%d:\n", id, case_idx);
                    generate_statement(c->body, out);
                } else if (c->type == AST_DEFAULT) {
                    fprintf(out, ".Lswitch_%d_default:\n", id);
                    generate_statement(c->body, out);
                }
                case_idx++;
                c = c->next;
            }
            
            fprintf(out, ".Lloop_break_%d:\n", id);
            pop_loop();
            break;
        }

        case AST_VAR_DECLARATION: {
            if (node->is_static) {
                break; // Las variables estáticas locales ya están ubicadas e inicializadas en .data
            }
            Symbol* sym = find_symbol(node->name);
            if (sym != NULL) {
                StructDef* sd = find_struct_def(sym->type_name);
                if (sd != NULL && sd->is_class) {
                    if (node->init_value != NULL && node->init_value->type == AST_CALL_EXPR) {
                        ASTNode* call = node->init_value;
                        ASTNode* cargs[64];
                        int carg_count = 0;
                        ASTNode* ca = call->args;
                        while (ca != NULL && carg_count < 64) {
                            cargs[carg_count++] = ca;
                            ca = ca->next;
                        }
                        for (int i = carg_count - 1; i >= 0; i--) {
                            push_arg_efficiently(cargs[i], out);
                        }
                        fprintf(out, "    leal %d(%%ebp), %%eax\n", sym->stack_offset);
                        fprintf(out, "    pushl %%eax\n");

                        DeclaredFunc* d_ctor = find_exact_declared_function(sd->name, sd->name);
                        if (!d_ctor) d_ctor = find_exact_declared_function("constructor", sd->name);
                        if (d_ctor != NULL) {
                            char ctor_label[512];
                            sprintf(ctor_label, "_%s_%s", sd->name, d_ctor->name);
                            fprintf(out, "    call %s\n", ctor_label);
                            fprintf(out, "    addl $%d, %%esp\n", (carg_count + 1) * 4);
                        } else {
                            fprintf(out, "    addl $4, %%esp\n");
                        }
                        break;
                    } else if (node->init_value == NULL) {
                        DeclaredFunc* d_ctor = find_exact_declared_function(sd->name, sd->name);
                        if (!d_ctor) d_ctor = find_exact_declared_function("constructor", sd->name);
                        if (d_ctor != NULL && d_ctor->node && (d_ctor->node->params == NULL || d_ctor->node->params->next == NULL)) {
                            fprintf(out, "    leal %d(%%ebp), %%eax\n", sym->stack_offset);
                            fprintf(out, "    pushl %%eax\n");
                            char ctor_label[512];
                            sprintf(ctor_label, "_%s_%s", sd->name, d_ctor->name);
                            fprintf(out, "    call %s\n", ctor_label);
                            fprintf(out, "    addl $4, %%esp\n");
                            break;
                        }
                    }
                }
                if (node->init_value != NULL) {
                    if (node->init_value->type == AST_NUMBER) {
                        fprintf(out, "    movl $%d, %d(%%ebp)\n", node->init_value->int_value, sym->stack_offset);
                    } else if (node->init_value->type == AST_NULL) {
                        fprintf(out, "    movl $0, %d(%%ebp)\n", sym->stack_offset);
                    } else if (node->init_value->type == AST_IDENTIFIER) {
                        int eval = 0;
                        if (find_constant(node->init_value->name, &eval)) {
                            fprintf(out, "    movl $%d, %d(%%ebp)\n", eval, sym->stack_offset);
                        } else {
                            generate_expression(node->init_value, out);
                            fprintf(out, "    movl %%eax, %d(%%ebp)\n", sym->stack_offset);
                        }
                    } else {
                        generate_expression(node->init_value, out);
                        fprintf(out, "    movl %%eax, %d(%%ebp)\n", sym->stack_offset);
                    }
                } else if (sym->is_string) {
                    // Inicializar buffer a cadena vacía
                    fprintf(out, "    movb $0, %d(%%ebp)\n", sym->stack_offset);
                } else if (!sym->is_array) {
                    fprintf(out, "    movl $0, %d(%%ebp)\n", sym->stack_offset);
                }
            }
            break;
        }
        
        case AST_ASSIGNMENT: {
            if (node->left->type == AST_IDENTIFIER) {
                Symbol* sym = find_symbol(node->left->name);
                if (sym != NULL) {
                    if (sym->is_const) {
                        printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", sym->name, node->line);
                        exit(1);
                    }
                    char dest[64];
                    if (sym->is_static && sym->asm_name != NULL) {
                        strcpy(dest, sym->asm_name);
                    } else {
                        sprintf(dest, "%d(%%ebp)", sym->stack_offset);
                    }
                    // Optimización de auto-asignación directa: dest = dest OP imm
                    if (node->init_value->type == AST_BINARY_EXPR &&
                        node->init_value->left->type == AST_IDENTIFIER &&
                        strcmp(node->init_value->left->name, node->left->name) == 0 &&
                        node->init_value->right->type == AST_NUMBER) {
                        int imm = node->init_value->right->int_value;
                        char op = node->init_value->op;
                        if (op == '+') {
                            if (imm == 1) fprintf(out, "    incl %s\n", dest);
                            else if (imm == -1) fprintf(out, "    decl %s\n", dest);
                            else if (imm != 0) fprintf(out, "    addl $%d, %s\n", imm, dest);
                            break;
                        } else if (op == '-') {
                            if (imm == 1) fprintf(out, "    decl %s\n", dest);
                            else if (imm == -1) fprintf(out, "    incl %s\n", dest);
                            else if (imm != 0) fprintf(out, "    subl $%d, %s\n", imm, dest);
                            break;
                        } else if (op == '&') {
                            if (imm == 0) fprintf(out, "    movl $0, %s\n", dest);
                            else if (imm != -1) fprintf(out, "    andl $%d, %s\n", imm, dest);
                            break;
                        } else if (op == '|') {
                            if (imm != 0) fprintf(out, "    orl $%d, %s\n", imm, dest);
                            break;
                        } else if (op == '^') {
                            if (imm == -1) fprintf(out, "    notl %s\n", dest);
                            else if (imm != 0) fprintf(out, "    xorl $%d, %s\n", imm, dest);
                            break;
                        } else if (op == 'S' && imm > 0) {
                            fprintf(out, "    shll $%d, %s\n", imm, dest);
                            break;
                        } else if (op == 'R' && imm > 0) {
                            fprintf(out, "    sarl $%d, %s\n", imm, dest);
                            break;
                        }
                    }
                    if (node->init_value->type == AST_NUMBER) {
                        fprintf(out, "    movl $%d, %s\n", node->init_value->int_value, dest);
                    } else if (node->init_value->type == AST_NULL) {
                        fprintf(out, "    movl $0, %s\n", dest);
                    } else if (node->init_value->type == AST_IDENTIFIER) {
                        int eval = 0;
                        if (find_constant(node->init_value->name, &eval)) {
                            fprintf(out, "    movl $%d, %s\n", eval, dest);
                        } else {
                            generate_expression(node->init_value, out);
                            fprintf(out, "    movl %%eax, %s\n", dest);
                        }
                    } else {
                        generate_expression(node->init_value, out);
                        fprintf(out, "    movl %%eax, %s\n", dest);
                    }
                } else {
                    GlobalVar* gv = find_global_var(node->left->name);
                    if (gv != NULL) {
                        if (gv->is_const) {
                            printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", gv->name, node->line);
                            exit(1);
                        }
                        if (node->init_value->type == AST_BINARY_EXPR &&
                            node->init_value->left->type == AST_IDENTIFIER &&
                            strcmp(node->init_value->left->name, node->left->name) == 0 &&
                            node->init_value->right->type == AST_NUMBER) {
                            int imm = node->init_value->right->int_value;
                            char op = node->init_value->op;
                            if (op == '+') {
                                if (imm == 1) fprintf(out, "    incl %s\n", gv->asm_name);
                                else if (imm == -1) fprintf(out, "    decl %s\n", gv->asm_name);
                                else if (imm != 0) fprintf(out, "    addl $%d, %s\n", imm, gv->asm_name);
                                break;
                            } else if (op == '-') {
                                if (imm == 1) fprintf(out, "    decl %s\n", gv->asm_name);
                                else if (imm == -1) fprintf(out, "    incl %s\n", gv->asm_name);
                                else if (imm != 0) fprintf(out, "    subl $%d, %s\n", imm, gv->asm_name);
                                break;
                            }
                        }
                        if (node->init_value->type == AST_NUMBER) {
                            fprintf(out, "    movl $%d, %s\n", node->init_value->int_value, gv->asm_name);
                        } else if (node->init_value->type == AST_NULL) {
                            fprintf(out, "    movl $0, %s\n", gv->asm_name);
                        } else {
                            generate_expression(node->init_value, out);
                            fprintf(out, "    movl %s, %s\n", "%eax", gv->asm_name);
                        }
                    } else if (current_func_namespace != NULL) {
                        StructDef* cdef = find_struct_def(current_func_namespace);
                        if (cdef != NULL) {
                            StructFieldInfo* f_info = find_struct_field(cdef, node->left->name);
                            if (f_info != NULL) {
                                int f_offset = f_info->offset;
                                if (f_info->is_const) {
                                    printf("Error semántico: no se puede modificar el miembro constante '%s' en la línea %d\n", node->left->name, node->line);
                                    exit(1);
                                }
                                if (f_info->is_string) {
                                    generate_expression(node->init_value, out);
                                    static int str_copy_counter = 0;
                                    int cid = str_copy_counter++;
                                    fprintf(out, "    movl %%eax, %%edx\n");
                                    fprintf(out, "    movl 8(%%ebp), %%ecx\n");
                                    if (f_offset > 0) fprintf(out, "    addl $%d, %%ecx\n", f_offset);
                                    fprintf(out, ".Lcopy_str_impl_%d:\n", cid);
                                    fprintf(out, "    movb (%%edx), %%al\n");
                                    fprintf(out, "    movb %%al, (%%ecx)\n");
                                    fprintf(out, "    incl %%edx\n");
                                    fprintf(out, "    incl %%ecx\n");
                                    fprintf(out, "    testb %%al, %%al\n");
                                    fprintf(out, "    jnz .Lcopy_str_impl_%d\n", cid);
                                } else {
                                    generate_expression(node->init_value, out);
                                    fprintf(out, "    movl 8(%%ebp), %%edx\n");
                                    fprintf(out, "    movl %%eax, %d(%%edx)\n", f_offset);
                                }
                            }
                        }
                    }
                }
            } else if (node->left->type == AST_SCOPED_IDENTIFIER) {
                GlobalVar* gv = find_global_var(node->left->name);
                if (gv != NULL) {
                    if (gv->is_const) {
                        printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", gv->name, node->line);
                        exit(1);
                    }
                    if (node->init_value->type == AST_NUMBER) {
                        fprintf(out, "    movl $%d, %s\n", node->init_value->int_value, gv->asm_name);
                    } else if (node->init_value->type == AST_NULL) {
                        fprintf(out, "    movl $0, %s\n", gv->asm_name);
                    } else {
                        generate_expression(node->init_value, out);
                        fprintf(out, "    movl %%eax, %s\n", gv->asm_name);
                    }
                }
            } else if (node->left->type == AST_INDEX_EXPR) {
                ASTNode* arr_node = node->left->left;
                ASTNode* idx_node = node->left->expr;
                char arr_mem[64];
                char idx_mem[64];
                int esz = get_expression_element_size(arr_node);
                int scale = (esz == 1) ? 1 : 4;
                
                // Si el array base está en memoria (variable local o global)
                if (get_identifier_memory_ref(arr_node, arr_mem)) {
                    if (idx_node->type == AST_NUMBER) {
                        int idx = idx_node->int_value;
                        generate_expression(node->init_value, out);
                        fprintf(out, "    movl %s, %%edx\n", arr_mem);
                        if (esz == 1) {
                            fprintf(out, "    movb %%al, %d(%%edx)\n", idx * scale);
                        } else {
                            fprintf(out, "    movl %%eax, %d(%%edx)\n", idx * scale);
                        }
                        break;
                    } else if (get_identifier_memory_ref(idx_node, idx_mem)) {
                        generate_expression(node->init_value, out);
                        fprintf(out, "    movl %s, %%edx\n", arr_mem);
                        fprintf(out, "    movl %s, %%ecx\n", idx_mem);
                        if (esz == 1) {
                            fprintf(out, "    movb %%al, (%%edx, %%ecx, 1)\n");
                        } else {
                            fprintf(out, "    movl %%eax, (%%edx, %%ecx, 4)\n");
                        }
                        break;
                    }
                }
                
                // Caso general
                generate_expression(node->left->expr, out);
                fprintf(out, "    pushl %%eax\n");
                generate_expression(node->left->left, out);
                fprintf(out, "    pushl %%eax\n");
                generate_expression(node->init_value, out);
                fprintf(out, "    movl (%%esp), %%edx\n");
                fprintf(out, "    movl 4(%%esp), %%ecx\n");
                fprintf(out, "    addl $8, %%esp\n");
                if (esz == 1) {
                    fprintf(out, "    movb %%al, (%%edx, %%ecx, 1)\n");
                } else {
                    fprintf(out, "    movl %%eax, (%%edx, %%ecx, 4)\n");
                }
            } else if (node->left->type == AST_MEMBER_EXPR) {
                MemberResolution res;
                if (resolve_member_access(node->left, &res, out, 1)) {
                    if (res.is_const) {
                        printf("Error semántico: no se puede modificar el miembro constante '%s' en la línea %d\n", node->left->name, node->line);
                        exit(1);
                    }
                    if (res.is_pointer) {
                        if (res.is_string) {
                            fprintf(out, "    pushl %%eax\n"); // guardar base del puntero
                            generate_expression(node->init_value, out);
                            static int str_copy_counter = 0;
                            int cid = str_copy_counter++;
                            fprintf(out, "    movl %%eax, %%edx\n");
                            fprintf(out, "    movl (%%esp), %%ecx\n");
                            fprintf(out, "    addl $4, %%esp\n");
                            if (res.ptr_offset > 0) {
                                fprintf(out, "    addl $%d, %%ecx\n", res.ptr_offset);
                            }
                            fprintf(out, ".Lcopy_str_ptr_%d:\n", cid);
                            fprintf(out, "    movb (%%edx), %%al\n");
                            fprintf(out, "    movb %%al, (%%ecx)\n");
                            fprintf(out, "    incl %%edx\n");
                            fprintf(out, "    incl %%ecx\n");
                            fprintf(out, "    testb %%al, %%al\n");
                            fprintf(out, "    jnz .Lcopy_str_ptr_%d\n", cid);
                        } else if (node->init_value->type == AST_NUMBER) {
                            fprintf(out, "    movl $%d, %d(%%eax)\n", node->init_value->int_value, res.ptr_offset);
                        } else if (node->init_value->type == AST_NULL) {
                            fprintf(out, "    movl $0, %d(%%eax)\n", res.ptr_offset);
                        } else {
                            fprintf(out, "    pushl %%eax\n"); // guardar base del puntero
                            generate_expression(node->init_value, out);
                            fprintf(out, "    movl (%%esp), %%edx\n");
                            fprintf(out, "    addl $4, %%esp\n");
                            fprintf(out, "    movl %%eax, %d(%%edx)\n", res.ptr_offset);
                        }
                    } else {
                        if (res.is_string) {
                            generate_expression(node->init_value, out);
                            static int str_copy_counter = 0;
                            int cid = str_copy_counter++;
                            fprintf(out, "    movl %%eax, %%edx\n");
                            fprintf(out, "    leal %d(%%ebp), %%ecx\n", res.stack_offset);
                            fprintf(out, ".Lcopy_str_%d:\n", cid);
                            fprintf(out, "    movb (%%edx), %%al\n");
                            fprintf(out, "    movb %%al, (%%ecx)\n");
                            fprintf(out, "    incl %%edx\n");
                            fprintf(out, "    incl %%ecx\n");
                            fprintf(out, "    testb %%al, %%al\n");
                            fprintf(out, "    jnz .Lcopy_str_%d\n", cid);
                        } else {
                            if (node->init_value->type == AST_NUMBER) {
                                fprintf(out, "    movl $%d, %d(%%ebp)\n", node->init_value->int_value, res.stack_offset);
                            } else if (node->init_value->type == AST_NULL) {
                                fprintf(out, "    movl $0, %d(%%ebp)\n", res.stack_offset);
                            } else if (node->init_value->type == AST_IDENTIFIER) {
                                int eval = 0;
                                if (find_constant(node->init_value->name, &eval)) {
                                    fprintf(out, "    movl $%d, %d(%%ebp)\n", eval, res.stack_offset);
                                } else {
                                    generate_expression(node->init_value, out);
                                    fprintf(out, "    movl %%eax, %d(%%ebp)\n", res.stack_offset);
                                }
                            } else {
                                generate_expression(node->init_value, out);
                                fprintf(out, "    movl %%eax, %d(%%ebp)\n", res.stack_offset);
                            }
                        }
                    }
                }
            } else if (node->left->type == AST_UNARY_EXPR && node->left->op == '*') {
                if (node->left->left->type == AST_IDENTIFIER) {
                    Symbol* psym = find_symbol(node->left->left->name);
                    if (psym != NULL && psym->type_name != NULL && strstr(psym->type_name, "const") != NULL) {
                        printf("Error semántico: no se puede modificar el valor apuntado por un puntero a constante '%s' en la línea %d\n", psym->name, node->line);
                        exit(1);
                    }
                }
                if (node->init_value->type == AST_NUMBER) {
                    generate_expression(node->left->left, out);
                    fprintf(out, "    movl $%d, (%%eax)\n", node->init_value->int_value);
                } else if (node->init_value->type == AST_NULL) {
                    generate_expression(node->left->left, out);
                    fprintf(out, "    movl $0, (%%eax)\n");
                } else {
                    generate_expression(node->left->left, out);
                    fprintf(out, "    pushl %%eax\n");
                    generate_expression(node->init_value, out);
                    fprintf(out, "    movl (%%esp), %%edx\n");
                    fprintf(out, "    addl $4, %%esp\n");
                    fprintf(out, "    movl %%eax, (%%edx)\n");
                }
            }
            break;
        }

        case AST_INC_DEC: {
            if (node->left->type == AST_IDENTIFIER) {
                Symbol* sym = find_symbol(node->left->name);
                if (sym != NULL) {
                    if (sym->is_const) {
                        printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", sym->name, node->line);
                        exit(1);
                    }
                    int is_inc = (strstr(node->op_str, "++") != NULL);
                    if (sym->is_static && sym->asm_name != NULL) {
                        if (is_inc) fprintf(out, "    incl %s\n", sym->asm_name);
                        else fprintf(out, "    decl %s\n", sym->asm_name);
                    } else {
                        if (is_inc) fprintf(out, "    incl %d(%%ebp)\n", sym->stack_offset);
                        else fprintf(out, "    decl %d(%%ebp)\n", sym->stack_offset);
                    }
                    break;
                }
                GlobalVar* gv = find_global_var(node->left->name);
                if (gv != NULL) {
                    if (gv->is_const) {
                        printf("Error semántico: no se puede modificar la variable constante '%s' en la línea %d\n", gv->name, node->line);
                        exit(1);
                    }
                    int is_inc = (strstr(node->op_str, "++") != NULL);
                    if (is_inc) fprintf(out, "    incl %s\n", gv->asm_name);
                    else fprintf(out, "    decl %s\n", gv->asm_name);
                    break;
                }
            }
            generate_expression(node, out);
            break;
        }

        case AST_COMPOUND_ASSIGNMENT:
            generate_expression(node, out);
            break;
        
        case AST_RETURN_STATEMENT:
            if (node->return_value != NULL) {
                generate_expression(node->return_value, out);
            } else {
                fprintf(out, "    xorl %%eax, %%eax\n");
            }
            if (current_inline_exit_label > 0) {
                fprintf(out, "    jmp .Linline_exit_%d\n", current_inline_exit_label);
            } else {
                fprintf(out, "    movl %%ebp, %%esp\n");
                fprintf(out, "    popl %%ebp\n");
                fprintf(out, "    ret\n");
            }
            break;
            
        case AST_EXPR_STATEMENT:
            if (node->expr != NULL) {
                if (node->expr->type == AST_ASSIGNMENT) {
                    generate_statement(node->expr, out);
                } else {
                    generate_expression(node->expr, out);
                }
            }
            break;

        case AST_INLINE_ASM:
            emit_inline_asm(node->string_value, out);
            break;

        case AST_INLINE_C:
            if (node->string_value != NULL) {
                int c_id = ++inline_c_count;
                char c_fname[64];
                sprintf(c_fname, "c_snippet_%d", c_id);
                char func_def[2048];
                snprintf(func_def, sizeof(func_def), "\nvoid %s(void) {\n    %s\n}\n", c_fname, node->string_value);
                append_inline_c_raw(func_def);
                fprintf(out, "    call _%s\n", c_fname);
            }
            break;
            
        default:
            break;
    }
}

static void scan_local_variables(ASTNode* node, int* current_offset) {
    while (node != NULL) {
        if (node->type == AST_VAR_DECLARATION) {
            if (node->is_static) {
                char asm_lbl[128];
                sprintf(asm_lbl, "_static_%s_%d", node->name, ++static_local_counter);
                
                int is_str = (node->type_name && strstr(node->type_name, "string") != NULL && strchr(node->type_name, '*') == NULL) ? 1 : 0;
                int is_arr = node->is_array;
                int arr_sz = node->array_size;
                
                int int_v = 0;
                char* str_v = NULL;
                if (node->init_value != NULL) {
                    int val = 0;
                    if (eval_const_expr(node->init_value, &val)) {
                        int_v = val;
                    } else if (node->init_value->type == AST_NUMBER) {
                        int_v = node->init_value->int_value;
                    } else if (node->init_value->type == AST_NULL) {
                        int_v = 0;
                    }
                    if (node->init_value->type == AST_STRING && node->init_value->string_value != NULL) {
                        str_v = node->init_value->string_value;
                    }
                }
                
                StaticLocalVar* slv = malloc(sizeof(StaticLocalVar));
                slv->asm_name = strdup(asm_lbl);
                slv->int_val = int_v;
                slv->string_val = str_v ? strdup(str_v) : NULL;
                slv->is_string = is_str;
                slv->is_array = is_arr;
                slv->array_size = arr_sz;
                slv->next = declared_static_locals;
                declared_static_locals = slv;
                
                add_static_symbol(node->name, asm_lbl, node->type_name, is_str, is_arr, arr_sz, node->is_const);
            } else {
                StructDef* sd = find_struct_def(node->type_name);
                int var_size = 4;
                int is_str = 0;
                int is_arr = node->is_array;
                int arr_sz = node->array_size;
                if (is_arr && arr_sz > 0) {
                    var_size = arr_sz * 4;
                } else if (sd != NULL) {
                    var_size = sd->total_size;
                } else if (node->type_name && strstr(node->type_name, "string") != NULL && strchr(node->type_name, '*') == NULL) {
                    var_size = 256;
                    is_str = 1;
                }
                *current_offset += var_size;
                int offset = -(*current_offset);
                add_symbol(node->name, offset, 0, node->type_name, is_str, is_arr, arr_sz, node->is_const);
            }
        } else if (node->type == AST_BLOCK) {
            scan_local_variables(node->statements, current_offset);
        } else if (node->type == AST_IF) {
            scan_local_variables(node->body, current_offset);
            if (node->else_branch) scan_local_variables(node->else_branch, current_offset);
        } else if (node->type == AST_WHILE) {
            scan_local_variables(node->body, current_offset);
        } else if (node->type == AST_FOR) {
            if (node->init_value) scan_local_variables(node->init_value, current_offset);
            scan_local_variables(node->body, current_offset);
        } else if (node->type == AST_DO_WHILE) {
            scan_local_variables(node->body, current_offset);
        } else if (node->type == AST_SWITCH) {
            ASTNode* c = node->body;
            while (c != NULL) {
                scan_local_variables(c->body, current_offset);
                c = c->next;
            }
        }
        node = node->next;
    }
}

static void generate_function(ASTNode* node, FILE* out) {
    if (node->body == NULL) return; // Prototipo o función externa en runtime
    current_func_namespace = node->namespace_prefix;
    current_func_name = node->name;
    free_symbols();
    
    // Determinar nombre ensamblador (con namespace mangling)
    char mangled_name[512];
    if (node->namespace_prefix != NULL && strlen(node->namespace_prefix) > 0) {
        char clean_ns[256];
        int c_idx = 0;
        for (int i = 0; node->namespace_prefix[i] != '\0' && c_idx < 250; i++) {
            if (node->namespace_prefix[i] == ':' && node->namespace_prefix[i+1] == ':') {
                clean_ns[c_idx++] = '_';
                i++;
            } else {
                clean_ns[c_idx++] = node->namespace_prefix[i];
            }
        }
        clean_ns[c_idx] = '\0';
        char clean_name[256];
        int n_idx = 0;
        for (int i = 0; node->name[i] != '\0' && n_idx < 250; i++) {
            if (node->name[i] == '~') {
                clean_name[n_idx++] = 'd';
                clean_name[n_idx++] = 't';
                clean_name[n_idx++] = 'o';
                clean_name[n_idx++] = 'r';
                clean_name[n_idx++] = '_';
            } else {
                clean_name[n_idx++] = node->name[i];
            }
        }
        clean_name[n_idx] = '\0';
        sprintf(mangled_name, "%s_%s", clean_ns, clean_name);
    } else {
        char clean_name[256];
        int n_idx = 0;
        for (int i = 0; node->name[i] != '\0' && n_idx < 250; i++) {
            if (node->name[i] == '~') {
                clean_name[n_idx++] = 'd';
                clean_name[n_idx++] = 't';
                clean_name[n_idx++] = 'o';
                clean_name[n_idx++] = 'r';
                clean_name[n_idx++] = '_';
            } else {
                clean_name[n_idx++] = node->name[i];
            }
        }
        clean_name[n_idx] = '\0';
        strcpy(mangled_name, clean_name);
    }
    
    // Registrar parámetros: 8(%ebp), 12(%ebp), etc.
    ASTNode* param = node->params;
    int param_idx = 0;
    while (param != NULL) {
        int offset = 8 + (param_idx * 4);
        int is_str = (param->type_name && strstr(param->type_name, "string") != NULL && strchr(param->type_name, '*') == NULL) ? 1 : 0;
        add_symbol(param->name, offset, 1, param->type_name, is_str, 0, 0, param->is_const);
        param_idx++;
        param = param->next;
    }
    
    // Escanear variables locales y asignar espacio en la pila
    int current_offset = 0;
    if (node->body != NULL) {
        scan_local_variables(node->body, &current_offset);
        scan_inlines(node->body, &current_offset, 0);
    }
    
    // Generar etiquetas de la función
    if (strcmp(node->name, "main") == 0) {
        fprintf(out, "\n.text\n");
    } else if (node->is_static) {
        fprintf(out, "\n.text\n");
    } else {
        fprintf(out, "\n.section .text$%s\n", mangled_name);
        fprintf(out, ".linkonce discard\n");
    }
    if (!node->is_static) {
        fprintf(out, ".globl _%s\n", mangled_name);
        fprintf(out, ".globl %s\n", mangled_name);
    }
    fprintf(out, "_%s:\n", mangled_name);
    fprintf(out, "%s:\n", mangled_name);
    
    // Prólogo (alineado a 16 bytes)
    int stack_alloc = current_offset + 32;
    stack_alloc = (stack_alloc + 15) & ~15;
    fprintf(out, "    pushl %%ebp\n");
    fprintf(out, "    movl %%esp, %%ebp\n");
    fprintf(out, "    subl $%d, %%esp\n", stack_alloc);
    
    // Generar sentencias del cuerpo
    int has_explicit_return = 0;
    if (node->body != NULL) {
        generate_statement(node->body, out);
    }
    
    // Epílogo por defecto si no terminó en return
    if (!has_explicit_return) {
        fprintf(out, "    xorl %%eax, %%eax\n");
        fprintf(out, "    movl %%ebp, %%esp\n");
        fprintf(out, "    popl %%ebp\n");
        fprintf(out, "    ret\n");
    }
    current_func_namespace = NULL;
    current_func_name = NULL;
}

static void generate_declarations(ASTNode* decl, FILE* out) {
    while (decl != NULL) {
        if (decl->type == AST_USING) {
            UsingNS* uns = malloc(sizeof(UsingNS));
            uns->name = strdup(decl->name);
            uns->next = active_usings;
            active_usings = uns;
        } else if (decl->type == AST_CLASS) {
            ASTNode* m = decl->body;
            while (m != NULL) {
                if (m->type == AST_FUNCTION_DECLARATION) {
                    if (!m->is_extern && m->body != NULL) {
                        generate_function(m, out);
                    }
                }
                m = m->next;
            }
        } else if (decl->type == AST_FUNCTION_DECLARATION) {
            if (!decl->is_extern && decl->body != NULL) {
                generate_function(decl, out);
            }
        } else if (decl->type == AST_NAMESPACE) {
            generate_declarations(decl->children, out);
        } else if (decl->type == AST_IMPORT) {
            if (decl->children != NULL) {
                generate_declarations(decl->children, out);
            }
        } else if (decl->type == AST_VAR_DECLARATION) {
            // Ya registrado y emitido en .data o .rdata
        } else if (decl->type == AST_INLINE_ASM) {
            emit_inline_asm(decl->string_value, out);
        } else if (decl->type == AST_INLINE_C) {
            if (decl->string_value != NULL) {
                inline_c_count++;
                append_inline_c_raw("\n");
                append_inline_c_raw(decl->string_value);
                append_inline_c_raw("\n");
            }
        }
        decl = decl->next;
    }
}

void generate_code(ASTNode* node, const char* output_filename) {
    inline_c_count = 0;
    inline_c_len = 0;
    if (inline_c_buffer) inline_c_buffer[0] = '\0';
    snprintf(inline_c_file_path, sizeof(inline_c_file_path), "%s.inline_c.c", output_filename);

    FILE* out = fopen(output_filename, "w");
    if (!out) {
        printf("Error al crear el archivo ensamblador %s\n", output_filename);
        exit(1);
    }
    
    // 1. Recolectar literales de cadena
    string_count = 0;
    collect_strings(node);
    
    // 2. Liberar listas previas y registrar structs, funciones y variables globales declaradas
    while (declared_structs) { StructDef* n = declared_structs->next; free(declared_structs->name); free(declared_structs); declared_structs = n; }
    while (declared_functions) { DeclaredFunc* n = declared_functions->next; free(declared_functions->name); free(declared_functions); declared_functions = n; }
    while (declared_globals) { GlobalVar* n = declared_globals->next; free(declared_globals->name); free(declared_globals); declared_globals = n; }
    while (declared_static_locals) { StaticLocalVar* n = declared_static_locals->next; free(declared_static_locals); declared_static_locals = n; }
    while (active_usings) { UsingNS* n = active_usings->next; free(active_usings->name); free(active_usings); active_usings = n; }
    declared_structs = NULL;
    declared_functions = NULL;
    declared_globals = NULL;
    declared_static_locals = NULL;
    static_local_counter = 0;
    active_usings = NULL;
    if (node->type == AST_PROGRAM) {
        register_declared_structs(node->children);
        register_declared_functions(node->children);
        register_declared_variables(node->children);
    }
    
    // 3. Sección de constantes / rdata
    fprintf(out, ".section .rdata\n");
    for (int i = 0; i < string_count; i++) {
        fprintf(out, ".LC%d:\n    .asciz ", string_table[i].id);
        print_assembly_string(out, string_table[i].value);
        fprintf(out, "\n");
    }
    
    // Emitir constantes globales en .rdata
    GlobalVar* gv = declared_globals;
    while (gv != NULL) {
        if (gv->is_const) {
            fprintf(out, ".align 4\n");
            fprintf(out, "%s:\n", gv->asm_name);
            if (gv->string_val != NULL) {
                fprintf(out, "    .asciz ");
                print_assembly_string(out, gv->string_val);
                fprintf(out, "\n");
            } else if (gv->is_array && gv->array_size > 0) {
                fprintf(out, "    .space %d\n", gv->array_size * 4);
            } else {
                fprintf(out, "    .long %d\n", gv->int_val);
            }
        }
        gv = gv->next;
    }
    
    // Emitir variables globales mutables en .data si las hay (omitiendo externs)
    int has_mutable = 0;
    gv = declared_globals;
    while (gv != NULL) {
        if (!gv->is_const && !gv->is_extern) {
            if (!has_mutable) {
                fprintf(out, "\n.section .data\n");
                has_mutable = 1;
            }
            if (!gv->is_static) {
                fprintf(out, ".globl %s\n", gv->asm_name);
            }
            fprintf(out, ".align 4\n");
            fprintf(out, "%s:\n", gv->asm_name);
            if (gv->string_val != NULL) {
                fprintf(out, "    .asciz ");
                print_assembly_string(out, gv->string_val);
                fprintf(out, "\n");
            } else if (gv->is_array && gv->array_size > 0) {
                fprintf(out, "    .space %d\n", gv->array_size * 4);
            } else {
                fprintf(out, "    .long %d\n", gv->int_val);
            }
        }
        gv = gv->next;
    }
    
    // 4. Sección de código (.text)
    fprintf(out, "\n.text\n");
    
    // 5. Generar funciones del programa Itsuki
    if (node->type == AST_PROGRAM) {
        generate_declarations(node->children, out);
    }
    
    // 6. Emitir variables estáticas locales acumuladas en .data
    if (declared_static_locals != NULL) {
        fprintf(out, "\n.section .data\n");
        StaticLocalVar* slv = declared_static_locals;
        while (slv != NULL) {
            fprintf(out, ".align 4\n");
            fprintf(out, "%s:\n", slv->asm_name);
            if (slv->string_val != NULL) {
                fprintf(out, "    .asciz ");
                print_assembly_string(out, slv->string_val);
                fprintf(out, "\n");
            } else if (slv->is_array && slv->array_size > 0) {
                fprintf(out, "    .space %d\n", slv->array_size * 4);
            } else {
                fprintf(out, "    .long %d\n", slv->int_val);
            }
            slv = slv->next;
        }
    }
    
    fclose(out);

    // Si se recopilaron fragmentos de C en línea, escribirlos en el archivo compañero
    if (inline_c_count > 0 && inline_c_buffer != NULL && inline_c_len > 0) {
        FILE* cf = fopen(inline_c_file_path, "w");
        if (cf) {
            fprintf(cf, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n");
            fputs(inline_c_buffer, cf);
            fclose(cf);
        }
    }

    // Optimizador peephole si opt_level >= 1
    if (generator_opt_level >= 1) {
        peephole_optimize_file(output_filename);
    }
}
