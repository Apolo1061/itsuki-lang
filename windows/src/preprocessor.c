#include "preprocessor.h"
#include "parser.h"
#include "embedded_lib.h"
#include "package_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_IF_DEPTH 128
#define MAX_INCLUDE_DEPTH 64
#define MAX_MACRO_PARAMS 32
#define MAX_PRAGMA_ONCE 512

typedef struct Macro {
    char name[128];
    int is_function_like;
    int param_count;
    char params[MAX_MACRO_PARAMS][64];
    int is_variadic;
    char* replacement;
    int is_expanding; // Guardia de recursión
    int is_predefined; // 1 si es de línea de comandos / compilador, 0 si fue definida en código fuente
    struct Macro* next;
} Macro;

static Macro* macro_table = NULL;

// Tabla de archivos con #pragma once
static char pragma_once_files[MAX_PRAGMA_ONCE][512];
static int pragma_once_count = 0;

// Pila de condicionales (#if / #ifdef / #ifndef / #elif / #else / #endif)
typedef struct {
    int condition_met;    // ¿Alguna rama (if/elif) ya fue verdadera?
    int currently_active; // ¿Esta rama está emitiendo código?
    int parent_active;    // ¿El bloque padre estaba activo?
    int line;             // Línea donde se abrió
    char file[256];       // Archivo donde se abrió
} IfStackEntry;

static IfStackEntry if_stack[MAX_IF_DEPTH];
static int if_depth = 0;

static int current_include_depth = 0;

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} Buffer;

static Buffer* buf_new(size_t initial_cap) {
    Buffer* b = malloc(sizeof(Buffer));
    if (initial_cap < 64) initial_cap = 64;
    b->data = malloc(initial_cap);
    b->data[0] = '\0';
    b->length = 0;
    b->capacity = initial_cap;
    return b;
}

static void buf_free(Buffer* b) {
    if (b) {
        if (b->data) free(b->data);
        free(b);
    }
}

static void buf_reserve(Buffer* b, size_t extra) {
    if (b->length + extra + 1 >= b->capacity) {
        size_t new_cap = (b->capacity * 2 > b->length + extra + 64) ? b->capacity * 2 : b->length + extra + 64;
        b->data = realloc(b->data, new_cap);
        b->capacity = new_cap;
    }
}

static void buf_append_char(Buffer* b, char c) {
    buf_reserve(b, 1);
    b->data[b->length++] = c;
    b->data[b->length] = '\0';
}

static void buf_append_str(Buffer* b, const char* str) {
    if (!str) return;
    size_t len = strlen(str);
    buf_reserve(b, len);
    memcpy(b->data + b->length, str, len);
    b->length += len;
    b->data[b->length] = '\0';
}

static void normalize_path(const char* path, char* out, size_t max_len) {
    size_t i = 0;
    for (; path[i] != '\0' && i < max_len - 1; i++) {
        if (path[i] == '\\') out[i] = '/';
        else out[i] = path[i];
    }
    out[i] = '\0';
}

static void get_directory(const char* filepath, char* out_dir, size_t max_len) {
    char norm[512];
    normalize_path(filepath, norm, sizeof(norm));
    char* last_slash = strrchr(norm, '/');
    if (last_slash != NULL) {
        size_t len = last_slash - norm;
        if (len >= max_len) len = max_len - 1;
        memcpy(out_dir, norm, len);
        out_dir[len] = '\0';
    } else {
        strcpy(out_dir, ".");
    }
}

static int is_pragma_once(const char* filepath) {
    char norm[512];
    normalize_path(filepath, norm, sizeof(norm));
    for (int i = 0; i < pragma_once_count; i++) {
        if (strcmp(pragma_once_files[i], norm) == 0) return 1;
    }
    return 0;
}

static void mark_pragma_once(const char* filepath) {
    if (pragma_once_count < MAX_PRAGMA_ONCE) {
        normalize_path(filepath, pragma_once_files[pragma_once_count], 512);
        pragma_once_count++;
    }
}

static Macro* find_macro(const char* name) {
    if (!name) return NULL;
    Macro* m = macro_table;
    while (m != NULL) {
        if (strcmp(m->name, name) == 0) return m;
        m = m->next;
    }
    return NULL;
}

static void define_macro_internal(const char* name, const char* value, int is_predefined) {
    preprocessor_undef(name);
    Macro* m = malloc(sizeof(Macro));
    strncpy(m->name, name, 127);
    m->name[127] = '\0';
    m->is_function_like = 0;
    m->param_count = 0;
    m->is_variadic = 0;
    m->replacement = value ? strdup(value) : strdup("");
    m->is_expanding = 0;
    m->is_predefined = is_predefined;
    m->next = macro_table;
    macro_table = m;
}

void preprocessor_define(const char* name, const char* value) {
    define_macro_internal(name, value, 1);
}

void preprocessor_undef(const char* name) {
    Macro** curr = &macro_table;
    while (*curr != NULL) {
        if (strcmp((*curr)->name, name) == 0) {
            Macro* to_free = *curr;
            *curr = (*curr)->next;
            if (to_free->replacement) free(to_free->replacement);
            free(to_free);
            return;
        }
        curr = &((*curr)->next);
    }
}

static void define_function_macro(const char* name, int param_count, char params[][64], int is_var, const char* body) {
    preprocessor_undef(name);
    Macro* m = malloc(sizeof(Macro));
    strncpy(m->name, name, 127);
    m->name[127] = '\0';
    m->is_function_like = 1;
    m->param_count = param_count;
    m->is_variadic = is_var;
    for (int i = 0; i < param_count; i++) {
        snprintf(m->params[i], sizeof(m->params[i]), "%.63s", params[i]);
        m->params[i][63] = '\0';
    }
    m->replacement = body ? strdup(body) : strdup("");
    m->is_expanding = 0;
    m->is_predefined = 0;
    m->next = macro_table;
    macro_table = m;
}

void preprocessor_reset_unit(void) {
    Macro** curr = &macro_table;
    while (*curr != NULL) {
        if (!(*curr)->is_predefined) {
            Macro* to_free = *curr;
            *curr = (*curr)->next;
            if (to_free->replacement) free(to_free->replacement);
            free(to_free);
        } else {
            curr = &((*curr)->next);
        }
    }
    pragma_once_count = 0;
    if_depth = 0;
    current_include_depth = 0;
}

void preprocessor_reset(void) {
    Macro* m = macro_table;
    while (m != NULL) {
        Macro* next = m->next;
        if (m->replacement) free(m->replacement);
        free(m);
        m = next;
    }
    macro_table = NULL;
    pragma_once_count = 0;
    if_depth = 0;
    current_include_depth = 0;

    // Inicializar macros estándar
    preprocessor_define("__ITSUKI__", "1");
    preprocessor_define("__ITSUKI_VERSION__", "2");
}

