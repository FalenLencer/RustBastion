/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "../map/map_gen.h"
#include "../map/pathfinding.h"
#include "../combat/enemy.h"
#include "../combat/tower.h"
#include "../combat/unit.h"
#include "hud.h"
#include "../game/meta.h"

typedef struct GameState GameState;

extern Color TOWER_FILL[TOWER_TYPE_COUNT];
extern Color UNIT_FILL [UNIT_TYPE_COUNT];
extern Color PROJ_COLOR[TOWER_TYPE_COUNT];
// Couleur d'identité par matériau (source unique, cf. MaterialType).
extern Color MATERIAL_COLORS[MAT_COUNT];

// Décalage horizontal de la carte dans le canvas (centrage, px virtuels)
extern int g_map_x_off;
// Largeur réelle du canvas virtuel (adapté au ratio de la fenêtre)
extern int g_canvas_virt_w;
// Largeur de base de la carte (MAP_W * TILE_SIZE), sans le padding latéral
extern int g_canvas_virt_w_base;
// Hauteur du canvas virtuel (MAP_H * TILE_SIZE + UI_HUD_HEIGHT)
extern int g_canvas_virt_h;
// Facteur de zoom de la carte pour les grandes cartes (1.0 = taille standard)
// La carte est rendue dans un Camera2D avec ce zoom pour tenir dans le canvas fixe.
extern float g_map_render_scale;
// Zoom JOUEUR (molette) + décalage (pan). Le rendu carte ET le mappage souris
// passent par les helpers ci-dessous (source unique → alignement garanti).
extern float g_map_zoom;             // 1..3
extern float g_map_pan_x, g_map_pan_y;
extern int   g_colorblind;           // 1 = palette ennemis daltonien-safe
extern int   g_show_entity_names;    // 1 = nom des ennemis au-dessus des sprites
extern int   g_units_3d;             // 1 = unités/ennemis en 3D (modèles) ; 0 = sprites 2D
float   map_eff_scale(void);                 // g_map_render_scale * g_map_zoom
Vector2 map_origin(void);                    // (OX, OY) du rendu carte
Vector2 map_screen_to_world(Vector2 s);      // souris (coords canvas) → monde (px)

Color renderer_tower_color(TowerType type);
Color renderer_unit_color (UnitType  type);
Color renderer_enemy_color(EnemyType type);
// Couleur d'identité d'une base (distincte par index) — partagée entre le
// bunker, l'onde de repérage et la barre de vie pour différencier les bases.
Color renderer_base_color (int base_idx, int is_primary);

// tp : pool de tours (peut être NULL) → une tour sur une tuile déblaie
// le décor de ruine sous elle (sol nu). NULL = aucun déblaiement.
/* Dessine les fonds de tuiles. `cull_to_view` = 1 : ne dessine QUE les
   tuiles visibles, déduites de la caméra carte globale (map_origin /
   map_eff_scale) — à n'utiliser QUE si l'appelant dessine avec cette
   caméra-là. Passer 0 pour tout dessiner (ex. plateau de référence MP,
   qui emploie sa propre caméra sans zoom ni pan). */
void render_map          (const Map *map, const TowerPool *tp,
                          int cull_to_view);
void render_bases        (const Map *map);
void render_paths        (const PathSet *ps);
void render_enemies      (const EnemyPool *pool);
void render_towers       (const TowerPool *tp);
void render_spawn_exclusion_zones(const Map *map);
/* Projectiles : départ à la bouche du canon 3D, traçante par type, impacts
   (renderer_tower_fx.c). */
void render_projectiles  (const TowerPool *tp);
/* Sol des tours (renderer_tower_fx.c) : socle peint + ombre de contact +
   poussière/onde de la POSE. Appelé 1×/frame par render_map (entre le sol et
   les ruines), donc AVANT render_towers : suit les tours apparues. */
void tower_fx_draw_ground(const TowerPool *tp, const Map *map);
/* Pose de la tour i (chute, écrasement) : décalage vertical, échelles X/Y
   autour du pied, opacité. Tour posée → 0, 1, 1, 1. */
void tower_fx_drop       (int i, float *dy, float *sx, float *sy, float *alpha);
/* Effets partagés tours / unités / ennemis (renderer_tower_fx.c) :
   étincelles d'impact (big = gros coup / explosion) et poussière au sol
   (u = progression 0→1, scale 1 = pose d'une tour). */
void  render_fx_impact    (float x, float y, int dmg_type, int big);
void  render_fx_dust      (float cx, float cy, float u, unsigned seed, float scale, Color col);
Color render_fx_dust_color(ThemeID th);
void render_units        (const UnitPool *up);
void render_tower_preview(const Map *map, const TowerPool *tp,
                          TowerType type, int tile_x, int tile_y);
void render_hud          (const GameState *gs);
/* Corps d'une tour / entité après sa transformation de pose (chute dy,
   écrasement sx/sy) AUTOUR DE SON PIED (fx, fy) : on s'écrase SUR le sol. */
static inline Rectangle renderer_squash_rect(Rectangle r, float fx, float fy,
                                             float dy, float sx, float sy) {
    return (Rectangle){ fx + (r.x - fx) * sx, fy + (r.y - fy) * sy + dy,
                        r.width * sx, r.height * sy };
}
// Interne aux renderer*.c : texture centrée dans une box, ratio conservé
// (1 = dessinée, 0 = texture absente). Défini dans renderer_entities.c.
int  renderer_sprite_fit (Texture2D tex, float cx, float cy,
                          float box_w, float box_h, Color tint);
// tp (peut être NULL) : une tour sur une roche minée déblaie le cratère.
/* hover_tx/ty : tuile survolée (-1 = aucune) → info-bulle des filons verrouillés ;
   wave_num : vague courante (compte à rebours d'ouverture). */
void render_deposits     (const Map *map, const TowerPool *tp,
                          int hover_tx, int hover_ty, int wave_num);
void render_dropped_mats (const DroppedMat *mats, int count);