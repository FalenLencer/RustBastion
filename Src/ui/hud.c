#include "hud.h"
#include "renderer.h"
#include "ui_utils.h"
#include "../engine/audio.h"
#include "../game/meta.h"
#include "../map/pathfinding.h"
#include "../combat/material.h"
#include <string.h>
#include <math.h>
#include "../game/game_state.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>

// ════════════════════════════════════════════════════
// TRANSFORMATION SOURIS
// ════════════════════════════════════════════════════
static float g_mouse_ox    = 0.0f;
static float g_mouse_oy    = 0.0f;
static float g_mouse_scale = 1.0f;

void ui_set_mouse_offset(float ox, float oy, float scale) {
    g_mouse_ox    = ox;
    g_mouse_oy    = oy;
    g_mouse_scale = scale > 0.001f ? scale : 1.0f;
}

static Vector2 virt_mouse(void) {
    Vector2 raw = GetMousePosition();
    return (Vector2){
        (raw.x - g_mouse_ox) / g_mouse_scale,
        (raw.y - g_mouse_oy) / g_mouse_scale,
    };
}

// ════════════════════════════════════════════════════
// DONNÉES DES OUTILS
// ════════════════════════════════════════════════════
typedef struct {
    const char *name;
    const char *shortname;
    const char *icon;
    const char *desc;
    int         cost;
    float       dmg;
    float       range;
    float       rate;
    int         is_unit;
} ToolInfo;

