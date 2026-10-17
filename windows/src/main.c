#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "generator.h"
#include "preprocessor.h"
#include "embedded_lib.h"
#include "package_manager.h"
#include "arch_config.h"

#ifdef _WIN32
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
#else
#include <unistd.h>
#endif

void print_indent(int level) {
    for (int i = 0; i < level; i++) printf("  ");
}

void print_ast(ASTNode* node, int level) {
    if (node == NULL) return;
    
    while (node != NULL) {
        print_indent(level);
        switch (node->type) {
            case AST_PROGRAM:
                printf("[Program]\n");
                print_ast(node->children, level + 1);
                break;
                
            case AST_IMPORT:
                printf("[Include] <%s>\n", node->name);
                break;
                
            case AST_NAMESPACE:
                printf("[Namespace] %s\n", node->name);
                print_ast(node->children, level + 1);
                break;
                
            case AST_USING:
                printf("[Using Namespace] %s\n", node->name);
                break;
                
            case AST_STRUCT:
                printf("[Struct] %s%s%s\n",
                       node->namespace_prefix ? node->namespace_prefix : "",
                       node->namespace_prefix ? "::" : "",
                       node->name);
                print_ast(node->fields, level + 1);
                break;

            case AST_CLASS:
                printf("[Class] %s%s%s%s%s\n",
                       node->namespace_prefix ? node->namespace_prefix : "",
                       node->namespace_prefix ? "::" : "",
                       node->name,
                       node->base_class ? " : " : "",
                       node->base_class ? node->base_class : "");
                if (node->fields) {
                    print_indent(level + 1);
                    printf("Fields:\n");
                    print_ast(node->fields, level + 2);
                }
                if (node->body) {
                    print_indent(level + 1);
                    printf("Methods:\n");
                    print_ast(node->body, level + 2);
                }
                break;

            case AST_UNION:
                printf("[Union] %s%s%s\n",
                       node->namespace_prefix ? node->namespace_prefix : "",
                       node->namespace_prefix ? "::" : "",
                       node->name);
                print_ast(node->fields, level + 1);
                break;

            case AST_TYPEDEF:
                printf("[Typedef] %s = %s\n", node->name, node->type_name ? node->type_name : "");
                if (node->children) print_ast(node->children, level + 1);
                break;

            case AST_ENUM:
                printf("[Enum] %s\n", node->name ? node->name : "(anon)");
                print_ast(node->fields, level + 1);
                break;

            case AST_ENUM_ITEM:
                printf("[EnumItem] %s = %d\n", node->name, node->int_value);
                break;
                
            case AST_STRUCT_FIELD:
                printf("[Field] %s : %s\n", node->name, node->type_name);
                break;
                
            case AST_FUNCTION_DECLARATION:
                printf("[%sFunction] %s%s%s -> %s\n",
                       node->is_inline ? "Inline " : "",
                       node->namespace_prefix ? node->namespace_prefix : "",
                       node->namespace_prefix ? "::" : "",
                       node->name,
                       node->type_name ? node->type_name : "void");
                if (node->params) {
                    print_indent(level + 1);
                    printf("Params:\n");
                    print_ast(node->params, level + 2);
                }
                if (node->body) {
                    print_indent(level + 1);
                    printf("Body:\n");
                    print_ast(node->body->statements, level + 2);
                }
                break;
                
            case AST_PARAM:
                printf("[Param] %s%s : %s\n", node->is_const ? "const " : "var ", node->name, node->type_name ? node->type_name : "auto");
                break;
                
            case AST_VAR_DECLARATION:
                printf("[VarDecl] %s%s : %s\n", node->is_const ? "const " : "var ", node->name, node->type_name ? node->type_name : "auto");
                if (node->init_value) {
                    print_indent(level + 1);
                    printf("Init:\n");
                    print_ast(node->init_value, level + 2);
                }
                break;
                
            case AST_ASSIGNMENT:
                printf("[Assign]\n");
                print_ast(node->left, level + 1);
                print_ast(node->init_value, level + 1);
                break;
                
            case AST_RETURN_STATEMENT:
                printf("[Return]\n");
                if (node->return_value) {
                    print_ast(node->return_value, level + 1);
                }
                break;
                
            case AST_EXPR_STATEMENT:
                printf("[ExprStmt]\n");
                print_ast(node->expr, level + 1);
                break;
                
            case AST_CALL_EXPR:
                printf("[Call] %s()\n", node->callee->name);
                if (node->args) {
                    print_indent(level + 1);
                    printf("Args:\n");
                    print_ast(node->args, level + 2);
                }
                break;
                
            case AST_BLOCK:
                printf("[Block]\n");
                print_ast(node->statements, level + 1);
                break;
                
            case AST_IF:
                printf("[If]\n");
                print_indent(level + 1);
                printf("Cond:\n");
                print_ast(node->expr, level + 2);
                print_indent(level + 1);
                printf("Then:\n");
                print_ast(node->body, level + 2);
                if (node->else_branch) {
                    print_indent(level + 1);
                    printf("Else:\n");
                    print_ast(node->else_branch, level + 2);
                }
                break;
                
            case AST_WHILE:
                printf("[While]\n");
                print_indent(level + 1);
                printf("Cond:\n");
                print_ast(node->expr, level + 2);
                print_indent(level + 1);
                printf("Body:\n");
                print_ast(node->body, level + 2);
                break;
                
            case AST_FOR:
                printf("[For]\n");
                if (node->init_value) {
                    print_indent(level + 1);
                    printf("Init:\n");
                    print_ast(node->init_value, level + 2);
                }
                if (node->expr) {
                    print_indent(level + 1);
                    printf("Cond:\n");
                    print_ast(node->expr, level + 2);
                }
                if (node->update_expr) {
                    print_indent(level + 1);
                    printf("Update:\n");
                    print_ast(node->update_expr, level + 2);
                }
                print_indent(level + 1);
                printf("Body:\n");
                print_ast(node->body, level + 2);
                break;
                
            case AST_DO_WHILE:
                printf("[DoWhile]\n");
                print_indent(level + 1);
                printf("Body:\n");
                print_ast(node->body, level + 2);
                print_indent(level + 1);
                printf("Cond:\n");
                print_ast(node->expr, level + 2);
                break;

            case AST_BREAK:
                printf("[Break]\n");
                break;

            case AST_CONTINUE:
                printf("[Continue]\n");
                break;

            case AST_SWITCH:
                printf("[Switch]\n");
                print_indent(level + 1);
                printf("Expr:\n");
                print_ast(node->expr, level + 2);
                print_indent(level + 1);
                printf("Cases:\n");
                print_ast(node->body, level + 2);
                break;

            case AST_CASE:
                printf("[Case] %d:\n", node->int_value);
                print_ast(node->body, level + 1);
                break;

            case AST_DEFAULT:
                printf("[Default]:\n");
                print_ast(node->body, level + 1);
                break;

            case AST_TERNARY:
                printf("[Ternary ?:]\n");
                print_indent(level + 1);
                printf("Cond:\n");
                print_ast(node->expr, level + 2);
                print_indent(level + 1);
                printf("Then:\n");
                print_ast(node->left, level + 2);
                print_indent(level + 1);
                printf("Else:\n");
                print_ast(node->right, level + 2);
                break;

            case AST_BINARY_EXPR:
                if (node->op_str[0] != '\0') {
                    printf("[BinaryOp] '%s'\n", node->op_str);
                } else {
                    printf("[BinaryOp] '%c'\n", node->op);
                }
                print_ast(node->left, level + 1);
                print_ast(node->right, level + 1);
                break;

            case AST_COMPOUND_ASSIGNMENT:
                printf("[CompoundAssign] '%s'\n", node->op_str);
                print_ast(node->left, level + 1);
                print_ast(node->init_value, level + 1);
                break;

            case AST_INC_DEC:
                printf("[IncDec] '%s' (%s)\n", node->op_str, node->int_value ? "postfix" : "prefix");
                print_ast(node->left, level + 1);
                break;
                
            case AST_INDEX_EXPR:
                printf("[IndexAccess]\n");
                print_indent(level + 1);
                printf("Array:\n");
                print_ast(node->left, level + 2);
                print_indent(level + 1);
                printf("Index:\n");
                print_ast(node->expr, level + 2);
                break;

            case AST_MEMBER_EXPR:
                printf("[MemberAccess] .%s\n", node->name);
                print_ast(node->left, level + 1);
                break;
                
            case AST_UNARY_EXPR:
                if (node->op_str[0] != '\0') {
                    printf("[UnaryOp] '%s'\n", node->op_str);
                } else {
                    printf("[UnaryOp] '%c'\n", node->op);
                }
                print_ast(node->left, level + 1);
                break;
                
            case AST_IDENTIFIER:
                printf("[Id] %s\n", node->name);
                break;
                
            case AST_SCOPED_IDENTIFIER:
                printf("[ScopedId] %s\n", node->name);
                break;
                
            case AST_NUMBER:
                printf("[Number] %d\n", node->int_value);
                break;

            case AST_BOOLEAN:
                printf("[Boolean] %s\n", node->int_value ? "true" : "false");
                break;
                
            case AST_STRING:
                printf("[String] \"%s\"\n", node->string_value);
                break;
                
            case AST_NULL:
                printf("[Null] %s\n", node->name ? node->name : "null");
                break;

            case AST_THIS:
                printf("[This]\n");
                break;
                
            case AST_INLINE_ASM:
                printf("[Inline Asm] %s\n", node->string_value ? node->string_value : "");
                break;

            case AST_INLINE_C:
                printf("[Inline C] %s\n", node->string_value ? node->string_value : "");
                break;
                
            default:
                printf("[Node %d]\n", node->type);
                break;
        }
        node = node->next;
    }
}

