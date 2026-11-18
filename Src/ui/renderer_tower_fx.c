/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  renderer_tower_fx.c ─ Les tours dans le décor : pose, sol, tirs.
 *
 *    • POSE : une tour qui apparaît TOMBE sur sa case, s'écrase et rebondit ;
 *      à l'impact, son socle peint (terrain_draw_tower_pad) s'étale, un
 *      nuage de poussière et une onde partent au sol, la caméra tressaille.
 *    • SOL  : socle + ombre de contact (ellipse douce) sous chaque tour — la
 *      tour s'ancre au terrain au lieu d'être un sprite posé dessus.
 *    • TIRS : chaque projectile part de la BOUCHE du canon 3D (et non du pied
 *      de la tour), avec une traçante propre au type ; étincelles d'impact.
 *
 *  État PUREMENT VISUEL, déduit du pool de tours/projectiles à chaque frame :
 *  rien n'est sauvegardé, aucun tirage aléatoire du jeu n'est consommé
 *  (particules analytiques, hachées). Horloge = GetTime (temps réel).
 */
#include "renderer.h"
#include "render3d.h"
#include "terrain.h"
#include "ui_anim.h"            /* ea_out_cubic / ea_out_back */
#include "../combat/fx.h"       /* fx_shake (tressaillement à l'impact) */
#include "../combat/tower.h"
#include "../map/theme.h"
#include <math.h>

// ── Pose (chute → écrasement → rebond) ───────────────────────────
#define TFX_DROP_DUR    0.26f   // chute (s)
#define TFX_SETTLE_DUR  0.34f   // écrasement + rebonds amortis (s)
#define TFX_DROP_H      38.0f   // hauteur de chute (px monde)
#define TFX_SQUASH      0.17f   // écrasement maximal à l'impact
#define TFX_BOUNCES     2.5f    // demi-oscillations du rebond
#define TFX_PAD_GROW    0.24f   // étalement du socle après l'impact (s)
#define TFX_PAD_MIN     0.55f   // taille du socle au moment de l'impact
#define TFX_SHAKE       2.6f    // tressaillement caméra à l'impact (px)
#define TFX_RESYNC      0.5f    // > 0,5 s sans rendu (menu, chargement) : pas d'anim
// ── Poussière + onde au sol ──────────────────────────────────────
#define TFX_DUST_N      20
#define TFX_DUST_DUR    0.60f
#define TFX_DUST_R0     10.0f   // rayon de départ (px)
#define TFX_DUST_R      24.0f   // course radiale (px)
#define TFX_DUST_RISE   7.0f    // montée des volutes (px)
#define TFX_RING_DUR    0.32f
#define TFX_RING_R0     9.0f
#define TFX_RING_R      30.0f
// ── Ombre de contact (ellipse douce au pied du modèle) ───────────
#define TFX_SHADOW_RX   17.0f
#define TFX_SHADOW_RY   7.5f
#define TFX_SHADOW_DX   1.5f    // lumière du terrain : NO → ombre vers le SE
#define TFX_SHADOW_DY   2.5f
#define TFX_SHADOW_A    70
// ── Projectiles ──────────────────────────────────────────────────
#define TFX_JUMP_PX     (TILE_SIZE * 1.5f)   // saut = slot réutilisé (nouveau tir)
#define TFX_HOP_PX      14.0f   // saut de cible = rebond de chaîne (Tesla)
#define TFX_SRC_TOL     6.0f    // tolérance tour-source (px) + 1 pas de vol
#define TFX_IMPACT_MAX  48
#define TFX_IMPACT_DUR  0.22f

