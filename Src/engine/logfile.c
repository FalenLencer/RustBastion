/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  logfile.c ─ Journal de session (cf. logfile.h). */
#include "logfile.h"
#include "paths.h"
#include "version.h"
#include "raylib.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#define LOG_REL_PATH   "config/rustbastion.log"
#define LOG_MAX_LINES  4000   /* au-delà : journal tronqué (erreur en boucle)   */

static FILE *g_log       = NULL;
static int   g_log_lines = 0;

static const char *level_tag(int lvl) {
    switch (lvl) {
    case LOG_TRACE:   return "TRACE";
    case LOG_DEBUG:   return "DEBUG";
    case LOG_INFO:    return "INFO ";
    case LOG_WARNING: return "AVERT";
    case LOG_ERROR:   return "ERREUR";
    case LOG_FATAL:   return "FATAL";
    default:          return "?    ";
    }
}

/* Rappel raylib : console (comportement d'origine) + fichier. */
static void log_cb(int lvl, const char *fmt, va_list args) {
    va_list cp;
    va_copy(cp, args);
    printf("%s: ", level_tag(lvl));
    vprintf(fmt, args);
    printf("\n");
    if (g_log && g_log_lines < LOG_MAX_LINES) {
        /* Heure murale (GetTime() n'est pas fiable avant InitWindow) */
        time_t now = time(NULL);
        char hms[12] = "--:--:--";
        const struct tm *tmv = localtime(&now);
        if (tmv) strftime(hms, sizeof(hms), "%H:%M:%S", tmv);
        fprintf(g_log, "[%s] %s ", hms, level_tag(lvl));
        vfprintf(g_log, fmt, cp);
        fputc('\n', g_log);
        if (++g_log_lines == LOG_MAX_LINES)
            fprintf(g_log, "... journal tronqué (%d lignes)\n", LOG_MAX_LINES);
        fflush(g_log);   /* rien de perdu si le jeu plante juste après */
    }
    va_end(cp);
}

void logfile_init(void) {
    char path[512];
    data_mkdir("config");
    g_log = fopen(data_path(path, sizeof(path), LOG_REL_PATH), "w");
    if (g_log) {
        time_t now = time(NULL);
        char when[32] = "?";
        const struct tm *tmv = localtime(&now);
        if (tmv) strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", tmv);
        fprintf(g_log, "RUST BASTION %s\nSession du %s\n\n", GAME_VERSION_FULL, when);
        fflush(g_log);
    }
    SetTraceLogCallback(log_cb);
}

void logfile_close(void) {
    if (!g_log) return;
    fprintf(g_log, "\nFin de session.\n");
    fclose(g_log);
    g_log = NULL;
}
