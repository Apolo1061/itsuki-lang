#include "arch_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>
#include <io.h>
#define popen _popen
#define pclose _pclose

#ifndef PROCESSOR_ARCHITECTURE_AMD64
#define PROCESSOR_ARCHITECTURE_AMD64 9
#endif
#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif
#ifndef PROCESSOR_ARCHITECTURE_INTEL
#define PROCESSOR_ARCHITECTURE_INTEL 0
#endif
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

static ArchConfig active_config;
static int is_initialized = 0;

static void trim_whitespace(char* s) {
    if (!s) return;
    char* start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static int file_exists(const char* path) {
    if (!path || path[0] == '\0') return 0;
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

static void get_default_config_path(char* out, size_t max_len) {
#ifdef _WIN32
    const char* user = getenv("USERPROFILE");
    if (!user) user = getenv("APPDATA");
    if (!user) user = ".";
    snprintf(out, max_len, "%s\\.itsuki\\arch.conf", user);
#else
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(out, max_len, "%s/.itsuki/arch.conf", home);
#endif
}

static int parse_config_file(const char* filepath, ArchConfig* cfg) {
    FILE* f = fopen(filepath, "r");
    if (!f) return 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim_whitespace(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') continue;

        char* eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char* key = line;
        char* val = eq + 1;
        trim_whitespace(key);
        trim_whitespace(val);

        if (strcmp(key, "os_name") == 0) {
            strncpy(cfg->os_name, val, sizeof(cfg->os_name) - 1);
            cfg->os_name[sizeof(cfg->os_name) - 1] = '\0';
        } else if (strcmp(key, "os_bits") == 0) {
            cfg->os_bits = atoi(val);
        } else if (strcmp(key, "host_arch") == 0) {
            strncpy(cfg->host_arch, val, sizeof(cfg->host_arch) - 1);
            cfg->host_arch[sizeof(cfg->host_arch) - 1] = '\0';
        } else if (strcmp(key, "gcc_path") == 0) {
            strncpy(cfg->gcc_path, val, sizeof(cfg->gcc_path) - 1);
            cfg->gcc_path[sizeof(cfg->gcc_path) - 1] = '\0';
        } else if (strcmp(key, "gcc_arch") == 0) {
            strncpy(cfg->gcc_arch, val, sizeof(cfg->gcc_arch) - 1);
            cfg->gcc_arch[sizeof(cfg->gcc_arch) - 1] = '\0';
        } else if (strcmp(key, "gcc_bits") == 0) {
            cfg->gcc_bits = atoi(val);
        } else if (strcmp(key, "multilib_supported") == 0) {
            cfg->multilib_supported = atoi(val);
        } else if (strcmp(key, "gcc_flags") == 0) {
            strncpy(cfg->gcc_flags, val, sizeof(cfg->gcc_flags) - 1);
            cfg->gcc_flags[sizeof(cfg->gcc_flags) - 1] = '\0';
        }
    }

    fclose(f);
    cfg->is_loaded_from_file = 1;
    strncpy(cfg->config_file_path, filepath, sizeof(cfg->config_file_path) - 1);
    cfg->config_file_path[sizeof(cfg->config_file_path) - 1] = '\0';
    return 1;
}

static void detect_system_arch(ArchConfig* cfg) {
    memset(cfg, 0, sizeof(ArchConfig));
    strcpy(cfg->gcc_path, "gcc");
    cfg->multilib_supported = 1;

#ifdef _WIN32
    strcpy(cfg->os_name, "windows");
    SYSTEM_INFO si;
    memset(&si, 0, sizeof(si));
    typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
    PGNSI pGNSI = (PGNSI)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetNativeSystemInfo");
    if (pGNSI) {
        pGNSI(&si);
    } else {
        GetSystemInfo(&si);
    }

    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        cfg->os_bits = 64;
        strcpy(cfg->host_arch, "x86_64");
    } else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) {
        cfg->os_bits = 64;
        strcpy(cfg->host_arch, "arm64");
    } else {
        cfg->os_bits = 32;
        strcpy(cfg->host_arch, "x86");
    }

    // Consultar arquitectura de GCC instalado
    FILE* gp = popen("gcc -dumpmachine 2>nul", "r");
    if (gp) {
        char buf[128] = {0};
        if (fgets(buf, sizeof(buf), gp)) {
            trim_whitespace(buf);
            strncpy(cfg->gcc_arch, buf, sizeof(cfg->gcc_arch) - 1);
            if (strstr(buf, "64") != NULL) {
                cfg->gcc_bits = 64;
            } else {
                cfg->gcc_bits = 32;
            }
        }
        pclose(gp);
    } else {
        strcpy(cfg->gcc_arch, "unknown");
        cfg->gcc_bits = cfg->os_bits;
    }

    // En Windows: si GCC es de 64 bits (MinGW-w64 x86_64), requiere -m32 para ensamblador x86
    if (cfg->gcc_bits == 64) {
        strcpy(cfg->gcc_flags, "-m32");
    } else {
        strcpy(cfg->gcc_flags, "");
    }

#else
    strcpy(cfg->os_name, "linux");
    struct utsname u;
    if (uname(&u) == 0) {
        strncpy(cfg->host_arch, u.machine, sizeof(cfg->host_arch) - 1);
        if (strstr(u.machine, "64") != NULL) {
            cfg->os_bits = 64;
        } else {
            cfg->os_bits = 32;
        }
    } else {
        strcpy(cfg->host_arch, "unknown");
        cfg->os_bits = 64;
    }

    FILE* gp = popen("gcc -dumpmachine 2>/dev/null", "r");
    if (gp) {
        char buf[128] = {0};
        if (fgets(buf, sizeof(buf), gp)) {
            trim_whitespace(buf);
            strncpy(cfg->gcc_arch, buf, sizeof(cfg->gcc_arch) - 1);
            if (strstr(buf, "64") != NULL) {
                cfg->gcc_bits = 64;
            } else {
                cfg->gcc_bits = 32;
            }
        }
        pclose(gp);
    } else {
        strcpy(cfg->gcc_arch, "unknown");
        cfg->gcc_bits = cfg->os_bits;
    }

    // En Linux: sistemas de 64 bits requieren -m32 -no-pie; sistemas 32 bits puros usan -no-pie
    if (cfg->os_bits == 64) {
        strcpy(cfg->gcc_flags, "-m32 -no-pie");
    } else {
        strcpy(cfg->gcc_flags, "-no-pie");
    }
#endif
}