static char* read_file(const char* filepath) {
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
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

static char* load_and_preprocess_file(const char* filepath) {
    preprocessor_reset_unit();
    char* raw = read_file(filepath);
    if (!raw) return NULL;
    char* sc = preprocess_source(raw, filepath);
    free(raw);
    return sc;
}

#ifdef _WIN32
#define ITSUKI_VERSION "5.0.0 (Native x86_32 Win32)"
#else
#define ITSUKI_VERSION "5.0.0 (Native Linux x86)"
#endif

static void print_banner(void) {
    const ArchConfig* arch = arch_get_current();
    printf("  Itsuki Programming Language - Compiler v%s [%s %d-bit -> Target x86_32]\n",
           ITSUKI_VERSION, arch->host_arch, arch->os_bits);
}

static void print_help(const char* prog_name) {
    print_banner();
    printf("\nUso: %s [comando] <archivo.suki|huki...> [opciones]\n\n", prog_name);
    printf("Comandos de Compilacion:\n");
    printf("  run   <archivos...>         Compila, enlaza con runtime nativo y ejecuta directamente\n");
    printf("  build <archivos...> [-o exe] Compila multiples archivos .suki/.huki y genera un ejecutable nativo\n");
    printf("  asm   <archivos...> [-o asm] Genera el codigo fuente ensamblador x86\n");
    printf("  ast   <archivos...>         Muestra el Arbol de Sintaxis Abstracta (AST)\n");
    printf("  cpp   <archivos...>         Ejecuta el preprocesador de C y muestra el codigo resultante\n\n");
    printf("Comandos de Gestor de Paquetes (Librerias Git/GitHub):\n");
    printf("  install   <url-git|usuario/repo>  Instala una libreria desde GitHub en el sistema (~/.itsuki/packages)\n");
    printf("  uninstall <nombre-libreria>       Desinstala una libreria instalada en el sistema\n");
    printf("  list                              Lista todas las librerias instaladas en el sistema\n\n");
    printf("Comandos de Diagnostico y Arquitectura:\n");
    printf("  doctor, info                Analiza y muestra la arquitectura de la PC, GCC y compatibilidad\n\n");
    printf("Opciones:\n");
    printf("  -E                          Ejecuta solo el preprocesador (equivalente al comando 'cpp')\n");
    printf("  -D<macro>[=valor]           Define una macro para el preprocesador\n");
    printf("  -l<lib>                     Enlaza bibliotecas de C/Windows (ej: -lws2_32, -lwinmm, -lm)\n");
    printf("  -L<dir>                     Agrega directorio de busqueda de bibliotecas\n");
    printf("  -I<dir>                     Agrega directorio de busqueda de cabeceras\n");
    printf("  -O0, -O1, -O2, -O3, -Os     Nivel de optimizacion\n");
    printf("  -o <salida>                 Nombre del archivo de salida\n");
    printf("  --arch                      Muestra el diagnostico de arquitectura de la maquina y GCC\n");
    printf("  -v, --version               Muestra la version del compilador\n");
    printf("  -h, --help                  Muestra esta ayuda\n\n");
}

static void get_runtime_path(char* out_path, size_t max_len) {
    FILE* f = fopen("lib/runtime.s", "rb");
    if (f) {
        fclose(f);
        strncpy(out_path, "lib/runtime.s", max_len - 1);
        out_path[max_len - 1] = '\0';
        return;
    }
#ifdef _WIN32
    f = fopen("lib\\runtime.s", "rb");
    if (f) {
        fclose(f);
        strncpy(out_path, "lib\\runtime.s", max_len - 1);
        out_path[max_len - 1] = '\0';
        return;
    }

    char exe_dir[512] = {0};
    if (GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir)) > 0) {
        char* last_slash = strrchr(exe_dir, '\\');
        if (!last_slash) last_slash = strrchr(exe_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(out_path, max_len, "%.490s\\lib\\runtime.s", exe_dir);
            f = fopen(out_path, "rb");
            if (f) {
                fclose(f);
                return;
            }
            snprintf(out_path, max_len, "%.490s\\..\\lib\\runtime.s", exe_dir);
            f = fopen(out_path, "rb");
            if (f) {
                fclose(f);
                return;
            }
        }
    }
    strncpy(out_path, "lib\\runtime.s", max_len - 1);
    out_path[max_len - 1] = '\0';
