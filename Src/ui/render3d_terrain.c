/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  render3d_terrain.c ─ MODE HÉROS : habillage procédural du sol.
 *  Aucune texture : tout est dérivé de la palette du THÈME (chaque
 *  thème a déjà ses couleurs eau/route/spawn/base en 2D — la 3D les
 *  réutilise enfin) + un hash déterministe par tuile pour le grain,
 *  les props et les traces. Extrait de render3d_world.c.
 */
#include "render3d_terrain.h"
#include "render3d_world.h"      /* W3D_TILE, W3D_RUIN_H, w3d_in_front   */
#include "renderer.h"            /* MATERIAL_COLORS                      */
#include "../map/theme.h"
#include "../combat/tower.h"
#include "rlgl.h"
#include <math.h>

/* ── Réglages (à ajuster au playtest — aucun affichage agent) ────── */
#define W3D_WATER_SINK      0.14f  /* enfoncement moyen de l'eau          */
#define W3D_TILE_FILL       0.95f  /* part remplie de la tuile → joint    */
#define W3D_JOINT_DEPTH     0.22f  /* prof. du sous-sol (sous l'eau)      */
#define W3D_JOINT_DARK      (-46)  /* assombrissement du sous-sol         */
#define W3D_TINT_VAR        12     /* grain par tuile : ±delta RGB (~5 %) */
#define W3D_WATER_WAVE_AMP  0.035f /* houle : amplitude verticale (monde) */
#define W3D_WATER_WAVE_SPD  1.6f   /* houle : pulsation (rad/s)           */
#define W3D_WATER_TINT_AMP  10.0f  /* houle : variation de teinte (RGB)   */
#define W3D_PATH_WEAR_DARK  (-20)  /* bande d'usure centrale des routes   */
#define W3D_PATH_TRACE_PCT  45u    /* % de tuiles route portant des traces*/
#define W3D_PROP_PCT        22u    /* % de tuiles libres portant un prop  */
#define W3D_COL_SCRAP       (Color){138, 76, 42, 255}  /* débris rouillés */

/* Hash déterministe (tx, ty) : grain, choix et position des props.
   Même graine à chaque frame → le décor ne « clignote » jamais. */
static unsigned w3d_tile_hash(int tx, int ty) {
    unsigned h = (unsigned)tx * 374761393u + (unsigned)ty * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* (Helpers de teinte w3d_tint3 / w3d_shade : inline dans render3d_world.h,
   partagés avec le ciel et le brouillard.) */

/* Une tour occupe-t-elle cette tuile ? (déblaie les gravats de ruine,
   et interdit un prop sous une tour) — source unique dans tower.c. */
static int tile_tower_present(const TowerPool *tp, int tx, int ty) {
    return tower_at_tile(tp, tx, ty);
}

static int tile_is_path(const Map *map, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= map->w || ty >= map->h) return 0;
    return map->tiles[ty][tx].type == TILE_PATH;
}

/* Prop décoratif d'une tuile libre : caillou / touffe / débris rouillé.
   Position et gabarit tirés du hash → stable d'une frame à l'autre.
   `fog` = facteur de brume de la tuile (w3d_fog_factor). */
static void draw_prop(unsigned hsh, Vector3 c, Color ground, float fog) {
    float ox = ((float)((hsh >> 8) & 63u) / 63.0f - 0.5f) * W3D_TILE * 0.6f;
    float oz = ((float)((hsh >> 14) & 63u) / 63.0f - 0.5f) * W3D_TILE * 0.6f;
    float vx = c.x + ox, vz = c.z + oz;
    switch ((hsh >> 20) % 3u) {
        case 0u: {   /* caillou */
            float s = 0.14f + (float)((hsh >> 26) & 7u) * 0.015f;
            DrawCube((Vector3){vx, s * 0.5f, vz}, s, s, s * 0.85f,
                     w3d_fog_mix(w3d_shade(ground, -34), fog));
        } break;
        case 1u: {   /* touffe : végétation rase, teinte du thème verdie */
            float ht = 0.22f + (float)((hsh >> 26) & 7u) * 0.02f;
            DrawCylinder((Vector3){vx, 0.0f, vz}, 0.02f, 0.11f, ht, 5,
                         w3d_fog_mix(w3d_tint3(ground, -14, 18, -12), fog));
        } break;
        default:     /* plaque de débris rouillée, posée à plat */
            DrawCube((Vector3){vx, 0.035f, vz}, 0.34f, 0.07f, 0.2f,
                     w3d_fog_mix(w3d_shade(W3D_COL_SCRAP,
                                           (int)((hsh >> 26) & 15u) - 8),
                                 fog));
            break;
    }
}

