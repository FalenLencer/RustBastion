/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_anim.c — Animation cinématique écran titre (v3)
 *
 *  Source rects vérifiés par inspection pixel (inspect_worker.c).
 *  Frames 7-8 ont un y différent (424/406) — rendus bottom-aligned.
 *
 *  Déroulé :
 *    WALK_IN  — ouvrier entre en portant la tour horizontale (7 s)
 *    SLAM     — plante violemment la tour (0.7 s)
 *    TOWER_POP — scale-pop + dust (1.0 s)
 *    VICTORY  — célèbre (3.0 s)
 *    PAUSE    — immobile (3.0 s)
 *    RESET    → boucle
 */

#include "menu_anim.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════
   SOURCE RECTS — inspection pixel confirmée
   anime_better.png 1264×842  (fond blanc retiré)
   ═══════════════════════════════════════════════════════════════ */
static const Rectangle WORKER_SRC[9] = {
    {  22, 310, 125, 177 },   /* 0 walk A         */
    { 161, 310, 114, 177 },   /* 1 walk B         */
    { 287, 310, 122, 177 },   /* 2 walk C         */
    { 431, 319,  99, 167 },   /* 3 carry A        */
    { 555, 333, 114, 154 },   /* 4 carry B        */
    { 679, 318, 148, 168 },   /* 5 victory A      */
    { 832, 309, 125, 176 },   /* 6 victory B      */
    { 962, 343, 159, 143 },   /* 7 slam (accroupi)*/
    {1125, 334, 119, 151 },   /* 8 slam relâché   */
};

/* dust_anim.png 1536×1024 — 5 frames */
static const Rectangle DUST_SRC[5] = {
    {  65, 497, 211,  99 },
    { 295, 358, 383, 248 },
    { 700, 428, 370, 186 },
    {1077, 476, 267, 128 },
    {1353, 531, 163,  64 },
};

/* ═══════════════════════════════════════════════════════════════
   FRAME SETS
   ═══════════════════════════════════════════════════════════════ */
#define WALK_FS   0   /* frames marche (pieds qui bougent)   */
#define WALK_FC   3
#define CARRY_FS  3   /* frames carry (bras levés, statique) */
#define CARRY_FC  2
#define SLAM_FS   7
#define SLAM_FC   2
#define VIC_FS    5
#define VIC_FC    2

/* ═══════════════════════════════════════════════════════════════
   DIMENSIONS D'AFFICHAGE

   Rendu bottom-aligned : la hauteur de chaque frame est
   proportionnelle à sa hauteur source (ref=217px → 160px).
   Les frames "accroupies" (7-8) apparaissent donc plus courtes,
   ce qui est naturel.
   ═══════════════════════════════════════════════════════════════ */
#define WORKER_SRC_REF   177.0f   /* hauteur de référence (frame droite) */
#define WORKER_DSP_REF   160.0f   /* hauteur affichage correspondante     */

/* Tour (rapport 2:3, portrait) */
#define TOWER_CARRY_DW   40.0f   /* petite tour portée                   */
#define TOWER_CARRY_DH   60.0f
#define TOWER_PLANT_DW  200.0f   /* grande tour plantée                  */
#define TOWER_PLANT_DH  300.0f

/* ═══════════════════════════════════════════════════════════════
   DURÉES (secondes)
   ═══════════════════════════════════════════════════════════════ */
#define DUR_CARRY_WALK   7.0f
#define DUR_SLAM         0.7f
#define DUR_TOWER_POP    1.0f
#define DUR_VICTORY      3.0f
#define DUR_PAUSE        3.0f

/* ═══════════════════════════════════════════════════════════════
   MATHS
   ═══════════════════════════════════════════════════════════════ */
static float lerpf (float a, float b, float t) { return a+(b-a)*t; }
static float clampf(float v, float lo, float hi){ return v<lo?lo:v>hi?hi:v; }
static float ease_out(float t){ float u=1.f-t; return 1.f-u*u*u; }
static float ease_in (float t){ return t*t; }

/* ═══════════════════════════════════════════════════════════════
   PARTICULES ÉTINCELLES
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
   ARC ÉLECTRIQUE PROCÉDURAL
   ═══════════════════════════════════════════════════════════════ */
