/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "fx.h"
#include "../ui/ui_utils.h"   // dtxt / mtxt (rendu des popups)
#include <math.h>
#include <string.h>
#include <stdio.h>

FxSystem g_fx;

// ── Réglages (pas de nombres magiques épars) ─────────────────
#define FX_GRAVITY        140.0f   // px/s² appliqués aux particules
#define FX_FRICTION         2.2f   // amortissement horizontal (1/s)
#define FX_PART_LIFE_MIN    0.30f
#define FX_PART_LIFE_MAX    0.65f
#define FX_POPUP_LIFE       0.90f  // durée de vie d'un texte flottant (s)
#define FX_POPUP_RISE       34.0f  // vitesse de montée (px/s)
#define FX_SHAKE_DECAY      11.0f  // décroissance exponentielle de la secousse (1/s)
#define FX_SHAKE_MAX        14.0f  // plafond de magnitude (px)

void fx_reset(void) {
    int en = g_fx.enabled;          // conserve la préférence
    memset(&g_fx, 0, sizeof(g_fx));
    g_fx.enabled = en;
}

void fx_update(float dt) {
    if (dt > 0.1f) dt = 0.1f;       // robustesse (gros hoquet de frame)

    for (int i = 0; i < FX_MAX_PARTICLES; i++) {
        FxParticle *p = &g_fx.particles[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.0f) { p->active = 0; continue; }
        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
        p->vy += FX_GRAVITY * dt;
        p->vx -= p->vx * FX_FRICTION * dt;
    }
    for (int i = 0; i < FX_MAX_POPUPS; i++) {
        FxPopup *u = &g_fx.popups[i];
        if (!u->active) continue;
        u->life -= dt;
        if (u->life <= 0.0f) { u->active = 0; continue; }
        u->y += u->vy * dt;
        u->vy -= u->vy * 1.5f * dt;   // ralentit en montant
    }
    g_fx.shake_mag -= g_fx.shake_mag * FX_SHAKE_DECAY * dt;
    if (g_fx.shake_mag < 0.15f) g_fx.shake_mag = 0.0f;
}

static FxParticle *fx_free_particle(void) {
    for (int i = 0; i < FX_MAX_PARTICLES; i++)
        if (!g_fx.particles[i].active) return &g_fx.particles[i];
    return NULL;
}

void fx_burst(float x, float y, Color col, int n, float speed) {
    if (!g_fx.enabled) return;
    for (int k = 0; k < n; k++) {
        FxParticle *p = fx_free_particle();
        if (!p) return;
        float ang = (float)GetRandomValue(0, 359) * DEG2RAD;
        float sp  = speed * (0.4f + (float)GetRandomValue(0, 100) / 100.0f);
        p->x = x; p->y = y;
        p->vx = cosf(ang) * sp;
        p->vy = sinf(ang) * sp - speed * 0.3f;   // léger biais vers le haut
        p->max_life = FX_PART_LIFE_MIN +
            (FX_PART_LIFE_MAX - FX_PART_LIFE_MIN) * (float)GetRandomValue(0, 100) / 100.0f;
        p->life = p->max_life;
        p->size = 2.0f + (float)GetRandomValue(0, 2);
        p->col  = col;
        p->active = 1;
    }
}

void fx_popup(float x, float y, const char *text, Color col) {
    if (!g_fx.enabled || !text) return;
    for (int i = 0; i < FX_MAX_POPUPS; i++) {
        FxPopup *u = &g_fx.popups[i];
        if (u->active) continue;
        u->x = x; u->y = y;
        u->vy = -FX_POPUP_RISE;
        u->life = u->max_life = FX_POPUP_LIFE;
        u->col = col;
        snprintf(u->text, sizeof(u->text), "%s", text);
        u->active = 1;
        return;
    }
}

void fx_popup_gold(float x, float y, int amount) {
    if (amount <= 0) return;
    char b[12];
    snprintf(b, sizeof(b), "+%d", amount);
    fx_popup(x, y, b, (Color){236, 198, 82, 255});
}

void fx_shake(float magnitude) {
    if (!g_fx.enabled) return;
    if (magnitude > FX_SHAKE_MAX) magnitude = FX_SHAKE_MAX;
    if (magnitude > g_fx.shake_mag) g_fx.shake_mag = magnitude;
}

void fx_render_world(void) {
    for (int i = 0; i < FX_MAX_PARTICLES; i++) {
        const FxParticle *p = &g_fx.particles[i];
        if (!p->active) continue;
        float t = p->life / p->max_life;          // 1 → 0
        unsigned char a = (unsigned char)(t * 255.0f);
        float s = p->size * (0.4f + 0.6f * t);
        DrawRectangle((int)(p->x - s*0.5f), (int)(p->y - s*0.5f),
                      (int)(s + 0.5f), (int)(s + 0.5f),
                      (Color){p->col.r, p->col.g, p->col.b, a});
    }
    for (int i = 0; i < FX_MAX_POPUPS; i++) {
        const FxPopup *u = &g_fx.popups[i];
        if (!u->active) continue;
        float t = u->life / u->max_life;          // 1 → 0
        unsigned char a = (unsigned char)((t > 0.5f ? 1.0f : t * 2.0f) * 255.0f);
        int tw = mtxt(u->text, 9);
        dtxt(u->text, (int)u->x - tw/2, (int)u->y, 9,
             (Color){u->col.r, u->col.g, u->col.b, a});
    }
}

float fx_shake_dx(void) {
    if (g_fx.shake_mag <= 0.0f) return 0.0f;
    return (float)GetRandomValue(-100, 100) / 100.0f * g_fx.shake_mag;
}
float fx_shake_dy(void) {
    if (g_fx.shake_mag <= 0.0f) return 0.0f;
    return (float)GetRandomValue(-100, 100) / 100.0f * g_fx.shake_mag;
}
