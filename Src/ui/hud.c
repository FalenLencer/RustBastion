/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud.c ─ Noyau du HUD (données, init, helpers partagés).
 *
 *  Contient :
 *    ui_init           — Initialisation de l'état UI
 *    ui_set_mouse_offset — Calibrage de la transformation souris
 *    ui_tool_name / ui_tool_is_tower / ui_tool_is_unit / ui_tool_to_*
 *    ui_push_notif / ui_disc_push / disc_pop
 *    Tableaux partagés : TOOL_INFO, TOOL_COLORS,
 *                        SLOT_*_COSTS, TOWER_UNLOCK_*
 *
 *  La logique d'entrée est dans hud_input.c.
 *  Le rendu est dans hud_render.c.
 */

#include "hud_internal.h"

// ════════════════════════════════════════════════════
// TRANSFORMATION SOURIS
// ════════════════════════════════════════════════════
static float g_mouse_ox = 0.0f;
static float g_mouse_oy = 0.0f;
static float g_mouse_sx = 1.0f;
static float g_mouse_sy = 1.0f;

void ui_set_mouse_offset(float ox, float oy, float sx, float sy) {
    g_mouse_ox = ox;
    g_mouse_oy = oy;
    g_mouse_sx = sx > 0.001f ? sx : 1.0f;
    g_mouse_sy = sy > 0.001f ? sy : 1.0f;
}

Vector2 virt_mouse(void) {
    Vector2 raw = GetMousePosition();
    return (Vector2){
        (raw.x - g_mouse_ox) / g_mouse_sx,
        (raw.y - g_mouse_oy) / g_mouse_sy,
    };
}

// ════════════════════════════════════════════════════
// DONNÉES DES OUTILS
// ════════════════════════════════════════════════════
const ToolInfo TOOL_INFO[TOOL_COUNT] = {
    [TOOL_TOWER_GUN]    = {"TOURELLE",  "Gun",     "T", "Polyvalente.",   15,  20, 3.5f, 1.5f, 0},
    [TOOL_TOWER_SNIPER] = {"SNIPER",    "Sniper",  "S", "Longue portee.", 25,  90, 6.5f, 0.4f, 0},
    [TOOL_TOWER_FLAME]  = {"FLAMMES",   "Flame",   "F", "Zone courte.",   30,  12, 2.0f, 3.0f, 0},
    [TOOL_TOWER_TESLA]  = {"TESLA",     "Tesla",   "E", "Chaine x2.",     50,  31, 4.0f, 0.8f, 0},
    [TOOL_UNIT_SOLDIER] = {"SOLDAT",    "Soldat",  "o", "Polyvalent.",    20,  25, 1.2f, 1.2f, 1},
    [TOOL_UNIT_HEAVY]   = {"LOURD",     "Lourd",   "H", "Tank.",          35,  50, 1.0f, 0.6f, 1},
    [TOOL_UNIT_MEDIC]   = {"MEDIC",     "Medic",   "+", "Soigneur.",      25,   8, 3.0f, 0.5f, 1},
    [TOOL_UNIT_DOG]     = {"CHIEN",     "Chien",   "d", "Rapide.",        10,  15, 0.8f, 2.0f, 1},
    [TOOL_UNIT_WORKER]  = {"OUVRIER",   "Ouvrier", "W", "Collecte mat.",  15,   5, 0.8f, 0.5f, 1},
};

const char *ui_tool_name(ToolID id) {
    if (id < 0 || id >= TOOL_COUNT) return "Aucun";
    return TOOL_INFO[id].shortname;
}

