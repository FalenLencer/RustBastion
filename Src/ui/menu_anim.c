/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_anim.c — Animation cinématique écran titre (v4, 100 % procédural)
 *
 *  Plus AUCUNE sprite sheet : l'ouvrier, la poussière et les étincelles
 *  sont dessinés en primitives raylib (comme les arcs électriques). Seule
 *  la tour utilise son splash art (tex_tower). L'anim tourne donc même si
 *  aucun asset d'animation n'est présent.
 *
 *  Déroulé (inchangé) :
 *    WALK_IN  — l'ouvrier entre en portant la tour, avance vers la cible
 *    SLAM     — plante violemment la tour (s'accroupit, bras vers le bas)
 *    TOWER_POP — scale-pop de la tour + gerbe de poussière + étincelles
 *    VICTORY  — célèbre bras levés
 *    PAUSE    — immobile, puis boucle
 */

#include "menu_anim.h"
#include "ui_anim.h"   /* ea_out_cubic (easing partagé) */
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════
   DIMENSIONS / DURÉES
   ═══════════════════════════════════════════════════════════════ */
#define WORKER_DSP_REF   160.0f   /* hauteur de l'ouvrier à l'écran (px)  */
#define WALK_CADENCE       1.7f   /* cycles de marche par seconde         */

#define TOWER_CARRY_DW    40.0f   /* petite tour portée                   */
#define TOWER_CARRY_DH    60.0f
#define TOWER_PLANT_DW   200.0f   /* grande tour plantée                  */
#define TOWER_PLANT_DH   300.0f

#define DUR_CARRY_WALK     3.0f   /* marche d'entrée (était 7 s : trop long
                                     à chaque lancement — P0.4)          */
#define DUR_SLAM           0.7f
#define DUR_TOWER_POP      1.0f
#define DUR_VICTORY        3.0f
#define DUR_PAUSE          3.0f

/* ═══════════════════════════════════════════════════════════════
   PALETTE OUVRIER (rust-punk, cohérente avec le reste du jeu)
   ═══════════════════════════════════════════════════════════════ */
#define W_HAT      (Color){224, 169,  62, 255}   /* casque jaune-alerte   */
#define W_HAT_D    (Color){170, 122,  40, 255}
#define W_SKIN     (Color){198, 142,  98, 255}
#define W_GOGGLE   (Color){110, 195, 150, 255}   /* lunettes vertes       */
#define W_JACKET   (Color){ 96, 104,  64, 255}   /* veste olive           */
#define W_JACKET_D (Color){ 64,  70,  42, 255}
#define W_PANT     (Color){ 92,  86,  64, 255}   /* pantalon kaki-gris    */
#define W_PANT_D   (Color){ 66,  62,  46, 255}   /* jambe arrière (ombre) */
#define W_BOOT     (Color){ 40,  34,  28, 255}
#define W_GLOVE    (Color){ 44,  38,  30, 255}

/* ═══════════════════════════════════════════════════════════════
   MATHS
   ═══════════════════════════════════════════════════════════════ */
static float lerpf (float a, float b, float t) { return a + (b - a) * t; }
static float clampf(float v, float lo, float hi){ return v < lo ? lo : v > hi ? hi : v; }
static float ease_in (float t){ return t * t; }

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
   OUVRIER PROCÉDURAL — chibi à casque, bottom-aligned

   fx, fy : centre des pieds (sol). h : hauteur d'affichage.
   walk   : phase de marche 0..1 (balancement des jambes).
   armsup : 0 bras le long du corps → 1 bras levés (porter / victoire).
   crouch : 0 debout → 1 accroupi (slam).
   lean   : inclinaison du buste vers l'avant (px, +x = vers la droite).
   bob    : oscillation verticale (px, marche / célébration).
   ═══════════════════════════════════════════════════════════════ */