static void draw_arc(Vector2 from, Vector2 to, int segs, Color col, float seed)
{
    if (segs > 18) segs = 18;
    float dx = (to.x-from.x)/segs, dy = (to.y-from.y)/segs;
    float px = -dy, py = dx, len = sqrtf(px*px+py*py);
    if (len > 0.001f) { px /= len; py /= len; }
    Vector2 prev = from;
    for (int i = 1; i <= segs; i++) {
        float t  = (float)i/segs;
        float bx = from.x+dx*i, by = from.y+dy*i;
        float amp = 14.f*(1.f-fabsf(t-.5f)*2.f);
        float n   = sinf(seed*8.3f+i*1.9f)*amp;
        Vector2 cur = {bx+px*n, by+py*n};
        DrawLineEx(prev, cur, 1.3f, col);
        prev = cur;
    }
}

/* ═══════════════════════════════════════════════════════════════
   DESSIN OUVRIER — bottom-aligned, taille proportionnelle

   feet_x = centre X (bas du sprite)
   feet_y = Y du sol
   ═══════════════════════════════════════════════════════════════ */
static void draw_worker(Texture2D tex, int idx,
                         float feet_x, float feet_y,
                         int flip, Color tint)
{
    if (!tex.id || idx < 0 || idx >= 9) return;
    Rectangle src = WORKER_SRC[idx];
    float scale = WORKER_DSP_REF / WORKER_SRC_REF;
    float dh = src.height * scale;
    float dw = src.width  * scale;
    if (flip) src.width = -src.width;
    DrawTexturePro(tex, src,
        (Rectangle){feet_x - dw*0.5f, feet_y - dh, dw, dh},
        (Vector2){0, 0}, 0.f, tint);
}

/* ═══════════════════════════════════════════════════════════════
   DESSIN TOUR — pivot au centre géométrique + rotation
   ═══════════════════════════════════════════════════════════════ */
static void draw_tower_center(Texture2D tex, float cx, float cy,
                               float dw, float dh, float rot, Color tint)
{
    if (!tex.id) return;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){cx, cy, dw, dh},
        (Vector2){dw*0.5f, dh*0.5f},
        rot, tint);
}

/* ═══════════════════════════════════════════════════════════════
   DESSIN TOUR PLANTÉE — pivot bas-centre, verticale
   ═══════════════════════════════════════════════════════════════ */
static void draw_tower_planted(Texture2D tex,
                                float base_cx, float base_y,
                                float dw, float dh, Color tint)
{
    if (!tex.id) return;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){base_cx - dw*0.5f, base_y - dh, dw, dh},
        (Vector2){0, 0}, 0.f, tint);
}

/* ═══════════════════════════════════════════════════════════════
   AVANCEMENT FRAME OUVRIER
   ═══════════════════════════════════════════════════════════════ */
static void frame_tick(MenuAnimState *a, float dt, float fps, int fs, int fc)
{
    a->worker_frame_t += dt;
    if (a->worker_frame_t >= 1.f/fps) {
        a->worker_frame_t -= 1.f/fps;
        int local = (a->worker_frame - fs + 1) % fc;
        a->worker_frame = fs + local;
    }
}

/* ═══════════════════════════════════════════════════════════════
   INIT / CLEANUP
   ═══════════════════════════════════════════════════════════════ */
void menu_anim_init(MenuAnimState *a)
{
    memset(a, 0, sizeof(*a));

    const char *pw = "assets/textures/Animation/anime_better.png";
    const char *pd = "assets/textures/Animation/dust_anim.png";
    const char *pt = "assets/textures/splash_art/tower_sniper.png";

    if (!FileExists(pw)) return;
    a->tex_worker = LoadTexture(pw);
    if (FileExists(pd)) a->tex_dust  = LoadTexture(pd);
    if (FileExists(pt)) a->tex_tower = LoadTexture(pt);

    SetTextureFilter(a->tex_worker, TEXTURE_FILTER_BILINEAR);
    if (a->tex_dust.id)  SetTextureFilter(a->tex_dust,  TEXTURE_FILTER_BILINEAR);
    if (a->tex_tower.id) SetTextureFilter(a->tex_tower, TEXTURE_FILTER_BILINEAR);

    a->loaded = 1;
    a->phase  = ANIM_PHASE_WALK_IN;
    a->timer  = 0.f;
    a->bg_timer     = 0.f;
    a->weld_spawn_t = 1.0f;

    /* Valeurs initiales (recalculées dans render dès le 1er frame) */
    a->tower_x = 1120.f * 0.35f;   /* ≈ 392 */
    a->tower_y = 830.f  * 0.91f;   /* ≈ 705 */

    /* Ouvrier démarre hors-champ gauche */
    a->worker_x       = -(TOWER_CARRY_DH + WORKER_DSP_REF * 0.7f);
    a->worker_frame   = WALK_FS;
    a->worker_frame_t = 0.f;
}

