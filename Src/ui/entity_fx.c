/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  entity_fx.c ─ Vie visuelle des unités et des ennemis (cf. entity_fx.h).
 *
 *  Tout est DÉDUIT des pools à chaque frame (apparition = slot activé ;
 *  coup porté = minuteur d'attaque qui remonte) : aucun champ ajouté aux
 *  structures sauvegardées, aucun tirage aléatoire du jeu consommé.
 *  Horloge = GetTime (temps réel), comme renderer_tower_fx.c.
 */
#include "entity_fx.h"
#include "renderer.h"          /* render_fx_impact / render_fx_dust, g_units_3d */
#include "render3d.h"          /* R3D_ENT_FOOT_DY (pied des modèles 3D)          */
#include "ui_anim.h"           /* ea_out_back                                    */
#include <math.h>

// ── Apparition : unité déployée (chute + écrasement) ─────────────
#define EFX_U_DROP_DUR    0.20f
#define EFX_U_SETTLE_DUR  0.26f
#define EFX_U_DROP_H      18.0f   // hauteur de chute (px)
#define EFX_U_SQUASH      0.18f
#define EFX_U_DUST_SCALE  0.55f   // poussière ≈ moitié de celle d'une tour
// ── Apparition : ennemi qui surgit du sol ────────────────────────
#define EFX_E_RISE_DUR    0.38f
#define EFX_E_SCALE0      0.35f   // taille de départ (part de la taille finale)
#define EFX_E_DUST_SCALE  0.60f
#define EFX_SMOKE_COL     ((Color){62, 38, 76, 255})   // fumée violacée des portails
#define EFX_DUST_DUR      0.50f
#define EFX_DONE          9.0f    // « animation terminée » (s)
#define EFX_RESYNC        0.5f    // > 0,5 s sans rendu (menu, chargement) : pas d'anim
#define EFX_JUMP_PX       (TILE_SIZE * 2.0f)   // saut = slot réutilisé
// ── Attaques ─────────────────────────────────────────────────────
#define EFX_TIMER_JUMP    0.02f   // remontée de minuteur = un coup porté
#define EFX_LUNGE_DUR     0.16f   // fente au corps à corps (s)
#define EFX_LUNGE_PX      4.0f
#define EFX_MUZZLE_UP_3D  1.9f    // hauteur de l'arme au-dessus de (x,y), × size (vue 3D)
#define EFX_MUZZLE_UP_2D  0.4f    //   … en vue 2D (sprite centré)
#define EFX_MUZZLE_FWD    0.9f    // avancée de l'arme vers la cible (× size)
#define EFX_TARGET_UP     0.8f    // point visé sur la cible (× sa hauteur d'arme)
#define EFX_SHOT_MAX      96
#define EFX_BULLET_SPD    950.0f  // px/s
#define EFX_SNIPE_SPD     1300.0f
#define EFX_SHELL_SPD     260.0f
#define EFX_SHELL_ARC     0.35f   // hauteur de la cloche (× distance)
#define EFX_FLASH_DUR     0.06f   // éclair de bouche au départ d'un tir (s)
#define EFX_ENGAGE_SLACK  6.0f    // tolérance de portée (px) pour la cible engagée

enum { SHOT_BULLET, SHOT_SNIPE, SHOT_SHELL };
typedef struct { float x0, y0, x1, y1, t, dur; int kind, on; } Shot;
typedef struct { int on, type; float t, lunge, lx, ly, pat, par, px, py; } EntVis;

static EntVis   g_uv[MAX_UNITS], g_ev[MAX_ENEMIES];
static Shot     g_shot[EFX_SHOT_MAX];
static int      g_shot_next;
static double   g_last = -1e9;
static unsigned g_key;
static ThemeID  g_theme;
static float    g_dt;

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static float muzzle_up(void) { return g_units_3d ? EFX_MUZZLE_UP_3D : EFX_MUZZLE_UP_2D; }
static float foot_dy(void)   { return g_units_3d ? R3D_ENT_FOOT_DY : 1.0f; }

// ════════════════════════════════════════════════════
// CIBLE D'UN ENNEMI (lecture seule)
// ════════════════════════════════════════════════════
static int unit_pos(const UnitPool *up, int j, float *tx, float *ty) {
    if (!up || j < 0 || j >= MAX_UNITS || !up->units[j].active) return 0;
    *tx = up->units[j].x; *ty = up->units[j].y;
    return 1;
}

int ent_enemy_target(const Enemy *e, const UnitPool *up, const TowerPool *tp,
                     float *tx, float *ty) {
    if (e->type == ENEMY_ARTILLERY && tp && e->arty_target >= 0 &&
        e->arty_target < MAX_TOWERS && tp->towers[e->arty_target].active) {
        *tx = tp->towers[e->arty_target].cx; *ty = tp->towers[e->arty_target].cy;
        return 1;
    }
    if (!up) return 0;
    if (e->type == ENEMY_HUNTER && unit_pos(up, e->hunt_target, tx, ty)) return 1;
    if (e->raiding && unit_pos(up, e->raid_target, tx, ty)) return 1;
    /* Mêlée : l'unité engagée = la plus proche à portée (même règle que enemy.c). */
    float best = e->unit_atk_range + EFX_ENGAGE_SLACK, b2 = best * best;
    int   bj   = -1;
    for (int j = 0; j < MAX_UNITS; j++) {
        const Unit *u = &up->units[j];
        if (!u->active) continue;
        float dx = u->x - e->x, dy = u->y - e->y, d2 = dx*dx + dy*dy;
        if (d2 < b2) { b2 = d2; bj = j; }
    }
    return unit_pos(up, bj, tx, ty);
}

// ════════════════════════════════════════════════════
// TIRS
// ════════════════════════════════════════════════════
static void shot_add(float x0, float y0, float x1, float y1, int kind) {
    float d   = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    float spd = (kind == SHOT_SHELL) ? EFX_SHELL_SPD : (kind == SHOT_SNIPE) ? EFX_SNIPE_SPD
                                                                           : EFX_BULLET_SPD;
    Shot *s = &g_shot[g_shot_next];
    g_shot_next = (g_shot_next + 1) % EFX_SHOT_MAX;
    *s = (Shot){x0, y0, x1, y1, 0.0f, fmaxf(d / spd, 0.05f), kind, 1};
}

/* Départ d'un tir : l'arme, à hauteur de buste, avancée vers la cible. */
static void fire(float x, float y, float size, float tx, float ty, float tsize, int kind) {
    float dx = tx - x, dy = ty - y, d = sqrtf(dx*dx + dy*dy);
    if (d < 1.0f) return;
    float ux = dx / d, uy = dy / d, up = muzzle_up();
    shot_add(x + ux * size * EFX_MUZZLE_FWD, y + uy * size * EFX_MUZZLE_FWD * 0.5f - size * up,
             tx, ty - tsize * up * EFX_TARGET_UP, kind);
}

/* Fente au corps à corps vers la cible + étincelles sur elle. */
static void lunge(EntVis *v, float x, float y, float tx, float ty, float tsize) {
    float dx = tx - x, dy = ty - y, d = sqrtf(dx*dx + dy*dy);
    if (d < 1.0f) return;
    v->lunge = EFX_LUNGE_DUR; v->lx = dx / d; v->ly = dy / d;
    render_fx_impact(tx, ty - tsize * muzzle_up() * EFX_TARGET_UP, DMG_PHYSICAL, 0);
}

void render_combat_fx(void) {
    for (int i = 0; i < EFX_SHOT_MAX; i++) {
        Shot *s = &g_shot[i];
        if (!s->on) continue;
        s->t += g_dt;
        float u = s->t / s->dur;
        if (u >= 1.0f) {                                   /* arrivée : impact */
            s->on = 0;
            render_fx_impact(s->x1, s->y1, (s->kind == SHOT_SHELL) ? DMG_FIRE : DMG_PHYSICAL,
                             s->kind == SHOT_SHELL);
            continue;
        }
        float dx = s->x1 - s->x0, dy = s->y1 - s->y0, d = sqrtf(dx*dx + dy*dy);
        float ux = dx / d, uy = dy / d;
        Vector2 h = {s->x0 + dx * u, s->y0 + dy * u};
        if (s->t < EFX_FLASH_DUR) {                        /* éclair de bouche */
            float k = 1.0f - s->t / EFX_FLASH_DUR;
            DrawCircleV((Vector2){s->x0, s->y0}, 2.0f + 3.0f * k, (Color){255, 220, 140, (unsigned char)(200.0f * k)});
        }
        if (s->kind == SHOT_SHELL) {                       /* obus en cloche + fumée */
            h.y -= EFX_SHELL_ARC * d * 4.0f * u * (1.0f - u);
            for (int k = 1; k <= 3; k++) {
                float uk = fmaxf(0.0f, u - 0.05f * k);
                Vector2 p = {s->x0 + dx * uk, s->y0 + dy * uk - EFX_SHELL_ARC * d * 4.0f * uk * (1.0f - uk)};
                DrawCircleV(p, 1.0f + 0.6f * k, (Color){90, 84, 78, (unsigned char)(150 - 40 * k)});
            }
            DrawCircleV(h, 3.2f, (Color){30, 26, 22, 255});
            DrawCircleV(h, 1.4f, (Color){255, 150, 60, 255});
        } else {                                           /* traçante */
            float len = (s->kind == SHOT_SNIPE) ? 18.0f : 10.0f;
            Color c   = (s->kind == SHOT_SNIPE) ? (Color){255, 90, 60, 255} : (Color){255, 214, 110, 255};
            Vector2 tl = {h.x - ux * len, h.y - uy * len};
            DrawLineEx(tl, h, 2.2f, (Color){c.r, c.g, c.b, 110});
            DrawLineEx((Vector2){h.x - ux * len * 0.5f, h.y - uy * len * 0.5f}, h, 1.2f,
                       (Color){255, 250, 225, 230});
            DrawCircleV(h, 1.4f, (Color){255, 255, 240, 255});
        }
    }
}

// ════════════════════════════════════════════════════
// SUIVI (1×/frame)
// ════════════════════════════════════════════════════
/* Nouvelle entité dans le slot (activé, type changé ou saut = réemploi). */
static int vis_fresh(EntVis *v, int type, float x, float y, int sync) {
    float jx = x - v->px, jy = y - v->py;
    int fresh = !v->on || v->type != type || jx*jx + jy*jy > EFX_JUMP_PX * EFX_JUMP_PX;
    if (fresh) {
        v->on = 1; v->type = type; v->lunge = 0.0f;
        v->t  = sync ? EFX_DONE : 0.0f;
    } else {
        v->t += g_dt;
    }
    v->px = x; v->py = y;
    v->lunge = fmaxf(0.0f, v->lunge - g_dt);
    return fresh;
}

void ent_fx_update(const UnitPool *up, const EnemyPool *ep,
                   const TowerPool *tp, const Map *map) {
    double   now  = GetTime();
    unsigned key  = (unsigned)map->seed * 2654435761u ^ ((unsigned)map->w << 8) ^ (unsigned)map->h;
    int      sync = (now - g_last > EFX_RESYNC) || key != g_key;
    g_dt = (float)(now - g_last);
    if (g_dt < 0.0f || g_dt > 0.1f) g_dt = 0.0f;
    g_last = now; g_key = key; g_theme = map->theme;

    for (int i = 0; up && i < MAX_UNITS; i++) {
        const Unit *u = &up->units[i];
        EntVis     *v = &g_uv[i];
        if (!u->active) { v->on = 0; continue; }
        int fresh = vis_fresh(v, (int)u->type, u->x, u->y, sync);
        /* Coup porté : tir à distance (soldat), fente au contact sinon. */
        if (!fresh && u->atk_timer > v->pat + EFX_TIMER_JUMP && ep &&
            u->target_idx >= 0 && u->target_idx < MAX_ENEMIES &&
            ep->enemies[u->target_idx].active) {
            const Enemy *e = &ep->enemies[u->target_idx];
            if (u->atk_range > UNIT_MELEE_ATK_THRESHOLD)
                fire(u->x, u->y, u->size, e->x, e->y, e->size, SHOT_BULLET);
            else
                lunge(v, u->x, u->y, e->x, e->y, e->size);
        }
        v->pat = u->atk_timer;
    }

    for (int i = 0; ep && i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        EntVis      *v = &g_ev[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) { v->on = 0; continue; }
        int fresh = vis_fresh(v, (int)e->type, e->x, e->y, sync);
        int hit_a = !fresh && e->atk_timer  > v->pat + EFX_TIMER_JUMP;
        int hit_r = !fresh && e->arty_timer > v->par + EFX_TIMER_JUMP;
        v->pat = e->atk_timer; v->par = e->arty_timer;
        if (!hit_a && !hit_r) continue;
        float tx, ty;
        if (!ent_enemy_target(e, up, tp, &tx, &ty)) continue;   /* siège d'une base */
        float tsize = hit_r ? (float)TILE_SIZE * 0.3f : e->size;   /* tour : buste fictif */
        if (e->type == ENEMY_ARTILLERY)   fire(e->x, e->y, e->size, tx, ty, tsize, SHOT_SHELL);
        else if (e->type == ENEMY_HUNTER) fire(e->x, e->y, e->size, tx, ty, tsize, SHOT_SNIPE);
        else                              lunge(v, e->x, e->y, tx, ty, tsize);
    }
}

// ════════════════════════════════════════════════════
// CORPS + SOL
// ════════════════════════════════════════════════════
static void body_lunge(const EntVis *v, EntBody *b) {
    if (v->lunge <= 0.0f) return;
    float w = sinf(PI * (1.0f - v->lunge / EFX_LUNGE_DUR));
    b->ox = v->lx * EFX_LUNGE_PX * w;
    b->oy = v->ly * EFX_LUNGE_PX * w * 0.5f;   /* sol vu en 3/4 : aplati */
}

void ent_fx_unit_body(int i, EntBody *b) {
    *b = (EntBody){0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f};
    if (i < 0 || i >= MAX_UNITS || !g_uv[i].on) return;
    const EntVis *v = &g_uv[i];
    float t = v->t;
    if (t < EFX_U_DROP_DUR) {                                 /* chute */
        float u = t / EFX_U_DROP_DUR;
        b->dy = -EFX_U_DROP_H * (1.0f - u * u);
        b->sx = 0.92f; b->sy = 1.10f;
        b->alpha = clamp01(u * 3.0f);
    } else if (t < EFX_U_DROP_DUR + EFX_U_SETTLE_DUR) {       /* écrasement */
        float u = (t - EFX_U_DROP_DUR) / EFX_U_SETTLE_DUR;
        float a = EFX_U_SQUASH * (1.0f - u) * cosf(u * PI * 2.5f);
        b->sy = 1.0f - a; b->sx = 1.0f + a * 0.8f;
    }
    body_lunge(v, b);
}

void ent_fx_enemy_body(int i, EntBody *b) {
    *b = (EntBody){0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f};
    if (i < 0 || i >= MAX_ENEMIES || !g_ev[i].on) return;
    const EntVis *v = &g_ev[i];
    if (v->t < EFX_E_RISE_DUR) {                              /* surgit du sol */
        float u = v->t / EFX_E_RISE_DUR;
        float s = EFX_E_SCALE0 + (1.0f - EFX_E_SCALE0) * ea_out_back(u);
        b->sx = s; b->sy = s;
        b->alpha = clamp01(u * 2.0f);
    }
    body_lunge(v, b);
}

void ent_fx_draw_ground(const UnitPool *up, const EnemyPool *ep) {
    float fd = foot_dy();
    for (int i = 0; up && i < MAX_UNITS; i++) {
        const EntVis *v = &g_uv[i];
        const Unit   *u = &up->units[i];
        float ti = v->t - EFX_U_DROP_DUR;
        if (!v->on || !u->active || ti < 0.0f || ti >= EFX_DUST_DUR) continue;
        render_fx_dust(u->x, u->y + u->size * fd, ti / EFX_DUST_DUR, (unsigned)i * 977u + 13u,
                       EFX_U_DUST_SCALE, render_fx_dust_color(g_theme));
    }
    for (int i = 0; ep && i < MAX_ENEMIES; i++) {
        const EntVis *v = &g_ev[i];
        const Enemy  *e = &ep->enemies[i];
        if (!v->on || !e->active || v->t >= EFX_DUST_DUR) continue;
        float u = v->t / EFX_DUST_DUR, fy = e->y + e->size * fd;
        /* trou de sortie sombre qui se referme + fumée violacée */
        DrawEllipse((int)e->x, (int)fy, e->size * 1.6f * (1.0f - u), e->size * 0.6f * (1.0f - u),
                    (Color){20, 10, 26, (unsigned char)(150.0f * (1.0f - u))});
        render_fx_dust(e->x, fy, u, (unsigned)i * 131u + 7u, EFX_E_DUST_SCALE, EFX_SMOKE_COL);
    }
}
