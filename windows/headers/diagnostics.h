#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

typedef enum {
    DIAG_NOTE = 0,
    DIAG_HELP = 1,
    DIAG_WARNING = 2,
    DIAG_ERROR = 3
} DiagSeverity;

void diag_init(void);
void diag_set_colors_enabled(int enabled);
int diag_get_colors_enabled(void);

void diag_report(DiagSeverity severity,
                 const char* filename,
                 const char* source_code,
                 int line,
                 int column,
                 int token_len,
                 const char* message,
                 const char* suggestion);

void diag_error(const char* filename,
                const char* source_code,
                int line,
                int column,
                int token_len,
                const char* message,
                const char* suggestion);

void diag_warning(const char* filename,
                  const char* source_code,
                  int line,
                  int column,
                  int token_len,
                  const char* message);

#endif // DIAGNOSTICS_H