typedef struct { int on, tx, ty, type; float t; } TowerVis;
static TowerVis g_tv[MAX_TOWERS];
static double   g_last = -1e9;
static unsigned g_map_key;

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static unsigned hash3(unsigned a, unsigned b, unsigned c) {
    unsigned h = a * 73856093u ^ b * 19349663u ^ c * 83492791u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static float hrand(unsigned a, unsigned b, unsigned c) {
    return (float)(hash3(a, b, c) & 0xffffu) / 65535.0f;
}

/* Teinte de poussière par thème : terre soulevée, nettement PLUS CLAIRE que
   le sol (sinon elle s'y confond et l'impact ne se lit pas). */
Color render_fx_dust_color(ThemeID th) {
    switch (th) {
        case THEME_SWAMP:   return (Color){184, 188, 146, 255};
        case THEME_DESERT:  return (Color){240, 218, 176, 255};
        case THEME_CITY:    return (Color){206, 204, 200, 255};
        case THEME_FACTORY: return (Color){196, 186, 172, 255};
        default:            return (Color){214, 188, 152, 255};
    }
}

/* Suivi des tours : une tour nouvelle (slot activé, ou case/type changés)
   démarre l'animation de pose. Après un trou de rendu ou un changement de
   carte (chargement, menu), les tours présentes sont considérées POSÉES. */
static void tower_fx_update(const TowerPool *tp, const Map *map) {
    double   now  = GetTime();
    unsigned key  = (unsigned)map->seed * 2654435761u ^ ((unsigned)map->w << 8) ^ (unsigned)map->h;
    int      sync = (now - g_last > TFX_RESYNC) || key != g_map_key;
    float    dt   = (float)(now - g_last);
    if (dt < 0.0f || dt > 0.1f) dt = 0.0f;
    g_last = now; g_map_key = key;

    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        TowerVis    *v  = &g_tv[i];
        if (!tw->active) { v->on = 0; continue; }
        if (!v->on || v->tx != tw->tile_x || v->ty != tw->tile_y || v->type != (int)tw->type) {
            v->on = 1; v->tx = tw->tile_x; v->ty = tw->tile_y; v->type = (int)tw->type;
            v->t  = sync ? (TFX_DROP_DUR + TFX_DUST_DUR) : 0.0f;
            continue;
        }
        float before = v->t;
        v->t += dt;
        if (before < TFX_DROP_DUR && v->t >= TFX_DROP_DUR) fx_shake(TFX_SHAKE);
    }
}

void tower_fx_drop(int i, float *dy, float *sx, float *sy, float *alpha) {
    *dy = 0.0f; *sx = 1.0f; *sy = 1.0f; *alpha = 1.0f;
    if (i < 0 || i >= MAX_TOWERS || !g_tv[i].on) return;
    float t = g_tv[i].t;
    if (t < TFX_DROP_DUR) {                       /* chute accélérée, étirée */
        float u = t / TFX_DROP_DUR;
        *dy = -TFX_DROP_H * (1.0f - u * u);
        *sx = 0.93f; *sy = 1.08f;
        *alpha = clamp01(u * 3.0f);
    } else if (t < TFX_DROP_DUR + TFX_SETTLE_DUR) { /* écrasement + rebonds */
        float u = (t - TFX_DROP_DUR) / TFX_SETTLE_DUR;
        float a = TFX_SQUASH * (1.0f - u) * cosf(u * PI * TFX_BOUNCES);
        *sy = 1.0f - a;
        *sx = 1.0f + a * 0.8f;
    }
}

/* Poussière soulevée à l'impact : volutes carrées (pixel art) qui s'étalent
   en anneau aplati (sol vu de dessus) et montent en s'estompant. scale :
   1 = pose d'une tour ; plus petit pour une unité, un ennemi qui surgit. */
void render_fx_dust(float cx, float cy, float u, unsigned seed, float scale, Color col) {
    float e = ea_out_cubic(u);
    unsigned char a = (unsigned char)(235.0f * powf(1.0f - u, 1.3f));
    for (int k = 0; k < TFX_DUST_N; k++) {
        float ang = ((float)k + 0.6f * hrand(seed, (unsigned)k, 1u)) * 6.2832f / TFX_DUST_N;
        float sp  = 0.55f + 0.45f * hrand(seed, (unsigned)k, 2u);
        float r   = (TFX_DUST_R0 + TFX_DUST_R * e * sp) * scale;
        float x   = cx + cosf(ang) * r;
        float y   = cy + sinf(ang) * r * 0.55f - TFX_DUST_RISE * scale * e * hrand(seed, (unsigned)k, 3u);
        /* volutes de 2 à 4 px (grain du décor), ombrées par-dessous */
        int   s   = 2 * (1 + (int)(1.9f * hrand(seed, (unsigned)k, 4u) * (1.0f - u * 0.6f)));
        DrawRectangle((int)x - s / 2, (int)y - s / 2 + 1, s, s, (Color){0, 0, 0, (unsigned char)(a / 4)});
        DrawRectangle((int)x - s / 2, (int)y - s / 2, s, s, (Color){col.r, col.g, col.b, a});
    }
}

