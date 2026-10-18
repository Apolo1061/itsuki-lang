#include "package_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEP '\\'
#define PATH_SEP_STR "\\"
#else
#include <unistd.h>
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#endif

static int file_exists(const char* path) {
    if (!path || path[0] == '\0') return 0;
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

static int dir_exists(const char* path) {
    if (!path || path[0] == '\0') return 0;
    DIR* d = opendir(path);
    if (d) {
        closedir(d);
        return 1;
    }
    return 0;
}

static void ensure_dir_exists(const char* path) {
    if (dir_exists(path)) return;
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "mkdir \"%s\" 2>nul", path);
#else
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" 2>/dev/null", path);
#endif
    (void)system(cmd);
}

void pkg_get_packages_dir(char* out_path, size_t max_len) {
    const char* env_path = getenv("ITSUKI_PATH");
    if (env_path && env_path[0] != '\0') {
        snprintf(out_path, max_len, "%s", env_path);
        return;
    }
#ifdef _WIN32
    const char* user = getenv("USERPROFILE");
    if (!user) user = getenv("APPDATA");
    if (!user) user = ".";
    snprintf(out_path, max_len, "%s\\.itsuki\\packages", user);
#else
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(out_path, max_len, "%s/.itsuki/packages", home);
#endif
}

static void normalize_slashes(char* path) {
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '\\') path[i] = '/';
    }
}

int pkg_resolve_include(const char* name, int is_system_include, char* out_path, size_t max_len) {
    (void)is_system_include;
    if (!name || name[0] == '\0') return 0;

    char pkg_dir[512];
    pkg_get_packages_dir(pkg_dir, sizeof(pkg_dir));
    normalize_slashes(pkg_dir);

    if (!dir_exists(pkg_dir)) return 0;

    char clean_name[256];
    strncpy(clean_name, name, sizeof(clean_name) - 1);
    clean_name[sizeof(clean_name) - 1] = '\0';
    normalize_slashes(clean_name);

    char cand[2048];
    const char* exts[] = {".huki", ".suki", ".hu", ".su", NULL};

    // Caso 1: name contiene subcarpeta (ej: libreria/modulo.huki)
    if (strchr(clean_name, '/') != NULL) {
        snprintf(cand, sizeof(cand), "%s/%s", pkg_dir, clean_name);
        if (file_exists(cand)) { strncpy(out_path, cand, max_len - 1); out_path[max_len - 1] = '\0'; return 1; }

        for (int e = 0; exts[e] != NULL; e++) {
            snprintf(cand, sizeof(cand), "%s/%s%s", pkg_dir, clean_name, exts[e]);
            if (file_exists(cand)) { strncpy(out_path, cand, max_len - 1); out_path[max_len - 1] = '\0'; return 1; }
        }
    }

    // Extraer nombre base sin extensión
    char base[256];
    strncpy(base, clean_name, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char* dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    // Caso 2: buscar directamente dentro de ~/.itsuki/packages/<base>/...
    const char* subdirs[] = {"", "/include", "/src", "/lib", NULL};
    for (int s = 0; subdirs[s] != NULL; s++) {
        // Con nombre exacto
        snprintf(cand, sizeof(cand), "%s/%s%s/%s", pkg_dir, base, subdirs[s], clean_name);
        if (file_exists(cand)) { strncpy(out_path, cand, max_len - 1); out_path[max_len - 1] = '\0'; return 1; }

        for (int e = 0; exts[e] != NULL; e++) {
            snprintf(cand, sizeof(cand), "%s/%s%s/%s%s", pkg_dir, base, subdirs[s], clean_name, exts[e]);
            if (file_exists(cand)) { strncpy(out_path, cand, max_len - 1); out_path[max_len - 1] = '\0'; return 1; }
        }

        // Si se pidió #include <libreria>, probar con <pkg_dir>/libreria/libreria.huki
        for (int e = 0; exts[e] != NULL; e++) {
            snprintf(cand, sizeof(cand), "%s/%s%s/%s%s", pkg_dir, base, subdirs[s], base, exts[e]);
            if (file_exists(cand)) { strncpy(out_path, cand, max_len - 1); out_path[max_len - 1] = '\0'; return 1; }
        }
    }

    // Caso 3: Escanear todos los paquetes instalados en ~/.itsuki/packages/
    DIR* d = opendir(pkg_dir);
    if (d) {
        struct dirent* ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            for (int s = 0; subdirs[s] != NULL; s++) {
                snprintf(cand, sizeof(cand), "%s/%s%s/%s", pkg_dir, ent->d_name, subdirs[s], clean_name);
                if (file_exists(cand)) {
                    closedir(d);
                    strncpy(out_path, cand, max_len - 1);
                    out_path[max_len - 1] = '\0';
                    return 1;
                }
                for (int e = 0; exts[e] != NULL; e++) {
                    snprintf(cand, sizeof(cand), "%s/%s%s/%s%s", pkg_dir, ent->d_name, subdirs[s], clean_name, exts[e]);
                    if (file_exists(cand)) {
                        closedir(d);
                        strncpy(out_path, cand, max_len - 1);
                        out_path[max_len - 1] = '\0';
                        return 1;
                    }
                }
            }
        }
        closedir(d);
    }

    return 0;
}