void arch_init(void) {
    if (is_initialized) return;

    // 1. Probar ruta estandar en ~/.itsuki/arch.conf
    char std_path[512];
    get_default_config_path(std_path, sizeof(std_path));
    if (file_exists(std_path)) {
        if (parse_config_file(std_path, &active_config)) {
            is_initialized = 1;
            return;
        }
    }

    // 2. Probar arch.conf junto al binario
    char local_path[512] = "arch.conf";
    if (file_exists(local_path)) {
        if (parse_config_file(local_path, &active_config)) {
            is_initialized = 1;
            return;
        }
    }

#ifndef _WIN32
    // 3. Probar instalacion global en Linux (/usr/local/share/itsuki/arch.conf)
    const char* sys_path = "/usr/local/share/itsuki/arch.conf";
    if (file_exists(sys_path)) {
        if (parse_config_file(sys_path, &active_config)) {
            is_initialized = 1;
            return;
        }
    }
#endif

    // 4. Si no hay archivo de configuracion previo, autodetectar en tiempo real
    detect_system_arch(&active_config);
    is_initialized = 1;
}

const ArchConfig* arch_get_current(void) {
    if (!is_initialized) {
        arch_init();
    }
    return &active_config;
}

void arch_set_target_bits(int bits) {
    if (!is_initialized) arch_init();
    if (bits == 32 || bits == 64) {
        active_config.target_bits = bits;
    }
}

int arch_get_target_bits(void) {
    if (!is_initialized) arch_init();
    return active_config.target_bits > 0 ? active_config.target_bits : ((active_config.gcc_bits == 64) ? 64 : 32);
}

void arch_print_diagnostics(void) {
    const ArchConfig* cfg = arch_get_current();
    printf("===========================================================\n");
    printf("         ITSUKI - DIAGNOSTICO DE ARQUITECTURA              \n");
    printf("===========================================================\n");
    printf("  Sistema Operativo   : %s (%d bits)\n", cfg->os_name, cfg->os_bits);
    printf("  Arquitectura Host   : %s\n", cfg->host_arch);
    printf("  Compilador C / GCC  : %s\n", cfg->gcc_path);
    printf("  GCC Machine Target  : %s (%d bits)\n", cfg->gcc_arch, cfg->gcc_bits);
    printf("  Arquitectura Target : %s (%d bits)\n", cfg->target_bits == 64 ? "x86_64" : "x86", cfg->target_bits == 64 ? 64 : 32);
    printf("  Flags de Enlace     : %s\n", cfg->gcc_flags[0] != '\0' ? cfg->gcc_flags : "(estandar)");
    printf("  Soporte Multilib    : %s\n", cfg->multilib_supported ? "Si [OK]" : "No detectado / Incompleto");
    printf("  Origen Configuracion: %s\n", cfg->is_loaded_from_file ? cfg->config_file_path : "Autodetectado en tiempo real");
    printf("  Salida de Ejecutable: Binario nativo %s (%d bits)\n", cfg->target_bits == 64 ? "x86_64" : "x86", cfg->target_bits == 64 ? 64 : 32);
    printf("===========================================================\n");
    if (cfg->target_bits == 64) {
        printf("  [INFO] Modo nativo x86_64 (64 bits) habilitado.\n");
        printf("         Soporta int64, float, double y llamadas nativas de 64 bits.\n");
    } else if (cfg->os_bits == 64) {
        printf("  [INFO] Modo de compatibilidad x86 (32 bits) habilitado.\n");
        printf("         Ejecutables compatibles nativamente via WOW64 / multilib.\n");
    }
    printf("===========================================================\n\n");
}