#else
    char exe_dir[512] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
    if (len > 0) {
        exe_dir[len] = '\0';
        char* last_slash = strrchr(exe_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(out_path, max_len, "%.490s/lib/runtime.s", exe_dir);
            f = fopen(out_path, "rb");
            if (f) {
                fclose(f);
                return;
            }
            snprintf(out_path, max_len, "%.490s/../lib/runtime.s", exe_dir);
            f = fopen(out_path, "rb");
            if (f) {
                fclose(f);
                return;
            }
        }
    }
    strncpy(out_path, "lib/runtime.s", max_len - 1);
    out_path[max_len - 1] = '\0';
#endif
    // Si no se encuentra en disco, extraer el runtime embebido en el propio binario
    if (extract_embedded_runtime(out_path, max_len)) {
        return;
    }
}

static void get_base_name(const char* filepath, char* out_base, size_t max_len) {
    if (strncmp(filepath, "embedded:", 9) == 0) filepath += 9;
    const char* slash = strrchr(filepath, '/');
    const char* bslash = strrchr(filepath, '\\');
    const char* name = filepath;
    if (slash && slash >= name) name = slash + 1;
    if (bslash && bslash >= name) name = bslash + 1;
    
    strncpy(out_base, name, max_len - 1);
    out_base[max_len - 1] = '\0';
    
    char* dot = strrchr(out_base, '.');
    if (dot) *dot = '\0';
}