/* ── Ombres portées « blob » (cf. render3d_terrain.h) ──────────────
   Un éventail de triangles UNE SEULE FACE (normale +Y) : pas le double
   voile d'alpha des 2 chapeaux d'un cylindre, et un seul niveau de
   profondeur. Placé sous les anneaux d'état des ennemis (0.03+) et
   au-dessus des traces de route (0.02). */
#define W3D_SHADOW_ALPHA   90.0f  /* opacité au sol (0-255)               */
#define W3D_SHADOW_FADE_K  0.85f  /* fondu par unité de hauteur           */
#define W3D_SHADOW_SLICES  10     /* facettes du disque                   */

void w3d_blob_shadow(Camera3D cam, float x, float y, float z,
                     float r, float height_above) {
    static float cs[W3D_SHADOW_SLICES + 1], sn[W3D_SHADOW_SLICES + 1];
    static int   trig_init = 0;
    if (!trig_init) {
        for (int i = 0; i <= W3D_SHADOW_SLICES; i++) {
            float a = 2.0f * PI * (float)i / (float)W3D_SHADOW_SLICES;
            cs[i] = cosf(a);
            sn[i] = sinf(a);
        }
        trig_init = 1;
    }
    float a = W3D_SHADOW_ALPHA / (1.0f + W3D_SHADOW_FADE_K * height_above);
    a *= 1.0f - w3d_fog_factor((Vector3){x, y, z}, cam);
    if (a < 4.0f) return;                   /* invisible : ne rien émettre */
    rlBegin(RL_TRIANGLES);
    rlColor4ub(12, 10, 8, (unsigned char)a);
    for (int i = 0; i < W3D_SHADOW_SLICES; i++) {
        /* ordre (centre, i+1, i) → normale +Y : face visible du dessus */
        rlVertex3f(x, y, z);
        rlVertex3f(x + cs[i + 1] * r, y, z + sn[i + 1] * r);
        rlVertex3f(x + cs[i] * r, y, z + sn[i] * r);
    }
    rlEnd();
}

