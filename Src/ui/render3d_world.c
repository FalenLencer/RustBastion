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
#include "renderer.h"            /* renderer_*_color, palettes           */
#include "render3d.h"            /* render3d_tower_draw_world            */
#include "render3d_units.h"      /* render3d_units_draw_world            */
#include "render3d_enemies.h"    /* render3d_enemies_draw_world          */
#include "../game/app.h"
#include "../game/hero.h"
#include "../map/theme.h"
#include "rlgl.h"
#include <math.h>

/* ── Réglages visuels (à ajuster au playtest — aucun affichage agent) ──
   (W3D_RUIN_H vit dans le header : partagé avec la collision du héros.) */
#define W3D_WATER_SINK    0.14f   /* enfoncement de l'eau                 */
#define W3D_BASE_W        2.6f    /* largeur du bâtiment de base          */
#define W3D_BASE_H        2.3f    /* hauteur du bâtiment de base          */
#define W3D_PORTAL_H      2.4f    /* hauteur des portails de spawn        */
#define W3D_TOWER_SCALE   1.0f    /* échelle des modèles de tour          */
#define W3D_UNIT_SCALE    1.0f    /* échelle des modèles d'unité          */
#define W3D_ENEMY_SCALE   1.0f    /* échelle des modèles d'ennemi         */
#define W3D_BOSS_MULT     1.8f    /* boss : facteur de taille             */
#define W3D_HERO_SCALE    1.0f    /* échelle du modèle du héros           */
#define W3D_PROJ_R        0.10f   /* rayon des projectiles de tour        */

/* Couleurs de terrain (complètent la palette de thème) */
#define W3D_COL_PATH      (Color){106,  86,  60, 255}
#define W3D_COL_WATER     (Color){ 34,  66, 112, 255}
#define W3D_COL_SPAWNPAD  (Color){ 96,  40,  40, 255}
#define W3D_COL_BASEPAD   (Color){ 60,  86,  62, 255}
#define W3D_COL_PORTAL    (Color){140,  46,  46, 255}
#define W3D_COL_BUILDING  (Color){ 92,  96, 104, 255}

/* Projectiles de tour : couleur par type de dégâts (indices DamageType) */
static const Color W3D_PROJ_COL[DAMAGE_TYPE_COUNT] = {
    {235, 220, 160, 255},   /* physique  */
    { 60, 200,  90, 255},   /* poison    */
    {120, 210, 245, 255},   /* électrique*/
    {150, 220, 255, 255},   /* cryo      */
    {190, 120, 240, 255},   /* nano      */
    {250, 150,  60, 255},   /* feu       */
};

/* Suivi de cap + horloge d'anim par entité (côté rendu uniquement) */
static float g_eh[MAX_ENEMIES], g_ea[MAX_ENEMIES];
static float g_epx[MAX_ENEMIES], g_epy[MAX_ENEMIES];
static float g_uh[MAX_UNITS],   g_ua[MAX_UNITS];
static float g_upx[MAX_UNITS],  g_upy[MAX_UNITS];

/* Cap monde depuis un déplacement sim (dx, dy) : atan2(x, z).
   Retourne 1 si l'entité a bougé cette frame (pilote l'anim marche). */
static int heading_update(float *head, float *ppx, float *ppy,
                          float x, float y) {
    float dx = x - *ppx, dy = y - *ppy;
    int moved = (dx * dx + dy * dy > 0.25f);   /* seuil anti-jitter (px²) */
    if (moved)
        *head = atan2f(dx, dy);
    *ppx = x; *ppy = y;
    return moved;
}

/* Une tour occupe-t-elle cette tuile ? (déblaie les gravats de ruine) */
static int tile_tower_present(const TowerPool *tp, int tx, int ty) {
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        if (tw->active && tw->tile_x == tx && tw->tile_y == ty) return 1;
    }
    return 0;
}