static int is_emitting(void) {
    if (if_depth == 0) return 1;
    return if_stack[if_depth - 1].currently_active;
}

static char* expand_macros_in_text(const char* text, const char* cur_file, int cur_line);

// Stringify: convierte texto en un literal de cadena con comillas y escapes
static char* stringify_arg(const char* arg) {
    Buffer* b = buf_new(strlen(arg) + 8);
    buf_append_char(b, '"');
    for (size_t i = 0; arg[i] != '\0'; i++) {
        if (arg[i] == '"' || arg[i] == '\\') {
            buf_append_char(b, '\\');
        }
        buf_append_char(b, arg[i]);
    }
    buf_append_char(b, '"');
    char* res = strdup(b->data);
    buf_free(b);
    return res;
}

// Expansión de macro tipo función
static char* expand_function_macro(Macro* m, const char* args_str, const char* cur_file, int cur_line) {
    // Parsear argumentos respetando paréntesis, cadenas y caracteres
    char actual_args[MAX_MACRO_PARAMS][512];
    int arg_count = 0;

    const char* p = args_str;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '(') p++;

    Buffer* cur_arg = buf_new(64);
    int paren_depth = 0;
    int in_str = 0;
    int in_chr = 0;

    while (*p != '\0') {
        char c = *p;
        if (!in_str && !in_chr) {
            if (c == '"') { in_str = 1; buf_append_char(cur_arg, c); }
            else if (c == '\'') { in_chr = 1; buf_append_char(cur_arg, c); }
            else if (c == '(') { paren_depth++; buf_append_char(cur_arg, c); }
            else if (c == ')') {
                if (paren_depth > 0) {
                    paren_depth--;
                    buf_append_char(cur_arg, c);
                } else {
                    break; // Fin de argumentos
                }
            } else if (c == ',' && paren_depth == 0) {
                // Trim y guardar argumento
                char* trimmed = cur_arg->data;
                while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
                int tlen = (int)strlen(trimmed);
                while (tlen > 0 && isspace((unsigned char)trimmed[tlen - 1])) tlen--;
                if (arg_count < MAX_MACRO_PARAMS) {
                    strncpy(actual_args[arg_count], trimmed, tlen);
                    actual_args[arg_count][tlen] = '\0';
                    arg_count++;
                }
                cur_arg->length = 0;
                cur_arg->data[0] = '\0';
            } else {
                buf_append_char(cur_arg, c);
            }
        } else if (in_str) {
            buf_append_char(cur_arg, c);
            if (c == '\\' && *(p + 1) != '\0') {
                buf_append_char(cur_arg, *(++p));
            } else if (c == '"') {
                in_str = 0;
            }
        } else if (in_chr) {
            buf_append_char(cur_arg, c);
            if (c == '\\' && *(p + 1) != '\0') {
                buf_append_char(cur_arg, *(++p));
            } else if (c == '\'') {
                in_chr = 0;
            }
        }
        p++;
    }

    // Último argumento si había algo
    if (cur_arg->length > 0 || arg_count > 0 || m->param_count > 0) {
        char* trimmed = cur_arg->data;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        int tlen = (int)strlen(trimmed);
        while (tlen > 0 && isspace((unsigned char)trimmed[tlen - 1])) tlen--;
        if (arg_count < MAX_MACRO_PARAMS) {
            strncpy(actual_args[arg_count], trimmed, tlen);
            actual_args[arg_count][tlen] = '\0';
            arg_count++;
        }
    }
    buf_free(cur_arg);

    // Sustitución en el reemplazo de la macro
    Buffer* res = buf_new(strlen(m->replacement) * 2 + 32);
    const char* r = m->replacement;

    while (*r != '\0') {
        // Operador Token Paste ##
        if (r[0] == '#' && r[1] == '#') {
            // Eliminar espacios finales del buffer previo
            while (res->length > 0 && isspace((unsigned char)res->data[res->length - 1])) {
                res->length--;
                res->data[res->length] = '\0';
            }
            r += 2;
            while (*r && isspace((unsigned char)*r)) r++;
            continue;
        }

        // Operador Stringify #param
        if (r[0] == '#' && isalpha((unsigned char)r[1])) {
            r++; // saltar '#'
            char p_name[64];
            int p_idx = 0;
            while ((isalnum((unsigned char)*r) || *r == '_') && p_idx < 63) {
                p_name[p_idx++] = *r++;
            }
            p_name[p_idx] = '\0';

            int found_arg = -1;
            for (int i = 0; i < m->param_count; i++) {
                if (strcmp(m->params[i], p_name) == 0) {
                    found_arg = i;
                    break;
                }
            }
            if (found_arg >= 0 && found_arg < arg_count) {
                char* s = stringify_arg(actual_args[found_arg]);
                buf_append_str(res, s);
                free(s);
            } else {
                buf_append_char(res, '#');
                buf_append_str(res, p_name);
            }
            continue;
        }

        // Identificador
        if (isalpha((unsigned char)*r) || *r == '_') {
            char id[64];
            int id_len = 0;
            while ((isalnum((unsigned char)*r) || *r == '_') && id_len < 63) {
                id[id_len++] = *r++;
            }
            id[id_len] = '\0';

            // Comprobar si coincide con un parámetro
            int found_arg = -1;
            for (int i = 0; i < m->param_count; i++) {
                if (strcmp(m->params[i], id) == 0) {
                    found_arg = i;
                    break;
                }
            }
            if (found_arg >= 0 && found_arg < arg_count) {
                buf_append_str(res, actual_args[found_arg]);
            } else if (m->is_variadic && strcmp(id, "__VA_ARGS__") == 0) {
                // Agregar argumentos variádicos separados por coma
                for (int i = m->param_count; i < arg_count; i++) {
                    if (i > m->param_count) buf_append_str(res, ", ");
                    buf_append_str(res, actual_args[i]);
                }
            } else {
                buf_append_str(res, id);
            }
            continue;
        }

        // Cadena o carácter literal en el reemplazo
        if (*r == '"' || *r == '\'') {
            char q = *r++;
            buf_append_char(res, q);
            while (*r != '\0') {
                char qc = *r++;
                buf_append_char(res, qc);
                if (qc == '\\' && *r != '\0') {
                    buf_append_char(res, *r++);
                } else if (qc == q) {
                    break;
                }
            }
            continue;
        }

        buf_append_char(res, *r++);
    }

    // Re-expandir recursivamente el resultado con protección contra bucles
    m->is_expanding = 1;
    char* final_expanded = expand_macros_in_text(res->data, cur_file, cur_line);
    m->is_expanding = 0;

    buf_free(res);
    return final_expanded;
}

