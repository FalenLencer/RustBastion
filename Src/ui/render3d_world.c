/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  render3d_world.c ─ MODE HÉROS : la partie entière en vraie 3D.
 *  Terrain extrudé depuis la carte générée, bases/portails/minerais,
 *  entités via les modèles GLB déjà chargés (render3d_*_draw_world),
 *  repli formes colorées si un modèle manque. Une seule passe 3D.
 */
#include "render3d_world.h"
#include "render3d_terrain.h"    /* sol procédural habillé               */
#include "render3d_fx.h"         /* FX 3D : impacts, fragments, traînées */
#include "renderer.h"            /* renderer_*_color, palettes           */
#include "render3d.h"            /* render3d_tower_draw_world            */
#include "../combat/combat_math.h" /* angle_approach (source unique)      */
#include "render3d_units.h"      /* render3d_units_draw_world            */
#include "render3d_enemies.h"    /* render3d_enemies_draw_world          */
#include "../game/app.h"
#include "../game/hero.h"
#include "../map/theme.h"
#include "rlgl.h"
#include <math.h>

/* ── Réglages visuels (à ajuster au playtest — aucun affichage agent) ──
   (W3D_RUIN_H vit dans le header : partagé avec la collision du héros.) */
#define W3D_BASE_W        2.6f    /* largeur du bâtiment de base          */
#define W3D_BASE_H        2.3f    /* hauteur du bâtiment de base          */
#define W3D_PORTAL_H      2.4f    /* hauteur des portails de spawn        */
#define W3D_TOWER_SCALE   1.0f    /* échelle des modèles de tour          */
#define W3D_UNIT_SCALE    1.0f    /* échelle des modèles d'unité          */
#define W3D_ENEMY_SCALE   1.0f    /* échelle des modèles d'ennemi         */
#define W3D_BOSS_MULT     1.8f    /* boss : facteur de taille             */
#define W3D_HERO_SCALE    1.0f    /* échelle du modèle du héros           */
#define W3D_PROJ_R        0.10f   /* rayon des projectiles de tour        */
#define W3D_PROJ_H        0.95f   /* hauteur de vol des projectiles       */

/* Couleurs des constructions (le sol dérive, lui, du thème :
   cf. render3d_terrain.c) */
#define W3D_COL_PORTAL    (Color){140,  46,  46, 255}
#define W3D_COL_BUILDING  (Color){ 92,  96, 104, 255}

/* ── Ambiance atmosphérique (ciel + brouillard, dérivés du thème) ──
   Le ciel est un dégradé vertical (bg du thème éclairci), le brouillard
   fond la géométrie lointaine vers la couleur d'HORIZON — coupé en vue
   tactique (une vue de planification doit rester lisible). */
#define W3D_SKY_TOP_D      14     /* delta RGB : haut du ciel (vs bg)     */
#define W3D_SKY_HOR_D      58     /* delta RGB : horizon (plus clair)     */
#define W3D_FOG_DENSITY    0.013f /* brume exp : ~40 % à 40 u. monde      */

static Color g_fog_col     = {0, 0, 0, 255};
static float g_fog_density = 0.0f;

void w3d_fog_setup(Color col, float density) {
    g_fog_col = col;
    g_fog_density = density;
}
float w3d_fog_factor(Vector3 p, Camera3D cam) {
    if (g_fog_density <= 0.0f) return 0.0f;
    float dx = p.x - cam.position.x;
    float dy = p.y - cam.position.y;
    float dz = p.z - cam.position.z;
    float f = 1.0f - expf(-g_fog_density * sqrtf(dx*dx + dy*dy + dz*dz));
    return (f < 0.0f) ? 0.0f : f;
}
Color w3d_fog_mix(Color c, float f) {
    return (Color){
        (unsigned char)((float)c.r + ((float)g_fog_col.r - (float)c.r) * f),
        (unsigned char)((float)c.g + ((float)g_fog_col.g - (float)c.g) * f),
        (unsigned char)((float)c.b + ((float)g_fog_col.b - (float)c.b) * f),
        c.a };
}

