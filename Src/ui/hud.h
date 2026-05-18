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

#define UI_HUD_HEIGHT    150
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

typedef struct {
    ToolID        selected_tool;
    TileSelection selection;
    Rectangle     tool_btns[TOOL_COUNT];
    Rectangle     wave_btn;
    Rectangle     sell_btn;
    Rectangle     apply_mat_btn;
    int           apply_mat_visible;
    int           hovered_tool;
    int           hovered_tile_x;
    int           hovered_tile_y;
    int           show_fps;
    int           speed_mult;
    int           worker_selected_idx;
    // Notifications flottantes
    FloatNotif    notifs[MAX_NOTIFS];
    int           notif_count;
} UIState;

void       ui_init            (UIState *ui);
void       ui_push_notif      (UIState *ui, const char *text, Color col);
void       ui_set_mouse_offset(float ox, float oy, float sx, float sy);
void       ui_update          (UIState *ui, GameState *gs);
void       ui_render          (const UIState *ui, const GameState *gs);
int        ui_tool_is_tower   (ToolID id);
int        ui_tool_is_unit    (ToolID id);
TowerType  ui_tool_to_tower   (ToolID id);
UnitType   ui_tool_to_unit    (ToolID id);
const char *ui_tool_name      (ToolID id);