// Expansión general de macros en una línea o bloque de texto con soporte de comentarios multilínea
static char* expand_macros_in_text_internal(const char* text, const char* cur_file, int cur_line, int* in_block_comment) {
    Buffer* out = buf_new(strlen(text) + 64);
    const char* p = text;

    while (*p != '\0') {
        // Si estamos dentro de un comentario de bloque multilínea abierto previamente
        if (in_block_comment && *in_block_comment) {
            if (p[0] == '*' && p[1] == '/') {
                buf_append_char(out, *p++);
                buf_append_char(out, *p++);
                *in_block_comment = 0;
            } else {
                buf_append_char(out, *p++);
            }
            continue;
        }

        // Preservar cadenas de texto
        if (*p == '"') {
            buf_append_char(out, *p++);
            while (*p != '\0') {
                char c = *p++;
                buf_append_char(out, c);
                if (c == '\\' && *p != '\0') {
                    buf_append_char(out, *p++);
                } else if (c == '"') {
                    break;
                }
            }
            continue;
        }

        // Preservar literales de caracter
        if (*p == '\'') {
            buf_append_char(out, *p++);
            while (*p != '\0') {
                char c = *p++;
                buf_append_char(out, c);
                if (c == '\\' && *p != '\0') {
                    buf_append_char(out, *p++);
                } else if (c == '\'') {
                    break;
                }
            }
            continue;
        }

        // Preservar comentarios de una sola línea
        if (p[0] == '/' && p[1] == '/') {
            while (*p != '\0' && *p != '\n') {
                buf_append_char(out, *p++);
            }
            continue;
        }

        // Preservar comentarios multilínea
        if (p[0] == '/' && p[1] == '*') {
            buf_append_char(out, *p++);
            buf_append_char(out, *p++);
            if (in_block_comment) *in_block_comment = 1;
            while (*p != '\0') {
                if (p[0] == '*' && p[1] == '/') {
                    buf_append_char(out, *p++);
                    buf_append_char(out, *p++);
                    if (in_block_comment) *in_block_comment = 0;
                    break;
                }
                buf_append_char(out, *p++);
            }
            continue;
        }

        // Identificador (posible macro)
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char* start = p;
            while (isalnum((unsigned char)*p) || *p == '_') {
                p++;
            }
            size_t id_len = p - start;
            char id[128];
            if (id_len >= 127) id_len = 127;
            strncpy(id, start, id_len);
            id[id_len] = '\0';

            // Macros dinámicas predefinidas
            if (strcmp(id, "__LINE__") == 0) {
                char num[32];
                sprintf(num, "%d", cur_line);
                buf_append_str(out, num);
                continue;
            }
            if (strcmp(id, "__FILE__") == 0) {
                char* qf = stringify_arg(cur_file ? cur_file : "unknown");
                buf_append_str(out, qf);
                free(qf);
                continue;
            }
            if (strcmp(id, "__DATE__") == 0) {
                time_t now = time(NULL);
                struct tm* t = localtime(&now);
                char d_buf[32];
                strftime(d_buf, sizeof(d_buf), "\"%b %d %Y\"", t);
                buf_append_str(out, d_buf);
                continue;
            }
            if (strcmp(id, "__TIME__") == 0) {
                time_t now = time(NULL);
                struct tm* t = localtime(&now);
                char t_buf[32];
                strftime(t_buf, sizeof(t_buf), "\"%H:%M:%S\"", t);
                buf_append_str(out, t_buf);
                continue;
            }

            Macro* m = find_macro(id);
            if (m != NULL && !m->is_expanding) {
                if (!m->is_function_like) {
                    // Macro de objeto
                    m->is_expanding = 1;
                    char* exp = expand_macros_in_text(m->replacement, cur_file, cur_line);
                    m->is_expanding = 0;
                    buf_append_str(out, exp);
                    free(exp);
                } else {
                    // Macro de función: comprobar si le sigue '('
                    const char* peek = p;
                    while (*peek && isspace((unsigned char)*peek) && *peek != '\n') peek++;
                    if (*peek == '(') {
                        // Recolectar toda la llamada hasta cerrar paréntesis
                        const char* call_start = peek;
                        int paren = 0;
                        int s_quote = 0, d_quote = 0;
                        while (*peek != '\0') {
                            if (!s_quote && !d_quote) {
                                if (*peek == '(') paren++;
                                else if (*peek == ')') {
                                    paren--;
                                    if (paren == 0) { peek++; break; }
                                } else if (*peek == '"') d_quote = 1;
                                else if (*peek == '\'') s_quote = 1;
                            } else if (d_quote) {
                                if (*peek == '\\' && *(peek + 1) != '\0') peek++;
                                else if (*peek == '"') d_quote = 0;
                            } else if (s_quote) {
                                if (*peek == '\\' && *(peek + 1) != '\0') peek++;
                                else if (*peek == '\'') s_quote = 0;
                            }
                            peek++;
                        }
                        size_t call_len = peek - call_start;
                        char* call_str = malloc(call_len + 1);
                        strncpy(call_str, call_start, call_len);
                        call_str[call_len] = '\0';

                        char* exp = expand_function_macro(m, call_str, cur_file, cur_line);
                        buf_append_str(out, exp);
                        free(exp);
                        free(call_str);
                        p = peek;
                    } else {
                        // No hay '(', no se expande
                        buf_append_str(out, id);
                    }
                }
            } else {
                buf_append_str(out, id);
            }
            continue;
        }

        buf_append_char(out, *p++);
    }

    char* res = strdup(out->data);
    buf_free(out);
    return res;
}

static char* expand_macros_in_text(const char* text, const char* cur_file, int cur_line) {
    int dummy = 0;
    return expand_macros_in_text_internal(text, cur_file, cur_line, &dummy);
}

typedef struct {
    const char* src;
    size_t pos;
    const char* cur_file;
    int cur_line;
} ExprParser;

static long long eval_expr_ternary(ExprParser* ep);

static void expr_skip_ws(ExprParser* ep) {
    while (ep->src[ep->pos] && isspace((unsigned char)ep->src[ep->pos])) ep->pos++;
}

static char expr_peek(ExprParser* ep) {
    expr_skip_ws(ep);
    return ep->src[ep->pos];
}