/* ── Terrain ───────────────────────────────────────────────────── */
static void world_draw_terrain(const Map *map, const TowerPool *tp) {
    const Theme *T = theme_get(map->theme);
    for (int ty = 0; ty < map->h; ty++) {
        for (int tx = 0; tx < map->w; tx++) {
            const Tile *t = &map->tiles[ty][tx];
            Vector3 c = { (tx + 0.5f) * W3D_TILE, 0.0f, (ty + 0.5f) * W3D_TILE };

            /* Nuance déterministe par tuile (relief discret) */
            int sh = (int)(t->noise_val * 22.0f) - 11;
            Color g = T->palette.ground_fill;
            int r = g.r + sh, gg = g.g + sh, b = g.b + sh;
            if (r < 0) r = 0;
            if (gg < 0) gg = 0;
            if (b < 0) b = 0;
            if (r > 255) r = 255;
            if (gg > 255) gg = 255;
            if (b > 255) b = 255;
            Color col = { (unsigned char)r, (unsigned char)gg,
                          (unsigned char)b, 255 };

            switch (t->type) {
                case TILE_WATER:
                    c.y = -W3D_WATER_SINK;
                    DrawPlane(c, (Vector2){W3D_TILE, W3D_TILE}, W3D_COL_WATER);
                    break;
                case TILE_RUIN:
                    DrawPlane(c, (Vector2){W3D_TILE, W3D_TILE}, col);
                    /* Chantier : construire une tour ici DÉBLAIE les
                       gravats (sinon la tour apparaît DANS le bloc). */
                    if (!tile_tower_present(tp, tx, ty))
                        DrawCube((Vector3){c.x, W3D_RUIN_H * 0.5f, c.z},
                                 W3D_TILE * 0.82f, W3D_RUIN_H, W3D_TILE * 0.82f,
                                 (Color){(unsigned char)(col.r / 2 + 40),
                                         (unsigned char)(col.g / 2 + 40),
                                         (unsigned char)(col.b / 2 + 40), 255});
                    break;
                case TILE_PATH:
                    DrawPlane(c, (Vector2){W3D_TILE, W3D_TILE}, W3D_COL_PATH);
                    break;
                case TILE_SPAWN:
                    DrawPlane(c, (Vector2){W3D_TILE, W3D_TILE}, W3D_COL_SPAWNPAD);
                    break;
                case TILE_BASE:
                    DrawPlane(c, (Vector2){W3D_TILE, W3D_TILE}, W3D_COL_BASEPAD);
                    break;
                default:
                    DrawPlane(c, (Vector2){W3D_TILE, W3D_TILE}, col);
                    break;
            }
        }
    }

    /* Minerais : cristal (filon actif) ou caillou (épuisé) */
    for (int d = 0; d < map->deposit_count; d++) {
        const MaterialDeposit *dep = &map->deposits[d];
        Vector3 c = { (dep->tile_x + 0.5f) * W3D_TILE, 0.0f,
                      (dep->tile_y + 0.5f) * W3D_TILE };
        if (dep->active) {
            Color mc = MATERIAL_COLORS[dep->type];
            rlPushMatrix();
                rlTranslatef(c.x, 0.0f, c.z);
                rlRotatef(45.0f, 0.0f, 1.0f, 0.0f);
                DrawCube((Vector3){0.0f, 0.45f, 0.0f}, 0.55f, 0.9f, 0.55f, mc);
                DrawCube((Vector3){0.25f, 0.22f, 0.18f}, 0.3f, 0.45f, 0.3f, mc);
            rlPopMatrix();
        } else if (dep->mined) {
            DrawCube((Vector3){c.x, 0.16f, c.z}, 0.7f, 0.32f, 0.7f,
                     (Color){96, 92, 88, 255});
        }
    }
}

