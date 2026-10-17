#ifndef ARCH_CONFIG_H
#define ARCH_CONFIG_H

#include <stddef.h>

typedef struct {
    char os_name[32];          // "windows" o "linux"
    int os_bits;               // 64 o 32
    char host_arch[32];        // "x86_64", "x86", "arm64", etc.
    char gcc_path[512];        // Comando o ruta al ejecutable gcc ("gcc")
    char gcc_arch[64];         // Arquitectura reportada por gcc (ej: "x86_64-w64-mingw32", "mingw32")
    int gcc_bits;              // 64 o 32
    int target_bits;           // 64 o 32 (arquitectura objetivo actual)
    int multilib_supported;    // 1 si soporta compilacion cruzada/enlace a 32 bits (-m32), 0 si no
    char gcc_flags[256];       // Flags especificos para enlazado en esta maquina (ej: "-m32", "-no-pie")
    int is_loaded_from_file;   // 1 si se leyo desde arch.conf, 0 si fue detectado en tiempo real
    char config_file_path[512];// Ruta del archivo arch.conf si existe
} ArchConfig;

// Inicializa el sistema de arquitectura (busca arch.conf o autodetecta el host y gcc)
void arch_init(void);

// Devuelve un puntero a la configuracion de arquitectura activa
const ArchConfig* arch_get_current(void);

// Configura y obtiene la arquitectura objetivo (64 o 32 bits)
void arch_set_target_bits(int bits);
int arch_get_target_bits(void);

// Muestra el informe detallado de arquitectura ("itsuki doctor" o "itsuki info")
void arch_print_diagnostics(void);

// Imprime consejos y solucion de problemas si el enlazador gcc falla
void arch_print_linker_troubleshooting(void);

// Guarda la configuracion en un archivo arch.conf
int arch_save_config(const char* filepath, const ArchConfig* cfg);

#endif // ARCH_CONFIG_H