void tower_fx_draw_ground(const TowerPool *tp, const Map *map) {
    tower_fx_update(tp, map);
    ThemeID th   = map->theme;
    Color   dust = render_fx_dust_color(th);
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        if (!tw->active) continue;
        float    t   = g_tv[i].t;
        unsigned key = hash3((unsigned)tw->tile_x, (unsigned)tw->tile_y, 0x9AD5u);

        /* Socle : apparaît à l'impact, s'étale (léger dépassement), reste. */
        if (t >= TFX_DROP_DUR) {
            float g = clamp01((t - TFX_DROP_DUR) / TFX_PAD_GROW);
            float s = TFX_PAD_MIN + (1.0f - TFX_PAD_MIN) * ea_out_back(g);
            terrain_draw_tower_pad(th, tw->cx, tw->cy, s, clamp01(g * 2.5f), key);
        }

        /* Ombre de contact : grandit et fonce pendant la chute. */
        float sk = (t < TFX_DROP_DUR) ? 0.35f + 0.65f * (t / TFX_DROP_DUR) * (t / TFX_DROP_DUR) : 1.0f;
        float sx = tw->cx + TFX_SHADOW_DX, sy = tw->cy + TFX_SHADOW_DY;
        DrawEllipse((int)sx, (int)sy, TFX_SHADOW_RX * sk, TFX_SHADOW_RY * sk,
                    (Color){8, 6, 4, (unsigned char)(TFX_SHADOW_A * 0.6f * sk)});
        DrawEllipse((int)sx, (int)sy, TFX_SHADOW_RX * 0.70f * sk, TFX_SHADOW_RY * 0.70f * sk,
                    (Color){8, 6, 4, (unsigned char)(TFX_SHADOW_A * sk)});

        /* Impact : onde au sol + poussière. */
        float ti = t - TFX_DROP_DUR;
        if (ti >= 0.0f && ti < TFX_RING_DUR) {
            float u = ti / TFX_RING_DUR, r = TFX_RING_R0 + TFX_RING_R * ea_out_cubic(u);
            unsigned char a = (unsigned char)(170.0f * (1.0f - u));
            DrawEllipseLines((int)tw->cx, (int)tw->cy + 2, r, r * 0.5f, (Color){dust.r, dust.g, dust.b, a});
            DrawEllipseLines((int)tw->cx, (int)tw->cy + 2, r - 1.0f, r * 0.5f - 1.0f,
                             (Color){255, 240, 210, (unsigned char)(a / 2)});
        }
        if (ti >= 0.0f && ti < TFX_DUST_DUR)
            render_fx_dust(tw->cx, tw->cy + 3.0f, ti / TFX_DUST_DUR, key, 1.0f, dust);
    }
}

// ════════════════════════════════════════════════════
// PROJECTILES — départ à la bouche, traçantes, impacts
// ════════════════════════════════════════════════════
typedef struct { int live; float x, y, tx, ty, sx, sy, lx, ly; int dmg, splash; } ProjVis;
typedef struct { float x, y, t; int dmg, splash, on; } Impact;
static ProjVis g_pv[MAX_PROJECTILES];
static Impact  g_imp[TFX_IMPACT_MAX];
static int     g_imp_next;
static double  g_imp_clock = -1.0;

/* Dimensionné par DAMAGE_TYPE_COUNT : DMG_FIRE (= 5, ajouté après coup)
   lisait hors tableau à chaque projectile de lance-flammes. */
static const Color DMG_COLORS[DAMAGE_TYPE_COUNT] = {
    [DMG_PHYSICAL] = {255, 200, 100, 255},
    [DMG_POISON]   = { 80, 220,  60, 255},
    [DMG_ELECTRIC] = {180, 100, 255, 255},
    [DMG_CRYO]     = {140, 220, 255, 255},
    [DMG_NANO]     = {220, 100, 220, 255},
    [DMG_FIRE]     = {255, 107,  53, 255},   /* = PROJ_COLOR[TOWER_FLAME] */
};
static Color dmg_col(int d) {
    return DMG_COLORS[(d >= 0 && d < DAMAGE_TYPE_COUNT) ? d : 0];
}

