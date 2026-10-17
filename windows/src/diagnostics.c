#include "diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#ifndef isatty
#define isatty _isatty
#endif
#ifndef fileno
#define fileno _fileno
#endif
#else
#include <unistd.h>
#endif

static int colors_enabled = 1;

void diag_init(void) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
    if (!isatty(fileno(stdout)) && !isatty(fileno(stderr))) {
        colors_enabled = 0;
    }
}

void diag_set_colors_enabled(int enabled) {
    colors_enabled = enabled;
}

int diag_get_colors_enabled(void) {
    return colors_enabled;
}

// Extrae una linea especifica (1-based) de la cadena de codigo fuente
static void extract_source_line(const char* source, int target_line, char* out_buf, size_t buf_size) {
    if (!out_buf || buf_size == 0) return;
    out_buf[0] = '\0';
    if (!source) return;

    int current_line = 1;
    const char* p = source;
    while (*p && current_line < target_line) {
        if (*p == '\n') current_line++;
        p++;
    }

    if (current_line != target_line) return;

    size_t idx = 0;
    while (*p && *p != '\n' && *p != '\r' && idx + 1 < buf_size) {
        if (*p == '\t') {
            // Expandir tab a 4 espacios para alineacion consistente
            for (int k = 0; k < 4 && idx + 1 < buf_size; k++) {
                out_buf[idx++] = ' ';
            }
            p++;
        } else {
            out_buf[idx++] = *p++;
        }
    }
    out_buf[idx] = '\0';
}

void diag_report(DiagSeverity severity,
                 const char* filename,
                 const char* source_code,
                 int line,
                 int column,
                 int token_len,
                 const char* message,
                 const char* suggestion) {
    const char* c_reset = colors_enabled ? "\033[0m" : "";
    const char* c_bold  = colors_enabled ? "\033[1m" : "";
    const char* c_blue  = colors_enabled ? "\033[1;34m" : "";
    const char* c_cyan  = colors_enabled ? "\033[1;36m" : "";
    const char* c_red   = colors_enabled ? "\033[1;31m" : "";
    const char* c_yellow= colors_enabled ? "\033[1;33m" : "";

    const char* label = "error";
    const char* label_color = c_red;
    if (severity == DIAG_WARNING) {
        label = "advertencia";
        label_color = c_yellow;
    } else if (severity == DIAG_HELP || severity == DIAG_NOTE) {
        label = "nota";
        label_color = c_cyan;
    }

    // Cabecera: error: <mensaje>
    fprintf(stderr, "\n%s%s:%s %s%s%s\n", label_color, label, c_reset, c_bold, message ? message : "", c_reset);

    // Ubicacion: --> archivo:linea:columna
    const char* file_str = (filename && strlen(filename) > 0) ? filename : "fuente";
    fprintf(stderr, "  %s-->%s %s:%d:%d\n", c_blue, c_reset, file_str, line > 0 ? line : 1, column > 0 ? column : 1);

    // Si hay codigo fuente o archivo disponible, mostrar el bloque contextual
    if (line > 0) {
        char line_buf[1024];
        line_buf[0] = '\0';
        if (filename && strlen(filename) > 0) {
            FILE* f = fopen(filename, "r");
            if (f) {
                int cur_l = 1;
                char raw[1024];
                while (fgets(raw, sizeof(raw), f)) {
                    if (cur_l == line) {
                        size_t rlen = strlen(raw);
                        while (rlen > 0 && (raw[rlen-1] == '\n' || raw[rlen-1] == '\r')) {
                            raw[--rlen] = '\0';
                        }
                        size_t o_idx = 0;
                        for (size_t k = 0; k < rlen && o_idx + 1 < sizeof(line_buf); k++) {
                            if (raw[k] == '\t') {
                                for (int sp = 0; sp < 4 && o_idx + 1 < sizeof(line_buf); sp++) {
                                    line_buf[o_idx++] = ' ';
                                }
                            } else {
                                line_buf[o_idx++] = raw[k];
                            }
                        }
                        line_buf[o_idx] = '\0';
                        break;
                    }
                    cur_l++;
                }
                fclose(f);
            }
        }
        if (line_buf[0] == '\0' && source_code) {
            extract_source_line(source_code, line, line_buf, sizeof(line_buf));
        }

        if (line_buf[0] != '\0') {
            // Margen superior
            fprintf(stderr, "   %s|%s\n", c_blue, c_reset);

            // Linea de codigo fuente
            fprintf(stderr, "%s%4d |%s %s\n", c_blue, line, c_reset, line_buf);

            // Indicador de error (subrayado con ^~~~~)
            int col_indent = (column > 1) ? column - 1 : 0;
            int underline_len = (token_len > 1) ? token_len : 1;
            if (underline_len > 80) underline_len = 80;

            fprintf(stderr, "   %s|%s ", c_blue, c_reset);
            for (int i = 0; i < col_indent; i++) {
                fprintf(stderr, " ");
            }

            fprintf(stderr, "%s^", label_color);
            for (int i = 1; i < underline_len; i++) {
                fprintf(stderr, "~");
            }
            fprintf(stderr, "%s\n", c_reset);

            // Margen inferior
            fprintf(stderr, "   %s|%s\n", c_blue, c_reset);
        }
    }

    // Sugerencia / Ayuda adicional
    if (suggestion && strlen(suggestion) > 0) {
        fprintf(stderr, "  %s= ayuda:%s %s\n", c_cyan, c_reset, suggestion);
    }
    fprintf(stderr, "\n");
}

void diag_error(const char* filename,
                const char* source_code,
                int line,
                int column,
                int token_len,
                const char* message,
                const char* suggestion) {
    diag_report(DIAG_ERROR, filename, source_code, line, column, token_len, message, suggestion);
}

void diag_warning(const char* filename,
                  const char* source_code,
                  int line,
                  int column,
                  int token_len,
                  const char* message) {
    diag_report(DIAG_WARNING, filename, source_code, line, column, token_len, message, NULL);
}