void arch_print_linker_troubleshooting(void) {
    const ArchConfig* cfg = arch_get_current();
    printf("-----------------------------------------------------------\n");
    printf(" [Itsuki Ayuda - Diagnostico de Enlace]\n");
    printf(" Arquitectura de tu maquina: %s (%d bits)\n", cfg->host_arch, cfg->os_bits);
    printf(" Arquitectura de tu GCC    : %s (%d bits)\n", cfg->gcc_arch, cfg->gcc_bits);
    printf(" Flags utilizados          : %s\n", cfg->gcc_flags[0] ? cfg->gcc_flags : "(ninguno)");
    printf("-----------------------------------------------------------\n");

#ifdef _WIN32
    if (cfg->gcc_bits == 64) {
        printf(" Causa probable en Windows:\n");
        printf(" Tu compilador GCC es de 64 bits y no encontro librerias x86 (-m32).\n");
        printf(" Solucion recomendada:\n");
        printf("  1. Instala un toolchain MinGW con soporte multilib o w64devkit:\n");
        printf("     https://github.com/skeeto/w64devkit/releases\n");
        printf("  2. O instala MinGW-w64 i686 (32 bits) para maxima compatibilidad.\n");
    } else {
        printf(" Verifica que GCC y kernel32 esten accesibles en tu variable PATH.\n");
    }
#else
    if (cfg->os_bits == 64) {
        printf(" Causa probable en Linux:\n");
        printf(" Tu sistema es de 64 bits y faltan las cabeceras/librerias de 32 bits (multilib).\n");
        printf(" Instala el soporte de 32 bits ejecutando el comando correspondiente:\n\n");
        printf("   Ubuntu / Debian / Linux Mint:\n");
        printf("     sudo apt update && sudo apt install gcc-multilib libc6-dev-i386\n\n");
        printf("   Fedora / RHEL / CentOS:\n");
        printf("     sudo dnf install glibc-devel.i686 libgcc.i686\n\n");
        printf("   Arch Linux / Manjaro:\n");
        printf("     sudo pacman -S lib32-glibc lib32-gcc-libs\n\n");
        printf("   openSUSE:\n");
        printf("     sudo zypper install gcc-32bit glibc-devel-32bit\n");
    } else {
        printf(" Verifica que 'gcc' este instalado correctamente y soporte '-no-pie'.\n");
    }
#endif
    printf("-----------------------------------------------------------\n");
}

int arch_save_config(const char* filepath, const ArchConfig* cfg) {
    if (!filepath || !cfg) return 0;
    FILE* f = fopen(filepath, "w");
    if (!f) return 0;

    fprintf(f, "# ===========================================================\n");
    fprintf(f, "#      ITSUKI PROGRAMMING LANGUAGE - ARCH CONFIG            \n");
    fprintf(f, "#   Generado durante la instalacion / configuracion del PC  \n");
    fprintf(f, "# ===========================================================\n\n");
    fprintf(f, "os_name=%s\n", cfg->os_name);
    fprintf(f, "os_bits=%d\n", cfg->os_bits);
    fprintf(f, "host_arch=%s\n", cfg->host_arch);
    fprintf(f, "gcc_path=%s\n", cfg->gcc_path);
    fprintf(f, "gcc_arch=%s\n", cfg->gcc_arch);
    fprintf(f, "gcc_bits=%d\n", cfg->gcc_bits);
    fprintf(f, "multilib_supported=%d\n", cfg->multilib_supported);
    fprintf(f, "gcc_flags=%s\n", cfg->gcc_flags);

    fclose(f);
    return 1;
}