void render3d_terrain_draw(const Map *map, const struct TowerPool *tp,
                           Camera3D cam) {
    const Theme *T = theme_get(map->theme);
    Color ground = T->palette.ground_fill;
    Color water  = T->palette.water_fill;
    Color path   = w3d_shade(T->palette.path_fill, -10);
    Color spawnp = w3d_shade(T->palette.spawn_fill, -8);
    Color basep  = w3d_shade(T->palette.base_fill, -8);
    float wt = (float)GetTime();   /* houle : décoratif pur (comme balise) */

    /* Sous-sol sombre : une seule grande plaque, visible dans les joints
       entre tuiles (jamais cullée : 1 quad). */
    DrawPlane((Vector3){ (float)map->w * W3D_TILE * 0.5f, -W3D_JOINT_DEPTH,
                         (float)map->h * W3D_TILE * 0.5f },
              (Vector2){ (float)map->w * W3D_TILE,
                         (float)map->h * W3D_TILE },
              w3d_shade(ground, W3D_JOINT_DARK));

    float fill = W3D_TILE * W3D_TILE_FILL;
    for (int ty = 0; ty < map->h; ty++) {
        for (int tx = 0; tx < map->w; tx++) {
            const Tile *t = &map->tiles[ty][tx];
            Vector3 c = { (tx + 0.5f) * W3D_TILE, 0.0f,
                          (ty + 0.5f) * W3D_TILE };
            if (!w3d_in_front(cam, c)) continue;   /* dos caméra : rien */

            unsigned hsh = w3d_tile_hash(tx, ty);
            /* Grain déterministe : casse l'aplat de couleur. */
            int gr = (int)(hsh % (unsigned)(2 * W3D_TINT_VAR + 1))
                     - W3D_TINT_VAR;
            /* Teinte du sol : relief (bruit de génération) + grain. */
            Color gcol = w3d_shade(ground,
                                   (int)(t->noise_val * 22.0f) - 11 + gr);
            /* Brume de distance de la tuile (0 si coupée). */
            float f = w3d_fog_factor(c, cam);

            switch (t->type) {
                case TILE_WATER: {
                    /* Houle : hauteur + teinte pulsées, déphasées par tuile
                       (pleine taille : pas de joint sur l'eau). */
                    float ph = (float)(tx * 13 + ty * 7) * 0.61f;
                    float s  = sinf(wt * W3D_WATER_WAVE_SPD + ph);
                    c.y = -W3D_WATER_SINK + s * W3D_WATER_WAVE_AMP;
                    DrawPlane(c, (Vector2){W3D_TILE, W3D_TILE},
                              w3d_fog_mix(w3d_shade(water, gr / 2 +
                                          (int)(s * W3D_WATER_TINT_AMP)),
                                          f));
                } break;
                case TILE_RUIN:
                    DrawPlane(c, (Vector2){fill, fill}, w3d_fog_mix(gcol, f));
                    /* Chantier : construire une tour ici DÉBLAIE les
                       gravats (sinon la tour apparaît DANS le bloc). */
                    if (!tile_tower_present(tp, tx, ty))
                        DrawCube((Vector3){c.x, W3D_RUIN_H * 0.5f, c.z},
                                 W3D_TILE * 0.82f, W3D_RUIN_H,
                                 W3D_TILE * 0.82f,
                                 w3d_fog_mix(
                                   (Color){(unsigned char)(gcol.r / 2 + 40),
                                           (unsigned char)(gcol.g / 2 + 40),
                                           (unsigned char)(gcol.b / 2 + 40),
                                           255}, f));
                    break;
                case TILE_PATH: {
                    Color pc = w3d_shade(path, gr);
                    DrawPlane(c, (Vector2){fill, fill}, w3d_fog_mix(pc, f));
                    /* Bande d'usure centrale, orientée selon les voisins
                       (croisement → les deux). */
                    Color wear = w3d_fog_mix(w3d_shade(pc, W3D_PATH_WEAR_DARK), f);
                    int ex = tile_is_path(map, tx - 1, ty) ||
                             tile_is_path(map, tx + 1, ty);
                    int ez = tile_is_path(map, tx, ty - 1) ||
                             tile_is_path(map, tx, ty + 1);
                    if (ex)
                        DrawPlane((Vector3){c.x, 0.012f, c.z},
                                  (Vector2){fill, W3D_TILE * 0.34f}, wear);
                    if (ez || !ex)
                        DrawPlane((Vector3){c.x, 0.012f, c.z},
                                  (Vector2){W3D_TILE * 0.34f, fill}, wear);
                    /* Petites traces de passage (pas / roues) */
                    if (hsh % 100u < W3D_PATH_TRACE_PCT) {
                        float ox = ((float)((hsh >> 8) & 31u) / 31.0f
                                    - 0.5f) * 0.9f;
                        float oz = ((float)((hsh >> 13) & 31u) / 31.0f
                                    - 0.5f) * 0.9f;
                        Color tr = w3d_fog_mix(w3d_shade(pc, -32), f);
                        DrawPlane((Vector3){c.x + ox, 0.02f, c.z + oz},
                                  (Vector2){0.12f, 0.2f}, tr);
                        DrawPlane((Vector3){c.x - ox * 0.6f, 0.02f,
                                            c.z - oz},
                                  (Vector2){0.12f, 0.2f}, tr);
                    }
                } break;
                case TILE_SPAWN:
                    DrawPlane(c, (Vector2){fill, fill},
                              w3d_fog_mix(w3d_shade(spawnp, gr), f));
                    break;
                case TILE_BASE:
                    DrawPlane(c, (Vector2){fill, fill},
                              w3d_fog_mix(w3d_shade(basep, gr), f));
                    break;
                default:   /* sol libre */
                    DrawPlane(c, (Vector2){fill, fill}, w3d_fog_mix(gcol, f));
                    /* Prop décoratif (max 1/tuile, jamais sous une tour
                       ni sur un minerai). */
                    if (hsh % 100u < W3D_PROP_PCT) {
                        int busy = tile_tower_present(tp, tx, ty);
                        for (int d = 0;
                             d < map->deposit_count && !busy; d++)
                            if (map->deposits[d].tile_x == tx &&
                                map->deposits[d].tile_y == ty) busy = 1;
                        if (!busy) draw_prop(hsh, c, gcol, f);
                    }
                    break;
            }
        }
    }

    /* Minerais : cristal (filon actif) ou caillou (épuisé) */
    for (int d = 0; d < map->deposit_count; d++) {
        const MaterialDeposit *dep = &map->deposits[d];
        Vector3 c = { (dep->tile_x + 0.5f) * W3D_TILE, 0.0f,
                      (dep->tile_y + 0.5f) * W3D_TILE };
        if (!w3d_in_front(cam, c)) continue;
        float f = w3d_fog_factor(c, cam);
        if (dep->active) {
            Color mc = w3d_fog_mix(MATERIAL_COLORS[dep->type], f);
            rlPushMatrix();
                rlTranslatef(c.x, 0.0f, c.z);
                rlRotatef(45.0f, 0.0f, 1.0f, 0.0f);
                DrawCube((Vector3){0.0f, 0.45f, 0.0f}, 0.55f, 0.9f, 0.55f, mc);
                DrawCube((Vector3){0.25f, 0.22f, 0.18f}, 0.3f, 0.45f, 0.3f, mc);
            rlPopMatrix();
        } else if (dep->mined &&
                   !tile_tower_present(tp, dep->tile_x, dep->tile_y)) {
            DrawCube((Vector3){c.x, 0.16f, c.z}, 0.7f, 0.32f, 0.7f,
                     w3d_fog_mix((Color){96, 92, 88, 255}, f));
        }
    }
}