/* Projectiles de tour : couleur par type de dégâts (indices DamageType).
   Exporté (render3d_world.h) : traînées et impacts 3D la réutilisent. */
const Color W3D_PROJ_COL[DAMAGE_TYPE_COUNT] = {
    {235, 220, 160, 255},   /* physique  */
    { 60, 200,  90, 255},   /* poison    */
    {120, 210, 245, 255},   /* électrique*/
    {150, 220, 255, 255},   /* cryo      */
    {190, 120, 240, 255},   /* nano      */
    {250, 150,  60, 255},   /* feu       */
};

/* ── Perf : cull + throttle du skinning CPU ────────────────────────
   UpdateModelAnimation ré-écrit les maillages sur CPU : c'est LE coût
   des grosses vagues. Entités DERRIÈRE la caméra : pas dessinées du
   tout. Entités lointaines : pose mise à jour 1 frame sur N.          */
#define W3D_CULL_BEHIND   (2.0f * W3D_TILE)  /* marge dos-caméra (monde) */
#define W3D_ANIM_NEAR     20.0f              /* anim chaque frame si <   */
#define W3D_ANIM_SKIP     2                  /* sinon 1 frame sur N      */
static unsigned g_wframe = 0;                /* compteur de frames scène */

/* (Ombres « blob » : w3d_blob_shadow + constantes W3D_SHADOW_*, dans
   render3d_terrain.c — c'est du décor au sol, exposé par son header.) */

/* 1 = l'entité (position monde) est devant la caméra (à dessiner).
   Exporté : le terrain (render3d_terrain.c) partage le même cull. */
int w3d_in_front(Camera3D cam, Vector3 p) {
    float fx = cam.target.x - cam.position.x;
    float fy = cam.target.y - cam.position.y;
    float fz = cam.target.z - cam.position.z;
    float dx = p.x - cam.position.x;
    float dy = p.y - cam.position.y;
    float dz = p.z - cam.position.z;
    return (fx * dx + fy * dy + fz * dz) > -W3D_CULL_BEHIND;
}

/* 1 = mettre à jour la pose d'anim de l'entité i ce frame. */
static int world_anim_tick(Camera3D cam, Vector3 p, int i) {
    float dx = p.x - cam.position.x, dz = p.z - cam.position.z;
    if (dx * dx + dz * dz < W3D_ANIM_NEAR * W3D_ANIM_NEAR) return 1;
    return ((g_wframe + (unsigned)i) % W3D_ANIM_SKIP) == 0u;
}

/* Suivi de cap + horloge d'anim par entité (côté rendu uniquement) */
static float g_eh[MAX_ENEMIES], g_ea[MAX_ENEMIES];
static float g_epx[MAX_ENEMIES], g_epy[MAX_ENEMIES];
static float g_uh[MAX_UNITS],   g_ua[MAX_UNITS];
static float g_upx[MAX_UNITS],  g_upy[MAX_UNITS];

/* Rotation douce des caps (même helper que tower.c/render3d_units). */
#define W3D_TURN_SPEED 7.5f   /* rad/s — vitesse de rotation des modèles */
/* Rotation douce des caps : angle_approach (combat_math.h). */

/* Cap monde depuis un déplacement sim (dx, dy) : atan2(x, z), LISSÉ
   (angle_approach) pour ne plus « regarder le mur » en glissant.
   Retourne 1 si l'entité a bougé cette frame — seuil TRÈS bas : une
   unité lente (0,3 px/frame) doit quand même jouer sa marche. */
static int heading_update(float *head, float *ppx, float *ppy,
                          float x, float y, float ft) {
    float dx = x - *ppx, dy = y - *ppy;
    float d2 = dx * dx + dy * dy;
    int moved = (d2 > 0.0009f);                 /* > 0,03 px : ça marche  */
    if (d2 > 0.36f)                             /* > 0,6 px : cap fiable  */
        *head = angle_approach(*head, atan2f(dx, dy),
                                   W3D_TURN_SPEED * ft);
    *ppx = x; *ppy = y;
    return moved;
}

