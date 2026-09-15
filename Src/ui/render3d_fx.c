/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  render3d_fx.c ─ MODE HÉROS : FX 3D (voir render3d_fx.h).
 *  Coordonnées SIM (pixels carte) sur les 3 axes — la hauteur `h` est en
 *  px et convertie ×W3D_PER_PX au rendu, comme le reste du monde. Le
 *  tick vit dans fx_update (hook) : mêmes horloges/pauses que les FX 2D.
 */
#include "render3d_fx.h"
#include "render3d_world.h"      /* w3d_from_sim, w3d_in_front, fog      */
#include "ui_utils.h"            /* ui_frnd (source unique)              */
#include "../combat/fx.h"        /* hooks g_fx3d_* + toggle g_fx.enabled */
#include "../combat/enemy.h"     /* DAMAGE_TYPE_COUNT                    */
#include <math.h>

/* ── Réglages (à ajuster au playtest — aucun affichage agent) ────── */
#define R3DFX_MAX          256    /* particules simultanées (pool fixe)  */
#define R3DFX_GRAV         420.0f /* gravité verticale (px/s²)           */
#define R3DFX_FRICTION     2.4f   /* amortissement horizontal (1/s)      */
#define R3DFX_GROUND_BRAKE 9.0f   /* freinage au sol (fragments posés)   */
#define R3DFX_IMPACT_N     6      /* particules par impact               */
#define R3DFX_IMPACT_H     10.0f  /* hauteur d'émission (px ≈ corps)     */
#define R3DFX_FRAG_MIN     6      /* fragments de mort (bornes)          */
#define R3DFX_FRAG_MAX     10
#define R3DFX_FRAG_LIFE    0.5f   /* durée de la désintégration (s)      */
#define R3DFX_TRAIL_N      3      /* fantômes de traînée par projectile  */
#define R3DFX_TRAIL_STEP   0.30f  /* espacement des fantômes (monde)     */

typedef struct {
    float x, y, h;                /* position sim (px), h = hauteur      */
    float vx, vy, vh;             /* vitesses (px/s)                     */
    float life, max_life, size;   /* size en px sim                      */
    Color col;
    int   active;
} R3dfxPart;

static R3dfxPart g_p[R3DFX_MAX];

static void spawn(float x, float y, float h, float speed, float up,
                  float life, float size, Color col) {
    for (int i = 0; i < R3DFX_MAX; i++) {
        R3dfxPart *p = &g_p[i];
        if (p->active) continue;
        float ang = ui_frnd(0.0f, 6.28318f);
        float sp  = speed * ui_frnd(0.4f, 1.4f);
        p->x = x; p->y = y; p->h = h;
        p->vx = cosf(ang) * sp;
        p->vy = sinf(ang) * sp;
        p->vh = up * ui_frnd(0.5f, 1.3f);
        p->life = p->max_life = life * ui_frnd(0.8f, 1.2f);
        p->size = size * ui_frnd(0.8f, 1.3f);
        p->col  = col;
        p->active = 1;
        return;
    }
}

/* ── Hooks (mêmes points d'émission que les FX 2D) ───────────────── */

/* fx_burst (mort d'un ennemi, jus divers) → fragments qui tombent.
   n et speed suivent la gerbe 2D : gros ennemi/boss = plus de morceaux. */
static void r3dfx_burst_hook(float x, float y, Color col, int n, float speed) {
    int frags = n / 2;
    if (frags < R3DFX_FRAG_MIN) frags = R3DFX_FRAG_MIN;
    if (frags > R3DFX_FRAG_MAX) frags = R3DFX_FRAG_MAX;
    for (int k = 0; k < frags; k++)
        spawn(x, y, ui_frnd(3.0f, 12.0f), speed * 0.5f, 180.0f,
              R3DFX_FRAG_LIFE, ui_frnd(2.6f, 4.8f), col);
}

void r3dfx_impact(float x, float y, int dmg_type) {
    if (!g_fx.enabled) return;
    if (dmg_type < 0 || dmg_type >= DAMAGE_TYPE_COUNT) dmg_type = 0;
    Color c = W3D_PROJ_COL[dmg_type];
    switch (dmg_type) {
        case DMG_FIRE:      /* braises : lentes, qui montent, persistent */
            for (int k = 0; k < R3DFX_IMPACT_N + 1; k++)
                spawn(x, y, R3DFX_IMPACT_H, 60.0f, 190.0f, 0.45f, 2.2f, c);
            break;
        case DMG_ELECTRIC:  /* étincelles : vives et brèves */
            for (int k = 0; k < R3DFX_IMPACT_N; k++)
                spawn(x, y, R3DFX_IMPACT_H, 210.0f, 70.0f, 0.22f, 1.6f, c);
            break;
        default:            /* éclats standard, couleur du type */
            for (int k = 0; k < R3DFX_IMPACT_N; k++)
                spawn(x, y, R3DFX_IMPACT_H, 120.0f, 95.0f, 0.34f, 1.9f, c);
            break;
    }
}