Color TOOL_COLORS[TOOL_COUNT] = {
    [TOOL_TOWER_GUN]    = {192,  57,  43, 255},
    [TOOL_TOWER_SNIPER] = { 52, 152, 219, 255},
    [TOOL_TOWER_FLAME]  = {230, 126,  34, 255},
    [TOOL_TOWER_TESLA]  = {155,  89, 182, 255},
    [TOOL_UNIT_SOLDIER] = { 39, 174,  96, 255},
    [TOOL_UNIT_HEAVY]   = { 41, 128, 185, 255},
    [TOOL_UNIT_MEDIC]   = {231,  76,  60, 255},
    [TOOL_UNIT_DOG]     = {243, 156,  18, 255},
    [TOOL_UNIT_WORKER]  = {200, 200,  50, 255},
};

// ════════════════════════════════════════════════════
// COÛTS DES ACHATS EN JEU
// ════════════════════════════════════════════════════
const int SLOT_TOWER_COSTS[SLOT_MAX_BUYS] = {  80, 160, 300,  550, 1000, 1800 };
const int SLOT_UNIT_COSTS [SLOT_MAX_BUYS] = {  60, 120, 220,  400,  720, 1300 };

/* Coût exponentiel de réparation d'une base (indexed par repair_count) */
int base_repair_cost(int n) {
    static const int tbl[] = { 40, 65, 105, 165, 265, 415 };
    if (n <= 0) return tbl[0];
    if (n >= 5) return tbl[5] + (n - 5) * 200;
    return tbl[n];
}

// ════════════════════════════════════════════════════
// CONVERSIONS
// ════════════════════════════════════════════════════
int ui_tool_is_tower(ToolID id) { return id >= TOOL_TOWER_GUN    && id <= TOOL_TOWER_TESLA;  }
int ui_tool_is_unit (ToolID id) { return id >= TOOL_UNIT_SOLDIER && id <= TOOL_UNIT_WORKER;  }
TowerType ui_tool_to_tower(ToolID id) { return (TowerType)(id - TOOL_TOWER_GUN);    }
UnitType  ui_tool_to_unit (ToolID id) { return (UnitType) (id - TOOL_UNIT_SOLDIER); }

// ════════════════════════════════════════════════════
// DÉBLOCAGE DES TOURS PAR PROGRESSION DE CAMPAGNE
// Seuil = index du dernier acte à compléter pour débloquer.
// -1 = toujours disponible. Arcade = tout débloqué.
// ════════════════════════════════════════════════════
const int TOWER_UNLOCK_AT[TOWER_TYPE_COUNT] = {
    [TOWER_GUN]    = -1,  // toujours disponible
    [TOWER_SNIPER] =  1,  // après avoir complété l'acte index 1 (acte 2)
    [TOWER_TESLA]  =  2,  // après avoir complété l'acte index 2 (acte 3)
    [TOWER_FLAME]  =  3,  // après avoir complété l'acte index 3 (acte 4)
};

const char *TOWER_UNLOCK_ACT_NAME[TOWER_TYPE_COUNT] = {
    [TOWER_GUN]    = NULL,
    [TOWER_SNIPER] = "Ch.1 — Acte 2",
    [TOWER_TESLA]  = "Ch.1 — Acte 3",
    [TOWER_FLAME]  = "Ch.2 — Acte 1",
};

int tool_is_unlocked(ToolID id, const GameState *gs) {
    if (!ui_tool_is_tower(id)) return 1;       // unités : toujours dispo
    TowerType tt = ui_tool_to_tower(id);
    int threshold = TOWER_UNLOCK_AT[tt];
    if (threshold < 0) return 1;               // Gun : toujours dispo
    return meta_max_stage_completed(&gs->meta) >= threshold;
}

// ════════════════════════════════════════════════════
// NOTIFICATIONS FLOTTANTES
// ════════════════════════════════════════════════════
void ui_push_notif(UIState *ui, const char *text, Color col) {
    if (ui->notif_count >= MAX_NOTIFS) {
        for (int i = 0; i < MAX_NOTIFS - 1; i++)
            ui->notifs[i] = ui->notifs[i + 1];
        ui->notif_count = MAX_NOTIFS - 1;
    }
    FloatNotif *n = &ui->notifs[ui->notif_count++];
    strncpy(n->text, text, sizeof(n->text) - 1);
    n->text[sizeof(n->text) - 1] = '\0';
    n->timer = 2.2f;
    n->y_off = 0.0f;
    n->col   = col;
}