static long long eval_expr_primary(ExprParser* ep) {
    expr_skip_ws(ep);
    char c = ep->src[ep->pos];

    if (c == '(') {
        ep->pos++;
        long long val = eval_expr_ternary(ep);
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == ')') ep->pos++;
        return val;
    }

    // Literales numéricos
    if (isdigit((unsigned char)c)) {
        char* endp;
        long long val = strtoll(&ep->src[ep->pos], &endp, 0);
        ep->pos = endp - ep->src;
        return val;
    }

    // Caracter literal 'a'
    if (c == '\'') {
        ep->pos++;
        long long val = 0;
        if (ep->src[ep->pos] == '\\') {
            ep->pos++;
            char ec = ep->src[ep->pos++];
            if (ec == 'n') val = '\n';
            else if (ec == 't') val = '\t';
            else if (ec == 'r') val = '\r';
            else if (ec == '0') val = '\0';
            else val = ec;
        } else {
            val = (unsigned char)ep->src[ep->pos++];
        }
        if (ep->src[ep->pos] == '\'') ep->pos++;
        return val;
    }

    // Identificador o 'defined'
    if (isalpha((unsigned char)c) || c == '_') {
        size_t start = ep->pos;
        while (isalnum((unsigned char)ep->src[ep->pos]) || ep->src[ep->pos] == '_') ep->pos++;
        size_t len = ep->pos - start;
        char name[128];
        if (len >= 127) len = 127;
        strncpy(name, &ep->src[start], len);
        name[len] = '\0';

        if (strcmp(name, "defined") == 0) {
            expr_skip_ws(ep);
            int has_paren = (ep->src[ep->pos] == '(');
            if (has_paren) ep->pos++;
            expr_skip_ws(ep);
            size_t id_start = ep->pos;
            while (isalnum((unsigned char)ep->src[ep->pos]) || ep->src[ep->pos] == '_') ep->pos++;
            size_t id_len = ep->pos - id_start;
            char id[128];
            if (id_len >= 127) id_len = 127;
            strncpy(id, &ep->src[id_start], id_len);
            id[id_len] = '\0';
            expr_skip_ws(ep);
            if (has_paren && ep->src[ep->pos] == ')') ep->pos++;
            return (find_macro(id) != NULL) ? 1 : 0;
        }

        // Según estándar C, identificador no definido en #if evalúa a 0
        return 0;
    }

    return 0;
}

static long long eval_expr_unary(ExprParser* ep) {
    expr_skip_ws(ep);
    char c = ep->src[ep->pos];
    if (c == '+') {
        ep->pos++;
        return eval_expr_unary(ep);
    }
    if (c == '-') {
        ep->pos++;
        return -eval_expr_unary(ep);
    }
    if (c == '!') {
        ep->pos++;
        return !eval_expr_unary(ep);
    }
    if (c == '~') {
        ep->pos++;
        return ~eval_expr_unary(ep);
    }
    return eval_expr_primary(ep);
}

static long long eval_expr_mul(ExprParser* ep) {
    long long left = eval_expr_unary(ep);
    while (1) {
        char c = expr_peek(ep);
        if (c == '*' && ep->src[ep->pos + 1] != '=') {
            ep->pos++;
            left *= eval_expr_unary(ep);
        } else if (c == '/' && ep->src[ep->pos + 1] != '=') {
            ep->pos++;
            long long right = eval_expr_unary(ep);
            left = (right != 0) ? (left / right) : 0;
        } else if (c == '%' && ep->src[ep->pos + 1] != '=') {
            ep->pos++;
            long long right = eval_expr_unary(ep);
            left = (right != 0) ? (left % right) : 0;
        } else {
            break;
        }
    }
    return left;
}

static long long eval_expr_add(ExprParser* ep) {
    long long left = eval_expr_mul(ep);
    while (1) {
        char c = expr_peek(ep);
        if (c == '+' && ep->src[ep->pos + 1] != '+' && ep->src[ep->pos + 1] != '=') {
            ep->pos++;
            left += eval_expr_mul(ep);
        } else if (c == '-' && ep->src[ep->pos + 1] != '-' && ep->src[ep->pos + 1] != '=' && ep->src[ep->pos + 1] != '>') {
            ep->pos++;
            left -= eval_expr_mul(ep);
        } else {
            break;
        }
    }
    return left;
}

static long long eval_expr_shift(ExprParser* ep) {
    long long left = eval_expr_add(ep);
    while (1) {
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == '<' && ep->src[ep->pos + 1] == '<') {
            ep->pos += 2;
            left <<= eval_expr_add(ep);
        } else if (ep->src[ep->pos] == '>' && ep->src[ep->pos + 1] == '>') {
            ep->pos += 2;
            left >>= eval_expr_add(ep);
        } else {
            break;
        }
    }
    return left;
}

static long long eval_expr_relational(ExprParser* ep) {
    long long left = eval_expr_shift(ep);
    while (1) {
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == '<' && ep->src[ep->pos + 1] == '=') {
            ep->pos += 2;
            left = (left <= eval_expr_shift(ep));
        } else if (ep->src[ep->pos] == '>' && ep->src[ep->pos + 1] == '=') {
            ep->pos += 2;
            left = (left >= eval_expr_shift(ep));
        } else if (ep->src[ep->pos] == '<' && ep->src[ep->pos + 1] != '<') {
            ep->pos++;
            left = (left < eval_expr_shift(ep));
        } else if (ep->src[ep->pos] == '>' && ep->src[ep->pos + 1] != '>') {
            ep->pos++;
            left = (left > eval_expr_shift(ep));
        } else {
            break;
        }
    }
    return left;
}

static long long eval_expr_equality(ExprParser* ep) {
    long long left = eval_expr_relational(ep);
    while (1) {
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == '=' && ep->src[ep->pos + 1] == '=') {
            ep->pos += 2;
            left = (left == eval_expr_relational(ep));
        } else if (ep->src[ep->pos] == '!' && ep->src[ep->pos + 1] == '=') {
            ep->pos += 2;
            left = (left != eval_expr_relational(ep));
        } else {
            break;
        }
    }
    return left;
}

static long long eval_expr_bit_and(ExprParser* ep) {
    long long left = eval_expr_equality(ep);
    while (1) {
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == '&' && ep->src[ep->pos + 1] != '&') {
            ep->pos++;
            left &= eval_expr_equality(ep);
        } else break;
    }
    return left;
}

static long long eval_expr_bit_xor(ExprParser* ep) {
    long long left = eval_expr_bit_and(ep);
    while (1) {
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == '^') {
            ep->pos++;
            left ^= eval_expr_bit_and(ep);
        } else break;
    }
    return left;
}

