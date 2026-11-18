/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_anim.c — Cinématique de l'écran titre (v5)
 *
 *  L'ouvrier est un personnage en PIXEL ART ARTICULÉ (menu_anim_worker.c :
 *  squelette + IK, repeint à chaque frame). Ce fichier orchestre la scène :
 *
 *    WALK_IN   — entre, caisse-tour sur l'épaule (pas calés sur la distance :
 *                les pieds ne glissent pas)
 *    WINDUP    — se campe, ramène la caisse derrière la tête (anticipation)
 *    THROW     — détente : lâcher, puis la caisse vole en cloche en tournoyant
 *                pendant que l'ouvrier accompagne le geste
 *    TOWER_POP — la caisse s'écrase, s'ouvre et se DÉPLOIE en tour (onde de
 *                choc, poussière, étincelles) ; l'ouvrier se protège
 *    VICTORY   — il s'époussette les mains puis se tient, poings aux hanches
 *    PAUSE     — respiration, puis la scène boucle
 *
 *  Les changements de phase partent de la pose courante (fondu de poses) :
 *  aucun saut d'une pose à l'autre.
 */

#include "menu_anim.h"
#include "ui_anim.h"   /* ea_out_cubic (easing partagé) */
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════
   DIMENSIONS / DURÉES
   ═══════════════════════════════════════════════════════════════ */
#define TOWER_PLANT_DW   200.0f   /* tour déployée (px)                   */
#define TOWER_PLANT_DH   300.0f
#define TOWER_X_K          0.35f  /* cible : fraction de la largeur       */
#define TOWER_Y_K          0.91f  /* sol : fraction de la hauteur         */

#define WORKER_START_X   -70.0f   /* entre hors champ (px virtuels)       */
#define WORKER_STOP_DX   175.0f   /* s'arrête à cette distance de la cible */

#define DUR_WALK           2.6f
#define DUR_WINDUP         0.50f
#define T_RELEASE          0.14f  /* dans THROW : lâcher de la caisse     */
#define T_FOLLOW           0.30f  /* geste d'accompagnement après lâcher  */
#define DUR_FLIGHT         0.72f
#define DUR_THROW         (T_RELEASE + DUR_FLIGHT)
#define FLIGHT_ARC       150.0f   /* hauteur de la cloche (px)            */
#define FLIGHT_SPIN       (-6.2832f) /* un tour complet vers l'avant      */
#define DUR_TOWER_POP      1.0f
#define T_CRATE_SQUASH     0.12f  /* la caisse s'écrase avant de s'ouvrir */
#define T_FLINCH           0.20f  /* l'ouvrier se protège…                */
#define T_RECOVER0         0.35f  /* … puis se redresse                   */
#define T_RECOVER          0.50f
#define DUR_VICTORY        3.0f
#define T_CLAP_END         1.10f  /* s'époussette les mains jusqu'à…      */
#define CLAP_HZ            2.4f
#define CLAP_SEP           3.2f   /* écart des mains (px d'art)           */
#define DUR_PAUSE          3.0f
#define SKIP_VICTORY_T     1.30f  /* état final : après les claquements   */
#define RING_DUR           0.55f  /* onde de choc au sol                  */
#define RING_R            150.0f
#define SHADOW_RX          18.0f  /* ombre de contact de l'ouvrier (px)   */
#define SHADOW_RY           5.0f

/* ═══════════════════════════════════════════════════════════════
   MATHS
   ═══════════════════════════════════════════════════════════════ */
