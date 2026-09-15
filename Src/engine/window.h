/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

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

// ── Diagnostic du processeur graphique ───────────────────────
// Le rendu passe TOUJOURS par OpenGL, donc par le GPU : il n'existe
// aucune API OpenGL permettant d'énumérer ou de CHOISIR une carte
// (c'est propre à Vulkan / Direct3D 12). Ce qui est utile et faisable,
// c'est de DIRE au joueur quel processeur graphique travaille — et de
// l'alerter si le pilote est retombé sur un rendu logiciel (llvmpipe,
// GDI générique…), auquel cas c'est bien le CPU qui dessine, avec les
// performances qu'on imagine.
// À n'appeler qu'APRÈS InitWindow() (le contexte GL doit exister).
const char *gpu_renderer_name(void);   // ex. "NVIDIA GeForce RTX 3060"
const char *gpu_vendor_name  (void);   // ex. "NVIDIA Corporation"
int         gpu_is_software  (void);   // 1 = rendu logiciel (CPU) détecté