void menu_anim_cleanup(MenuAnimState *a)
{
    if (!a->loaded) return;
    if (a->tex_worker.id) UnloadTexture(a->tex_worker);
    if (a->tex_dust.id)   UnloadTexture(a->tex_dust);
    if (a->tex_tower.id)  UnloadTexture(a->tex_tower);
    a->loaded = 0;
}

/* ═══════════════════════════════════════════════════════════════
   UPDATE
   ═══════════════════════════════════════════════════════════════ */
void menu_anim_update(MenuAnimState *a, float dt)
{
    if (!a->loaded) return;

    a->bg_timer += dt;

    /* Spawn d'étincelles sur les bords */
    a->weld_spawn_t -= dt;
    if (a->weld_spawn_t <= 0.f) {
        float vw = a->tower_x / 0.35f;
        float vh = a->tower_y / 0.65f;
        int side = GetRandomValue(0, 2);
        float sx, sy;
        if (side == 0) {
            sx = (float)GetRandomValue(4, (int)(vw*.22f));
            sy = (float)GetRandomValue(10, (int)(vh-10));
        } else if (side == 1) {
            sx = (float)GetRandomValue((int)(vw*.78f), (int)(vw-4));
            sy = (float)GetRandomValue(10, (int)(vh-10));
        } else {
            sx = (float)GetRandomValue(10, (int)(vw-10));
            sy = (float)GetRandomValue(4, (int)(vh*.2f));
        }
        weld_burst(a, sx, sy, GetRandomValue(2, 5));
        a->weld_spawn_t = 2.0f + (float)GetRandomValue(0, 200)/100.f;
    }

    /* Mise à jour particules */
    for (int i = 0; i < WELD_PARTICLE_MAX; i++) {
        WeldParticle *p = &a->weld[i];
        if (!p->active) continue;
        p->life -= dt;
        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
        p->vy += 80.f  * dt;
        if (p->life <= 0.f) p->active = 0;
    }

    /* Poussière */
    if (a->dust_active) {
        a->dust_frame_t += dt;
        while (a->dust_frame_t >= 1.f/8.f) {
            a->dust_frame_t -= 1.f/8.f;
            a->dust_frame++;
            if (a->dust_frame >= 5) { a->dust_active = 0; a->dust_frame = 0; break; }
        }
    }

    a->timer += dt;

    float tgt_x = a->tower_x;
    float tgt_y = a->tower_y;
    /* "end_x" : centre X où l'ouvrier se place pour planter */
    float slam_x = tgt_x - WORKER_DSP_REF * 0.7f * 0.3f;   /* ≈ tgt_x - 34 */

    switch (a->phase) {

    /* ── WALK_IN : entre en portant la tour, avance vers la cible ── */
    case ANIM_PHASE_WALK_IN: {
        float start_x = -(TOWER_CARRY_DH + WORKER_DSP_REF * 0.7f);
        float end_x   = tgt_x - WORKER_DSP_REF * 0.7f * 0.55f;   /* ≈ tgt_x - 62 */
        float t       = clampf(a->timer / DUR_CARRY_WALK, 0.f, 1.f);
        a->worker_x   = lerpf(start_x, end_x, ease_out(t));

        frame_tick(a, dt, 6.f, WALK_FS, WALK_FC);

        if (a->timer >= DUR_CARRY_WALK) {
            a->phase          = ANIM_PHASE_SLAM;
            a->timer          = 0.f;
            a->worker_frame   = SLAM_FS;
            a->worker_frame_t = 0.f;
        }
        break;
    }

    /* ── SLAM : plante la tour ── */
    case ANIM_PHASE_SLAM: {
        float prev_x = tgt_x - WORKER_DSP_REF * 0.7f * 0.55f;
        float t      = clampf(a->timer / DUR_SLAM, 0.f, 1.f);
        a->worker_x  = lerpf(prev_x, slam_x, t);

        frame_tick(a, dt, (float)SLAM_FC / DUR_SLAM, SLAM_FS, SLAM_FC);

        if (a->timer >= DUR_SLAM) {
            a->phase         = ANIM_PHASE_TOWER_POP;
            a->timer         = 0.f;
            a->tower_planted = 1;
            a->tower_scale   = 0.15f;

            weld_burst(a, tgt_x, tgt_y - 5.f, 18);

            a->dust_active  = 1;
            a->dust_frame   = 0;
            a->dust_frame_t = 0.f;
            a->dust_x       = tgt_x;
            a->dust_y       = tgt_y;

            a->worker_frame   = VIC_FS;
            a->worker_frame_t = 0.f;
        }
        break;
    }

    /* ── TOWER_POP : scale 0.15→1.2→1.0 ── */
    case ANIM_PHASE_TOWER_POP: {
        float t = clampf(a->timer / DUR_TOWER_POP, 0.f, 1.f);
        if (t <= 0.6f)
            a->tower_scale = lerpf(0.15f, 1.2f, ease_out(t / 0.6f));
        else
            a->tower_scale = lerpf(1.2f, 1.0f, ease_in((t - 0.6f) / 0.4f));
        a->tower_scale = clampf(a->tower_scale, 0.f, 1.25f);

        /* L'ouvrier recule un peu sous l'effet du choc */
        a->worker_x -= 45.f * dt;

        frame_tick(a, dt, 3.f, VIC_FS, VIC_FC);

        if (a->timer >= DUR_TOWER_POP) {
            a->phase       = ANIM_PHASE_VICTORY;
            a->timer       = 0.f;
            a->tower_scale = 1.f;
        }
        break;
    }

    /* ── VICTORY ── */
    case ANIM_PHASE_VICTORY: {
        frame_tick(a, dt, 3.f, VIC_FS, VIC_FC);
        if (a->timer >= DUR_VICTORY) {
            a->phase = ANIM_PHASE_PAUSE;
            a->timer = 0.f;
        }
        break;
    }

    /* ── PAUSE ── */
    case ANIM_PHASE_PAUSE: {
        if (a->timer >= DUR_PAUSE) {
            a->phase = ANIM_PHASE_RESET;
            a->timer = 0.f;
        }
        break;
    }

    /* ── RESET ── */
    case ANIM_PHASE_RESET: {
        a->phase          = ANIM_PHASE_WALK_IN;
        a->timer          = 0.f;
        a->worker_x       = -(TOWER_CARRY_DH + WORKER_DSP_REF * 0.7f);
        a->worker_frame   = WALK_FS;
        a->worker_frame_t = 0.f;
        a->worker_flip    = 0;
        a->tower_scale    = 0.f;
        a->tower_planted  = 0;
        a->dust_active    = 0;
        a->dust_frame     = 0;
        break;
    }

    default:
        a->phase = ANIM_PHASE_WALK_IN;
        a->timer = 0.f;
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════
   RENDER
   ═══════════════════════════════════════════════════════════════ */
void menu_anim_render(const MenuAnimState *a, int vw, int vh)
{
    if (!a->loaded) return;

    float tgt_x = (float)vw * 0.35f;
    float tgt_y = (float)vh * 0.91f;
    /* Mise à jour position cible pour update() */
    ((MenuAnimState*)a)->tower_x = tgt_x;
    ((MenuAnimState*)a)->tower_y = tgt_y;

    float feet_y = tgt_y;   /* sol : ouvrier et base de tour au même Y */

    /* ── 1. Arcs électriques (coins) ── */
    {
        float bt = a->bg_timer;
        unsigned char alf = (unsigned char)(45 + 30*sinf(bt*2.8f));
        Color arc = {85, 165, 255, alf};
        if ((int)(bt*1.9f)%3 != 0)
            draw_arc((Vector2){0, 0},
                     (Vector2){75+16*sinf(bt*4.1f), 55+12*cosf(bt*3.7f)},
                     9, arc, bt);
        if ((int)(bt*2.3f)%3 != 1)
            draw_arc((Vector2){(float)vw, 0},
                     (Vector2){(float)vw-82+16*cosf(bt*5.1f), 50+16*sinf(bt*4.3f)},
                     9, arc, bt+1.7f);
        if ((int)(bt*1.7f)%4 != 2)
            draw_arc((Vector2){0, (float)vh},
                     (Vector2){65+13*sinf(bt*3.3f), (float)vh-44-13*cosf(bt*4.9f)},
                     9, arc, bt+3.4f);
        if ((int)(bt*2.1f)%3 != 0)
            draw_arc((Vector2){(float)vw, (float)vh},
                     (Vector2){(float)vw-70+20*cosf(bt*3.6f), (float)vh-52+17*sinf(bt*5.2f)},
                     9, arc, bt+5.1f);
    }

    /* ── 2. Étincelles ── */
    for (int i = 0; i < WELD_PARTICLE_MAX; i++) {
        const WeldParticle *p = &a->weld[i];
        if (!p->active) continue;
        float fade = clampf(p->life / p->max_life, 0.f, 1.f);
        unsigned char alf = (unsigned char)(fade * 230.f);
        float sz = 2.5f*fade + 0.5f;
        Color col = fade > 0.5f ? (Color){255, 190, 30, alf}
                                : (Color){255, 100, 10, alf};
        DrawCircleV((Vector2){p->x, p->y}, sz, col);
    }

    /* ── 3. Tour plantée (derrière l'ouvrier) ── */
    if (a->tower_planted) {
        float dw = TOWER_PLANT_DW * a->tower_scale;
        float dh = TOWER_PLANT_DH * a->tower_scale;
        draw_tower_planted(a->tex_tower, tgt_x, tgt_y, dw, dh, WHITE);
    }

    /* ── 4. Ouvrier ── */
    draw_worker(a->tex_worker, a->worker_frame,
                a->worker_x, feet_y,
                a->worker_flip, WHITE);

    /* ── 5. Tour portée / en train d'être plantée ── */
    if (!a->tower_planted) {
        /* Centre de la tour au-dessus de la tête de l'ouvrier */
        float carry_cx = a->worker_x;
        float carry_cy = feet_y - WORKER_DSP_REF - TOWER_CARRY_DH * 0.5f + 8.f;

        if (a->phase == ANIM_PHASE_WALK_IN) {
            /* Tour horizontale (rotation=90°) portée au-dessus de la tête */
            draw_tower_center(a->tex_tower,
                              carry_cx, carry_cy,
                              TOWER_CARRY_DW, TOWER_CARRY_DH,
                              90.f, WHITE);

        } else if (a->phase == ANIM_PHASE_SLAM) {
            /* Tour pivote 90°→0° et tombe vers la cible */
            float st  = clampf(a->timer / DUR_SLAM, 0.f, 1.f);
            float rot = lerpf(90.f, 0.f, ease_in(st));

            /* Position : du centre de portage vers mi-hauteur de la cible */
            float end_cy = tgt_y - TOWER_PLANT_DH * 0.5f;
            float cur_cx = lerpf(carry_cx, tgt_x,   ease_in(st));
            float cur_cy = lerpf(carry_cy, end_cy,   ease_in(st));

            /* Taille : grandit de la petite taille à la taille finale */
            float dw = lerpf(TOWER_CARRY_DW, TOWER_PLANT_DW, st);
            float dh = lerpf(TOWER_CARRY_DH, TOWER_PLANT_DH, st);

            draw_tower_center(a->tex_tower, cur_cx, cur_cy, dw, dh, rot, WHITE);
        }
    }

    /* ── 6. Dust VFX ── */
    if (a->dust_active && a->tex_dust.id && a->dust_frame < 5) {
        Rectangle src = DUST_SRC[a->dust_frame];
        float dh = 130.f;
        float dw = (src.width / src.height) * dh;
        DrawTexturePro(a->tex_dust, src,
            (Rectangle){a->dust_x - dw*0.5f, a->dust_y - dh*0.91f, dw, dh},
            (Vector2){0, 0}, 0.f, WHITE);
    }
}