void render_fx_impact(float x, float y, int dmg, int big) {
    Impact *m = &g_imp[g_imp_next];
    g_imp_next = (g_imp_next + 1) % TFX_IMPACT_MAX;
    *m = (Impact){x, y, 0.0f, dmg, big, 1};
}

/* Tour d'où part un projectile neuf : même type, centre à ≤ 1 pas de vol. */
static void proj_source(const TowerPool *tp, const Projectile *p, ProjVis *v) {
    float best = TFX_SRC_TOL + p->speed / 30.0f, b2 = best * best;
    int   bi   = -1;
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        if (!tw->active || tw->type != p->origin) continue;
        float dx = tw->cx - p->x, dy = tw->cy - p->y, d2 = dx*dx + dy*dy;
        if (d2 < b2) { b2 = d2; bi = i; }
    }
    v->sx = p->x; v->sy = p->y; v->lx = 0.0f; v->ly = 0.0f;
    if (bi < 0) return;                                 /* rebond depuis un ennemi */
    const Tower *tw = &tp->towers[bi];
    Vector2 m;
    Rectangle dst = render3d_tower_dst(bi, tw->cx, tw->cy, (float)TILE_SIZE);
    v->sx = tw->cx; v->sy = tw->cy;
    if (render3d_tower_muzzle(bi, dst, 1, &m)) { v->lx = m.x - tw->cx; v->ly = m.y - tw->cy; }
}

/* Traçante selon l'arme : head = position affichée, (ux,uy) = direction. */
static void draw_shot(int origin, int splash, Color col, Vector2 head, float ux, float uy,
                      unsigned seed, float t) {
    switch (origin) {
        case TOWER_SNIPER: {                 /* trait long et fin, très vif */
            Vector2 tail = {head.x - ux * 30.0f, head.y - uy * 30.0f};
            DrawLineEx(tail, head, 3.0f, (Color){col.r, col.g, col.b, 70});
            DrawLineEx((Vector2){head.x - ux * 18.0f, head.y - uy * 18.0f}, head, 1.4f,
                       (Color){255, 250, 225, 235});
            DrawCircleV(head, 2.0f, (Color){255, 255, 240, 255});
            break;
        }
        case TOWER_FLAME: {                  /* boule de feu + braises en traîne */
            float fl = 0.85f + 0.25f * sinf(t * 40.0f + (float)seed);
            for (int k = 3; k >= 1; k--) {
                Vector2 e = {head.x - ux * 5.0f * k, head.y - uy * 5.0f * k
                                                    - 1.5f * hrand(seed, (unsigned)k, 7u)};
                DrawCircleV(e, 2.6f - 0.5f * k, (Color){255, 140 - 25 * k, 40, (unsigned char)(200 - 45 * k)});
            }
            DrawCircleV(head, 6.5f * fl, (Color){210, 60, 20, 110});
            DrawCircleV(head, 4.2f * fl, (Color){255, 128, 36, 230});
            DrawCircleV(head, 2.0f * fl, (Color){255, 240, 170, 255});
            break;
        }
        case TOWER_TESLA: {                  /* éclair brisé qui suit la boule */
            Vector2 prev = {head.x - ux * 20.0f, head.y - uy * 20.0f};
            unsigned tb = (unsigned)(t * 30.0f);
            for (int s = 1; s <= 4; s++) {
                float f = (float)s / 4.0f, j = (hrand(seed, (unsigned)s, tb) * 2.0f - 1.0f) * 4.0f * (1.0f - f);
                Vector2 cur = {head.x - ux * 20.0f * (1.0f - f) - uy * j,
                               head.y - uy * 20.0f * (1.0f - f) + ux * j};
                DrawLineEx(prev, cur, 3.0f, (Color){col.r, col.g, col.b, 110});
                DrawLineEx(prev, cur, 1.2f, (Color){235, 220, 255, 230});
                prev = cur;
            }
            DrawCircleV(head, 5.0f, (Color){col.r, col.g, col.b, 100});
            DrawCircleV(head, 2.4f, (Color){240, 230, 255, 255});
            break;
        }
        default: {                           /* mitrailleuse : traçante courte */
            Vector2 tail = {head.x - ux * 14.0f, head.y - uy * 14.0f};
            DrawLineEx(tail, head, 2.4f, (Color){col.r, col.g, col.b, 120});
            float r = splash ? 4.5f : 3.0f;
            DrawCircleV(head, r + 2.5f, (Color){col.r, col.g, col.b, 90});
            DrawCircleV(head, r, col);
            DrawCircleV(head, r * 0.5f, (Color){255, 255, 235, 255});
            break;
        }
    }
}

