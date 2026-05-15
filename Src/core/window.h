#pragma once
#include "raylib.h"

// Applique une taille de fenêtre et la centre sur le moniteur
void window_apply_size(int w, int h);

// Centre la fenêtre sur le moniteur principal
void window_center(void);

// Désactive le VSync au niveau GLFW (swap interval = 0)
// À appeler UNE FOIS après InitWindow(), avant la boucle principale.
// Fonctionne sur Windows, Linux (X11 + Wayland) et macOS
// indépendamment des réglages driver/compositeur système.
void window_disable_vsync(void);