int pkg_find_companion_source(const char* header_path, char* out_su_path, size_t max_len) {
    if (!header_path || header_path[0] == '\0') return 0;

    char cand[2048];
    strncpy(cand, header_path, sizeof(cand) - 1);
    cand[sizeof(cand) - 1] = '\0';
    normalize_slashes(cand);

    char* huki_ptr = strstr(cand, ".huki");
    if (huki_ptr) {
        strcpy(huki_ptr, ".suki");
    } else {
        char* hu_ptr = strstr(cand, ".hu");
        if (hu_ptr) {
            strcpy(hu_ptr, ".suki");
        } else {
            return 0;
        }
    }

    // 1. Mismo directorio cambiando .huki por .suki
    if (file_exists(cand)) {
        strncpy(out_su_path, cand, max_len - 1);
        out_su_path[max_len - 1] = '\0';
        return 1;
    }

    // Probar si el companero era .su
    char legacy_cand[2048];
    strncpy(legacy_cand, cand, sizeof(legacy_cand) - 1);
    legacy_cand[sizeof(legacy_cand) - 1] = '\0';
    char* suki_ptr = strstr(legacy_cand, ".suki");
    if (suki_ptr) {
        strcpy(suki_ptr, ".su");
        if (file_exists(legacy_cand)) {
            strncpy(out_su_path, legacy_cand, max_len - 1);
            out_su_path[max_len - 1] = '\0';
            return 1;
        }
    }

    // Probar si el companero era .c o .cpp
    char c_cand[2048];
    strncpy(c_cand, header_path, sizeof(c_cand) - 1);
    c_cand[sizeof(c_cand) - 1] = '\0';
    normalize_slashes(c_cand);
    char* ext_ptr = strstr(c_cand, ".huki");
    if (!ext_ptr) ext_ptr = strstr(c_cand, ".hu");
    if (!ext_ptr) ext_ptr = strstr(c_cand, ".h");
    if (ext_ptr) {
        strcpy(ext_ptr, ".c");
        if (file_exists(c_cand)) {
            strncpy(out_su_path, c_cand, max_len - 1);
            out_su_path[max_len - 1] = '\0';
            return 1;
        }
        strcpy(ext_ptr, ".cpp");
        if (file_exists(c_cand)) {
            strncpy(out_su_path, c_cand, max_len - 1);
            out_su_path[max_len - 1] = '\0';
            return 1;
        }
    }

    // 2. Si estaba en include/, buscar en src/ o en el padre
    char alt_cand[2048];
    char* inc_sub = strstr(cand, "/include/");
    if (inc_sub) {
        int prefix_len = inc_sub - cand;
        snprintf(alt_cand, sizeof(alt_cand), "%.*s/src/%s", prefix_len, cand, inc_sub + 9);
        if (file_exists(alt_cand)) {
            strncpy(out_su_path, alt_cand, max_len - 1);
            out_su_path[max_len - 1] = '\0';
            return 1;
        }
        snprintf(alt_cand, sizeof(alt_cand), "%.*s/%s", prefix_len, cand, inc_sub + 9);
        if (file_exists(alt_cand)) {
            strncpy(out_su_path, alt_cand, max_len - 1);
            out_su_path[max_len - 1] = '\0';
            return 1;
        }
    }

    return 0;
}