static void draw_impacts(float dt) {
    for (int i = 0; i < TFX_IMPACT_MAX; i++) {
        Impact *m = &g_imp[i];
        if (!m->on) continue;
        m->t += dt;
        if (m->t >= TFX_IMPACT_DUR) { m->on = 0; continue; }
        float u = m->t / TFX_IMPACT_DUR, big = m->splash ? 1.8f : 1.0f;
        Color c = dmg_col(m->dmg);
        unsigned char a = (unsigned char)(230.0f * (1.0f - u));
        DrawCircleV((Vector2){m->x, m->y}, (3.0f + 7.0f * u) * big, (Color){c.r, c.g, c.b, (unsigned char)(a / 3)});
        DrawCircleV((Vector2){m->x, m->y}, (2.5f - 2.0f * u) * big, (Color){255, 250, 220, a});
        for (int k = 0; k < 5; k++) {                     /* étincelles */
            float ang = hrand((unsigned)i, (unsigned)k, (unsigned)(m->x * 7.0f)) * 6.2832f;
            float r0  = (2.0f + 6.0f * u) * big, r1 = r0 + 3.0f * (1.0f - u) * big;
            DrawLineEx((Vector2){m->x + cosf(ang) * r0, m->y + sinf(ang) * r0},
                       (Vector2){m->x + cosf(ang) * r1, m->y + sinf(ang) * r1}, 1.2f,
                       (Color){c.r, c.g, c.b, a});
        }
    }
}

void render_projectiles(const TowerPool *tp) {
    double now = GetTime();
    float  dt  = (g_imp_clock < 0.0) ? 0.0f : (float)(now - g_imp_clock);
    if (dt < 0.0f || dt > 0.1f) dt = 0.0f;
    g_imp_clock = now;
    float t = (float)now;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *p = &tp->projectiles[i];
        ProjVis *v = &g_pv[i];
        if (!p->active) {                                   /* tir terminé : impact */
            if (v->live) render_fx_impact(v->tx, v->ty, v->dmg, v->splash);
            v->live = 0;
            continue;
        }
        float jx = p->x - v->x, jy = p->y - v->y;
        if (!v->live || jx*jx + jy*jy > TFX_JUMP_PX * TFX_JUMP_PX) {   /* tir neuf */
            if (v->live) render_fx_impact(v->tx, v->ty, v->dmg, v->splash);
            proj_source(tp, p, v);
        } else {
            float hx = p->tx - v->tx, hy = p->ty - v->ty;
            if (hx*hx + hy*hy > TFX_HOP_PX * TFX_HOP_PX) {             /* rebond */
                render_fx_impact(p->x, p->y, v->dmg, v->splash);
                v->sx = p->x; v->sy = p->y; v->lx = 0.0f; v->ly = 0.0f;
            }
        }
        v->live = 1; v->x = p->x; v->y = p->y; v->tx = p->tx; v->ty = p->ty;
        v->dmg = (int)p->dmg_type; v->splash = p->splash;

        /* Couleur : type de dégâts si matériau appliqué, sinon couleur de la tour */
        Color col = (p->dmg_type != DMG_PHYSICAL || p->origin == TOWER_FLAME ||
                     p->origin == TOWER_TESLA) ? dmg_col(p->dmg_type) : PROJ_COLOR[p->origin];

        /* Départ à la bouche : décalage (bouche − centre) qui s'annule à
           mesure que le tir avance vers sa cible. */
        float trav = sqrtf((p->x - v->sx) * (p->x - v->sx) + (p->y - v->sy) * (p->y - v->sy));
        float rem  = sqrtf((p->tx - p->x) * (p->tx - p->x) + (p->ty - p->y) * (p->ty - p->y));
        float lift = 1.0f - trav / (trav + rem + 0.001f);
        Vector2 head = {p->x + v->lx * lift, p->y + v->ly * lift};
        float dx = p->tx - head.x, dy = p->ty - head.y, len = sqrtf(dx*dx + dy*dy);
        if (len < 0.001f) len = 0.001f;
        draw_shot((int)p->origin, p->splash, col, head, dx / len, dy / len, (unsigned)i, t);
    }
    draw_impacts(dt);
}