// ════════════════════════════════════════════════════
// FILE DE DÉCOUVERTE
// ════════════════════════════════════════════════════
void ui_disc_push(UIState *ui, DiscType type, int idx) {
    if (ui->disc_count >= DISC_QUEUE_CAP) return;
    // Évite les doublons (ex. ennemi actif plusieurs frames de suite)
    for (int i = 0; i < ui->disc_count; i++)
        if (ui->disc_queue[i].type == type && ui->disc_queue[i].idx == idx) return;
    ui->disc_queue[ui->disc_count++] = (DiscEntry){type, idx};
}

void disc_pop(UIState *ui) {
    if (ui->disc_count <= 0) return;
    for (int i = 0; i < ui->disc_count - 1; i++)
        ui->disc_queue[i] = ui->disc_queue[i + 1];
    ui->disc_count--;
}

// ════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════
void ui_init(UIState *ui) {
    memset(ui, 0, sizeof(UIState));
    ui->selected_tool       = TOOL_TOWER_GUN;
    ui->hovered_tool        = -1;
    ui->hovered_tile_x      = -1;
    ui->hovered_tile_y      = -1;
    ui->speed_mult          = 1;
    ui->apply_mat_visible   = 0;
    ui->worker_selected_idx = -1;
    ui->sell_unit_idx       = -1;
    ui->overlay_tl_pos      = (Vector2){-1.0f, -1.0f};  // sentinel : init au 1er frame
    ui->overlay_tr_pos      = (Vector2){-1.0f, -1.0f};
    ui->dragging_overlay    = -1;

    const int HUD_Y  = MAP_H * TILE_SIZE;
    const int M      = UI_MARGIN;
    const int GAP    = 6;

    // ── Rangées de boutons ────────────────────────────────────
    int row1_y = HUD_Y + 18;
    int row2_y = row1_y + UI_BTN_H + GAP + 14;
    int col0_x = UI_LEFT_PANEL_W + M + 6;

    for (int i = 0; i < 4; i++) {
        ui->tool_btns[TOOL_TOWER_GUN + i] = (Rectangle){
            col0_x + i * (UI_BTN_W + GAP),
            row1_y, UI_BTN_W, UI_BTN_H
        };
    }
    for (int i = 0; i < 5; i++) {
        ui->tool_btns[TOOL_UNIT_SOLDIER + i] = (Rectangle){
            col0_x + i * (UI_BTN_W + GAP),
            row2_y, UI_BTN_W, UI_BTN_H
        };
    }

    // ── Bouton LANCER VAGUE ───────────────────────────────────
    int wave_x = col0_x + 5 * (UI_BTN_W + GAP) + M;
    int wave_h = UI_BTN_H * 2 + GAP + 14;
    ui->wave_btn = (Rectangle){
        wave_x, row1_y, 108, wave_h
    };

    // ── Bouton PAUSE ─────────────────────────────────────────
    ui->pause_btn = (Rectangle){
        wave_x + 108 + M, row1_y, 36, wave_h
    };

    // ── Boutons panneau droit (positions initiales, recalculées dans ui_update) ─
    int right_x = (MAP_W * TILE_SIZE) - UI_PANEL_W + M;
    int right_w = UI_PANEL_W - M * 2;
    int btn_h   = 34;

    ui->sell_btn = (Rectangle){
        right_x,
        HUD_Y + UI_HUD_HEIGHT - M - btn_h * 2 - 5,
        right_w, btn_h
    };

    ui->unit_sell_btn = (Rectangle){
        right_x,
        HUD_Y + UI_HUD_HEIGHT - M - btn_h,
        right_w, btn_h
    };

    ui->apply_mat_btn = (Rectangle){
        right_x,
        HUD_Y + UI_HUD_HEIGHT - M - btn_h,
        right_w, btn_h
    };
}
