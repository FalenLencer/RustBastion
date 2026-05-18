#pragma once
#include <stdio.h>
#ifdef _WIN32
#  include <direct.h>
#else
#  include <sys/stat.h>
#endif

/* Préfixe du répertoire de données (saves/, config/).
 * "" dans les builds packagés, "build/" dans le build de dev.
 * Défini et initialisé dans engine/paths.c. */
extern char g_data_prefix[256];

/* Détecte l'emplacement de l'exe et positionne le CWD + g_data_prefix.
 * Doit être appelé avant tout accès fichier. */
void setup_working_dir(void);

static inline const char *data_path(char *buf, int sz, const char *rel) {
    snprintf(buf, sz, "%s%s", g_data_prefix, rel);
    return buf;
}

static inline void data_mkdir(const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s%s", g_data_prefix, dir);
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}
