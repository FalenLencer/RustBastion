/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  fx.h ─ Effets de « jus » (feedback visuel) : particules de mort,
 *  textes flottants (or), secousse caméra. Système global léger, piloté par
 *  data : le gameplay (enemy.c…) ne fait qu'émettre des effets (aucun dessin),
 *  le rendu est fait par fx_render_world() dans la caméra de la carte.
 */
#pragma once
#include "raylib.h"

#define FX_MAX_PARTICLES 320
#define FX_MAX_POPUPS     48

typedef struct {
    float x, y, vx, vy;
    float life, max_life, size;
    Color col;
    int   active;
} FxParticle;

typedef struct {
    float x, y, vy, life, max_life;
    char  text[12];
    Color col;
    int   active;
} FxPopup;

typedef struct {
    FxParticle particles[FX_MAX_PARTICLES];
    FxPopup    popups[FX_MAX_POPUPS];
    float      shake_mag;   // magnitude de secousse courante (décroît)
    int        enabled;     // option : effets activés (1) ou non (0)
} FxSystem;

extern FxSystem g_fx;

// Réinitialise le système (début de partie). Conserve `enabled`.
void  fx_reset(void);
// Met à jour particules / popups / secousse.
void  fx_update(float dt);

// Gerbe de `n` particules depuis (x,y), couleur `col`, vitesse ~`speed`.
void  fx_burst(float x, float y, Color col, int n, float speed);
// Texte flottant montant (ex. "+12").
void  fx_popup(float x, float y, const char *text, Color col);
// Pop d'or "+N" doré (ignoré si amount <= 0).
void  fx_popup_gold(float x, float y, int amount);
// Déclenche une secousse caméra de magnitude `magnitude` (px). Prend le max.
void  fx_shake(float magnitude);

// Dessine particules + popups (à appeler DANS la caméra de la carte).
void  fx_render_world(void);

// Décalage de secousse à appliquer à la caméra (0 si désactivé / au repos).
float fx_shake_dx(void);
float fx_shake_dy(void);
