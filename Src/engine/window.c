/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "window.h"
#include "canvas.h"
#include <string.h>   /* strstr : détection du rendu logiciel */

// ── Accès direct à GLFW sous Raylib ──────────────────────────
// Raylib utilise GLFW en interne sur toutes les plateformes desktop.
// glfwSwapInterval(0) désactive le VSync au niveau du driver OpenGL,
// ce qui contourne les compositeurs système (Mutter, KWin, DWM…)
// et les réglages "Force VSync" des panneaux de contrôle GPU.
//
// Le header est livré avec Raylib dans external/GLFW/glfw3.h.
// On utilise RLGL_IMPLEMENTATION pour ne pas avoir à linker GLFW
// séparément — Raylib l'embarque déjà dans libraylib.a.
#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
#else
    #define GLFW_EXPOSE_NATIVE_X11
#endif

// Déclaration minimale pour éviter d'inclure tout glfw3.h
// (qui nécessiterait des types platform-specific non disponibles ici)
// On passe par le symbole exporté directement depuis libraylib.
//
// Raylib réexporte glfwSwapInterval dans son .a/.so sur toutes plateformes.
// La déclaration suffit — le linker trouve le symbole dans -lraylib.
extern void glfwSwapInterval(int interval);

// Retourne la position centrée sur le moniteur pour une fenêtre de taille (w, h).
// Si la détection du moniteur échoue (XRandR non configuré, WSLg, etc.),
// retourne (50, 50) pour rester visible plutôt que de placer hors-écran.
static void safe_center(int w, int h, int *out_x, int *out_y) {
    int mw = GetMonitorWidth(0), mh = GetMonitorHeight(0);
    if (mw <= 0 || mh <= 0) { *out_x = 50; *out_y = 50; return; }
    Vector2 mp = GetMonitorPosition(0);
    int x = (int)(mp.x + (mw - w) / 2);
    int y = (int)(mp.y + (mh - h) / 2);
    if (x < (int)mp.x) x = (int)mp.x;   // ne pas sortir du bord gauche/haut
    if (y < (int)mp.y) y = (int)mp.y;
    *out_x = x; *out_y = y;
}

void window_apply_size(int w, int h) {
    SetWindowSize(w, h);
    int x, y; safe_center(w, h, &x, &y);
    SetWindowPosition(x, y);
}

void window_center(void) {
    int x, y; safe_center(VIRT_W, VIRT_H, &x, &y);
    SetWindowPosition(x, y);
}

// Désactive le VSync en forçant le swap interval GLFW à 0.
// Doit être appelée APRÈS InitWindow() car GLFW doit être initialisé.
// Après cet appel, SetTargetFPS() contrôle seul le framerate.
void window_disable_vsync(void) {
    glfwSwapInterval(0);
}

// ════════════════════════════════════════════════════════════════
// DIAGNOSTIC GPU
// ════════════════════════════════════════════════════════════════
// GL_VENDOR / GL_RENDERER sont des constantes d'OpenGL 1.1, présentes
// sur toutes les implémentations. On déclare glGetString ICI plutôt que
// d'inclure <GL/gl.h> : raylib embarque son propre chargeur (GLAD) et
// cumuler les deux provoque des redéfinitions selon les plateformes.
// Le symbole est exporté par libGL (Linux) et opengl32.dll (Windows),
// tous deux déjà liés (cf. LDFLAGS du Makefile). Pas de souci de
// convention d'appel : la cible Windows est x86_64, où __stdcall et
// l'ABI par défaut sont confondus.
#define GL_VENDOR    0x1F00
#define GL_RENDERER  0x1F01
extern const unsigned char *glGetString(unsigned int name);

static const char *gl_string_or(unsigned int id, const char *fallback) {
    const unsigned char *s = glGetString(id);
    return (s && s[0]) ? (const char *)s : fallback;
}

const char *gpu_renderer_name(void) {
    return gl_string_or(GL_RENDERER, "inconnu");
}

const char *gpu_vendor_name(void) {
    return gl_string_or(GL_VENDOR, "inconnu");
}

// Rendu logiciel = le CPU rastérise. Signatures connues des pilotes de
// repli : Mesa (llvmpipe / softpipe / swrast / SWR), Microsoft (GDI
// générique, Basic Render Driver), Mesa offscreen.
int gpu_is_software(void) {
    static const char *const SOFT[] = {
        "llvmpipe", "softpipe", "swrast", "swr",
        "gdi generic", "basic render", "offscreen", "software",
    };
    const char *r = gpu_renderer_name();
    char low[128];
    int  n = 0;
    for (; r[n] && n < (int)sizeof(low) - 1; n++) {
        char c = r[n];
        low[n] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    low[n] = '\0';
    for (int i = 0; i < (int)(sizeof(SOFT) / sizeof(SOFT[0])); i++)
        if (strstr(low, SOFT[i])) return 1;
    return 0;
}