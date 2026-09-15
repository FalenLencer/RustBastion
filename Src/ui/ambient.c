/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  ambient.c ─ météo d'ambiance 2D (voir ambient.h). Décoratif pur :
 *  aucun impact gameplay. Horloge ACCUMULÉE (jamais GetTime pour la
 *  logique) ; le dt reçu est déjà × speed_mult et vaut 0 en pause
 *  (l'appelant ne nous tick pas) → la couche gèle avec la sim.
 */
#include "ambient.h"
#include "renderer.h"        /* g_canvas_virt_w / g_canvas_virt_h    */
#include "ui_utils.h"        /* ui_frnd (source unique)              */
#include "../combat/fx.h"    /* toggle g_fx.enabled (fx_effects)     */
#include <math.h>

// ── Réglages ─────────────────────────────────────────────────
#define AMB_MAX        64     // particules simultanées (pool fixe)
#define AMB_WRAP_PAD   8.0f   // marge de rebouclage hors écran (px)
#define AMB_SWAY_HZ    1.3f   // pulsation de l'oscillation latérale
#define AMB_STREAK_LEN 0.045f // longueur des traits = vitesse × ceci

typedef struct { float x, y, phase, speed; } AmbParticle;

// Définition d'une météo : dérive de base, oscillation, gabarit.
typedef struct {
    int   count;            // particules actives (≤ AMB_MAX)
    float vx, vy;           // dérive de base (px/s, espace canvas)
    float wobble;           // amplitude d'oscillation latérale (px/s)
    int   streak;           // 1 = trait (pluie/rafale), 0 = grain
    Color col;              // couleur (alpha = opacité faible)
} AmbDef;

static AmbDef amb_def(ThemeID theme) {
    switch (theme) {
        case THEME_DESERT:    // rafales de sable, quasi horizontales
            return (AmbDef){60, 90.0f, 8.0f, 14.0f, 1,
                            (Color){222, 196, 130, 46}};
        case THEME_SWAMP:     // pluie fine, légèrement penchée
            return (AmbDef){70, -18.0f, 160.0f, 4.0f, 1,
                            (Color){170, 210, 190, 40}};
        case THEME_CITY:      // cendres / flocons qui voltigent en tombant
            return (AmbDef){50, 6.0f, 22.0f, 18.0f, 0,
                            (Color){188, 186, 182, 44}};
        case THEME_FACTORY:   // suies qui MONTENT des ateliers
            return (AmbDef){44, 10.0f, -16.0f, 10.0f, 0,
                            (Color){70, 62, 56, 52}};
        case THEME_WASTELAND:
        default:              // poussière ocre en dérive lente
            return (AmbDef){50, 16.0f, 5.0f, 12.0f, 0,
                            (Color){205, 150, 90, 40}};
    }
}

static AmbParticle g_p[AMB_MAX];
static AmbDef      g_def;
static ThemeID     g_theme = THEME_COUNT;   // force le seed au 1er rendu
static float       g_clock = 0.0f;          // horloge accumulée

static void amb_seed(ThemeID theme) {
    g_theme = theme;
    g_def   = amb_def(theme);
    for (int i = 0; i < AMB_MAX; i++) {
        g_p[i].x     = ui_frnd(0.0f, 1.0f) * (float)g_canvas_virt_w;
        g_p[i].y     = ui_frnd(0.0f, 1.0f) * (float)g_canvas_virt_h;
        g_p[i].phase = ui_frnd(0.0f, 1.0f) * 6.28318f;
        g_p[i].speed = 0.6f + ui_frnd(0.0f, 1.0f) * 0.8f;   // variation par particule
    }
}

void ambient_update(float dt) {
    if (g_theme == THEME_COUNT || !g_fx.enabled) return;
    g_clock += dt;
    float spanx = (float)g_canvas_virt_w + AMB_WRAP_PAD * 2.0f;
    float spany = (float)g_canvas_virt_h + AMB_WRAP_PAD * 2.0f;
    for (int i = 0; i < g_def.count && i < AMB_MAX; i++) {
        AmbParticle *p = &g_p[i];
        float sway = sinf(g_clock * AMB_SWAY_HZ + p->phase) * g_def.wobble;
        p->x += (g_def.vx * p->speed + sway) * dt;
        p->y += g_def.vy * p->speed * dt;
        // Rebouclage aux bords : la couche est continue
        if (p->x < -AMB_WRAP_PAD)                              p->x += spanx;
        if (p->x > (float)g_canvas_virt_w + AMB_WRAP_PAD)      p->x -= spanx;
        if (p->y < -AMB_WRAP_PAD)                              p->y += spany;
        if (p->y > (float)g_canvas_virt_h + AMB_WRAP_PAD)      p->y -= spany;
    }
}

void ambient_render(ThemeID theme) {
    if (!g_fx.enabled) return;
    if (theme != g_theme) amb_seed(theme);
    for (int i = 0; i < g_def.count && i < AMB_MAX; i++) {
        const AmbParticle *p = &g_p[i];
        if (g_def.streak) {
            // Trait orienté selon la dérive (pluie fine / rafale de sable)
            float lx = g_def.vx * AMB_STREAK_LEN * p->speed;
            float ly = g_def.vy * AMB_STREAK_LEN * p->speed;
            DrawLineEx((Vector2){p->x, p->y},
                       (Vector2){p->x + lx, p->y + ly}, 1.0f, g_def.col);
        } else {
            int s = 1 + ((i & 3) == 0);   // quelques grains plus gros
            DrawRectangle((int)p->x, (int)p->y, s, s, g_def.col);
        }
    }
}