/* ── Bases + portails ──────────────────────────────────────────── */
static void world_draw_bases(const Map *map, Camera3D cam) {
    for (int b = 0; b < map->base_count; b++) {
        const BaseInfo *bi = &map->bases[b];
        Vector3 c = { (bi->pos.x + 0.5f) * W3D_TILE, 0.0f,
                      (bi->pos.y + 0.5f) * W3D_TILE };
        float f = w3d_fog_factor(c, cam);
        if (!bi->active) {   /* ruine de base tombée */
            DrawCube((Vector3){c.x, 0.35f, c.z}, W3D_BASE_W * 0.8f, 0.7f,
                     W3D_BASE_W * 0.8f,
                     w3d_fog_mix((Color){60, 52, 48, 255}, f));
            continue;
        }
        /* Bâtiment + porte (côté hangar : là où on recrute) */
        DrawCube((Vector3){c.x, W3D_BASE_H * 0.5f, c.z},
                 W3D_BASE_W, W3D_BASE_H, W3D_BASE_W,
                 w3d_fog_mix(W3D_COL_BUILDING, f));
        DrawCubeWires((Vector3){c.x, W3D_BASE_H * 0.5f, c.z},
                      W3D_BASE_W, W3D_BASE_H, W3D_BASE_W,
                      w3d_fog_mix((Color){30, 32, 38, 255}, f));
        DrawCube((Vector3){c.x, 0.55f, c.z + W3D_BASE_W * 0.5f},
                 0.9f, 1.1f, 0.08f,
                 w3d_fog_mix((Color){40, 44, 52, 255}, f));
        if (bi->is_primary) {   /* antenne = base principale */
            DrawCylinder((Vector3){c.x, W3D_BASE_H, c.z}, 0.03f, 0.03f,
                         1.1f, 6,
                         w3d_fog_mix((Color){180, 180, 190, 255}, f));
            DrawSphere((Vector3){c.x, W3D_BASE_H + 1.15f, c.z}, 0.09f,
                       (Color){230, 90, 70, 255});
        }
        /* Barre de PV au-dessus du toit */
        float ratio = (bi->max_hp > 0) ? (float)bi->hp / (float)bi->max_hp : 0.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        float bw = W3D_BASE_W * 0.9f;
        DrawCube((Vector3){c.x, W3D_BASE_H + 0.35f, c.z}, bw, 0.1f, 0.1f,
                 (Color){40, 14, 14, 255});
        DrawCube((Vector3){c.x - bw * (1.0f - ratio) * 0.5f,
                           W3D_BASE_H + 0.35f, c.z},
                 bw * ratio, 0.12f, 0.12f,
                 (Color){70, 200, 90, 255});
    }

    /* Portails de spawn (pilier + linteau + fond sombre) */
    for (int p = 0; p < map->path_count; p++) {
        const PathDef *pd = &map->paths[p];
        if (!pd->active) continue;
        Vector3 c = { (pd->spawn.x + 0.5f) * W3D_TILE, 0.0f,
                      (pd->spawn.y + 0.5f) * W3D_TILE };
        float f = w3d_fog_factor(c, cam);
        Color pc = w3d_fog_mix(W3D_COL_PORTAL, f);
        float half = W3D_TILE * 0.42f;
        DrawCube((Vector3){c.x - half, W3D_PORTAL_H * 0.5f, c.z},
                 0.24f, W3D_PORTAL_H, 0.24f, pc);
        DrawCube((Vector3){c.x + half, W3D_PORTAL_H * 0.5f, c.z},
                 0.24f, W3D_PORTAL_H, 0.24f, pc);
        DrawCube((Vector3){c.x, W3D_PORTAL_H, c.z},
                 half * 2.0f + 0.24f, 0.22f, 0.26f, pc);
        DrawCube((Vector3){c.x, W3D_PORTAL_H * 0.45f, c.z},
                 half * 2.0f - 0.05f, W3D_PORTAL_H * 0.9f, 0.06f,
                 w3d_fog_mix((Color){16, 8, 12, 235}, f));
    }
}