static const ToolInfo TOOL_INFO[TOOL_COUNT] = {
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

static Color TOOL_COLORS[TOOL_COUNT] = {
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
// CONVERSIONS
// ════════════════════════════════════════════════════
int ui_tool_is_tower(ToolID id) { return id >= TOOL_TOWER_GUN    && id <= TOOL_TOWER_TESLA;  }
int ui_tool_is_unit (ToolID id) { return id >= TOOL_UNIT_SOLDIER && id <= TOOL_UNIT_WORKER;  }
TowerType ui_tool_to_tower(ToolID id) { return (TowerType)(id - TOOL_TOWER_GUN);    }
UnitType  ui_tool_to_unit (ToolID id) { return (UnitType) (id - TOOL_UNIT_SOLDIER); }

// ════════════════════════════════════════════════════
// HELPERS DE RENDU INTERNES
// ════════════════════════════════════════════════════

static void draw_bar(int x, int y, int w, int h,
                     float ratio, Color fill, Color bg) {
    float r = ratio < 0.0f ? 0.0f : ratio > 1.0f ? 1.0f : ratio;
    float rnd = h > 4 ? 0.5f : 0.3f;
    DrawRectangleRounded((Rectangle){(float)x,(float)y,(float)w,(float)h},
                         rnd, 4, bg);
    int fw = (int)(w * r);
    if (fw > 1)
        DrawRectangleRounded((Rectangle){(float)x,(float)y,(float)fw,(float)h},
                             rnd, 4, fill);
}

static void draw_sep(int x, int y, int w, Color col) {
    DrawLine(x, y, x + w, y, col);
}

static void draw_tool_btn(const Rectangle *r, ToolID id,
                           int is_selected, int is_hovered,
                           int can_afford)
{
    const ToolInfo *info = &TOOL_INFO[id];
    Color col = TOOL_COLORS[id];
    float rnd = (float)UI_RADIUS / r->height;

    Color bg = is_selected ? (Color){38, 24,  7, 255} :
               is_hovered  ? (Color){28, 18,  5, 255} :
                             (Color){15,  9,  3, 255};
    DrawRectangleRounded(*r, rnd, 6, bg);

    Color border = is_selected ? col :
                   is_hovered  ? (Color){col.r/2, col.g/2, col.b/2, 200} :
                                 (Color){45, 30, 12, 180};
    float bw = is_selected ? 2.0f : 1.0f;
    DrawRectangleRoundedLinesEx(*r, rnd, 6, bw, border);

    if (!can_afford) col = (Color){65, 50, 32, 255};

    // Icône
    int icon_fs = 20;
    int iw = MeasureText(info->icon, icon_fs);
    DrawText(info->icon,
             (int)(r->x + r->width/2 - iw/2),
             (int)(r->y + 5), icon_fs, col);

    // Nom abrégé
    int name_fs = 9;
    int nw = MeasureText(info->shortname, name_fs);
    DrawText(info->shortname,
             (int)(r->x + r->width/2 - nw/2),
             (int)(r->y + 29), name_fs,
             can_afford ? (Color){150, 130, 100, 255}
                        : (Color){75, 58, 38, 255});

    // Coût
    char cost_buf[20];
    snprintf(cost_buf, sizeof(cost_buf), "%dor", info->cost);
    int cw = MeasureText(cost_buf, name_fs);
    DrawText(cost_buf,
             (int)(r->x + r->width/2 - cw/2),
             (int)(r->y + 41), name_fs,
             can_afford ? (Color){230, 150, 32, 255}
                        : (Color){130, 55, 35, 255});

    if (is_selected)
        DrawRectangleRoundedLinesEx(
            (Rectangle){r->x-2, r->y-2, r->width+4, r->height+4},
            rnd, 6, 1.0f,
            (Color){col.r, col.g, col.b, 60});
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

    const int HUD_Y  = MAP_H * TILE_SIZE;
    const int VIRT_W = MAP_W * TILE_SIZE;
    const int M      = UI_MARGIN;
    const int GAP    = 6;

    // ── Rangées de boutons ────────────────────────────────────
    int row1_y = HUD_Y + 18;
    int row2_y = row1_y + UI_BTN_H + GAP + 14;
    int col0_x = UI_PANEL_W + M + 6;

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

    // ── Boutons panneau droit (positions fixes en bas) ────────
    int right_x = VIRT_W - UI_PANEL_W + M;
    int right_w = UI_PANEL_W - M * 2;
    int btn_h   = 26;

    // sell_btn en avant-dernier position
    ui->sell_btn = (Rectangle){
        right_x,
        HUD_Y + UI_HUD_HEIGHT - M - btn_h * 2 - GAP,
        right_w, btn_h
    };

    // apply_mat_btn tout en bas
    ui->apply_mat_btn = (Rectangle){
        right_x,
        HUD_Y + UI_HUD_HEIGHT - M - btn_h,
        right_w, btn_h
    };
}

// ════════════════════════════════════════════════════
// MISE À JOUR
// ════════════════════════════════════════════════════
void ui_update(UIState *ui, GameState *gs) {
    Vector2   mouse = virt_mouse();
    const int HUD_Y = MAP_H * TILE_SIZE;

    // Nettoyage ouvrier mort : désélectionner si l'unité n'est plus active
    if (ui->worker_selected_idx >= 0) {
        const Unit *wu = &gs->units.units[ui->worker_selected_idx];
        if (!wu->active) {
            ui->worker_selected_idx = -1;
            gs->units.selected_unit = -1;
        }
    }

    // Hover outil
    ui->hovered_tool = -1;
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (CheckCollisionPointRec(mouse, ui->tool_btns[i])) {
            ui->hovered_tool = i;
            break;
        }
    }

    // Hover tuile
    if (mouse.y >= 0 && mouse.y < HUD_Y &&
        mouse.x >= 0 && mouse.x < MAP_W * TILE_SIZE) {
        ui->hovered_tile_x = (int)(mouse.x / TILE_SIZE);
        ui->hovered_tile_y = (int)(mouse.y / TILE_SIZE);
    } else {
        ui->hovered_tile_x = -1;
        ui->hovered_tile_y = -1;
    }

    // Visibilité bouton matériau (+ vérif que la tour est encore active)
    {
        int _mat_ok = 0;
        if (ui->selection.active && gs->inventory_count > 0) {
            int _tidx = ui->selection.tower_idx;
            if (_tidx >= 0 && _tidx < MAX_TOWERS &&
                gs->towers.towers[_tidx].active)
                _mat_ok = 1;
        }
        ui->apply_mat_visible = _mat_ok;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

        if (ui->hovered_tool != -1) {
            ui->selected_tool       = (ToolID)ui->hovered_tool;
            ui->selection.active    = 0;
            ui->worker_selected_idx = -1;
            gs->units.selected_unit = -1;
        }
        else if (CheckCollisionPointRec(mouse, ui->wave_btn)) {
            if (gs->phase == PHASE_PREP) {
                int bonus = (int)(gs->wave_manager.prep_timer / 20.0f * 15.0f);
                gs->gold += bonus;
                wave_start(&gs->wave_manager);
                gs->phase = PHASE_WAVE;
            }
        }
        else if (ui->selection.active &&
                 CheckCollisionPointRec(mouse, ui->sell_btn)) {
            Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
            if (tw->active) {
                // Remboursement basé sur le coût réel payé (tuile ruin = x2)
                int real_cost = tower_cost_on_tile(tw->type, &gs->map,
                                                   tw->tile_x, tw->tile_y);
                gs->gold += (int)(real_cost * 0.6f);
                gs->map.tiles[tw->tile_y][tw->tile_x].buildable = 1;
                tw->active = 0;
                gs->towers.tower_count--;
            }
            ui->selection.active = 0;
        }
        else if (ui->selection.active &&
                 ui->apply_mat_visible &&
                 CheckCollisionPointRec(mouse, ui->apply_mat_btn)) {
            int _tidx = ui->selection.tower_idx;
            if (_tidx >= 0 && _tidx < MAX_TOWERS) {
                Tower *tw = &gs->towers.towers[_tidx];
                if (tw->active && gs->inventory_count > 0) {
                    MaterialType mat = gs->inventory[0];
                    switch (mat) {
                        case MAT_ACID:
                            tw->dmg_type = DMG_POISON;
                            tw->material = MAT_ACID;
                            break;
                        case MAT_PLASMA:
                            tw->dmg_type = DMG_ELECTRIC;
                            tw->material = MAT_PLASMA;
                            break;
                        case MAT_CRYO:
                            tw->dmg_type = DMG_CRYO;
                            tw->material = MAT_CRYO;
                            break;
                        case MAT_NANO:
                            tw->dmg_type = DMG_NANO;
                            tw->material = MAT_NANO;
                            break;
                        case MAT_IRON:
                            // Guard: n'empile pas le +10% si déjà appliqué
                            if (tw->material != MAT_IRON) {
                                tw->damage  *= 1.10f;
                                tw->material = MAT_IRON;
                            }
                            break;
                        default:
                            break;
                    }
                    // Retire de l'inventaire
                    for (int k = 0; k < gs->inventory_count - 1; k++)
                        gs->inventory[k] = gs->inventory[k + 1];
                    gs->inventory[gs->inventory_count - 1] = MAT_NONE;
                    gs->inventory_count--;
                    // Feedback sonore
                    audio_play_sfx(AUDIO_SFX_MATERIAL_APPLY);
                    if (gs->inventory_count == 0)
                        ui->apply_mat_visible = 0;
                }
            }
        }
        else if (mouse.y < HUD_Y && ui->hovered_tile_x >= 0) {
            int tx = ui->hovered_tile_x;
            int ty = ui->hovered_tile_y;

            // Ouvrier sélectionné → dépôt
            if (ui->worker_selected_idx >= 0) {
                int assigned = 0;
                for (int d = 0; d < gs->map.deposit_count; d++) {
                    MaterialDeposit *dep = &gs->map.deposits[d];
                    if (dep->active &&
                        dep->tile_x == tx && dep->tile_y == ty) {
                        unit_assign_deposit(&gs->units,
                                            ui->worker_selected_idx, d);
                        ui->worker_selected_idx = -1;
                        gs->units.selected_unit = -1;
                        assigned = 1;
                        break;
                    }
                }
                if (!assigned) {
                    ui->worker_selected_idx = -1;
                    gs->units.selected_unit = -1;
                }
            }

            // Tour existante ?
            int clicked_tower = -1;
            for (int i = 0; i < MAX_TOWERS; i++) {
                Tower *tw = &gs->towers.towers[i];
                if (!tw->active) continue;
                if (tw->tile_x == tx && tw->tile_y == ty) {
                    clicked_tower = i; break;
                }
            }

            if (clicked_tower != -1) {
                ui->selection.active    = 1;
                ui->selection.tower_idx = clicked_tower;
                ui->selected_tool       = TOOL_NONE;
                ui->worker_selected_idx = -1;
                gs->units.selected_unit = -1;
            } else if (ui->selected_tool != TOOL_NONE) {
                if (ui_tool_is_tower(ui->selected_tool)) {
                    if (gs->towers.tower_count < gs->towers.tower_limit) {
                        if (tower_place(&gs->towers,
                                        ui_tool_to_tower(ui->selected_tool),
                                        tx, ty, &gs->map,
                                        &gs->gold, &gs->bonuses))
                        {
                            audio_play_sfx(AUDIO_SFX_TOWER_PLACE);
                        }
                    }
                } else if (ui_tool_is_unit(ui->selected_tool)) {
                    if (gs->units.count < gs->units.unit_limit) {
                        // Vérifie si le clic est proche d'UNE QUELCONQUE base
                        int near_any_base = 0;
                        for (int b = 0; b < gs->map.base_count; b++) {
                            if (!gs->map.bases[b].active) continue;
                            float bpx = gs->map.bases[b].pos.x * TILE_SIZE
                                        + TILE_SIZE / 2.0f;
                            float bpy = gs->map.bases[b].pos.y * TILE_SIZE
                                        + TILE_SIZE / 2.0f;
                            float dx = mouse.x - bpx;
                            float dy = mouse.y - bpy;
                            if (sqrtf(dx*dx + dy*dy) <= 5.0f * TILE_SIZE) {
                                near_any_base = 1;
                                break;
                            }
                        }
                        if (near_any_base) {
                            unit_spawn(&gs->units,
                                       ui_tool_to_unit(ui->selected_tool),
                                       &gs->gold, &gs->bonuses);
                        }
                    }
                }
            } else {
                // Ouvrier cliqué ?
                for (int j = 0; j < MAX_UNITS; j++) {
                    Unit *u = &gs->units.units[j];
                    if (!u->active || u->type != UNIT_WORKER) continue;
                    float dx = mouse.x - u->x;
                    float dy = mouse.y - u->y;
                    if (sqrtf(dx*dx + dy*dy) <= u->size + 6.0f) {
                        gs->units.selected_unit  = j;
                        ui->worker_selected_idx  = j;
                        ui->selection.active     = 0;
                        break;
                    }
                }
            }
        }
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        ui->selected_tool       = TOOL_NONE;
        ui->selection.active    = 0;
        ui->worker_selected_idx = -1;
        gs->units.selected_unit = -1;
    }

    if (IsKeyPressed(KEY_ONE))   ui->selected_tool = TOOL_TOWER_GUN;
    if (IsKeyPressed(KEY_TWO))   ui->selected_tool = TOOL_TOWER_SNIPER;
    if (IsKeyPressed(KEY_THREE)) ui->selected_tool = TOOL_TOWER_FLAME;
    if (IsKeyPressed(KEY_FOUR))  ui->selected_tool = TOOL_TOWER_TESLA;
    if (IsKeyPressed(KEY_FIVE))  ui->selected_tool = TOOL_UNIT_SOLDIER;
    if (IsKeyPressed(KEY_SIX))   ui->selected_tool = TOOL_UNIT_HEAVY;
    if (IsKeyPressed(KEY_SEVEN)) ui->selected_tool = TOOL_UNIT_MEDIC;
    if (IsKeyPressed(KEY_EIGHT)) ui->selected_tool = TOOL_UNIT_DOG;
    if (IsKeyPressed(KEY_NINE))  ui->selected_tool = TOOL_UNIT_WORKER;
    if (IsKeyPressed(KEY_X))     ui->speed_mult    = (ui->speed_mult % 3) + 1;
    if (IsKeyPressed(KEY_F))     ui->show_fps     ^= 1;
    if (IsKeyPressed(KEY_ESCAPE)) {
        ui->selected_tool       = TOOL_NONE;
        ui->selection.active    = 0;
        ui->worker_selected_idx = -1;
        gs->units.selected_unit = -1;
    }
}

// ════════════════════════════════════════════════════
// RENDU
// ════════════════════════════════════════════════════
void ui_render(const UIState *ui, const GameState *gs) {
    const int VIRT_W = MAP_W * TILE_SIZE;
    const int HUD_Y  = MAP_H * TILE_SIZE;
    const int HUD_H  = UI_HUD_HEIGHT;
    const int M      = UI_MARGIN;
    const int GAP    = 6;

    const Theme *th = theme_get(gs->map.theme);

    // ── Fond HUD ──────────────────────────────────────────────
    DrawRectangle(0, HUD_Y, VIRT_W, HUD_H, (Color){10, 6, 2, 255});
    DrawLine(0, HUD_Y, VIRT_W, HUD_Y, (Color){70, 44, 0, 200});

    DrawRectangle(0,                   HUD_Y, UI_PANEL_W, HUD_H, (Color){15, 8, 3, 235});
    DrawRectangle(VIRT_W - UI_PANEL_W, HUD_Y, UI_PANEL_W, HUD_H, (Color){15, 8, 3, 235});
    DrawLine(UI_PANEL_W,          HUD_Y, UI_PANEL_W,
             HUD_Y + HUD_H, (Color){50, 32, 8, 160});
    DrawLine(VIRT_W - UI_PANEL_W, HUD_Y, VIRT_W - UI_PANEL_W,
             HUD_Y + HUD_H, (Color){50, 32, 8, 160});

    // ════════════════════════════════════════════════
    // PANNEAU GAUCHE
    // ════════════════════════════════════════════════
    {
        const int px    = M + 2;
        const int bar_x = px + 28;
        const int bar_w = UI_PANEL_W - 28 - 30 - M;
        const int val_x = UI_PANEL_W - 30;
        int py = HUD_Y + M;

        // OR
        {
            int next_cost = 500;
            for (int t2 = 0; t2 < TOOL_COUNT; t2++) {
                if (gs->gold < TOOL_INFO[t2].cost &&
                    TOOL_INFO[t2].cost < next_cost)
                    next_cost = TOOL_INFO[t2].cost;
            }
            if (next_cost <= gs->gold)
                next_cost = gs->gold > 0 ? gs->gold : 1;
            float ratio = fminf((float)gs->gold / (float)next_cost, 1.0f);
            DrawText("OR", px, py + 1, 10, (Color){85, 55, 0, 255});
            draw_bar(bar_x, py + 2, bar_w, 7, ratio,
                     (Color){239, 159, 39, 255}, (Color){26, 12, 0, 200});
            char gbuf[12];
            if (gs->gold >= 10000)
                snprintf(gbuf, sizeof(gbuf), "%dk", gs->gold/1000);
            else
                snprintf(gbuf, sizeof(gbuf), "%d", gs->gold);
            DrawText(gbuf, val_x, py + 1, 10, (Color){239, 159, 39, 255});
            py += 16;
        }

        // VIES — ratio calculé sur le total HP max des bases encore actives
        {
            int max_hp_sum = 0;
            for (int b = 0; b < gs->map.base_count; b++)
                if (gs->map.bases[b].active)
                    max_hp_sum += gs->map.bases[b].max_hp;
            float lr = max_hp_sum > 0
                ? (float)gs->lives / (float)max_hp_sum : 0.0f;
            Color lc = lr > 0.5f  ? (Color){46, 204, 113, 255}
                     : lr > 0.25f ? (Color){243, 156,  18, 255}
                                  : (Color){231,  76,  60, 255};
            DrawText("VIE", px, py + 1, 10, (Color){85, 18, 18, 255});
            draw_bar(bar_x, py + 2, bar_w, 7, fmaxf(lr, 0.0f),
                     lc, (Color){26, 4, 4, 200});
            DrawText(TextFormat("%d", gs->lives),
                     val_x, py + 1, 10, lc);
            py += 16;
        }

        // FERRAILLE — uniquement en campagne (pas d'or en arcade)
        if (gs->is_campaign) {
            float sr = fminf((float)gs->meta.scrap / 200.0f, 1.0f);
            DrawText("SCR", px, py + 1, 10, (Color){30, 65, 30, 255});
            draw_bar(bar_x, py + 2, bar_w, 7, sr,
                     (Color){127, 200, 50, 255}, (Color){4, 16, 4, 200});
            DrawText(TextFormat("%d", gs->meta.scrap),
                     val_x, py + 1, 10, (Color){127, 200, 50, 255});
            py += 16;
        }

        // Séparateur fin
        DrawLine(px, py, UI_PANEL_W - M, py, (Color){40, 25, 6, 130});
        py += 5;

        // Vague
        DrawText(TextFormat("Vague %d", gs->wave_manager.number),
                 px, py, 9, (Color){110, 92, 58, 255});
        py += 12;

        // Compteurs tours / unités
        {
            int tlimit = gs->towers.tower_limit;
            int ulimit = gs->units.unit_limit;
            Color tc = gs->towers.tower_count >= tlimit
                ? (Color){231,76,60,255} : (Color){120,100,65,255};
            Color uc = gs->units.count >= ulimit
                ? (Color){231,76,60,255} : (Color){120,100,65,255};
            DrawText(TextFormat("Tours  %d/%d", gs->towers.tower_count, tlimit),
                     px, py, 9, tc); py += 12;
            DrawText(TextFormat("Unites %d/%d", gs->units.count, ulimit),
                     px, py, 9, uc); py += 12;
        }

         // ── Bases multiples ───────────────────────────────────
        {
            draw_sep(px, py, UI_PANEL_W - M * 2,
                     (Color){40, 25, 6, 130});
            py += 5;
            DrawText("BASES", px, py, 9, (Color){80, 60, 30, 255});
            py += 12;
 
            for (int b = 0; b < gs->map.base_count; b++) {
                const BaseInfo *base = &gs->map.bases[b];
 
                float ratio = (base->max_hp > 0)
                    ? (float)base->hp / (float)base->max_hp : 0.0f;
 
                Color bc;
                if (!base->active || base->hp <= 0) {
                    bc = (Color){100, 35, 35, 255};
                } else if (base->is_primary) {
                    bc = ratio > 0.5f ? (Color){46, 204, 113, 255}
                       : ratio > 0.25f ? (Color){243, 156, 18, 255}
                                       : (Color){231, 76, 60, 255};
                } else {
                    bc = ratio > 0.5f ? (Color){52, 152, 219, 255}
                       : ratio > 0.25f ? (Color){155, 89, 182, 255}
                                       : (Color){231, 76, 60, 255};
                }
 
                const char *name = base->is_primary ? "Principale" : TextFormat("Sec. %d", b);
                DrawText(name, px, py, 8, bc);
 
                int bw2 = UI_PANEL_W - M * 2;
                draw_bar(px, py + 10, bw2, 5,
                         fmaxf(ratio, 0.0f), bc,
                         (Color){18, 10, 4, 200});
 
                if (!base->active || base->hp <= 0)
                    DrawText("DETRUITE", px + bw2/2 - 22, py + 10, 7,
                             (Color){180, 60, 60, 255});
 
                py += 20;
            }
        }

        // Thème
        if (py + 12 <= HUD_Y + HUD_H) {
            char tbuf[26];
            clip_text(th->name, UI_PANEL_W - M * 2, 9, tbuf, sizeof(tbuf));
            DrawText(tbuf, px, py, 9, (Color){48, 82, 48, 255});
            py += 12;
        }

        // Vitesse
        if (py + 12 <= HUD_Y + HUD_H) {
            const char *sl[] = {"Vitesse x1","Vitesse x2","Vitesse x3"};
            const Color sc[] = {
                {80,118,80,255},{239,159,39,255},{231,76,60,255}
            };
            int idx = (ui->speed_mult >= 1 && ui->speed_mult <= 3)
                    ? ui->speed_mult - 1 : 0;
            DrawText(sl[idx], px, py, 9, sc[idx]);
            DrawText("[X]", px + 66, py, 8, (Color){50, 40, 22, 255});
            py += 12;
        }

        // Inventaire
        if (gs->inventory_count > 0 && py + 10 <= HUD_Y + HUD_H) {
            DrawText(TextFormat("Mat: %d", gs->inventory_count),
                     px, py, 9, (Color){62, 165, 185, 255});
        }
    }

    // ════════════════════════════════════════════════
    // PANNEAU CENTRAL — boutons
    // ════════════════════════════════════════════════
    {
        int lx    = UI_PANEL_W + M + 6;
        int row1y = (int)ui->tool_btns[TOOL_TOWER_GUN].y;
        int row2y = (int)ui->tool_btns[TOOL_UNIT_SOLDIER].y;

        DrawText("TOURS",  lx, row1y - 12, 9, (Color){100, 70, 22, 255});
        DrawText("UNITES", lx, row2y - 12, 9, (Color){38, 100, 38, 255});

        for (int i = 0; i < TOOL_COUNT; i++) {
            int can_afford = gs->gold >= TOOL_INFO[i].cost;

            // Griser si limite atteinte
            if (ui_tool_is_tower((ToolID)i) &&
                gs->towers.tower_count >= gs->towers.tower_limit)
                can_afford = 0;
            if (ui_tool_is_unit((ToolID)i) &&
                gs->units.count >= gs->units.unit_limit)
                can_afford = 0;

            draw_tool_btn(&ui->tool_btns[i], (ToolID)i,
                          ui->selected_tool == i,
                          ui->hovered_tool  == i,
                          can_afford);
        }

        // Message ouvrier sélectionné
        if (ui->worker_selected_idx >= 0) {
            DrawText("Cliquez sur un depot",
                     lx, row2y + UI_BTN_H + GAP, 9,
                     (Color){185, 185, 42, 200});
        }

        // Tooltip
        if (ui->hovered_tool != -1) {
            const ToolInfo  *info = &TOOL_INFO[ui->hovered_tool];
            const Rectangle *rb  = &ui->tool_btns[ui->hovered_tool];
            const int TW = 170, TH = 62;

            int tx = (int)rb->x;
            int ty = (int)rb->y - TH - GAP;
            if (ty < HUD_Y + 2) ty = HUD_Y + 2;
            if (ty + TH > HUD_Y + HUD_H - 2) ty = HUD_Y + HUD_H - TH - 2;
            if (tx < UI_PANEL_W + M) tx = UI_PANEL_W + M;
            if (tx + TW > VIRT_W - UI_PANEL_W - M)
                tx = VIRT_W - UI_PANEL_W - M - TW;

            float trnd = (float)UI_RADIUS / TH;
            DrawRectangleRounded(
                (Rectangle){(float)tx,(float)ty,(float)TW,(float)TH},
                trnd, 6, (Color){10, 6, 2, 252});
            DrawRectangleRoundedLinesEx(
                (Rectangle){(float)tx,(float)ty,(float)TW,(float)TH},
                trnd, 6, 1.5f, TOOL_COLORS[ui->hovered_tool]);

            char dbuf[48];
            clip_text(info->name, TW - M*2, 11, dbuf, sizeof(dbuf));
            DrawText(dbuf, tx+M, ty+M, 11, TOOL_COLORS[ui->hovered_tool]);
            clip_text(TextFormat("Dmg:%.0f  Port:%.1ft", info->dmg, info->range),
                      TW-M*2, 10, dbuf, sizeof(dbuf));
            DrawText(dbuf, tx+M, ty+23, 10, (Color){145,125,92,255});
            clip_text(TextFormat("Cad:%.1f/s  Cout:%dor", info->rate, info->cost),
                      TW-M*2, 10, dbuf, sizeof(dbuf));
            DrawText(dbuf, tx+M, ty+35, 10, (Color){145,125,92,255});
            clip_text(info->desc, TW-M*2, 9, dbuf, sizeof(dbuf));
            DrawText(dbuf, tx+M, ty+49, 9, (Color){82,65,40,255});

            int at_tower_limit = ui_tool_is_tower((ToolID)ui->hovered_tool) &&
                                 gs->towers.tower_count >= gs->towers.tower_limit;
            int at_unit_limit  = ui_tool_is_unit((ToolID)ui->hovered_tool) &&
                                 gs->units.count >= gs->units.unit_limit;
            if (at_tower_limit || at_unit_limit) {
                DrawText(TextFormat("LIMITE (%d bases)",
                             gs->map.base_count),
                         tx + M, ty + TH - 14, 9,
                         (Color){231, 76, 60, 255});
            }
        }
    }

    // ════════════════════════════════════════════════
    // BOUTON LANCER VAGUE
    // ════════════════════════════════════════════════
    {
        const Rectangle *wb = &ui->wave_btn;
        int   in_prep = (gs->phase == PHASE_PREP);
        float ratio   = gs->wave_manager.prep_timer / 20.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        float wrnd = (float)UI_RADIUS / wb->height;
        Color wbg  = in_prep ? (Color){5, 20, 8, 255} : (Color){7, 7, 7, 255};
        Color wbrd = in_prep ? (Color){22, 72, 32, 255} : (Color){30, 30, 30, 255};
        DrawRectangleRounded(*wb, wrnd, 6, wbg);
        DrawRectangleRoundedLinesEx(*wb, wrnd, 6, 1.5f, wbrd);

        Color tcol = ratio > 0.5f ? (Color){46,204,113,255}
                   : ratio > 0.2f ? (Color){243,156,18,255}
                                  : (Color){231,76,60,255};
        draw_bar((int)wb->x + GAP,
                 (int)wb->y + (int)wb->height - 12,
                 (int)wb->width - GAP*2, 5,
                 ratio, tcol, (Color){16,16,16,255});

        int wx = (int)(wb->x + wb->width/2);
        Color wlbl = in_prep ? (Color){42,188,105,255} : (Color){58,58,58,255};

        if (in_prep) {
            const char *l1 = "LANCER";
            DrawText(l1, wx - MeasureText(l1,13)/2, (int)wb->y + M, 13, wlbl);

            char b2[14];
            snprintf(b2, sizeof(b2), "+%dor", (int)(ratio*15.0f));
            DrawText(b2, wx - MeasureText(b2,10)/2, (int)wb->y+27, 10,
                     (Color){225,145,28,255});

            char b3[10];
            snprintf(b3, sizeof(b3), "%.0fs", gs->wave_manager.prep_timer);
            DrawText(b3, wx - MeasureText(b3,10)/2, (int)wb->y+40, 10,
                     (Color){82,65,40,255});
        } else {
            const char *l1 = "EN COURS";
            DrawText(l1, wx - MeasureText(l1,11)/2,
                     (int)(wb->y + wb->height/2 - 8), 11, wlbl);
        }
    }

    // ════════════════════════════════════════════════
    // PANNEAU DROIT
    // ════════════════════════════════════════════════
    {
        const int rx    = VIRT_W - UI_PANEL_W + M;
        const int max_w = UI_PANEL_W - M * 2;
        int py = HUD_Y + M;
        char buf[48];

        if (ui->selection.active) {
            const Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
            if (!tw->active) goto panel_right_empty;

            const TowerStats *st = &TOWER_BASE_STATS[tw->type];
            Color col = TOWER_FILL[tw->type];

            // Nom de la tour
            clip_text(st->name, max_w, 12, buf, sizeof(buf));
            DrawText(buf, rx, py, 12, col);
            py += 15;
            DrawLine(rx, py, rx + max_w, py, (Color){44, 28, 8, 140});
            py += 5;

            // Stats
            DrawText(TextFormat("Dmg  %.0f",   tw->damage),
                     rx, py, 10, (Color){148,128,95,255}); py += 12;
            DrawText(TextFormat("Port %.1ft",   tw->range),
                     rx, py, 10, (Color){148,128,95,255}); py += 12;
            DrawText(TextFormat("Cad  %.1f/s",  tw->fire_rate),
                     rx, py, 10, (Color){148,128,95,255}); py += 12;
            DrawText(TextFormat("Niv  %d",      tw->level),
                     rx, py, 10, (Color){212,138,25,255}); py += 12;
            DrawText(TextFormat("Type %s", DAMAGE_NAMES[tw->dmg_type]),
                     rx, py, 10, (Color){82,155,200,255}); py += 12;

            if (tw->material != MAT_NONE) {
                clip_text(TextFormat("[%s]", MATERIAL_NAMES[tw->material]),
                          max_w, 9, buf, sizeof(buf));
                DrawText(buf, rx, py, 9, (Color){62,172,192,255});
                py += 11;
            }

            // Bouton VENDRE — position fixe en bas du panneau
            {
                Rectangle sb  = ui->sell_btn;
                float     srnd = (float)UI_RADIUS / sb.height;
                Vector2   m   = virt_mouse();
                int hov = CheckCollisionPointRec(m, sb);
                DrawRectangleRounded(sb, srnd, 6,
                    hov ? (Color){48,8,8,255} : (Color){20,4,4,255});
                DrawRectangleRoundedLinesEx(sb, srnd, 6, 1.5f,
                    hov ? (Color){192,48,48,255} : (Color){90,18,18,255});
                int refund = (int)(tower_cost_on_tile(tw->type, &gs->map,
                                       tw->tile_x, tw->tile_y) * 0.6f);
                snprintf(buf, sizeof(buf), "Vendre +%dor", refund);
                int bw = MeasureText(buf, 9);
                DrawText(buf, (int)(sb.x + sb.width/2 - bw/2),
                         (int)(sb.y + sb.height/2 - 4),
                         9, (Color){192,58,42,255});
            }

            // Bouton APPLIQUER MATÉRIAU — position fixe tout en bas
            if (ui->apply_mat_visible && gs->inventory_count > 0) {
                Rectangle ab   = ui->apply_mat_btn;
                float     arnd = (float)UI_RADIUS / ab.height;
                Vector2   m    = virt_mouse();
                int hov = CheckCollisionPointRec(m, ab);
                DrawRectangleRounded(ab, arnd, 6,
                    hov ? (Color){4,26,36,255} : (Color){3,14,20,255});
                DrawRectangleRoundedLinesEx(ab, arnd, 6, 1.5f,
                    hov ? (Color){55,165,195,255} : (Color){24,82,100,255});
                MaterialType mat = gs->inventory[0];
                clip_text(TextFormat("+ %s", MATERIAL_NAMES[mat]),
                          max_w - M, 9, buf, sizeof(buf));
                int bw = MeasureText(buf, 9);
                DrawText(buf, (int)(ab.x + ab.width/2 - bw/2),
                         (int)(ab.y + ab.height/2 - 4),
                         9, (Color){62,175,200,255});
            }

        } else if (ui->worker_selected_idx >= 0) {
            const Unit *u = &gs->units.units[ui->worker_selected_idx];
            if (!u->active) goto panel_right_empty;

            DrawText("OUVRIER", rx, py, 12, (Color){192,192,42,255});
            py += 15;
            DrawLine(rx, py, rx+max_w, py, (Color){44,44,8,140});
            py += 5;

            DrawText(TextFormat("HP  %.0f/%.0f", u->hp, u->max_hp),
                     rx, py, 10, (Color){148,128,95,255}); py += 12;

            const char *ss;
            switch (u->state) {
                case USTATE_GOTO_DEPOSIT: ss = "-> Depot";    break;
                case USTATE_COLLECT:      ss = "Collecte..."; break;
                case USTATE_GOTO_BASE:    ss = "<- Base";     break;
                default:                  ss = "Patrouille";  break;
            }
            DrawText(ss, rx, py, 10, (Color){182,182,38,255}); py += 12;

            if (u->state == USTATE_COLLECT && u->collect_duration > 0.0f) {
                float prog = 1.0f - (u->collect_timer/u->collect_duration);
                draw_bar(rx, py, max_w, 6, prog,
                         (Color){62,175,200,255}, (Color){16,16,16,200});
                py += 10;
            }
            if (u->has_material && u->carried_mat != MAT_NONE) {
                DrawText(TextFormat("Porte %s",
                             MATERIAL_NAMES[u->carried_mat]),
                         rx, py, 9, (Color){62,175,200,255}); py += 11;
            }
            py += 4;
            DrawText("Clic depot = mission", rx, py, 9,
                     (Color){92,92,35,175});

        } else if (ui->selected_tool != TOOL_NONE) {
            const ToolInfo *info = &TOOL_INFO[ui->selected_tool];
            Color col = TOOL_COLORS[ui->selected_tool];

            clip_text(info->name, max_w, 12, buf, sizeof(buf));
            DrawText(buf, rx, py, 12, col);
            py += 15;
            DrawLine(rx, py, rx+max_w, py, (Color){44,28,8,140});
            py += 5;

            DrawText(TextFormat("Dmg  %.0f",  info->dmg),
                     rx, py, 10, (Color){148,128,95,255}); py += 12;
            DrawText(TextFormat("Port %.1ft", info->range),
                     rx, py, 10, (Color){148,128,95,255}); py += 12;
            DrawText(TextFormat("Cad  %.1f/s",info->rate),
                     rx, py, 10, (Color){148,128,95,255}); py += 12;

            if (ui_tool_is_tower(ui->selected_tool) &&
                ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0) {
                TowerType tt = ui_tool_to_tower(ui->selected_tool);
                int real_cost = tower_cost_on_tile(tt, &gs->map,
                                    ui->hovered_tile_x,
                                    ui->hovered_tile_y);
                int is_ruin = (gs->map.tiles[ui->hovered_tile_y]
                                            [ui->hovered_tile_x].type
                               == TILE_RUIN);
                if (is_ruin) {
                    DrawText(TextFormat("Cout %dor (x2)", real_cost),
                             rx, py, 10, (Color){205,108,22,255}); py += 12;
                } else {
                    DrawText(TextFormat("Cout %dor", real_cost),
                             rx, py, 10, (Color){212,138,25,255}); py += 12;
                }
            } else {
                DrawText(TextFormat("Cout %dor", info->cost),
                         rx, py, 10, (Color){212,138,25,255}); py += 12;
            }
            clip_text(info->desc, max_w, 9, buf, sizeof(buf));
            DrawText(buf, rx, py, 9, (Color){82,65,40,255});

        } else {
            panel_right_empty:
            DrawText("Clic sur",    rx, py, 9, (Color){50,40,25,255}); py += 12;
            DrawText("un outil",    rx, py, 9, (Color){50,40,25,255}); py += 12;
            DrawText("ou une tour", rx, py, 9, (Color){50,40,25,255}); py += 12;
            DrawText("posee.",      rx, py, 9, (Color){50,40,25,255});
        }
    }

    // ════════════════════════════════════════════════
    // PRÉVISUALISATION
    // ════════════════════════════════════════════════
    if (ui->selected_tool != TOOL_NONE &&
        ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0) {
        if (ui_tool_is_tower(ui->selected_tool)) {
            render_tower_preview(&gs->map, &gs->towers,
                                 ui_tool_to_tower(ui->selected_tool),
                                 ui->hovered_tile_x,
                                 ui->hovered_tile_y);
        } else {
            // Cercle de spawn autour de CHAQUE base active
            for (int b = 0; b < gs->map.base_count; b++) {
                if (!gs->map.bases[b].active) continue;
                float bpx = gs->map.bases[b].pos.x * TILE_SIZE + TILE_SIZE/2.0f;
                float bpy = gs->map.bases[b].pos.y * TILE_SIZE + TILE_SIZE/2.0f;
                DrawCircleLines((int)bpx, (int)bpy,
                                5.0f * TILE_SIZE,
                                (Color){39, 174, 96, 100});
            }
        }
    }

    // Highlight dépôts si ouvrier sélectionné
    if (ui->worker_selected_idx >= 0) {
        float t = (float)GetTime();
        for (int d = 0; d < gs->map.deposit_count; d++) {
            const MaterialDeposit *dep = &gs->map.deposits[d];
            if (!dep->active) continue;
            int cx = dep->tile_x * TILE_SIZE + TILE_SIZE/2;
            int cy = dep->tile_y * TILE_SIZE + TILE_SIZE/2;
            float pulse = (sinf(t * 5.0f + (float)d) + 1.0f) * 0.5f;
            DrawCircleLines(cx, cy, TILE_SIZE/2 + 3,
                (Color){192,192,42,
                        (unsigned char)(95 + (int)(pulse*100))});
        }
    }

    // ════════════════════════════════════════════════
    // FPS
    // ════════════════════════════════════════════════
    if (ui->show_fps) {
        int fps = GetFPS();
        char fb[14];
        snprintf(fb, sizeof(fb), "%d FPS", fps);
        Color fc = fps >= 150 ? (Color){46,204,113,255}
                 : fps >= 60  ? (Color){243,156,18,255}
                              : (Color){231,76,60,255};
        DrawText(fb, VIRT_W - 66, HUD_Y + M, 11, fc);
    }
}