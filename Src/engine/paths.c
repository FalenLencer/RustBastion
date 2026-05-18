/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/* ════════════════════════════════════════════════════════════════
   engine/paths.c — gestion du répertoire de travail
   Définit g_data_prefix et setup_working_dir().
   ════════════════════════════════════════════════════════════════ */
#include "paths.h"
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#else
#  include <sys/stat.h>
#  include <unistd.h>
#endif

/* Préfixe de données : "" en build packagé, "build/" en dev. */
char g_data_prefix[256] = "";

/* ────────────────────────────────────────────────────────────────
   Se place dans le répertoire qui contient assets/ :
   - Build de dev   : exe dans build/  → chdir(parent), prefix="build/"
   - Package Linux  : assets/ à côté  → chdir(exe_dir), prefix=""
   - Package Windows: idem
   Doit être appelé avant tout accès fichier.
   ──────────────────────────────────────────────────────────────── */
void setup_working_dir(void) {
    char exe[512] = {0};
    char dir[512] = {0};

#ifdef _WIN32
    if (!GetModuleFileNameA(NULL, exe, sizeof(exe))) return;
    char *sep = strrchr(exe, '\\');
    if (!sep) return;
    *sep = '\0';
    snprintf(dir, sizeof(dir), "%s", exe);

    char test[512];
    snprintf(test, sizeof(test), "%s\\assets", dir);
    DWORD attr = GetFileAttributesA(test);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        _chdir(dir); return;
    }
    sep = strrchr(dir, '\\');
    if (sep) {
        *sep = '\0';
        snprintf(test, sizeof(test), "%s\\assets", dir);
        attr = GetFileAttributesA(test);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            _chdir(dir);
            snprintf(g_data_prefix, sizeof(g_data_prefix), "build/");
        }
    }
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) return;
    exe[n] = '\0';

    char *sep = strrchr(exe, '/');
    if (!sep) return;
    *sep = '\0';
    snprintf(dir, sizeof(dir), "%s", exe);

    char test[512];
    struct stat st;
    snprintf(test, sizeof(test), "%s/assets", dir);
    if (stat(test, &st) == 0 && S_ISDIR(st.st_mode)) {
        int r = chdir(dir); (void)r; return;
    }
    sep = strrchr(dir, '/');
    if (sep) {
        *sep = '\0';
        snprintf(test, sizeof(test), "%s/assets", dir);
        if (stat(test, &st) == 0 && S_ISDIR(st.st_mode)) {
            int r = chdir(dir); (void)r;
            snprintf(g_data_prefix, sizeof(g_data_prefix), "build/");
        }
    }
#endif
}