int pkg_install(const char* repo_arg) {
    if (!repo_arg || repo_arg[0] == '\0') {
        printf("Error: Falta la URL o identificador del repositorio.\n\n");
        printf("Uso:\n");
        printf("  itsuki install <url-git|usuario/repositorio>\n\n");
        printf("Ejemplos:\n");
        printf("  itsuki install https://github.com/apolo1061/libreria.git\n");
        printf("  itsuki install apolo1061/libreria\n\n");
        return 1;
    }

    char url[512];
    if (strncmp(repo_arg, "http://", 7) == 0 ||
        strncmp(repo_arg, "https://", 8) == 0 ||
        strncmp(repo_arg, "git@", 4) == 0 ||
        repo_arg[0] == '/' || repo_arg[0] == '\\' || repo_arg[0] == '~' ||
        strncmp(repo_arg, "./", 2) == 0 || strncmp(repo_arg, ".\\", 2) == 0 ||
        strncmp(repo_arg, "../", 3) == 0 || strncmp(repo_arg, "..\\", 3) == 0 ||
        dir_exists(repo_arg) ||
        (isalpha((unsigned char)repo_arg[0]) && repo_arg[1] == ':')) {
        snprintf(url, sizeof(url), "%s", repo_arg);
    } else {
        snprintf(url, sizeof(url), "https://github.com/%s.git", repo_arg);
    }

    // Extraer nombre del paquete
    char clean_url[512];
    strncpy(clean_url, url, sizeof(clean_url) - 1);
    clean_url[sizeof(clean_url) - 1] = '\0';

    size_t ulen = strlen(clean_url);
    if (ulen > 4 && strcmp(clean_url + ulen - 4, ".git") == 0) {
        clean_url[ulen - 4] = '\0';
    }
    while (strlen(clean_url) > 0 && (clean_url[strlen(clean_url) - 1] == '/' || clean_url[strlen(clean_url) - 1] == '\\')) {
        clean_url[strlen(clean_url) - 1] = '\0';
    }

    const char* slash = strrchr(clean_url, '/');
    const char* bslash = strrchr(clean_url, '\\');
    const char* pkg_name = clean_url;
    if (slash && slash >= pkg_name) pkg_name = slash + 1;
    if (bslash && bslash >= pkg_name) pkg_name = bslash + 1;

    if (pkg_name[0] == '\0') pkg_name = "pkg";

    char packages_dir[512];
    pkg_get_packages_dir(packages_dir, sizeof(packages_dir));
    ensure_dir_exists(packages_dir);

    char target_dir[1024];
    snprintf(target_dir, sizeof(target_dir), "%s%c%s", packages_dir, PATH_SEP, pkg_name);

    printf("===================================================\n");
    printf("   Itsuki Package Manager - Instalador de Librerias\n");
    printf("===================================================\n");
    printf(" Paquete  : %s\n", pkg_name);
    printf(" Origen   : %s\n", url);
    printf(" Destino  : %s\n", target_dir);
    printf("===================================================\n");

    char git_cmd[2048];
    if (dir_exists(target_dir)) {
        printf("[Itsuki PM] La libreria ya existe en el sistema. Actualizando con 'git pull'...\n\n");
        snprintf(git_cmd, sizeof(git_cmd), "git -C \"%s\" pull", target_dir);
    } else {
        printf("[Itsuki PM] Clonando repositorio con Git...\n\n");
        snprintf(git_cmd, sizeof(git_cmd), "git clone --depth 1 \"%s\" \"%s\"", url, target_dir);
    }

    int res = system(git_cmd);
    if (res != 0) {
        printf("\n[Itsuki PM] [ERROR] Fallo al clonar o actualizar el repositorio (codigo %d).\n", res);
        printf("[Itsuki PM] Asegurate de tener 'git' instalado en tu terminal y conexion a internet.\n\n");
        return 1;
    }

    printf("\n[Itsuki PM] [OK] Libreria '%s' instalada correctamente!\n", pkg_name);
    printf("[Itsuki PM] Para usarla en cualquier codigo .suki de tu sistema, escribe:\n");
    printf("            #include <%s.huki>\n\n", pkg_name);
    return 0;
}