/* ── Entités ───────────────────────────────────────────────────── */
static void world_draw_towers(const TowerPool *tp, const EnemyPool *ep) {
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        if (!tw->active) continue;
        Vector3 pos = w3d_from_sim(tw->cx, tw->cy, 0.0f);
        if (!render3d_tower_draw_world_aimed(tw, i, ep, pos,
                                             W3D_TOWER_SCALE)) {
            /* Repli : socle + fût colorés */
            Color c = renderer_tower_color(tw->type);
            DrawCylinder(pos, 0.55f, 0.7f, 0.5f, 10, (Color){70, 72, 80, 255});
            DrawCylinder((Vector3){pos.x, 0.5f, pos.z}, 0.3f, 0.35f, 1.1f, 10, c);
        }
        if (tw->stun_timer > 0.0f)   /* étourdie par l'onde EMP du boss */
            DrawSphere((Vector3){pos.x, 2.1f, pos.z}, 0.14f,
                       (Color){250, 220, 80, 230});
    }
}

static void world_draw_units(const UnitPool *up, const EnemyPool *ep,
                             const HeroState *h, Camera3D cam) {
    float ft = GetFrameTime();
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &up->units[i];
        if (!u->active) continue;
        int mv = heading_update(&g_uh[i], &g_upx[i], &g_upy[i],
                                u->x, u->y, ft);
        /* En ATTAQUE : face à l'ENNEMI visé (pas au mur / au déplacement). */
        if (u->state == USTATE_ATTACK &&
            u->target_idx >= 0 && u->target_idx < MAX_ENEMIES) {
            const Enemy *te = &ep->enemies[u->target_idx];
            if (te->active && !te->dead)
                g_uh[i] = angle_approach(g_uh[i],
                              atan2f(te->x - u->x, te->y - u->y),
                              W3D_TURN_SPEED * ft);
        }
        g_ua[i] += ft;
        Vector3 pos = w3d_from_sim(u->x, u->y, 0.0f);
        if (!w3d_in_front(cam, pos)) continue;     /* dos caméra : rien */
        /* Ombre au sol : ancre le modèle */
        float sr = u->size * W3D_PER_PX * W3D_SHADOW_R_MULT;
        if (sr < W3D_SHADOW_R_MIN) sr = W3D_SHADOW_R_MIN;
        w3d_blob_shadow(cam, pos.x, W3D_SHADOW_Y, pos.z, sr, 0.0f);
        /* Anim selon l'état RÉEL : attaque/minage > marche > repos.
           (USTATE_COLLECT → anim "mine" de l'ouvrier via a_attack.) */
        int kind = (u->state == USTATE_ATTACK ||
                    u->state == USTATE_COLLECT) ? 2 : (mv ? 1 : 0);
        if (!render3d_units_draw_world((int)u->type, pos, g_uh[i],
                                       W3D_UNIT_SCALE, kind, g_ua[i],
                                       world_anim_tick(cam, pos, i))) {
            Color c = renderer_unit_color(u->type);
            DrawCylinder(pos, 0.22f, 0.26f, 1.0f, 8, c);
            DrawSphere((Vector3){pos.x, 1.12f, pos.z}, 0.2f, c);
        }
        /* Ouvrier chargé : cube du minerai porté au-dessus de la tête */
        if (u->type == UNIT_WORKER && u->has_material &&
            u->carried_mat != MAT_NONE)
            DrawCube((Vector3){pos.x, 1.5f, pos.z}, 0.26f, 0.26f, 0.26f,
                     MATERIAL_COLORS[u->carried_mat]);

        /* REPÈRE OUVRIER : balise verte flottante — c'est via un ouvrier
           que le héros construit une tour ([E]). Doit être trouvable de loin. */
        if (u->type == UNIT_WORKER) {
            float bob = sinf((float)GetTime() * 2.5f + (float)i) * 0.12f;
            float by  = 1.85f + bob;
            Color gc  = (Color){60, 230, 90, 255};
            rlPushMatrix();
                rlTranslatef(pos.x, by, pos.z);
                rlRotatef((float)GetTime() * 90.0f, 0.0f, 1.0f, 0.0f);
                DrawCube((Vector3){0, 0, 0}, 0.24f, 0.24f, 0.24f, gc);
                DrawCubeWires((Vector3){0, 0, 0}, 0.30f, 0.30f, 0.30f,
                              (Color){220, 255, 220, 255});
            rlPopMatrix();
            /* Fin faisceau vertical pour le repérage à distance. */
            DrawCube((Vector3){pos.x, by * 0.5f, pos.z}, 0.04f, by, 0.04f,
                     (Color){80, 230, 110, 90});
        }
    }
    (void)h;
}