static long long eval_expr_bit_or(ExprParser* ep) {
    long long left = eval_expr_bit_xor(ep);
    while (1) {
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == '|' && ep->src[ep->pos + 1] != '|') {
            ep->pos++;
            left |= eval_expr_bit_xor(ep);
        } else break;
    }
    return left;
}

static long long eval_expr_log_and(ExprParser* ep) {
    long long left = eval_expr_bit_or(ep);
    while (1) {
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == '&' && ep->src[ep->pos + 1] == '&') {
            ep->pos += 2;
            long long right = eval_expr_bit_or(ep);
            left = (left && right);
        } else break;
    }
    return left;
}

static long long eval_expr_log_or(ExprParser* ep) {
    long long left = eval_expr_log_and(ep);
    while (1) {
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == '|' && ep->src[ep->pos + 1] == '|') {
            ep->pos += 2;
            long long right = eval_expr_log_and(ep);
            left = (left || right);
        } else break;
    }
    return left;
}

static long long eval_expr_ternary(ExprParser* ep) {
    long long cond = eval_expr_log_or(ep);
    expr_skip_ws(ep);
    if (ep->src[ep->pos] == '?') {
        ep->pos++;
        long long true_val = eval_expr_ternary(ep);
        expr_skip_ws(ep);
        if (ep->src[ep->pos] == ':') {
            ep->pos++;
            long long false_val = eval_expr_ternary(ep);
            return cond ? true_val : false_val;
        }
        return true_val;
    }
    return cond;
}

// Evalúa una expresión de preprocesador como string
static int evaluate_condition_expression(const char* expr_str, const char* cur_file, int cur_line) {
    // 1. Expandir 'defined(NAME)' o 'defined NAME' primero
    Buffer* def_expanded = buf_new(strlen(expr_str) + 32);
    const char* p = expr_str;

    while (*p != '\0') {
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char* start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            size_t id_len = p - start;
            char id[64];
            if (id_len >= 63) id_len = 63;
            strncpy(id, start, id_len);
            id[id_len] = '\0';

            if (strcmp(id, "defined") == 0) {
                while (*p && isspace((unsigned char)*p)) p++;
                int has_paren = (*p == '(');
                if (has_paren) p++;
                while (*p && isspace((unsigned char)*p)) p++;
                const char* target_start = p;
                while (isalnum((unsigned char)*p) || *p == '_') p++;
                size_t t_len = p - target_start;
                char target[64];
                if (t_len >= 63) t_len = 63;
                strncpy(target, target_start, t_len);
                target[t_len] = '\0';
                while (*p && isspace((unsigned char)*p)) p++;
                if (has_paren && *p == ')') p++;

                int is_def = (find_macro(target) != NULL);
                buf_append_str(def_expanded, is_def ? "1" : "0");
                continue;
            }
            buf_append_str(def_expanded, id);
            continue;
        }
        buf_append_char(def_expanded, *p++);
    }

    // 2. Expandir el resto de las macros
    char* macro_expanded = expand_macros_in_text(def_expanded->data, cur_file, cur_line);
    buf_free(def_expanded);

    // 3. Evaluar la expresión aritmética
    ExprParser ep;
    ep.src = macro_expanded;
    ep.pos = 0;
    ep.cur_file = cur_file;
    ep.cur_line = cur_line;
    long long res = eval_expr_ternary(&ep);

    free(macro_expanded);
    return (res != 0);
}

static char* read_file_content(const char* filepath) {
    if (strncmp(filepath, "embedded:", 9) == 0) {
        const char* em = get_embedded_file(filepath + 9, NULL);
        if (em) return strdup(em);
    }
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        const char* em = get_embedded_file(filepath, NULL);
        if (em) return strdup(em);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static int try_resolve_file(const char* base_dir, const char* name, int is_system_include, char* out_path) {
    if (!name || name[0] == '\0') return 0;

    int has_ext = (strchr(name, '.') != NULL);

    if (is_system_include) {
        // 1. Directivas <...>: buscar primero en librerias estandar integradas (embebidas)
        if (has_embedded_file(name)) {
            snprintf(out_path, 512, "embedded:%.490s", name);
            return 1;
        }
        char with_ext[256];
        snprintf(with_ext, sizeof(with_ext), "%.240s.huki", name);
        if (has_embedded_file(with_ext)) {
            snprintf(out_path, 512, "embedded:%.490s", with_ext);
            return 1;
        }
        snprintf(with_ext, sizeof(with_ext), "%.240s.suki", name);
        if (has_embedded_file(with_ext)) {
            snprintf(out_path, 512, "embedded:%.490s", with_ext);
            return 1;
        }
        snprintf(with_ext, sizeof(with_ext), "%.240s.hu", name);
        if (has_embedded_file(with_ext)) {
            snprintf(out_path, 512, "embedded:%.490s", with_ext);
            return 1;
        }
        snprintf(with_ext, sizeof(with_ext), "%.240s.su", name);
        if (has_embedded_file(with_ext)) {
            snprintf(out_path, 512, "embedded:%.490s", with_ext);
            return 1;
        }

        // 2. Buscar en paquetes globales instalados en el sistema (~/.itsuki/packages/)
        if (pkg_resolve_include(name, 1, out_path, 512)) {
            return 1;
        }

        // 3. Buscar en directorios lib/ (local, ~/.itsuki/lib, exe_dir/lib, exe_dir/../lib)
        char cand_lib[2048];
        char base_search_dirs[4][512];
        int search_dir_count = 0;
        
        strcpy(base_search_dirs[search_dir_count++], "lib");
        
#ifdef _WIN32
        const char* u_home = getenv("USERPROFILE");
        if (!u_home) u_home = getenv("APPDATA");
        if (u_home) {
            snprintf(base_search_dirs[search_dir_count++], 512, "%s\\.itsuki\\lib", u_home);
        }
        char exe_p[512] = {0};
        extern unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
        if (GetModuleFileNameA(NULL, exe_p, sizeof(exe_p)) > 0) {
            char* ls = strrchr(exe_p, '\\');
            if (!ls) ls = strrchr(exe_p, '/');
            if (ls) {
                *ls = '\0';
                snprintf(base_search_dirs[search_dir_count++], 512, "%s\\lib", exe_p);
                snprintf(base_search_dirs[search_dir_count++], 512, "%s\\..\\lib", exe_p);
            }
        }
#else
        const char* u_home = getenv("HOME");
        if (u_home) {
            snprintf(base_search_dirs[search_dir_count++], 512, "%s/.itsuki/lib", u_home);
        }
#endif

        for (int d = 0; d < search_dir_count; d++) {
            if (has_ext) {
                snprintf(cand_lib, sizeof(cand_lib), "%s/%s", base_search_dirs[d], name);
                FILE* test = fopen(cand_lib, "rb");
                if (test != NULL) {
                    fclose(test);
                    normalize_path(cand_lib, out_path, 512);
                    return 1;
                }
            } else {
                const char* lib_exts[] = {".huki", ".suki", ".hu", ".su", NULL};
                for (int e = 0; lib_exts[e] != NULL; e++) {
                    snprintf(cand_lib, sizeof(cand_lib), "%s/%s%s", base_search_dirs[d], name, lib_exts[e]);
                    FILE* test = fopen(cand_lib, "rb");
                    if (test != NULL) {
                        fclose(test);
                        normalize_path(cand_lib, out_path, 512);
                        return 1;
                    }
                }
            }
        }

        // NO buscar en el directorio local del usuario
        return 0;
    }

    // Directivas con comillas: #include "archivo.huki"
    // Busca en la ruta local del archivo o ruta especificada
    char candidates[16][1024];
    int cand_count = 0;
    if (has_ext) {
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s/%s", base_dir, name);
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s", name);
    } else {
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s/%s.huki", base_dir, name);
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s/%s.suki", base_dir, name);
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s/%s.hu", base_dir, name);
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s/%s.su", base_dir, name);
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s.huki", name);
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s.suki", name);
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s.hu", name);
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s.su", name);
        snprintf(candidates[cand_count++], sizeof(candidates[0]), "%s", name);
    }
    for (int i = 0; i < cand_count; i++) {
        FILE* test = fopen(candidates[i], "rb");
        if (test != NULL) {
            fclose(test);
            normalize_path(candidates[i], out_path, 512);
            return 1;
        }
    }

    // Fallback si no se encontro localmente: verificar paquetes instalados o embebidos
    if (pkg_resolve_include(name, 0, out_path, 512)) {
        return 1;
    }
    if (has_embedded_file(name)) {
        snprintf(out_path, 512, "embedded:%.490s", name);
        return 1;
    }

    return 0;
}

static char* find_directive_start(char* line, int* in_block_comment) {
    char* p = line;
    while (*p != '\0') {
        if (*in_block_comment) {
            if (p[0] == '*' && p[1] == '/') {
                *in_block_comment = 0;
                p += 2;
            } else {
                p++;
            }
            continue;
        }
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '/') {
            return NULL;
        }
        if (p[0] == '/' && p[1] == '*') {
            *in_block_comment = 1;
            p += 2;
            continue;
        }
        if (*p == '#') {
            return p;
        }
        return NULL;
    }
    return NULL;
}