static void draw_worker_proc(float fx, float fy, float h,
                             float walk, float armsup, float crouch,
                             float lean, float bob)
{
    float baseY = fy - bob;
    float hipV  = (0.42f - 0.10f * crouch) * h;   /* hauteur des hanches   */
    float torso = (0.30f - 0.05f * crouch) * h;   /* hanches → épaules     */
    float shoV  = hipV + torso;
    float hipY  = baseY - hipV;
    float shoY  = baseY - shoV;
    float ux    = fx + lean;                       /* buste incliné         */

    float bodyW = 0.40f * h;
    float legTh = 0.15f * h;
    float armTh = 0.10f * h;
    float rHead = 0.13f * h;

    /* ── Jambes (balancement opposé) ── */
    float sw    = sinf(walk * 6.2832f) * 0.10f * h * (1.f - 0.7f * crouch);
    float hipLx = fx - 0.07f * h, hipRx = fx + 0.07f * h;
    /* jambe arrière (gauche) : plus sombre */
    DrawLineEx((Vector2){hipLx, hipY}, (Vector2){hipLx - sw, fy},
               legTh, W_PANT_D);
    DrawRectangleRounded(
        (Rectangle){hipLx - sw - 0.05f * h, fy - 0.06f * h, 0.16f * h, 0.06f * h},
        0.5f, 4, W_BOOT);
    /* jambe avant (droite) */
    DrawLineEx((Vector2){hipRx, hipY}, (Vector2){hipRx + sw, fy},
               legTh, W_PANT);
    DrawRectangleRounded(
        (Rectangle){hipRx + sw - 0.04f * h, fy - 0.06f * h, 0.17f * h, 0.06f * h},
        0.5f, 4, W_BOOT);

    /* ── Torse (veste) + ceinture ── */
    DrawRectangleRounded(
        (Rectangle){ux - bodyW * 0.5f, shoY, bodyW, hipY - shoY + 0.02f * h},
        0.35f, 6, W_JACKET);
    DrawRectangleRounded(
        (Rectangle){ux - bodyW * 0.5f, shoY, bodyW * 0.32f, hipY - shoY},
        0.35f, 6, W_JACKET_D);                     /* pan d'ombre à gauche  */
    DrawRectangle((int)(ux - bodyW * 0.5f), (int)(hipY - 0.03f * h),
                  (int)bodyW, (int)(0.04f * h), W_JACKET_D);   /* ceinture  */

    /* ── Bras (interpolés bas ↔ haut selon armsup) ── */
    for (int s = -1; s <= 1; s += 2) {
        float shx = ux + s * bodyW * 0.42f;
        float shy = shoY + 0.02f * h;
        float downX = ux + s * bodyW * 0.55f, downY = hipY - 0.01f * h;
        float upX   = ux + s * 0.08f * h,     upY   = shoY - 0.18f * h;
        float hx = lerpf(downX, upX, armsup) + lean * 0.5f;
        float hy = lerpf(downY, upY, armsup);
        DrawLineEx((Vector2){shx, shy}, (Vector2){hx, hy}, armTh, W_JACKET);
        DrawCircleV((Vector2){hx, hy}, armTh * 0.6f, W_GLOVE);   /* gant     */
    }

    /* ── Tête + casque + lunettes ── */
    float hx = ux + lean * 0.3f;
    float hy = shoY - rHead * 0.7f;
    DrawCircleV((Vector2){hx, hy}, rHead, W_SKIN);
    /* dôme du casque (recouvre le haut de la tête) */
    DrawCircleV((Vector2){hx, hy - rHead * 0.45f}, rHead * 0.92f, W_HAT);
    /* visière / bord avant */
    DrawRectangleRounded(
        (Rectangle){hx - rHead * 1.0f, hy - rHead * 0.32f,
                    rHead * 2.3f, rHead * 0.30f},
        0.5f, 4, W_HAT_D);
    /* bande de lunettes (sur le front) */
    DrawRectangleRounded(
        (Rectangle){hx - rHead * 0.78f, hy - rHead * 0.02f,
                    rHead * 1.55f, rHead * 0.34f},
        0.5f, 4, W_GOGGLE);
}

/* ═══════════════════════════════════════════════════════════════
   TOUR (splash art) — porté (rotation) ou planté (vertical)
   ═══════════════════════════════════════════════════════════════ */