static void world_draw_enemies(const EnemyPool *ep, Camera3D cam) {
    float ft = GetFrameTime();
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
        int mv = heading_update(&g_eh[i], &g_epx[i], &g_epy[i],
                                e->x, e->y, ft);
        g_ea[i] += ft;
        Vector3 pos = w3d_from_sim(e->x, e->y, 0.0f);
        if (!w3d_in_front(cam, pos)) continue;     /* dos caméra : rien */
        float sc = W3D_ENEMY_SCALE * (e->is_boss ? W3D_BOSS_MULT : 1.0f);

        /* Ombre au sol (pas pour le spectre invisible) */
        if (!e->invisible) {
            float sr = e->size * W3D_PER_PX * W3D_SHADOW_R_MULT
                       * (e->is_boss ? W3D_BOSS_MULT : 1.0f);
            if (sr < W3D_SHADOW_R_MIN) sr = W3D_SHADOW_R_MIN;
            w3d_blob_shadow(cam, pos.x, W3D_SHADOW_Y, pos.z, sr, 0.0f);
        }

        /* Anneaux d'état au sol (gelé / empoisonné / boss) */
        if (e->slow_timer > 0.0f)
            DrawCircle3D((Vector3){pos.x, 0.03f, pos.z}, 0.55f * sc,
                         (Vector3){1, 0, 0}, 90.0f, (Color){140, 220, 255, 220});
        if (e->poison_timer > 0.0f)
            DrawCircle3D((Vector3){pos.x, 0.06f, pos.z}, 0.45f * sc,
                         (Vector3){1, 0, 0}, 90.0f, (Color){90, 220, 90, 220});
        if (e->is_boss) {
            float pulse = (sinf((float)GetTime() * 4.0f) + 1.0f) * 0.5f;
            DrawCircle3D((Vector3){pos.x, 0.05f, pos.z},
                         (0.9f + 0.25f * pulse) * sc,
                         (Vector3){1, 0, 0}, 90.0f, (Color){230, 60, 60, 240});
            /* Télégraphe de capacité : anneau qui gonfle avant l'effet */
            if (e->telegraph_timer > 0.0f)
                DrawCircle3D((Vector3){pos.x, 0.08f, pos.z},
                             BOSS_STUN_RADIUS * W3D_TILE, (Vector3){1, 0, 0},
                             90.0f, (Color){250, 210, 60, 230});
        }

        /* Spectre invisible : silhouette fantôme seulement */
        if (e->invisible) {
            DrawSphere((Vector3){pos.x, W3D_ENEMY_BODY_H, pos.z},
                       e->size * W3D_PER_PX + 0.15f,
                       (Color){200, 220, 255, 60});
            continue;
        }

        /* Anim selon l'état RÉEL : en train de frapper > marche > repos. */
        int kind = (e->engage_timer > 0.0f || e->atk_timer > 0.0f) ? 2
                 : (mv ? 1 : 0);
        if (!render3d_enemies_draw_world((int)e->type, pos, g_eh[i],
                                         sc, kind, g_ea[i],
                                         world_anim_tick(cam, pos, i))) {
            Color c = renderer_enemy_color(e->type);
            float r = e->size * W3D_PER_PX + 0.12f;
            DrawSphere((Vector3){pos.x, W3D_ENEMY_BODY_H, pos.z}, r, c);
        }
        /* Éclair blanc bref à l'impact (même feedback que la 2D) */
        if (e->hit_flash > 0.3f)
            DrawSphere((Vector3){pos.x, W3D_ENEMY_BODY_H, pos.z},
                       e->size * W3D_PER_PX + 0.2f,
                       (Color){255, 255, 255, 90});
        /* Bouclier de boss : dôme translucide */
        if (e->is_boss && e->boss_shield > 0.0f)
            DrawSphere((Vector3){pos.x, W3D_ENEMY_BODY_H + 0.2f, pos.z},
                       e->size * W3D_PER_PX * 1.6f,
                       (Color){120, 180, 255, 70});
    }
}

