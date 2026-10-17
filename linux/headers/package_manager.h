#ifndef PACKAGE_MANAGER_H
#define PACKAGE_MANAGER_H

#include <stddef.h>

void pkg_get_packages_dir(char* out_path, size_t max_len);
int pkg_resolve_include(const char* name, int is_system_include, char* out_path, size_t max_len);
int pkg_find_companion_source(const char* header_path, char* out_su_path, size_t max_len);

int pkg_install(const char* repo_arg);
int pkg_uninstall(const char* pkg_name);
int pkg_list(void);

#endif