static float lerpf (float a, float b, float t) { return a + (b - a) * t; }
static float clampf(float v, float lo, float hi){ return v < lo ? lo : v > hi ? hi : v; }
static float ease_in (float t){ return t * t; }
static float smooth01(float t){ t = clampf(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
static float ease_out_quad(float t){ t = clampf(t, 0.f, 1.f); return 1.f - (1.f - t) * (1.f - t); }

/* ═══════════════════════════════════════════════════════════════
   ÉTINCELLES DE SOUDURE (procédural)
   ═══════════════════════════════════════════════════════════════ */
static void weld_spawn(WeldParticle *pool, float sx, float sy)
{
    for (int i = 0; i < WELD_PARTICLE_MAX; i++) {
        if (pool[i].active) continue;
        float ang = (float)GetRandomValue(0, 628) / 100.f;
        float spd = (float)GetRandomValue(20, 90);
        float ml  = 0.3f + (float)GetRandomValue(0, 30) / 100.f;
        pool[i].x        = sx;
        pool[i].y        = sy;
        pool[i].vx       = cosf(ang) * spd;
        pool[i].vy       = sinf(ang) * spd;
        pool[i].life     = ml;
        pool[i].max_life = ml;
        pool[i].active   = 1;
        return;
    }
}
static void weld_burst(MenuAnimState *a, float sx, float sy, int n)
{
    for (int i = 0; i < n; i++) weld_spawn(a->weld, sx, sy);
}

/* ═══════════════════════════════════════════════════════════════
   POUSSIÈRE (procédural) — puffs qui gonflent, montent et s'estompent
   ═══════════════════════════════════════════════════════════════ */
static void dust_burst(MenuAnimState *a, float x, float y, int n)
{
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < DUST_PARTICLE_MAX; i++) {
            if (a->dust[i].active) continue;
            float dir = (float)GetRandomValue(-25, 205) / 100.f;   /* ~horizontal + haut */
            float spd = (float)GetRandomValue(40, 130);
            float ml  = 0.5f + (float)GetRandomValue(0, 45) / 100.f;
            a->dust[i].x        = x + (float)GetRandomValue(-6, 6);
            a->dust[i].y        = y + (float)GetRandomValue(-4, 2);
            a->dust[i].vx       = cosf(dir) * spd;
            a->dust[i].vy       = -fabsf(sinf(dir)) * spd * 0.7f
                                  - (float)GetRandomValue(10, 45);
            a->dust[i].r0       = (float)GetRandomValue(6, 15);
            a->dust[i].life     = ml;
            a->dust[i].max_life = ml;
            a->dust[i].active   = 1;
            break;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
   ARC ÉLECTRIQUE PROCÉDURAL (coins de l'écran)
   ═══════════════════════════════════════════════════════════════ */
static void draw_arc(Vector2 from, Vector2 to, int segs, Color col, float seed)
{
    if (segs > 18) segs = 18;
    float dx = (to.x - from.x) / segs, dy = (to.y - from.y) / segs;
    float px = -dy, py = dx, len = sqrtf(px * px + py * py);
    if (len > 0.001f) { px /= len; py /= len; }
    Vector2 prev = from;
    for (int i = 1; i <= segs; i++) {
        float t  = (float)i / segs;
        float bx = from.x + dx * i, by = from.y + dy * i;
        float amp = 14.f * (1.f - fabsf(t - .5f) * 2.f);
        float n   = sinf(seed * 8.3f + i * 1.9f) * amp;
        Vector2 cur = { bx + px * n, by + py * n };
        DrawLineEx(prev, cur, 1.3f, col);
        prev = cur;
    }
}

/* ═══════════════════════════════════════════════════════════════
   TOUR (splash art) — déployée, ancrée par le bas
   ═══════════════════════════════════════════════════════════════ */
static void draw_tower_planted(Texture2D tex, float base_cx, float base_y,
                                float dw, float dh, Color tint)
{
    if (!tex.id) return;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){base_cx - dw * 0.5f, base_y - dh, dw, dh},
        (Vector2){0, 0}, 0.f, tint);
}

/* ═══════════════════════════════════════════════════════════════
   INIT / CLEANUP
   ═══════════════════════════════════════════════════════════════ */
static void go_phase(MenuAnimState *a, AnimPhase ph)
{
    a->pose_from = a->pose;          /* la phase suivante part d'ICI */
    a->phase     = ph;
    a->timer     = 0.f;
}

static void reset_scene(MenuAnimState *a)
{
    a->phase         = ANIM_PHASE_WALK_IN;
    a->timer         = 0.f;
    a->worker_x      = WORKER_START_X;
    a->walk_cycle    = 0.f;
    a->tower_scale   = 0.f;
    a->tower_planted = 0;
    a->crate_air     = 0;
    a->pose          = worker_pose_walk(0.f);
    a->pose_from     = a->pose;
}

void menu_anim_init(MenuAnimState *a)
{
    memset(a, 0, sizeof(*a));

    /* Seule dépendance : le splash art de la tour (optionnel). */
    const char *pt = "assets/textures/splash_art/tower_sniper.png";
    if (FileExists(pt)) {
        a->tex_tower = LoadTexture(pt);
        SetTextureFilter(a->tex_tower, TEXTURE_FILTER_BILINEAR);
    }
    worker_art_init();

    a->loaded       = 1;                 /* tout le reste est procédural */
    a->weld_spawn_t = 1.0f;
    a->tower_x      = 1120.f * TOWER_X_K;
    a->tower_y      = 830.f  * TOWER_Y_K;
    reset_scene(a);
}

void menu_anim_skip(MenuAnimState *a)
{
    if (!a->loaded) return;
    a->phase         = ANIM_PHASE_VICTORY;
    a->timer         = SKIP_VICTORY_T;
    a->tower_planted = 1;
    a->tower_scale   = 1.f;
    a->crate_air     = 0;
    a->worker_x      = a->tower_x - WORKER_STOP_DX;
    a->pose          = worker_pose_idle(a->bg_timer, 0.f, 0.f);
    a->pose_from     = a->pose;
    for (int i = 0; i < DUST_PARTICLE_MAX; i++) a->dust[i].active = 0;
}

int menu_anim_in_intro(const MenuAnimState *a)
{
    return a->loaded && a->phase < ANIM_PHASE_VICTORY;
}

void menu_anim_cleanup(MenuAnimState *a)
{
    if (!a->loaded) return;
    if (a->tex_tower.id) UnloadTexture(a->tex_tower);
    worker_art_unload();
    a->loaded = 0;
}

/* ═══════════════════════════════════════════════════════════════
   UPDATE
   ═══════════════════════════════════════════════════════════════ */
static void update_particles(MenuAnimState *a, float dt)
{
    /* Spawn d'étincelles sur les bords (soudure d'ambiance) */
    a->weld_spawn_t -= dt;
    if (a->weld_spawn_t <= 0.f) {
        float vw = a->tower_x / TOWER_X_K;
        float vh = a->tower_y / 0.65f;
        int side = GetRandomValue(0, 2);
        float sx, sy;
        if (side == 0) {
            sx = (float)GetRandomValue(4, (int)(vw * .22f));
            sy = (float)GetRandomValue(10, (int)(vh - 10));
        } else if (side == 1) {
            sx = (float)GetRandomValue((int)(vw * .78f), (int)(vw - 4));
            sy = (float)GetRandomValue(10, (int)(vh - 10));
        } else {
            sx = (float)GetRandomValue(10, (int)(vw - 10));
            sy = (float)GetRandomValue(4, (int)(vh * .2f));
        }
        weld_burst(a, sx, sy, GetRandomValue(2, 5));
        a->weld_spawn_t = 2.0f + (float)GetRandomValue(0, 200) / 100.f;
    }
    for (int i = 0; i < WELD_PARTICLE_MAX; i++) {          /* gravité + décès */
        WeldParticle *p = &a->weld[i];
        if (!p->active) continue;
        p->life -= dt;
        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
        p->vy += 80.f * dt;
        if (p->life <= 0.f) p->active = 0;
    }
    for (int i = 0; i < DUST_PARTICLE_MAX; i++) {          /* gonfle, retombe */
        DustParticle *p = &a->dust[i];
        if (!p->active) continue;
        p->life -= dt;
        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
        p->vy += 90.f * dt;
        p->vx -= p->vx * 1.6f * dt;
        if (p->life <= 0.f) p->active = 0;
    }
}

/* Caisse en vol : cloche de la main jusqu'au sol de la cible, un tour complet. */
static void update_flight(MenuAnimState *a, float s)
{
    float land_y = a->tower_y - worker_crate_half_h() * WK_PX;
    a->crate_x   = lerpf(a->rel_x, a->tower_x, s);
    a->crate_y   = lerpf(a->rel_y, land_y, s) - FLIGHT_ARC * 4.f * s * (1.f - s);
    a->crate_rot = lerpf(a->rel_rot, FLIGHT_SPIN, s);
}

static void impact(MenuAnimState *a)
{
    a->crate_air = 2;
    a->crate_x   = a->tower_x;
    a->crate_y   = a->tower_y - worker_crate_half_h() * WK_PX;
    a->crate_rot = 0.f;
    weld_burst(a, a->tower_x, a->tower_y - 8.f, 22);
    dust_burst(a, a->tower_x, a->tower_y, DUST_PARTICLE_MAX);
}

void menu_anim_update(MenuAnimState *a, float dt)
{
    if (!a->loaded) return;
    a->bg_timer += dt;
    a->timer    += dt;
    update_particles(a, dt);

    float stop_x = a->tower_x - WORKER_STOP_DX;

    switch (a->phase) {

    case ANIM_PHASE_WALK_IN: {
        float t  = clampf(a->timer / DUR_WALK, 0.f, 1.f);
        float nx = lerpf(WORKER_START_X, stop_x, ease_out_quad(t));
        /* Cycle calé sur la distance parcourue : le pied d'appui reste collé. */
        a->walk_cycle += (nx - a->worker_x) / WK_PX / worker_walk_stride();
        a->worker_x    = nx;
        a->pose        = worker_pose_walk(a->walk_cycle);
        if (t >= 1.f) go_phase(a, ANIM_PHASE_WINDUP);
        break;
    }

    case ANIM_PHASE_WINDUP: {
        WorkerPose w = worker_pose_windup();
        a->pose = worker_pose_lerp(&a->pose_from, &w, smooth01(a->timer / DUR_WINDUP));
        if (a->timer >= DUR_WINDUP) go_phase(a, ANIM_PHASE_THROW);
        break;
    }

    case ANIM_PHASE_THROW: {
        WorkerPose r = worker_pose_release();
        if (a->timer < T_RELEASE) {                     /* détente explosive */
            a->pose = worker_pose_lerp(&a->pose_from, &r, ease_in(a->timer / T_RELEASE));
            break;
        }
        if (a->crate_air == 0) {                        /* lâcher : la caisse part */
            a->crate_air = 1;
            a->rel_x     = a->worker_x + r.crate.x * WK_PX;
            a->rel_y     = a->tower_y - r.crate.y * WK_PX;
            a->rel_rot   = r.crate_rot;
        }
        WorkerPose f = worker_pose_follow();
        float k = ea_out_cubic(clampf((a->timer - T_RELEASE) / T_FOLLOW, 0.f, 1.f));
        a->pose = worker_pose_lerp(&r, &f, k);
        a->pose.carry = 0.f;
        update_flight(a, clampf((a->timer - T_RELEASE) / DUR_FLIGHT, 0.f, 1.f));
        if (a->timer >= DUR_THROW) { impact(a); go_phase(a, ANIM_PHASE_TOWER_POP); }
        break;
    }

    case ANIM_PHASE_TOWER_POP: {
        /* Déploiement : la caisse s'écrase, puis la tour jaillit 0.15→1.2→1. */
        if (a->timer >= T_CRATE_SQUASH && !a->tower_planted) {
            a->tower_planted = 1;
            a->crate_air     = 0;
            weld_burst(a, a->tower_x, a->tower_y - 40.f, 10);
        }
        float t = clampf((a->timer - T_CRATE_SQUASH) / (DUR_TOWER_POP - T_CRATE_SQUASH), 0.f, 1.f);
        a->tower_scale = (t <= 0.6f) ? lerpf(0.15f, 1.2f, ea_out_cubic(t / 0.6f))
                                     : lerpf(1.2f, 1.0f, ease_in((t - 0.6f) / 0.4f));
        if (!a->tower_planted) a->tower_scale = 0.f;
        /* L'ouvrier se protège du souffle, puis se redresse. */
        WorkerPose fl = worker_pose_flinch(), id = worker_pose_idle(a->bg_timer, 0.f, 0.f);
        if (a->timer < T_RECOVER0)
            a->pose = worker_pose_lerp(&a->pose_from, &fl, ea_out_cubic(clampf(a->timer / T_FLINCH, 0.f, 1.f)));
        else
            a->pose = worker_pose_lerp(&fl, &id, smooth01((a->timer - T_RECOVER0) / T_RECOVER));
        if (a->timer >= DUR_TOWER_POP) { a->tower_scale = 1.f; go_phase(a, ANIM_PHASE_VICTORY); }
        break;
    }

    case ANIM_PHASE_VICTORY: {
        /* S'époussette les mains (claquements), puis poings aux hanches. */
        float cb  = smooth01(a->timer / 0.15f) * (1.f - smooth01((a->timer - (T_CLAP_END - 0.25f)) / 0.25f));
        float sep = CLAP_SEP * (0.5f + 0.5f * cosf(a->timer * CLAP_HZ * 6.2832f));
        a->pose = worker_pose_idle(a->bg_timer, cb, sep);
        if (a->timer >= DUR_VICTORY) go_phase(a, ANIM_PHASE_PAUSE);
        break;
    }

    case ANIM_PHASE_PAUSE:
        a->pose = worker_pose_idle(a->bg_timer, 0.f, 0.f);
        if (a->timer >= DUR_PAUSE) go_phase(a, ANIM_PHASE_RESET);
        break;

    case ANIM_PHASE_RESET:
    default:
        reset_scene(a);
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════
   RENDER
   ═══════════════════════════════════════════════════════════════ */
static void render_arcs(float bt, int vw, int vh)
{
    unsigned char alf = (unsigned char)(45 + 30 * sinf(bt * 2.8f));
    Color arc = {85, 165, 255, alf};
    if ((int)(bt * 1.9f) % 3 != 0)
        draw_arc((Vector2){0, 0},
                 (Vector2){75 + 16 * sinf(bt * 4.1f), 55 + 12 * cosf(bt * 3.7f)}, 9, arc, bt);
    if ((int)(bt * 2.3f) % 3 != 1)
        draw_arc((Vector2){(float)vw, 0},
                 (Vector2){(float)vw - 82 + 16 * cosf(bt * 5.1f), 50 + 16 * sinf(bt * 4.3f)}, 9, arc, bt + 1.7f);
    if ((int)(bt * 1.7f) % 4 != 2)
        draw_arc((Vector2){0, (float)vh},
                 (Vector2){65 + 13 * sinf(bt * 3.3f), (float)vh - 44 - 13 * cosf(bt * 4.9f)}, 9, arc, bt + 3.4f);
    if ((int)(bt * 2.1f) % 3 != 0)
        draw_arc((Vector2){(float)vw, (float)vh},
                 (Vector2){(float)vw - 70 + 20 * cosf(bt * 3.6f), (float)vh - 52 + 17 * sinf(bt * 5.2f)}, 9, arc, bt + 5.1f);
}

void menu_anim_render(const MenuAnimState *a, int vw, int vh)
{
    if (!a->loaded) return;

    float tgt_x = (float)vw * TOWER_X_K;
    float tgt_y = (float)vh * TOWER_Y_K;
    ((MenuAnimState *)a)->tower_x = tgt_x;   /* nourrit update() */
    ((MenuAnimState *)a)->tower_y = tgt_y;

    /* ── 1. Arcs électriques (coins) + étincelles ── */
    render_arcs(a->bg_timer, vw, vh);
    for (int i = 0; i < WELD_PARTICLE_MAX; i++) {
        const WeldParticle *p = &a->weld[i];
        if (!p->active) continue;
        float fade = clampf(p->life / p->max_life, 0.f, 1.f);
        unsigned char alf = (unsigned char)(fade * 230.f);
        Color col = fade > 0.5f ? (Color){255, 190, 30, alf} : (Color){255, 100, 10, alf};
        DrawCircleV((Vector2){p->x, p->y}, 2.5f * fade + 0.5f, col);
    }

    /* ── 2. Tour déployée (derrière l'ouvrier) ── */
    if (a->tower_planted)
        draw_tower_planted(a->tex_tower, tgt_x, tgt_y,
                           TOWER_PLANT_DW * a->tower_scale, TOWER_PLANT_DH * a->tower_scale, WHITE);

    /* ── 3. Onde de choc au sol (impact) ── */
    if (a->phase == ANIM_PHASE_TOWER_POP && a->timer < RING_DUR) {
        float k = a->timer / RING_DUR, r = 20.f + RING_R * ea_out_cubic(k);
        unsigned char al = (unsigned char)((1.f - k) * 200.f);
        DrawEllipseLines((int)tgt_x, (int)tgt_y, r, r * 0.26f, (Color){255, 200, 120, al});
        DrawEllipseLines((int)tgt_x, (int)tgt_y, r * 0.7f, r * 0.18f, (Color){255, 150, 70, (unsigned char)(al / 2)});
    }

    /* ── 4. Ombres au sol : ouvrier + caisse en vol ── */
    float wx = a->worker_x + a->pose.hip.x * WK_PX;
    DrawEllipse((int)wx, (int)tgt_y, SHADOW_RX, SHADOW_RY, (Color){0, 0, 0, 110});
    if (a->crate_air == 1) {
        float h = clampf((tgt_y - a->crate_y) / (FLIGHT_ARC * 1.6f), 0.f, 1.f);
        DrawEllipse((int)a->crate_x, (int)tgt_y, 20.f * (1.f - 0.5f * h), 5.f * (1.f - 0.5f * h),
                    (Color){0, 0, 0, (unsigned char)(110.f * (1.f - 0.6f * h))});
    }

    /* ── 5. Ouvrier (pixel art articulé) ── */
    worker_art_draw(&a->pose, a->worker_x, tgt_y, a->bg_timer);

    /* ── 6. Caisse en vol / qui s'écrase à l'impact ── */
    if (a->crate_air == 1)
        worker_art_draw_crate(a->crate_x, a->crate_y, a->crate_rot, 0.f);
    else if (a->crate_air == 2)
        worker_art_draw_crate(a->crate_x, a->crate_y, 0.f,
                              sinf(3.14159f * clampf(a->timer / T_CRATE_SQUASH, 0.f, 1.f)));

    /* ── 7. Poussière (puffs qui gonflent) ── */
    for (int i = 0; i < DUST_PARTICLE_MAX; i++) {
        const DustParticle *p = &a->dust[i];
        if (!p->active) continue;
        float t = clampf(p->life / p->max_life, 0.f, 1.f);   /* 1 → 0 */
        float r = p->r0 * (1.5f - 0.5f * t);                  /* gonfle */
        unsigned char alf = (unsigned char)(t * 150.f);
        DrawCircleV((Vector2){p->x, p->y}, r, (Color){200, 170, 120, alf});
        DrawCircleV((Vector2){p->x, p->y}, r * 0.6f, (Color){225, 200, 155, (unsigned char)(alf * 0.7f)});
    }
}