/* ── Bases + portails ──────────────────────────────────────────── */
static void world_draw_bases(const Map *map) {
    for (int b = 0; b < map->base_count; b++) {
        const BaseInfo *bi = &map->bases[b];
        Vector3 c = { (bi->pos.x + 0.5f) * W3D_TILE, 0.0f,
                      (bi->pos.y + 0.5f) * W3D_TILE };
        if (!bi->active) {   /* ruine de base tombée */
            DrawCube((Vector3){c.x, 0.35f, c.z}, W3D_BASE_W * 0.8f, 0.7f,
                     W3D_BASE_W * 0.8f, (Color){60, 52, 48, 255});
            continue;
        }
        /* Bâtiment + porte (côté hangar : là où on recrute) */
        DrawCube((Vector3){c.x, W3D_BASE_H * 0.5f, c.z},
                 W3D_BASE_W, W3D_BASE_H, W3D_BASE_W, W3D_COL_BUILDING);
        DrawCubeWires((Vector3){c.x, W3D_BASE_H * 0.5f, c.z},
                      W3D_BASE_W, W3D_BASE_H, W3D_BASE_W,
                      (Color){30, 32, 38, 255});
        DrawCube((Vector3){c.x, 0.55f, c.z + W3D_BASE_W * 0.5f},
                 0.9f, 1.1f, 0.08f, (Color){40, 44, 52, 255});
        if (bi->is_primary) {   /* antenne = base principale */
            DrawCylinder((Vector3){c.x, W3D_BASE_H, c.z}, 0.03f, 0.03f,
                         1.1f, 6, (Color){180, 180, 190, 255});
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
        float half = W3D_TILE * 0.42f;
        DrawCube((Vector3){c.x - half, W3D_PORTAL_H * 0.5f, c.z},
                 0.24f, W3D_PORTAL_H, 0.24f, W3D_COL_PORTAL);
        DrawCube((Vector3){c.x + half, W3D_PORTAL_H * 0.5f, c.z},
                 0.24f, W3D_PORTAL_H, 0.24f, W3D_COL_PORTAL);
        DrawCube((Vector3){c.x, W3D_PORTAL_H, c.z},
                 half * 2.0f + 0.24f, 0.22f, 0.26f, W3D_COL_PORTAL);
        DrawCube((Vector3){c.x, W3D_PORTAL_H * 0.45f, c.z},
                 half * 2.0f - 0.05f, W3D_PORTAL_H * 0.9f, 0.06f,
                 (Color){16, 8, 12, 235});
    }
}

/* ── Entités ───────────────────────────────────────────────────── */
static void world_draw_towers(const TowerPool *tp) {
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        if (!tw->active) continue;
        Vector3 pos = w3d_from_sim(tw->cx, tw->cy, 0.0f);
        if (!render3d_tower_draw_world((int)tw->type, pos, tw->angle,
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

static void world_draw_units(const UnitPool *up, const HeroState *h) {
    float ft = GetFrameTime();
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &up->units[i];
        if (!u->active) continue;
        int mv = heading_update(&g_uh[i], &g_upx[i], &g_upy[i], u->x, u->y);
        g_ua[i] += ft;
        Vector3 pos = w3d_from_sim(u->x, u->y, 0.0f);
        /* Anim selon l'état RÉEL : attaque/minage > marche > repos.
           (USTATE_COLLECT → anim "mine" de l'ouvrier via a_attack.) */
        int kind = (u->state == USTATE_ATTACK ||
                    u->state == USTATE_COLLECT) ? 2 : (mv ? 1 : 0);
        if (!render3d_units_draw_world((int)u->type, pos, g_uh[i],
                                       W3D_UNIT_SCALE, kind, g_ua[i])) {
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

static void world_draw_enemies(const EnemyPool *ep) {
    float ft = GetFrameTime();
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
        int mv = heading_update(&g_eh[i], &g_epx[i], &g_epy[i], e->x, e->y);
        g_ea[i] += ft;
        Vector3 pos = w3d_from_sim(e->x, e->y, 0.0f);
        float sc = W3D_ENEMY_SCALE * (e->is_boss ? W3D_BOSS_MULT : 1.0f);

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
                                         sc, kind, g_ea[i])) {
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

static void world_draw_projectiles(const TowerPool *tp) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *p = &tp->projectiles[i];
        if (!p->active) continue;
        int di = (int)p->dmg_type;
        if (di < 0 || di >= DAMAGE_TYPE_COUNT) di = 0;
        Vector3 pos = w3d_from_sim(p->x, p->y, 0.95f);
        DrawSphere(pos, W3D_PROJ_R, W3D_PROJ_COL[di]);
    }
}

/* ── Scène complète ────────────────────────────────────────────── */
void render3d_world_render(AppContext *ctx, Camera3D cam) {
    GameState *gs = &ctx->gs;
    HeroState *h  = &ctx->hero;
    const Theme *T = theme_get(gs->map.theme);

    ClearBackground(T->palette.bg);

    BeginMode3D(cam);
        world_draw_terrain(&gs->map, &gs->towers);
        world_draw_bases(&gs->map);
        world_draw_towers(&gs->towers);
        world_draw_units(&gs->units, h);
        world_draw_enemies(&gs->enemies);
        world_draw_projectiles(&gs->towers);

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

        /* Héros (3e personne uniquement) : soldat GLB en attendant un
           modèle dédié ; anim tir > marche > repos. */
        if (!h->first_person) {
            Vector3 pos = w3d_from_sim(h->px, h->py, h->hz);
            int kind = (h->fire_flash > 0.0f) ? 2 : (h->moving ? 1 : 0);
            if (!render3d_units_draw_world((int)UNIT_SOLDIER, pos, h->yaw,
                                           W3D_HERO_SCALE, kind, h->anim_t)) {
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
