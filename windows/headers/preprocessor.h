#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

// Preprocesa una cadena de código fuente de Itsuki.
// 'source': Cadena con el código fuente original.
// 'filepath': Ruta del archivo que se está procesando (para resolución relativa y __FILE__).
// Retorna: Una nueva cadena con el código preprocesado (debe liberarse con free()).
char* preprocess_source(const char* source, const char* filepath);

// Preprocesa un archivo directamente a partir de su ruta en el sistema de archivos.
char* preprocess_file(const char* filepath);

// Define una macro globalmente (por ejemplo -DNAME=VAL o macros del compilador).
void preprocessor_define(const char* name, const char* value);

// Elimina una macro globalmente.
void preprocessor_undef(const char* name);

// Reinicia el estado del preprocesador para una nueva unidad de traducción (conservando macros predefinidas/línea de comandos).
void preprocessor_reset_unit(void);

// Reinicia el estado del preprocesador completamente (macros, guardas de inclusión, etc.).
void preprocessor_reset(void);

#ifdef __cplusplus
}
#endif

#endif // PREPROCESSOR_H