/* Tick — appelé PAR fx_update (donc mêmes pauses/speed_mult que la 2D,
   et le pool se vide aussi hors mode héros). */
static void r3dfx_tick(float dt) {
    for (int i = 0; i < R3DFX_MAX; i++) {
        R3dfxPart *p = &g_p[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.0f) { p->active = 0; continue; }
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->h += p->vh * dt;
        p->vh -= R3DFX_GRAV * dt;
        if (p->h <= 0.0f) {              /* touche le sol : se pose */
            p->h  = 0.0f;
            p->vh = 0.0f;
            p->vx -= p->vx * R3DFX_GROUND_BRAKE * dt;
            p->vy -= p->vy * R3DFX_GROUND_BRAKE * dt;
        } else {
            p->vx -= p->vx * R3DFX_FRICTION * dt;
            p->vy -= p->vy * R3DFX_FRICTION * dt;
        }
    }
}

void r3dfx_install(void) {
    for (int i = 0; i < R3DFX_MAX; i++) g_p[i].active = 0;
    g_fx3d_burst  = r3dfx_burst_hook;
    g_fx3d_impact = r3dfx_impact;
    g_fx3d_update = r3dfx_tick;
}

/* ── Rendu ───────────────────────────────────────────────────────── */

void r3dfx_render(Camera3D cam) {
    for (int i = 0; i < R3DFX_MAX; i++) {
        const R3dfxPart *p = &g_p[i];
        if (!p->active) continue;
        Vector3 pos = w3d_from_sim(p->x, p->y, p->h * W3D_PER_PX);
        if (!w3d_in_front(cam, pos)) continue;
        float t = p->life / p->max_life;             /* 1 → 0 */
        float s = p->size * W3D_PER_PX * (0.5f + 0.5f * t);
        Color c = w3d_fog_mix(p->col, w3d_fog_factor(pos, cam));
        c.a = (unsigned char)(255.0f * t);
        DrawCube(pos, s, s, s, c);
    }
}

/* Traînée d'un projectile : style par type de dégâts. Le jitter est
   re-tiré chaque frame → l'arc élec grésille, la flamme vacille. */
void r3dfx_proj_trail(Camera3D cam, Vector3 pos, float dxn, float dyn,
                      int dmg_type) {
    if (!g_fx.enabled) return;
    if (dmg_type < 0 || dmg_type >= DAMAGE_TYPE_COUNT) dmg_type = 0;
    Color base = W3D_PROJ_COL[dmg_type];
    float fog  = w3d_fog_factor(pos, cam);
    Color c    = w3d_fog_mix(base, fog);
    float bx   = dxn * R3DFX_TRAIL_STEP;   /* pas monde vers l'arrière */
    float bz   = dyn * R3DFX_TRAIL_STEP;

    if (dmg_type == DMG_ELECTRIC) {        /* arc brisé qui grésille */
        Vector3 tail = { pos.x - bx * (float)R3DFX_TRAIL_N, pos.y,
                         pos.z - bz * (float)R3DFX_TRAIL_N };
        Vector3 mid  = { (pos.x + tail.x) * 0.5f + ui_frnd(-0.12f, 0.12f),
                         pos.y + ui_frnd(-0.12f, 0.12f),
                         (pos.z + tail.z) * 0.5f + ui_frnd(-0.12f, 0.12f) };
        DrawLine3D(pos, mid, c);
        DrawLine3D(mid, tail, c);
        return;
    }
    for (int k = 1; k <= R3DFX_TRAIL_N; k++) {
        float fade = 1.0f - (float)k / (float)(R3DFX_TRAIL_N + 1);
        Color g = c;
        g.a = (unsigned char)(150.0f * fade);
        Vector3 b = { pos.x - bx * (float)k, pos.y, pos.z - bz * (float)k };
        if (dmg_type == DMG_FIRE) {        /* la flamme monte et vacille */
            b.y += 0.06f * (float)k;
            b.x += ui_frnd(-0.04f, 0.04f);
            b.z += ui_frnd(-0.04f, 0.04f);
        }
        DrawSphereEx(b, 0.085f * fade + 0.03f, 6, 6, g);
    }
}