static void draw_tower_center(Texture2D tex, float cx, float cy,
                               float dw, float dh, float rot, Color tint)
{
    if (!tex.id) return;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){cx, cy, dw, dh},
        (Vector2){dw * 0.5f, dh * 0.5f}, rot, tint);
}
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
   INIT / CLEANUP — aucun asset d'animation requis (procédural)
   ═══════════════════════════════════════════════════════════════ */
void menu_anim_init(MenuAnimState *a)
{
    memset(a, 0, sizeof(*a));

    /* Seule dépendance : le splash art de la tour (optionnel). */
    const char *pt = "assets/textures/splash_art/tower_sniper.png";
    if (FileExists(pt)) {
        a->tex_tower = LoadTexture(pt);
        SetTextureFilter(a->tex_tower, TEXTURE_FILTER_BILINEAR);
    }

    a->loaded       = 1;                 /* l'ouvrier/poussière sont procéduraux */
    a->phase        = ANIM_PHASE_WALK_IN;
    a->timer        = 0.f;
    a->bg_timer     = 0.f;
    a->weld_spawn_t = 1.0f;
    a->walk_cycle   = 0.f;

    a->tower_x  = 1120.f * 0.35f;
    a->tower_y  = 830.f  * 0.91f;
    a->worker_x = -(TOWER_CARRY_DH + WORKER_DSP_REF * 0.7f);
}

void menu_anim_skip(MenuAnimState *a)
{
    if (!a->loaded) return;
    a->phase         = ANIM_PHASE_VICTORY;
    a->timer         = 0.f;
    a->tower_planted = 1;
    a->tower_scale   = 1.f;
    /* Ouvrier posé à côté de la tour, comme après le slam */
    a->worker_x      = a->tower_x - WORKER_DSP_REF * 0.7f * 0.55f;
    a->walk_cycle    = 0.f;
    for (int i = 0; i < DUST_PARTICLE_MAX; i++) a->dust[i].active = 0;
}

void menu_anim_cleanup(MenuAnimState *a)
{
    if (!a->loaded) return;
    if (a->tex_tower.id) UnloadTexture(a->tex_tower);
    a->loaded = 0;
}

/* ═══════════════════════════════════════════════════════════════
   UPDATE
   ═══════════════════════════════════════════════════════════════ */