static int resolve_source_file(const char* raw, char* resolved, size_t max_len) {
    if (raw == NULL) return 0;
    if (strncmp(raw, "embedded:", 9) == 0) {
        strncpy(resolved, raw, max_len - 1);
        resolved[max_len - 1] = '\0';
        return 1;
    }
    FILE* f = fopen(raw, "rb");
    if (f) {
        fclose(f);
        strncpy(resolved, raw, max_len - 1);
        resolved[max_len - 1] = '\0';
        return 1;
    }
    snprintf(resolved, max_len, "%s.suki", raw);
    f = fopen(resolved, "rb");
    if (f) { fclose(f); return 1; }
    snprintf(resolved, max_len, "%s.huki", raw);
    f = fopen(resolved, "rb");
    if (f) { fclose(f); return 1; }
    snprintf(resolved, max_len, "%s.su", raw);
    f = fopen(resolved, "rb");
    if (f) { fclose(f); return 1; }
    snprintf(resolved, max_len, "%s.hu", raw);
    f = fopen(resolved, "rb");
    if (f) { fclose(f); return 1; }
    snprintf(resolved, max_len, "%s.it", raw);
    f = fopen(resolved, "rb");
    if (f) { fclose(f); return 1; }

    // Fallback a archivos embebidos
    if (has_embedded_file(raw)) {
        snprintf(resolved, max_len, "embedded:%.490s", raw);
        return 1;
    }
    char with_ext[512];
    snprintf(with_ext, sizeof(with_ext), "%.490s.suki", raw);
    if (has_embedded_file(with_ext)) {
        snprintf(resolved, max_len, "embedded:%.490s", with_ext);
        return 1;
    }
    snprintf(with_ext, sizeof(with_ext), "%.490s.huki", raw);
    if (has_embedded_file(with_ext)) {
        snprintf(resolved, max_len, "embedded:%.490s", with_ext);
        return 1;
    }
    snprintf(with_ext, sizeof(with_ext), "%.490s.su", raw);
    if (has_embedded_file(with_ext)) {
        snprintf(resolved, max_len, "embedded:%.490s", with_ext);
        return 1;
    }
    snprintf(with_ext, sizeof(with_ext), "%.490s.hu", raw);
    if (has_embedded_file(with_ext)) {
        snprintf(resolved, max_len, "embedded:%.490s", with_ext);
        return 1;
    }

    // Fallback a paquetes instalados en el sistema (~/.itsuki/packages/)
    if (pkg_resolve_include(raw, 1, resolved, max_len)) {
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    arch_init();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            const ArchConfig* arch = arch_get_current();
            printf("Itsuki Compiler v%s [Host: %s %d-bit, Linker: %s %s]\n",
                   ITSUKI_VERSION, arch->host_arch, arch->os_bits, arch->gcc_path,
                   arch->gcc_flags[0] ? arch->gcc_flags : "(standard)");
            return 0;
        }
        if (strcmp(argv[i], "--arch") == 0) {
            arch_print_diagnostics();
            return 0;
        }
    }

    if (argc < 2) {
        printf("itsuki: fatal error: no input files\ncompilation terminated.\n");
        return 1;
    }

    // Comandos de Diagnostico y Arquitectura
    if (strcmp(argv[1], "doctor") == 0 || strcmp(argv[1], "info") == 0) {
        arch_print_diagnostics();
        return 0;
    }

    // Comandos de Gestor de Paquetes
    if (strcmp(argv[1], "install") == 0) {
        return pkg_install(argc > 2 ? argv[2] : NULL);
    }
    if (strcmp(argv[1], "uninstall") == 0 || strcmp(argv[1], "remove") == 0) {
        return pkg_uninstall(argc > 2 ? argv[2] : NULL);
    }
    if (strcmp(argv[1], "list") == 0) {
        return pkg_list();
    }

    const char* command = "run";
    int file_arg_idx = 1;

    if (strcmp(argv[1], "run") == 0 || strcmp(argv[1], "build") == 0 ||
        strcmp(argv[1], "asm") == 0 || strcmp(argv[1], "ast") == 0 ||
        strcmp(argv[1], "cpp") == 0 || strcmp(argv[1], "-E") == 0) {
        command = argv[1];
        file_arg_idx = 2;
    }

    const char* custom_output = NULL;
    char source_files[64][512];
    int source_file_count = 0;
    char extra_c_files[64][512];
    int extra_c_file_count = 0;
    char linker_flags[64][256];
    int linker_flag_count = 0;
    int opt_level = 2;
    char opt_flag[32] = "-O2";

    for (int i = file_arg_idx; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            custom_output = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "-E") == 0) {
            command = "cpp";
        } else if (strncmp(argv[i], "-O", 2) == 0) {
            strncpy(opt_flag, argv[i], 31);
            opt_flag[31] = '\0';
            if (strcmp(argv[i], "-O0") == 0) opt_level = 0;
            else if (strcmp(argv[i], "-O1") == 0) opt_level = 1;
            else if (strcmp(argv[i], "-O2") == 0) opt_level = 2;
            else if (strcmp(argv[i], "-O3") == 0) opt_level = 3;
            else if (strcmp(argv[i], "-Os") == 0) opt_level = 2;
            else if (strcmp(argv[i], "-Ofast") == 0) opt_level = 3;
            else opt_level = 2;
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            const char* def_str = argv[i] + 2;
            if (*def_str == '\0' && i + 1 < argc) {
                def_str = argv[++i];
            }
            char d_name[128];
            char d_val[256] = "1";
            const char* eq = strchr(def_str, '=');
            if (eq) {
                size_t nlen = eq - def_str;
                if (nlen >= 127) nlen = 127;
                strncpy(d_name, def_str, nlen);
                d_name[nlen] = '\0';
                strncpy(d_val, eq + 1, 255);
                d_val[255] = '\0';
            } else {
                strncpy(d_name, def_str, 127);
                d_name[127] = '\0';
            }
            preprocessor_define(d_name, d_val);
        } else if (strcmp(argv[i], "-m32") == 0) {
            // Aceptado explicitamente para arquitectura x86 32-bit
        } else if (strcmp(argv[i], "-m64") == 0) {
            printf("[Itsuki Info] Itsuki genera ejecutables x86 (32 bits), compatibles de forma transparente con sistemas de 64 bits (WOW64 en Windows y multilib en Linux).\n");
        } else if (strncmp(argv[i], "-l", 2) == 0 || strncmp(argv[i], "-L", 2) == 0 || strncmp(argv[i], "-I", 2) == 0) {
            if (linker_flag_count < 64) {
                strncpy(linker_flags[linker_flag_count], argv[i], 255);
                linker_flags[linker_flag_count][255] = '\0';
                linker_flag_count++;
            }
        } else if (argv[i][0] != '-') {
            char res_path[512];
            if (resolve_source_file(argv[i], res_path, sizeof(res_path))) {
                const char* dot = strrchr(res_path, '.');
                if (dot != NULL && (strcmp(dot, ".c") == 0 || strcmp(dot, ".cpp") == 0 ||
                                   strcmp(dot, ".cc") == 0 || strcmp(dot, ".o") == 0 ||
                                   strcmp(dot, ".a") == 0 || strcmp(dot, ".obj") == 0 ||
                                   strcmp(dot, ".lib") == 0)) {
                    int exists = 0;
                    for (int j = 0; j < extra_c_file_count; j++) {
                        if (strcmp(extra_c_files[j], res_path) == 0) {
                            exists = 1;
                            break;
                        }
                    }
                    if (!exists && extra_c_file_count < 64) {
                        snprintf(extra_c_files[extra_c_file_count], sizeof(extra_c_files[extra_c_file_count]), "%s", res_path);
                        extra_c_files[extra_c_file_count][511] = '\0';
                        extra_c_file_count++;
                    }
                } else {
                    int exists = 0;
                    for (int j = 0; j < source_file_count; j++) {
                        if (strcmp(source_files[j], res_path) == 0) {
                            exists = 1;
                            break;
                        }
                    }
                    if (!exists && source_file_count < 64) {
                        snprintf(source_files[source_file_count], sizeof(source_files[source_file_count]), "%s", res_path);
                        source_files[source_file_count][511] = '\0';
                        source_file_count++;
                    }
                }
            } else {
                printf("[Itsuki Error] No se pudo encontrar el archivo '%s'\n", argv[i]);
                return 1;
            }
        }
    }

    if (source_file_count == 0) {
        printf("itsuki: fatal error: no input files\ncompilation terminated.\n");
        return 1;
    }

    // Escanear recursivamente archivos fuente companeros (.suki) descubiertos a traves de .huki incluidos
    int scanned_idx = 0;
    while (scanned_idx < source_file_count) {
        clear_discovered_sources();
        char* sc = load_and_preprocess_file(source_files[scanned_idx]);
        if (sc != NULL) {
            parser_reset();
            Lexer* l = init_lexer(sc);
            Parser* p = init_parser(l);
            ASTNode* a = parser_parse(p);
            (void)a;
            
            int disc_count = get_discovered_source_count();
            for (int d = 0; d < disc_count; d++) {
                const char* disc = get_discovered_source(d);
                int already = 0;
                for (int k = 0; k < source_file_count; k++) {
                    if (strcmp(source_files[k], disc) == 0) {
                        already = 1;
                        break;
                    }
                }
                if (!already && source_file_count < 64) {
                    strncpy(source_files[source_file_count], disc, 511);
                    source_files[source_file_count][511] = '\0';
                    source_file_count++;
                }
            }
            free(sc);
        }
        scanned_idx++;
    }

    if (strcmp(command, "cpp") == 0 || strcmp(command, "-E") == 0) {
        for (int i = 0; i < source_file_count; i++) {
            char* sc = load_and_preprocess_file(source_files[i]);
            if (!sc) {
                printf("[Itsuki Error] No se pudo leer '%s'\n", source_files[i]);
                return 1;
            }
            if (custom_output && source_file_count == 1) {
                FILE* out = fopen(custom_output, "w");
                if (out) {
                    fputs(sc, out);
                    fclose(out);
                }
            } else {
                fputs(sc, stdout);
            }
            free(sc);
        }
        return 0;
    }

    if (strcmp(command, "ast") == 0) {
        for (int i = 0; i < source_file_count; i++) {
            char* sc = load_and_preprocess_file(source_files[i]);
            parser_reset();
            Lexer* l = init_lexer(sc);
            Parser* p = init_parser(l);
            ASTNode* a = parser_parse(p);
            printf("--- AST (%s) ---\n", source_files[i]);
            print_ast(a, 0);
            free(sc);
        }
        return 0;
    }

    if (strcmp(command, "asm") == 0) {
        for (int i = 0; i < source_file_count; i++) {
            char base[256];
            get_base_name(source_files[i], base, sizeof(base));
            char out_asm[512];
            if (custom_output && source_file_count == 1) {
                snprintf(out_asm, sizeof(out_asm), "%s", custom_output);
            } else {
                snprintf(out_asm, sizeof(out_asm), "%s.s", base);
            }
            char* sc = load_and_preprocess_file(source_files[i]);
            parser_reset();
            Lexer* l = init_lexer(sc);
            Parser* p = init_parser(l);
            ASTNode* a = parser_parse(p);
            generator_set_opt_level(opt_level);
            generate_code(a, out_asm);
            printf("[Itsuki] Codigo ensamblador x86 generado: %s\n", out_asm);
            free(sc);
        }
        return 0;
    }

    char main_base[256];
    get_base_name(source_files[0], main_base, sizeof(main_base));
    
    char runtime_path[512];
    get_runtime_path(runtime_path, sizeof(runtime_path));

    char tmp_asm_files[64][512];
    int asm_count = 0;

    generator_set_opt_level(opt_level);

    for (int i = 0; i < source_file_count; i++) {
        // Si el archivo es un .huki/.hu solo sin implementaciones y hay mas archivos, omitir ensamble
        if ((strstr(source_files[i], ".huki") != NULL || strstr(source_files[i], ".hu") != NULL) && source_file_count > 1) {
            continue;
        }
        char base[256];
        get_base_name(source_files[i], base, sizeof(base));
        snprintf(tmp_asm_files[asm_count], sizeof(tmp_asm_files[asm_count]), "%s.tmp.%d.s", base, i);
        
        char* sc = load_and_preprocess_file(source_files[i]);
        if (!sc) {
            printf("[Itsuki Error] No se pudo leer '%s'\n", source_files[i]);
            return 1;
        }
        parser_reset();
        Lexer* l = init_lexer(sc);
        Parser* p = init_parser(l);
        ASTNode* a = parser_parse(p);
        generate_code(a, tmp_asm_files[asm_count]);
        free(sc);
        asm_count++;
    }

    char target_exe[512];
    if (strcmp(command, "build") == 0) {
        if (custom_output) {
            snprintf(target_exe, sizeof(target_exe), "%s", custom_output);
        } else {
#ifdef _WIN32
            snprintf(target_exe, sizeof(target_exe), "%s.exe", main_base);
#else
            snprintf(target_exe, sizeof(target_exe), "%s", main_base);
#endif
        }
    } else {
#ifdef _WIN32
        snprintf(target_exe, sizeof(target_exe), "%s.tmp.exe", main_base);
#else
        snprintf(target_exe, sizeof(target_exe), "%s.tmp.out", main_base);
#endif
    }

    char gcc_cmd[4096];
    const ArchConfig* arch = arch_get_current();
    const char* gpath = (arch->gcc_path[0] != '\0') ? arch->gcc_path : "gcc";
    int pos = 0;
    if (arch->gcc_flags[0] != '\0') {
        pos = snprintf(gcc_cmd, sizeof(gcc_cmd), "%s %s", gpath, arch->gcc_flags);
    } else {
        pos = snprintf(gcc_cmd, sizeof(gcc_cmd), "%s", gpath);
    }

    for (int i = 0; i < asm_count; i++) {
        pos += snprintf(gcc_cmd + pos, sizeof(gcc_cmd) - pos, " \"%s\"", tmp_asm_files[i]);
    }
    for (int i = 0; i < extra_c_file_count; i++) {
        pos += snprintf(gcc_cmd + pos, sizeof(gcc_cmd) - pos, " \"%s\"", extra_c_files[i]);
    }
    const char* inline_c_f = generator_get_inline_c_file();
    if (inline_c_f != NULL) {
        FILE* chk = fopen(inline_c_f, "r");
        if (chk) {
            fclose(chk);
            pos += snprintf(gcc_cmd + pos, sizeof(gcc_cmd) - pos, " \"%s\"", inline_c_f);
        }
    }
    if (opt_flag[0] != '\0') {
        pos += snprintf(gcc_cmd + pos, sizeof(gcc_cmd) - pos, " %s", opt_flag);
    }