int pkg_uninstall(const char* pkg_name) {
    if (!pkg_name || pkg_name[0] == '\0') {
        printf("Error: Especifica el nombre de la libreria a desinstalar.\n");
        printf("Uso: itsuki uninstall <nombre-libreria>\n");
        printf("Ejemplo: itsuki uninstall libreria\n\n");
        return 1;
    }

    char packages_dir[512];
    pkg_get_packages_dir(packages_dir, sizeof(packages_dir));

    char target_dir[1024];
    snprintf(target_dir, sizeof(target_dir), "%s%c%s", packages_dir, PATH_SEP, pkg_name);

    if (!dir_exists(target_dir)) {
        printf("[Itsuki PM] La libreria '%s' no esta instalada en:\n  %s\n\n", pkg_name, target_dir);
        return 1;
    }

    printf("[Itsuki PM] Desinstalando libreria '%s'...\n", pkg_name);

    char rm_cmd[2048];
#ifdef _WIN32
    snprintf(rm_cmd, sizeof(rm_cmd), "rmdir /s /q \"%s\"", target_dir);
#else
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", target_dir);
#endif

    int res = system(rm_cmd);
    if (res != 0 || dir_exists(target_dir)) {
        printf("[Itsuki PM] [ERROR] No se pudo eliminar el directorio '%s'.\n\n", target_dir);
        return 1;
    }

    printf("[Itsuki PM] [OK] Libreria '%s' desinstalada exitosamente.\n\n", pkg_name);
    return 0;
}

int pkg_list(void) {
    char packages_dir[512];
    pkg_get_packages_dir(packages_dir, sizeof(packages_dir));

    printf("===================================================\n");
    printf("   Librerias de Itsuki Instaladas en el Sistema    \n");
    printf("===================================================\n");
    printf("Ruta de paquetes: %s\n\n", packages_dir);

    DIR* d = opendir(packages_dir);
    int count = 0;
    if (d) {
        struct dirent* ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s%c%s", packages_dir, PATH_SEP, ent->d_name);
            if (dir_exists(full_path)) {
                count++;
                printf("  [%d] %-20s (en: %s)\n", count, ent->d_name, full_path);
            }
        }
        closedir(d);
    }

    if (count == 0) {
        printf("  (No hay librerias externas instaladas)\n");
        printf("\nPara instalar una libreria desde GitHub:\n");
        printf("  itsuki install https://github.com/apolo1061/libreria.git\n");
        printf("  itsuki install apolo1061/libreria\n");
    }

    printf("\nLibrerias estandar integradas (embebidas en itsuki):\n");
    printf("  * <fundamental>  - Runtime nativo, gestion de memoria, cadenas basicas, I/O\n");
    printf("  * <math>         - Biblioteca matematica, geometria, trigonometria\n");
    printf("  * <string>       - Cadenas dinamicas struct String y utilidades\n");
    printf("  * <c>            - Interoperabilidad directa con C (libc / stdio / math)\n");
    printf("===================================================\n\n");
    return 0;
}