void menu_anim_update(MenuAnimState *a, float dt)
{
    if (!a->loaded) return;

    a->bg_timer += dt;

    /* Spawn d'étincelles sur les bords (soudure d'ambiance) */
    a->weld_spawn_t -= dt;
    if (a->weld_spawn_t <= 0.f) {
        float vw = a->tower_x / 0.35f;
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

    /* Étincelles : gravité + décès */
    for (int i = 0; i < WELD_PARTICLE_MAX; i++) {
        WeldParticle *p = &a->weld[i];
        if (!p->active) continue;
        p->life -= dt;
        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
        p->vy += 80.f * dt;
        if (p->life <= 0.f) p->active = 0;
    }

    /* Poussière : gonfle en montant, retombe, s'estompe */
    for (int i = 0; i < DUST_PARTICLE_MAX; i++) {
        DustParticle *p = &a->dust[i];
        if (!p->active) continue;
        p->life -= dt;
        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
        p->vy += 90.f * dt;                 /* retombe doucement */
        p->vx -= p->vx * 1.6f * dt;         /* freinage horizontal */
        if (p->life <= 0.f) p->active = 0;
    }

    a->timer += dt;

    float tgt_x  = a->tower_x;
    float tgt_y  = a->tower_y;
    float slam_x = tgt_x - WORKER_DSP_REF * 0.7f * 0.3f;

    switch (a->phase) {

    /* ── WALK_IN : entre en portant la tour ── */
    case ANIM_PHASE_WALK_IN: {
        float start_x = -(TOWER_CARRY_DH + WORKER_DSP_REF * 0.7f);
        float end_x   = tgt_x - WORKER_DSP_REF * 0.7f * 0.55f;
        float t       = clampf(a->timer / DUR_CARRY_WALK, 0.f, 1.f);
        a->worker_x   = lerpf(start_x, end_x, ea_out_cubic(t));
        a->walk_cycle += dt * WALK_CADENCE;
        if (a->walk_cycle >= 1.f) a->walk_cycle -= 1.f;
        if (a->timer >= DUR_CARRY_WALK) {
            a->phase = ANIM_PHASE_SLAM;
            a->timer = 0.f;
        }
        break;
    }

    /* ── SLAM : plante la tour ── */
    case ANIM_PHASE_SLAM: {
        float prev_x = tgt_x - WORKER_DSP_REF * 0.7f * 0.55f;
        float t      = clampf(a->timer / DUR_SLAM, 0.f, 1.f);
        a->worker_x  = lerpf(prev_x, slam_x, t);
        if (a->timer >= DUR_SLAM) {
            a->phase         = ANIM_PHASE_TOWER_POP;
            a->timer         = 0.f;
            a->tower_planted = 1;
            a->tower_scale   = 0.15f;
            weld_burst(a, tgt_x, tgt_y - 5.f, 18);
            dust_burst(a, tgt_x, tgt_y, DUST_PARTICLE_MAX);
        }
        break;
    }

    /* ── TOWER_POP : scale 0.15→1.2→1.0 ── */
    case ANIM_PHASE_TOWER_POP: {
        float t = clampf(a->timer / DUR_TOWER_POP, 0.f, 1.f);
        if (t <= 0.6f)
            a->tower_scale = lerpf(0.15f, 1.2f, ea_out_cubic(t / 0.6f));
        else
            a->tower_scale = lerpf(1.2f, 1.0f, ease_in((t - 0.6f) / 0.4f));
        a->tower_scale = clampf(a->tower_scale, 0.f, 1.25f);
        a->worker_x -= 45.f * dt;            /* recule sous le choc */
        if (a->timer >= DUR_TOWER_POP) {
            a->phase       = ANIM_PHASE_VICTORY;
            a->timer       = 0.f;
            a->tower_scale = 1.f;
        }
        break;
    }

    /* ── VICTORY ── */
    case ANIM_PHASE_VICTORY:
        if (a->timer >= DUR_VICTORY) {
            a->phase = ANIM_PHASE_PAUSE;
            a->timer = 0.f;
        }
        break;

    /* ── PAUSE ── */
    case ANIM_PHASE_PAUSE:
        if (a->timer >= DUR_PAUSE) {
            a->phase = ANIM_PHASE_RESET;
            a->timer = 0.f;
        }
        break;

    /* ── RESET ── */
    case ANIM_PHASE_RESET:
        a->phase         = ANIM_PHASE_WALK_IN;
        a->timer         = 0.f;
        a->worker_x      = -(TOWER_CARRY_DH + WORKER_DSP_REF * 0.7f);
        a->walk_cycle    = 0.f;
        a->tower_scale   = 0.f;
        a->tower_planted = 0;
        break;

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
    ((MenuAnimState *)a)->tower_x = tgt_x;   /* nourrit update() */
    ((MenuAnimState *)a)->tower_y = tgt_y;

    float feet_y = tgt_y;
    float H      = WORKER_DSP_REF;

    /* ── 1. Arcs électriques (coins) ── */
    {
        float bt = a->bg_timer;
        unsigned char alf = (unsigned char)(45 + 30 * sinf(bt * 2.8f));
        Color arc = {85, 165, 255, alf};
        if ((int)(bt * 1.9f) % 3 != 0)
            draw_arc((Vector2){0, 0},
                     (Vector2){75 + 16 * sinf(bt * 4.1f), 55 + 12 * cosf(bt * 3.7f)},
                     9, arc, bt);
        if ((int)(bt * 2.3f) % 3 != 1)
            draw_arc((Vector2){(float)vw, 0},
                     (Vector2){(float)vw - 82 + 16 * cosf(bt * 5.1f), 50 + 16 * sinf(bt * 4.3f)},
                     9, arc, bt + 1.7f);
        if ((int)(bt * 1.7f) % 4 != 2)
            draw_arc((Vector2){0, (float)vh},
                     (Vector2){65 + 13 * sinf(bt * 3.3f), (float)vh - 44 - 13 * cosf(bt * 4.9f)},
                     9, arc, bt + 3.4f);
        if ((int)(bt * 2.1f) % 3 != 0)
            draw_arc((Vector2){(float)vw, (float)vh},
                     (Vector2){(float)vw - 70 + 20 * cosf(bt * 3.6f), (float)vh - 52 + 17 * sinf(bt * 5.2f)},
                     9, arc, bt + 5.1f);
    }

    /* ── 2. Étincelles ── */
    for (int i = 0; i < WELD_PARTICLE_MAX; i++) {
        const WeldParticle *p = &a->weld[i];
        if (!p->active) continue;
        float fade = clampf(p->life / p->max_life, 0.f, 1.f);
        unsigned char alf = (unsigned char)(fade * 230.f);
        float sz = 2.5f * fade + 0.5f;
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

    /* ── 4. Ouvrier PROCÉDURAL (pose selon la phase) ── */
    {
        float walk = 0.f, armsup = 1.f, crouch = 0.f, lean = 0.f, bob = 0.f;
        switch (a->phase) {
        case ANIM_PHASE_WALK_IN:
            walk   = a->walk_cycle;
            armsup = 1.f;                                     /* porte la tour */
            bob    = fabsf(sinf(a->walk_cycle * 6.2832f)) * 2.0f;
            break;
        case ANIM_PHASE_SLAM: {
            float t = clampf(a->timer / DUR_SLAM, 0.f, 1.f);
            crouch  = ea_out_cubic(t);
            armsup  = 1.f - t;                                /* bras plongent */
            lean    = t * 0.10f * H;
            break;
        }
        case ANIM_PHASE_TOWER_POP: {
            float t = clampf(a->timer / DUR_TOWER_POP, 0.f, 1.f);
            crouch  = (1.f - t) * 0.6f;
            armsup  = 0.2f;
            lean    = -3.f * (1.f - t);                       /* recul */
            break;
        }
        case ANIM_PHASE_VICTORY:
        case ANIM_PHASE_PAUSE:
            armsup = 1.f;                                     /* bras en V */
            bob    = sinf(a->bg_timer * 3.0f) * 2.5f;
            break;
        default: break;
        }
        draw_worker_proc(a->worker_x, feet_y, H, walk, armsup, crouch,
                         lean, bob);
    }

    /* ── 5. Tour portée / en cours de plantation ── */
    if (!a->tower_planted) {
        float carry_cx = a->worker_x;
        float carry_cy = feet_y - WORKER_DSP_REF - TOWER_CARRY_DH * 0.5f + 8.f;

        if (a->phase == ANIM_PHASE_WALK_IN) {
            draw_tower_center(a->tex_tower, carry_cx, carry_cy,
                              TOWER_CARRY_DW, TOWER_CARRY_DH, 90.f, WHITE);
        } else if (a->phase == ANIM_PHASE_SLAM) {
            float st  = clampf(a->timer / DUR_SLAM, 0.f, 1.f);
            float rot = lerpf(90.f, 0.f, ease_in(st));
            float end_cy = tgt_y - TOWER_PLANT_DH * 0.5f;
            float cur_cx = lerpf(carry_cx, tgt_x, ease_in(st));
            float cur_cy = lerpf(carry_cy, end_cy, ease_in(st));
            float dw = lerpf(TOWER_CARRY_DW, TOWER_PLANT_DW, st);
            float dh = lerpf(TOWER_CARRY_DH, TOWER_PLANT_DH, st);
            draw_tower_center(a->tex_tower, cur_cx, cur_cy, dw, dh, rot, WHITE);
        }
    }

    /* ── 6. Poussière PROCÉDURALE (puffs qui gonflent) ── */
    for (int i = 0; i < DUST_PARTICLE_MAX; i++) {
        const DustParticle *p = &a->dust[i];
        if (!p->active) continue;
        float t = clampf(p->life / p->max_life, 0.f, 1.f);   /* 1 → 0 */
        float r = p->r0 * (1.5f - 0.5f * t);                  /* gonfle */
        unsigned char alf = (unsigned char)(t * 150.f);
        DrawCircleV((Vector2){p->x, p->y}, r, (Color){200, 170, 120, alf});
        DrawCircleV((Vector2){p->x, p->y}, r * 0.6f,
                    (Color){225, 200, 155, (unsigned char)(alf * 0.7f)});
    }
}