static char* clean_directive_comments(const char* str, int* in_block_comment) {
    Buffer* b = buf_new(strlen(str) + 16);
    const char* p = str;
    while (*p != '\0') {
        if (*in_block_comment) {
            if (p[0] == '*' && p[1] == '/') {
                *in_block_comment = 0;
                p += 2;
                buf_append_char(b, ' ');
            } else {
                p++;
            }
            continue;
        }
        if (*p == '"') {
            buf_append_char(b, *p++);
            while (*p != '\0') {
                char c = *p++;
                buf_append_char(b, c);
                if (c == '\\' && *p != '\0') {
                    buf_append_char(b, *p++);
                } else if (c == '"') {
                    break;
                }
            }
            continue;
        }
        if (*p == '\'') {
            buf_append_char(b, *p++);
            while (*p != '\0') {
                char c = *p++;
                buf_append_char(b, c);
                if (c == '\\' && *p != '\0') {
                    buf_append_char(b, *p++);
                } else if (c == '\'') {
                    break;
                }
            }
            continue;
        }
        if (p[0] == '/' && p[1] == '/') {
            break;
        }
        if (p[0] == '/' && p[1] == '*') {
            *in_block_comment = 1;
            p += 2;
            continue;
        }
        buf_append_char(b, *p++);
    }
    while (b->length > 0 && isspace((unsigned char)b->data[b->length - 1])) {
        b->length--;
        b->data[b->length] = '\0';
    }
    char* res = strdup(b->data);
    buf_free(b);
    return res;
}