#ifdef _WIN32
    pos += snprintf(gcc_cmd + pos, sizeof(gcc_cmd) - pos, " \"%s\" -o \"%s\" -s -Wl,--gc-sections -lkernel32", runtime_path, target_exe);
#else
    pos += snprintf(gcc_cmd + pos, sizeof(gcc_cmd) - pos, " \"%s\" -o \"%s\" -s -Wl,--gc-sections -lm", runtime_path, target_exe);
#endif
    for (int i = 0; i < linker_flag_count; i++) {
        pos += snprintf(gcc_cmd + pos, sizeof(gcc_cmd) - pos, " %s", linker_flags[i]);
    }

    int build_ret = system(gcc_cmd);

    for (int i = 0; i < asm_count; i++) {
        remove(tmp_asm_files[i]);
    }
    if (inline_c_f != NULL) {
        remove(inline_c_f);
    }

    if (build_ret != 0) {
        printf("\n[Itsuki Error] Fallo al compilar y enlazar el ejecutable '%s'\n", target_exe);
        arch_print_linker_troubleshooting();
        return 1;
    }

    if (strcmp(command, "build") == 0) {
        printf("[Itsuki] Compilado exitosamente -> %s\n", target_exe);
        return 0;
    }

    if (strcmp(command, "run") == 0) {
        char run_cmd[1024];
#ifdef _WIN32
        snprintf(run_cmd, sizeof(run_cmd), ".\\\"%s\"", target_exe);
#else
        snprintf(run_cmd, sizeof(run_cmd), "./\"%s\"", target_exe);
#endif
        int run_ret = system(run_cmd);
        remove(target_exe);
        return run_ret;
    }

    return 0;
}
