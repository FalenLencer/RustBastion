#pragma once
#include "raylib.h"
#include "../combat/tower.h"
#include "../combat/unit.h"
#include "../meta/meta.h"

// Forward declaration — évite l'inclusion circulaire
typedef struct GameState GameState;

#define UI_HUD_HEIGHT    130
#define UI_PANEL_W       190
#define UI_BTN_W          58
#define UI_BTN_H          52

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
    TOOL_COUNT
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
    int           hovered_tool;
    int           hovered_tile_x;
    int           hovered_tile_y;
} UIState;

// ── API ──────────────────────────────────────────────────────
void       ui_init            (UIState *ui);

// Appelé chaque frame par main.c pour que l'UI convertisse
// les coordonnées souris (fenêtre réelle → canvas virtuel).
void       ui_set_mouse_offset(float ox, float oy, float scale);

void       ui_update          (UIState *ui, GameState *gs);
void       ui_render          (const UIState *ui, const GameState *gs);
int        ui_tool_is_tower   (ToolID id);
int        ui_tool_is_unit    (ToolID id);
TowerType  ui_tool_to_tower   (ToolID id);
UnitType   ui_tool_to_unit    (ToolID id);
const char *ui_tool_name      (ToolID id);