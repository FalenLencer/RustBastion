/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "../combat/tower.h"
#include "../combat/unit.h"
#include "../game/meta.h"

typedef struct GameState GameState;

#define UI_HUD_HEIGHT    190
#define UI_LEFT_PANEL_W  160   // panneau gauche (barres HP)
#define UI_PANEL_W       360   // panneau droit (stats / info)
#define UI_BTN_W          68
#define UI_BTN_H          52
#define UI_MARGIN          8
#define UI_RADIUS          4
#define MAX_NOTIFS         8

// Notification flottante (texte qui monte et disparaît)
typedef struct {
    char  text[52];
    float timer;   // durée restante (2.2s total)
    float y_off;   // offset Y vers le haut (px, croît avec le temps)
    Color col;
} FloatNotif;

typedef enum {
    TOOL_NONE = -1,
    TOOL_TOWER_GUN    = 0,
    TOOL_TOWER_SNIPER,
    TOOL_TOWER_FLAME,
    TOOL_TOWER_TESLA,
    TOOL_UNIT_SOLDIER,
    TOOL_UNIT_HEAVY,
    TOOL_UNIT_MEDIC,
    TOOL_UNIT_DOG,
    TOOL_UNIT_WORKER,
    TOOL_COUNT,
} ToolID;

typedef struct {
    int active;
    int tower_idx;
} TileSelection;

// ── Fiche de découverte ──────────────────────────────────────────
typedef enum {
    DISC_ENEMY = 0,
    DISC_TOWER,
    DISC_UNIT,
    DISC_MATERIAL,   // minerai : ferme la boucle minerai -> type de degats -> contre
} DiscType;

typedef struct {
    DiscType type;
    int      idx;
} DiscEntry;

#define DISC_QUEUE_CAP 8

typedef struct {
    ToolID        selected_tool;
    TileSelection selection;
    Rectangle     tool_btns[TOOL_COUNT];
    Rectangle     wave_btn;
    Rectangle     pause_btn;
    Rectangle     sell_btn;
    Rectangle     unit_sell_btn;
    Rectangle     apply_mat_btn;
    int           apply_mat_visible;
    int           sel_mat_idx;       /* index dans l'inventaire du matériau sélectionné */
    // Boutons amélioration tour sélectionnée
    Rectangle     upg_dmg_btn;
    Rectangle     upg_range_btn;
    Rectangle     upg_rate_btn;
    Rectangle     repair_tower_btn;
    // Boutons achat slots (panneau droit vide)
    Rectangle     buy_tower_slot_btn;
    Rectangle     buy_unit_slot_btn;
    // Boutons réparation bases (panneau gauche)
    Rectangle     repair_base_btn[MAX_BASES];
    int           hovered_tool;
    int           hovered_tile_x;
    int           hovered_tile_y;
    int           show_fps;
    int           speed_mult;
    int           worker_selected_idx;
    int           sell_unit_idx;       // unité sélectionnée pour vente (-1 = aucune)
    // Comportement d'unités de combat
    Rectangle     unit_beh_btns[5];   // PATROL | GARDE | ESCORTE | MANUEL | SUIVRE(medic)
    int           behavior_pending;    // UBEH_* en attente d'assignation (-1 = aucun)
    int           behavior_pending_unit; // index de l'unité attendant l'assignation
    // Overlays déplaçables (positions en coordonnées virtuelles)
    Vector2       overlay_tl_pos;     // panneau haut-gauche (or / tours / unités)
    Vector2       overlay_tr_pos;     // panneau haut-droit  (vague / kills / ennemis)
    Vector2       overlay_bl_pos;     // panneau bas-gauche  (bases HP / réparation)
    int           dragging_overlay;   // -1=aucun, 0=TL, 1=TR, 2=BL
    Vector2       drag_grab;          // décalage souris → coin overlay au début du drag
    // Sélection de GROUPE d'unités (glisser-boîte gauche → ordre clic droit)
    int           group_sel[MAX_UNITS]; // 1 = unité (par slot) dans le groupe courant
    int           group_count;          // nombre d'unités de combat sélectionnées
    int           box_dragging;         // 1 = glisser de sélection en cours
    Vector2       box_start;            // coin de départ du glisser (coords virtuelles)
    Vector2       box_cur;              // coin courant du glisser
    // Multijoueur : 1 = un panneau MP a consommé le clic ce frame (ne pas placer de tour)
    int           mp_block_click;
    // Notifications flottantes
    FloatNotif    notifs[MAX_NOTIFS];
    int           notif_count;
    // Fiches de découverte (campagne) — file d'attente, gèle le jeu tant que non vide
    DiscEntry     disc_queue[DISC_QUEUE_CAP];
    int           disc_count;
} UIState;

void       ui_init            (UIState *ui);
void       ui_disc_push       (UIState *ui, DiscType type, int idx);
void       ui_push_notif      (UIState *ui, const char *text, Color col);
void       ui_set_mouse_offset(float ox, float oy, float sx, float sy);
// Souris en coordonnées virtuelles (applique offset + échelle de présentation).
Vector2    virt_mouse         (void);
void       ui_update          (UIState *ui, GameState *gs);
void       ui_render          (const UIState *ui, const GameState *gs);
int        ui_tool_is_tower   (ToolID id);
int        ui_tool_is_unit    (ToolID id);
TowerType  ui_tool_to_tower   (ToolID id);
UnitType   ui_tool_to_unit    (ToolID id);
const char *ui_tool_name      (ToolID id);