static void world_draw_projectiles(const TowerPool *tp, Camera3D cam) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *p = &tp->projectiles[i];
        if (!p->active) continue;
        int di = (int)p->dmg_type;
        if (di < 0 || di >= DAMAGE_TYPE_COUNT) di = 0;
        Vector3 pos = w3d_from_sim(p->x, p->y, W3D_PROJ_H);
        if (!w3d_in_front(cam, pos)) continue;     /* dos caméra : rien */
        /* Ombre au sol, atténuée par la hauteur de vol */
        w3d_blob_shadow(cam, pos.x, W3D_SHADOW_Y, pos.z,
                        W3D_SHADOW_PROJ_R, W3D_PROJ_H);
        /* Traînée typée (flamme / arc / fantômes) derrière le vol */
        float dx = p->tx - p->x, dy = p->ty - p->y;
        float d  = sqrtf(dx * dx + dy * dy);
        if (d > 1.0f)
            r3dfx_proj_trail(cam, pos, dx / d, dy / d, di);
        DrawSphere(pos, W3D_PROJ_R, W3D_PROJ_COL[di]);
    }
}

/* ── Scène complète ────────────────────────────────────────────── */
void render3d_world_render(AppContext *ctx, Camera3D cam) {
    GameState *gs = &ctx->gs;
    HeroState *h  = &ctx->hero;
    const Theme *T = theme_get(gs->map.theme);

    /* Ciel : dégradé vertical dérivé du fond du thème (haut sombre →
       horizon clair). La couleur d'horizon sert aussi de brouillard :
       la géométrie lointaine s'y fond. */
    Color sky_top = w3d_shade(T->palette.bg, W3D_SKY_TOP_D);
    Color sky_hor = w3d_shade(T->palette.bg, W3D_SKY_HOR_D);
    ClearBackground(sky_top);
    DrawRectangleGradientV(0, 0, g_canvas_virt_w, g_canvas_virt_h,
                           sky_top, sky_hor);

    /* Brouillard de distance — coupé en vue tactique (lisibilité). */
    float fog_d = h->tactical_view ? 0.0f : W3D_FOG_DENSITY;
    w3d_fog_setup(sky_hor, fog_d);                 /* formes CPU        */
    render3d_set_fog(sky_hor, fog_d);              /* shaders tours     */
    render3d_units_set_fog(sky_hor, fog_d);        /* shader unités     */
    render3d_enemies_set_fog(sky_hor, fog_d);      /* shader ennemis    */

    g_wframe++;   /* horloge du throttle d'animation */

    BeginMode3D(cam);
        render3d_terrain_draw(&gs->map, &gs->towers, cam);
        world_draw_bases(&gs->map, cam);
        world_draw_towers(&gs->towers, &gs->enemies);
        world_draw_units(&gs->units, &gs->enemies, h, cam);
        world_draw_enemies(&gs->enemies, cam);
        world_draw_projectiles(&gs->towers, cam);
        r3dfx_render(cam);   /* impacts, fragments de mort (FX 3D) */

        /* Matériaux lâchés au sol (ouvrier tué en portant) */
        for (int d = 0; d < gs->dropped_mat_count; d++) {
            const DroppedMat *dm = &gs->dropped_mats[d];
            if (!dm->active) continue;
            Vector3 pos = w3d_from_sim(dm->x, dm->y, 0.3f);
            rlPushMatrix();
                rlTranslatef(pos.x, pos.y, pos.z);
                rlRotatef((float)GetTime() * 60.0f, 0.0f, 1.0f, 0.0f);
                DrawCube((Vector3){0, 0, 0}, 0.28f, 0.28f, 0.28f,
                         MATERIAL_COLORS[dm->type]);
            rlPopMatrix();
        }

        /* Ombre du héros — aussi en 1re personne (visible en regardant
           au sol) ; posé sur une ruine → l'ombre suit (hz), en l'air →
           elle reste au sol et s'estompe avec la hauteur du saut. */
        if (h->control_tower < 0) {
            Vector3 hp = w3d_from_sim(h->px, h->py, 0.0f);
            float gy = h->on_ground ? h->hz : 0.0f;
            w3d_blob_shadow(cam, hp.x, gy + W3D_SHADOW_Y, hp.z,
                            W3D_SHADOW_HERO_R, h->hz - gy);
        }

        /* Héros (3e personne + vue tactique) : modèle DÉDIÉ (manteau/
           casque/fusil, anims Idle/Run/Shoot) ; replis : soldat GLB
           puis formes colorées. Anim tir > course > repos. */
        if ((!h->first_person || h->tactical_view) && h->control_tower < 0) {
            Vector3 pos = w3d_from_sim(h->px, h->py, h->hz);
            int kind = (h->fire_flash > 0.0f) ? 2 : (h->moving ? 1 : 0);
            if (!render3d_hero_draw_world(pos, h->yaw, W3D_HERO_SCALE,
                                          kind, h->anim_t, 1) &&
                !render3d_units_draw_world((int)UNIT_SOLDIER, pos, h->yaw,
                                           W3D_HERO_SCALE, kind, h->anim_t,
                                           1)) {
                DrawCylinder(pos, 0.24f, 0.28f, 1.15f, 8,
                             (Color){225, 210, 160, 255});
                DrawSphere((Vector3){pos.x, 1.3f, pos.z}, 0.2f,
                           (Color){225, 210, 160, 255});
            }
        }

        /* Traceur + flash du tir héros */
        if (h->trace_t > 0.0f)
            DrawCylinderEx(h->trace_a, h->trace_b, 0.02f, 0.015f, 5,
                           (Color){255, 235, 170, 220});
        if (h->fire_flash > 0.06f)
            DrawSphere(h->trace_a, 0.07f, (Color){255, 210, 120, 240});
        /* IMPACT visible : sphère qui gonfle au point touché */
        if (h->trace_t > 0.0f && h->trace_hit) {
            float k = 1.0f - h->trace_t / HERO_TRACE_TIME;   /* 0 → 1 */
            DrawSphere(h->trace_b, 0.10f + k * 0.28f,
                       (Color){255, 190, 90, (unsigned char)(220 - k * 150)});
        }

        /* Arme en 1re personne (dernier : par-dessus tout, hors depth) */
        hero_draw_viewmodel(h, cam);

        /* Fantôme de placement de tour */
        if (h->place_mode) {
            Vector3 c = { (h->place_tx + 0.5f) * W3D_TILE, 0.0f,
                          (h->place_ty + 0.5f) * W3D_TILE };
            Color gc = h->place_ok ? (Color){90, 230, 110, 255}
                                   : (Color){235, 80, 70, 255};
            DrawCubeWires((Vector3){c.x, 0.5f, c.z},
                          W3D_TILE, 1.0f, W3D_TILE, gc);
            DrawCylinder(c, 0.32f, 0.36f, 1.2f, 10,
                         (Color){gc.r, gc.g, gc.b, 110});
        }
    EndMode3D();
}