static void preprocess_internal(const char* source, const char* filepath, Buffer* out) {
    if (current_include_depth >= MAX_INCLUDE_DEPTH) {
        printf("[Itsuki Error] Profundidad maxima de #include excedida (%d) en '%s'\n", MAX_INCLUDE_DEPTH, filepath);
        exit(1);
    }
    current_include_depth++;

    char cur_dir[512];
    get_directory(filepath, cur_dir, sizeof(cur_dir));

    const char* p = source;
    if (p && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
        p += 3;
    }
    int line_number = 1;
    int in_block_comment = 0;

    while (*p != '\0') {
        // Unir líneas continuadas con backslash '\'
        Buffer* line_buf = buf_new(128);
        while (*p != '\0') {
            if (*p == '\\') {
                const char* peek = p + 1;
                while (*peek && isspace((unsigned char)*peek) && *peek != '\n') peek++;
                if (*peek == '\n') {
                    p = peek + 1;
                    line_number++;
                    continue;
                } else if (*peek == '\r' && *(peek + 1) == '\n') {
                    p = peek + 2;
                    line_number++;
                    continue;
                }
            }
            if (*p == '\n') {
                p++;
                break;
            } else if (*p == '\r') {
                p++;
                if (*p == '\n') p++;
                break;
            }
            buf_append_char(line_buf, *p++);
        }

        char* line = line_buf->data;

        // Comprobar si la línea es una directiva de preprocesador
        int test_comment = in_block_comment;
        char* dir_hash = find_directive_start(line, &test_comment);

        if (dir_hash != NULL) {
            in_block_comment = test_comment; // Actualizar estado de comentario
            char* trimmed = dir_hash + 1; // saltar '#'
            while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;

            char directive[64];
            int d_idx = 0;
            while (isalpha((unsigned char)*trimmed) && d_idx < 63) {
                directive[d_idx++] = *trimmed++;
            }
            directive[d_idx] = '\0';
            while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;

            // 1. #ifdef
            if (strcmp(directive, "ifdef") == 0) {
                char* clean_args = clean_directive_comments(trimmed, &in_block_comment);
                char name[128] = {0};
                sscanf(clean_args, "%127s", name);
                free(clean_args);
                int parent_act = is_emitting();
                int cond = parent_act && (find_macro(name) != NULL);
                if (if_depth < MAX_IF_DEPTH) {
                    if_stack[if_depth].condition_met = cond;
                    if_stack[if_depth].currently_active = cond;
                    if_stack[if_depth].parent_active = parent_act;
                    if_stack[if_depth].line = line_number;
                    strncpy(if_stack[if_depth].file, filepath, 255);
                    if_depth++;
                }
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 2. #ifndef
            if (strcmp(directive, "ifndef") == 0) {
                char* clean_args = clean_directive_comments(trimmed, &in_block_comment);
                char name[128] = {0};
                sscanf(clean_args, "%127s", name);
                free(clean_args);
                int parent_act = is_emitting();
                int cond = parent_act && (find_macro(name) == NULL);
                if (if_depth < MAX_IF_DEPTH) {
                    if_stack[if_depth].condition_met = cond;
                    if_stack[if_depth].currently_active = cond;
                    if_stack[if_depth].parent_active = parent_act;
                    if_stack[if_depth].line = line_number;
                    strncpy(if_stack[if_depth].file, filepath, 255);
                    if_depth++;
                }
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 3. #if
            if (strcmp(directive, "if") == 0) {
                char* clean_args = clean_directive_comments(trimmed, &in_block_comment);
                int parent_act = is_emitting();
                int cond = parent_act && evaluate_condition_expression(clean_args, filepath, line_number);
                free(clean_args);
                if (if_depth < MAX_IF_DEPTH) {
                    if_stack[if_depth].condition_met = cond;
                    if_stack[if_depth].currently_active = cond;
                    if_stack[if_depth].parent_active = parent_act;
                    if_stack[if_depth].line = line_number;
                    strncpy(if_stack[if_depth].file, filepath, 255);
                    if_depth++;
                }
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 4. #elif
            if (strcmp(directive, "elif") == 0) {
                char* clean_args = clean_directive_comments(trimmed, &in_block_comment);
                if (if_depth > 0) {
                    IfStackEntry* top = &if_stack[if_depth - 1];
                    if (top->parent_active && !top->condition_met) {
                        int cond = evaluate_condition_expression(clean_args, filepath, line_number);
                        if (cond) {
                            top->condition_met = 1;
                            top->currently_active = 1;
                        } else {
                            top->currently_active = 0;
                        }
                    } else {
                        top->currently_active = 0;
                    }
                } else {
                    printf("[Itsuki Error] #elif sin #if correspondiente en '%s':%d\n", filepath, line_number);
                    exit(1);
                }
                free(clean_args);
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 5. #else
            if (strcmp(directive, "else") == 0) {
                if (if_depth > 0) {
                    IfStackEntry* top = &if_stack[if_depth - 1];
                    if (top->parent_active && !top->condition_met) {
                        top->condition_met = 1;
                        top->currently_active = 1;
                    } else {
                        top->currently_active = 0;
                    }
                } else {
                    printf("[Itsuki Error] #else sin #if correspondiente en '%s':%d\n", filepath, line_number);
                    exit(1);
                }
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 6. #endif
            if (strcmp(directive, "endif") == 0) {
                if (if_depth > 0) {
                    if_depth--;
                } else {
                    printf("[Itsuki Error] #endif sin #if correspondiente en '%s':%d\n", filepath, line_number);
                    exit(1);
                }
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // Si la rama condicional actual está desactivada, omitir directivas de acción
            if (!is_emitting()) {
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 7. #define
            if (strcmp(directive, "define") == 0) {
                char macro_name[128];
                int m_idx = 0;
                while ((isalnum((unsigned char)*trimmed) || *trimmed == '_') && m_idx < 127) {
                    macro_name[m_idx++] = *trimmed++;
                }
                macro_name[m_idx] = '\0';

                if (*trimmed == '(') {
                    // Macro tipo función: #define NAME(a, b, ...) replacement
                    trimmed++; // saltar '('
                    char params[MAX_MACRO_PARAMS][64];
                    int param_count = 0;
                    int is_var = 0;

                    while (*trimmed && *trimmed != ')') {
                        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
                        if (trimmed[0] == '.' && trimmed[1] == '.' && trimmed[2] == '.') {
                            is_var = 1;
                            trimmed += 3;
                            while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
                            break;
                        }
                        char pname[64];
                        int p_idx = 0;
                        while ((isalnum((unsigned char)*trimmed) || *trimmed == '_') && p_idx < 63) {
                            pname[p_idx++] = *trimmed++;
                        }
                        pname[p_idx] = '\0';
                        if (p_idx > 0 && param_count < MAX_MACRO_PARAMS) {
                            strcpy(params[param_count++], pname);
                        }
                        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
                        if (*trimmed == ',') trimmed++;
                    }
                    if (*trimmed == ')') trimmed++;
                    while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;

                    char* clean_body = clean_directive_comments(trimmed, &in_block_comment);
                    define_function_macro(macro_name, param_count, params, is_var, clean_body);
                    free(clean_body);
                } else {
                    // Macro de objeto: #define NAME replacement
                    while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
                    char* clean_val = clean_directive_comments(trimmed, &in_block_comment);
                    define_macro_internal(macro_name, clean_val, 0);
                    free(clean_val);
                }
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 8. #undef
            if (strcmp(directive, "undef") == 0) {
                char* clean_args = clean_directive_comments(trimmed, &in_block_comment);
                char macro_name[128] = {0};
                sscanf(clean_args, "%127s", macro_name);
                free(clean_args);
                preprocessor_undef(macro_name);
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 9. #include y #import
            if (strcmp(directive, "include") == 0 || strcmp(directive, "import") == 0) {
                char* clean_args = clean_directive_comments(trimmed, &in_block_comment);
                int is_import_directive = (strcmp(directive, "import") == 0);
                char inc_name[256] = {0};
                char* cur_t = clean_args;
                while (*cur_t && isspace((unsigned char)*cur_t)) cur_t++;

                int is_system_include = 0;
                if (*cur_t == '<') {
                    is_system_include = 1;
                    cur_t++;
                    char* close_angle = strchr(cur_t, '>');
                    if (close_angle) {
                        size_t l = close_angle - cur_t;
                        if (l >= 255) l = 255;
                        strncpy(inc_name, cur_t, l);
                        inc_name[l] = '\0';
                    }
                } else if (*cur_t == '"') {
                    is_system_include = 0;
                    cur_t++;
                    char* close_quote = strchr(cur_t, '"');
                    if (close_quote) {
                        size_t l = close_quote - cur_t;
                        if (l >= 255) l = 255;
                        strncpy(inc_name, cur_t, l);
                        inc_name[l] = '\0';
                    }
                } else {
                    // Posible macro que se expande a la ruta
                    char* exp = expand_macros_in_text(cur_t, filepath, line_number);
                    char* et = exp;
                    while (*et && isspace((unsigned char)*et)) et++;
                    if (*et == '<' || *et == '"') {
                        is_system_include = (*et == '<');
                        char delim = (*et == '<') ? '>' : '"';
                        et++;
                        char* cl = strchr(et, delim);
                        if (cl) {
                            size_t l = cl - et;
                            if (l >= 255) l = 255;
                            strncpy(inc_name, et, l);
                            inc_name[l] = '\0';
                        }
                    } else {
                        sscanf(et, "%255s", inc_name);
                    }
                    free(exp);
                }
                free(clean_args);

                if (inc_name[0] != '\0') {
                    char resolved_path[512] = {0};
                    if (try_resolve_file(cur_dir, inc_name, is_system_include, resolved_path)) {
                        // Descubrimiento de fuentes compañeros (.huki -> .suki)
                        if (strstr(resolved_path, ".huki") != NULL || strstr(resolved_path, ".hu") != NULL) {
                            char comp_path[512];
                            if (pkg_find_companion_source(resolved_path, comp_path, sizeof(comp_path))) {
                                add_discovered_source(comp_path);
                            } else {
                                strcpy(comp_path, resolved_path);
                                char* huki_ptr = strstr(comp_path, ".huki");
                                if (huki_ptr) {
                                    strcpy(huki_ptr, ".suki");
                                } else {
                                    char* hu_ptr = strstr(comp_path, ".hu");
                                    if (hu_ptr) strcpy(hu_ptr, ".suki");
                                }
                                FILE* cf = fopen(comp_path, "rb");
                                if (cf) {
                                    fclose(cf);
                                    add_discovered_source(comp_path);
                                } else {
                                    const char* emb_name = comp_path + (strncmp(comp_path, "embedded:", 9) == 0 ? 9 : 0);
                                    if (has_embedded_file(emb_name)) {
                                        char emb_comp[512];
                                        snprintf(emb_comp, sizeof(emb_comp), "embedded:%s", emb_name);
                                        add_discovered_source(emb_comp);
                                    }
                                }
                            }
                        }

                        // Verificar #pragma once o semántica #import
                        if (is_import_directive) {
                            if (is_pragma_once(resolved_path)) {
                                buf_append_char(out, '\n');
                                line_number++;
                                buf_free(line_buf);
                                continue;
                            }
                            mark_pragma_once(resolved_path);
                        } else if (is_pragma_once(resolved_path)) {
                            buf_append_char(out, '\n');
                            line_number++;
                            buf_free(line_buf);
                            continue;
                        }

                        char* inc_content = read_file_content(resolved_path);
                        if (inc_content != NULL) {
                            // Emitir directiva #line para el archivo incluido
                            char line_dir[1024];
                            snprintf(line_dir, sizeof(line_dir), "#line 1 \"%s\"\n", resolved_path);
                            buf_append_str(out, line_dir);

                            // Preprocesar recursivamente el archivo incluido
                            preprocess_internal(inc_content, resolved_path, out);
                            free(inc_content);

                            // Restaurar #line para el archivo actual
                            snprintf(line_dir, sizeof(line_dir), "\n#line %d \"%s\"\n", line_number + 1, filepath);
                            buf_append_str(out, line_dir);
                        }
                    } else {
                        printf("[Itsuki Error] No se pudo encontrar el archivo de inclusion '%s' en '%s':%d\n",
                               inc_name, filepath, line_number);
                        exit(1);
                    }
                }
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 10. #pragma
            if (strcmp(directive, "pragma") == 0) {
                char* clean_args = clean_directive_comments(trimmed, &in_block_comment);
                char* pt = clean_args;
                while (*pt && isspace((unsigned char)*pt)) pt++;
                if (strncmp(pt, "once", 4) == 0) {
                    mark_pragma_once(filepath);
                }
                free(clean_args);
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 11. #error
            if (strcmp(directive, "error") == 0) {
                char* clean_args = clean_directive_comments(trimmed, &in_block_comment);
                printf("[Itsuki Error de Preprocesador] %s (%s:%d)\n", clean_args, filepath, line_number);
                free(clean_args);
                exit(1);
            }

            // 12. #warning
            if (strcmp(directive, "warning") == 0) {
                char* clean_args = clean_directive_comments(trimmed, &in_block_comment);
                printf("[Itsuki Aviso de Preprocesador] %s (%s:%d)\n", clean_args, filepath, line_number);
                free(clean_args);
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }

            // 13. #line
            if (strcmp(directive, "line") == 0) {
                // Pasar la directiva #line tal cual al lexer
                buf_append_str(out, line);
                buf_append_char(out, '\n');
                line_number++;
                buf_free(line_buf);
                continue;
            }
        }

        // Si la rama condicional actual está activa, expandir macros y emitir línea
        if (is_emitting()) {
            char* expanded = expand_macros_in_text_internal(line, filepath, line_number, &in_block_comment);
            buf_append_str(out, expanded);
            free(expanded);
            buf_append_char(out, '\n');
        } else {
            // Línea inactiva: actualizar in_block_comment si la línea tenía comentarios
            const char* lp = line;
            while (*lp) {
                if (in_block_comment) {
                    if (lp[0] == '*' && lp[1] == '/') { in_block_comment = 0; lp += 2; }
                    else lp++;
                } else {
                    if (lp[0] == '/' && lp[1] == '/') break;
                    if (lp[0] == '/' && lp[1] == '*') { in_block_comment = 1; lp += 2; }
                    else lp++;
                }
            }
            buf_append_char(out, '\n');
        }

        line_number++;
        buf_free(line_buf);
    }

    current_include_depth--;
}

char* preprocess_source(const char* source, const char* filepath) {
    if (!source) return NULL;

    // Asegurar macros base
    if (find_macro("__ITSUKI__") == NULL) {
        preprocessor_define("__ITSUKI__", "1");
        preprocessor_define("__ITSUKI_VERSION__", "2");
    }

    Buffer* out = buf_new(strlen(source) + 256);
    preprocess_internal(source, filepath ? filepath : "main.suki", out);

    if (if_depth > 0) {
        printf("[Itsuki Error] Directiva condicional '#if' sin cerrar en '%s':%d\n",
               if_stack[if_depth - 1].file, if_stack[if_depth - 1].line);
        exit(1);
    }

    char* result = strdup(out->data);
    buf_free(out);
    return result;
}

char* preprocess_file(const char* filepath) {
    char* content = read_file_content(filepath);
    if (!content) return NULL;
    char* preprocessed = preprocess_source(content, filepath);
    free(content);
    return preprocessed;
}
