#include "window.h"
#include "canvas.h"

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

void window_apply_size(int w, int h) {
    SetWindowSize(w, h);
    int mw = GetMonitorWidth(0), mh = GetMonitorHeight(0);
    Vector2 mp = GetMonitorPosition(0);
    SetWindowPosition((int)(mp.x+(mw-w)/2), (int)(mp.y+(mh-h)/2));
}

void window_center(void) {
    int mw = GetMonitorWidth(0), mh = GetMonitorHeight(0);
    Vector2 mp = GetMonitorPosition(0);
    SetWindowPosition((int)(mp.x+(mw-VIRT_W)/2),
                      (int)(mp.y+(mh-VIRT_H)/2));
}

// Désactive le VSync en forçant le swap interval GLFW à 0.
// Doit être appelée APRÈS InitWindow() car GLFW doit être initialisé.
// Après cet appel, SetTargetFPS() contrôle seul le framerate.
void window_disable_vsync(void) {
    glfwSwapInterval(0);
}