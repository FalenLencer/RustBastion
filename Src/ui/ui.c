#include "ui.h"
#include "renderer.h"
#include "../meta/meta.h"
#include "../map/pathfinding.h"
#include <string.h>
#include <math.h>
#include "../core/game_state.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>

// ════════════════════════════════════════════════════
// TRANSFORMATION SOURIS
// main.c appelle ui_set_mouse_offset() chaque frame
// pour que les tests de collision utilisent les
// coordonnées du canvas virtuel (1120×770).
// ════════════════════════════════════════════════════
static float g_mouse_ox    = 0.0f;
static float g_mouse_oy    = 0.0f;
static float g_mouse_scale = 1.0f;

void ui_set_mouse_offset(float ox, float oy, float scale) {
    g_mouse_ox    = ox;
    g_mouse_oy    = oy;
    g_mouse_scale = scale > 0.001f ? scale : 1.0f;
}

// Retourne la position souris dans l'espace virtuel
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
    [TOOL_TOWER_GUN]    = {"TOURELLE",  "Gun",    "T", "Polyvalente.",   15, 20,  3.5f, 1.5f, 0},
    [TOOL_TOWER_SNIPER] = {"SNIPER",    "Sniper", "S", "Longue portee.", 25, 90,  6.5f, 0.5f, 0},
    [TOOL_TOWER_FLAME]  = {"FLAMMES",   "Flame",  "F", "Zone courte.",   30, 12,  2.5f, 3.0f, 0},
    [TOOL_TOWER_TESLA]  = {"TESLA",     "Tesla",  "E", "Chaine x3.",     40, 45,  4.0f, 0.8f, 0},
    [TOOL_UNIT_SOLDIER] = {"SOLDAT",    "Soldat", "o", "Polyvalent.",    20, 25,  1.2f, 1.2f, 1},
    [TOOL_UNIT_HEAVY]   = {"LOURD",     "Lourd",  "H", "Tank.",          35, 50,  1.0f, 0.6f, 1},
    [TOOL_UNIT_MEDIC]   = {"MEDIC",     "Medic",  "+", "Soigneur.",      25,  8,  3.0f, 0.5f, 1},
    [TOOL_UNIT_DOG]     = {"CHIEN",     "Chien",  "d", "Rapide.",        10, 15,  0.8f, 2.0f, 1},
};

const char *ui_tool_name(ToolID id) {
    if (id < 0 || id >= TOOL_COUNT) return "Aucun";
    return TOOL_INFO[id].shortname;
}

static Color TOOL_COLORS[TOOL_COUNT] = {
    [TOOL_TOWER_GUN]    = {192, 57,  43,  255},
    [TOOL_TOWER_SNIPER] = { 52,152, 219,  255},
    [TOOL_TOWER_FLAME]  = {230,126,  34,  255},
    [TOOL_TOWER_TESLA]  = {155, 89, 182,  255},
    [TOOL_UNIT_SOLDIER] = { 39,174,  96,  255},
    [TOOL_UNIT_HEAVY]   = { 41,128, 185,  255},
    [TOOL_UNIT_MEDIC]   = {231, 76,  60,  255},
    [TOOL_UNIT_DOG]     = {243,156,  18,  255},
};

// ════════════════════════════════════════════════════
// CONVERSIONS
// ════════════════════════════════════════════════════
int ui_tool_is_tower(ToolID id) { return id >= TOOL_TOWER_GUN    && id <= TOOL_TOWER_TESLA; }
int ui_tool_is_unit (ToolID id) { return id >= TOOL_UNIT_SOLDIER && id <= TOOL_UNIT_DOG;    }
TowerType ui_tool_to_tower(ToolID id) { return (TowerType)(id - TOOL_TOWER_GUN);     }
UnitType  ui_tool_to_unit (ToolID id) { return (UnitType) (id - TOOL_UNIT_SOLDIER);  }

// ════════════════════════════════════════════════════
// INIT — rectangles en coordonnées VIRTUELLES (fixes)
// ════════════════════════════════════════════════════
void ui_init(UIState *ui) {
    memset(ui, 0, sizeof(UIState));
    ui->selected_tool  = TOOL_TOWER_GUN;
    ui->hovered_tool   = -1;
    ui->hovered_tile_x = -1;
    ui->hovered_tile_y = -1;

    // Le HUD commence à MAP_H*TILE_SIZE en coordonnées virtuelles
    const int HUD_Y   = MAP_H * TILE_SIZE;         // 640
    const int PAD     = 10;
    const int BTN_GAP = 6;

    // Panneau gauche : largeur UI_PANEL_W (190)
    // Panneau droit  : largeur UI_PANEL_W
    // Centre : boutons outils + bouton vague

    int center_x  = UI_PANEL_W + PAD + 8;
    int btn_row1  = HUD_Y + 14;                    // rangée tours
    int btn_row2  = btn_row1 + UI_BTN_H + BTN_GAP; // rangée unités

    // 4 boutons tours (rangée 1)
    for (int i = 0; i < 4; i++) {
        ui->tool_btns[TOOL_TOWER_GUN + i] = (Rectangle){
            center_x + i * (UI_BTN_W + BTN_GAP),
            btn_row1,
            UI_BTN_W, UI_BTN_H
        };
    }
    // 4 boutons unités (rangée 2)
    for (int i = 0; i < 4; i++) {
        ui->tool_btns[TOOL_UNIT_SOLDIER + i] = (Rectangle){
            center_x + i * (UI_BTN_W + BTN_GAP),
            btn_row2,
            UI_BTN_W, UI_BTN_H
        };
    }

    // Bouton LANCER VAGUE — à droite des boutons outils
    int wave_x = center_x + 4 * (UI_BTN_W + BTN_GAP) + 12;
    ui->wave_btn = (Rectangle){
        wave_x,
        btn_row1,
        118, UI_BTN_H * 2 + BTN_GAP
    };

    // Bouton vendre — panneau droit
    const int VIRT_W = MAP_W * TILE_SIZE;  // 1120
    ui->sell_btn = (Rectangle){
        VIRT_W - UI_PANEL_W + PAD,
        HUD_Y + 92,
        UI_PANEL_W - PAD * 2 - 10,
        22
    };
}

// ════════════════════════════════════════════════════
// MISE À JOUR — coordonnées virtuelles
// ════════════════════════════════════════════════════
void ui_update(UIState *ui, GameState *gs) {
    Vector2 mouse  = virt_mouse();
    const int HUD_Y = MAP_H * TILE_SIZE;

    // ── Hover outil ───────────────────────────────────────
    ui->hovered_tool = -1;
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (CheckCollisionPointRec(mouse, ui->tool_btns[i])) {
            ui->hovered_tool = i;
            break;
        }
    }

    // ── Hover tuile (zone map uniquement) ─────────────────
    if (mouse.y >= 0 && mouse.y < HUD_Y &&
        mouse.x >= 0 && mouse.x < MAP_W * TILE_SIZE) {
        ui->hovered_tile_x = (int)(mouse.x / TILE_SIZE);
        ui->hovered_tile_y = (int)(mouse.y / TILE_SIZE);
    } else {
        ui->hovered_tile_x = -1;
        ui->hovered_tile_y = -1;
    }

    // ── Clic gauche ───────────────────────────────────────
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

        // Bouton outil
        if (ui->hovered_tool != -1) {
            ui->selected_tool    = (ToolID)ui->hovered_tool;
            ui->selection.active = 0;
        }
        // Bouton vague
        else if (CheckCollisionPointRec(mouse, ui->wave_btn)) {
            if (gs->phase == PHASE_PREP) {
                int bonus = (int)(gs->wave_manager.prep_timer / 20.0f * 30.0f);
                gs->gold += bonus;
                wave_start(&gs->wave_manager);
                gs->phase = PHASE_WAVE;
            }
        }
        // Clic sur la carte
        else if (mouse.y < HUD_Y && ui->hovered_tile_x >= 0) {
            int tx = ui->hovered_tile_x;
            int ty = ui->hovered_tile_y;

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
            } else if (ui->selected_tool != TOOL_NONE) {
                if (ui_tool_is_tower(ui->selected_tool)) {
                    tower_place(&gs->towers,
                                ui_tool_to_tower(ui->selected_tool),
                                tx, ty, &gs->map, &gs->gold,
                                &gs->bonuses);
                    ui->selection.active = 0;
                } else if (ui_tool_is_unit(ui->selected_tool)) {
                    float base_px = gs->units.base_px;
                    float base_py = gs->units.base_py;
                    float dx = mouse.x - base_px;
                    float dy = mouse.y - base_py;
                    float d  = sqrtf(dx*dx + dy*dy);
                    if (d <= 5.0f * TILE_SIZE)
                        unit_spawn(&gs->units,
                                   ui_tool_to_unit(ui->selected_tool),
                                   &gs->gold, &gs->bonuses);
                }
            }
        }
        // Bouton vendre
        else if (ui->selection.active &&
                 CheckCollisionPointRec(mouse, ui->sell_btn)) {
            Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
            if (tw->active) {
                gs->gold += (int)(TOWER_BASE_STATS[tw->type].cost * 0.6f);
                gs->map.tiles[tw->tile_y][tw->tile_x].buildable = 1;
                tw->active = 0;
                gs->towers.tower_count--;
            }
            ui->selection.active = 0;
        }
    }

    // ── Clic droit — désélectionne ────────────────────────
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        ui->selected_tool    = TOOL_NONE;
        ui->selection.active = 0;
    }

    // ── Raccourcis clavier ────────────────────────────────
    if (IsKeyPressed(KEY_ONE))   ui->selected_tool = TOOL_TOWER_GUN;
    if (IsKeyPressed(KEY_TWO))   ui->selected_tool = TOOL_TOWER_SNIPER;
    if (IsKeyPressed(KEY_THREE)) ui->selected_tool = TOOL_TOWER_FLAME;
    if (IsKeyPressed(KEY_FOUR))  ui->selected_tool = TOOL_TOWER_TESLA;
    if (IsKeyPressed(KEY_FIVE))  ui->selected_tool = TOOL_UNIT_SOLDIER;
    if (IsKeyPressed(KEY_SIX))   ui->selected_tool = TOOL_UNIT_HEAVY;
    if (IsKeyPressed(KEY_SEVEN)) ui->selected_tool = TOOL_UNIT_MEDIC;
    if (IsKeyPressed(KEY_EIGHT)) ui->selected_tool = TOOL_UNIT_DOG;
    if (IsKeyPressed(KEY_ESCAPE)) {
        ui->selected_tool    = TOOL_NONE;
        ui->selection.active = 0;
    }
}

// ════════════════════════════════════════════════════
// HELPERS DE RENDU (coordonnées virtuelles, pas de S())
// ════════════════════════════════════════════════════
static void draw_bar(int x, int y, int w, int h,
                     float ratio, Color fill, Color bg) {
    DrawRectangle(x, y, w, h, bg);
    int fw = (int)(w * (ratio < 0.0f ? 0.0f : ratio > 1.0f ? 1.0f : ratio));
    DrawRectangle(x, y, fw, h, fill);
    DrawRectangleLines(x, y, w, h, (Color){60, 40, 20, 180});
}

static void draw_tool_btn(const Rectangle *r, ToolID id,
                           int is_selected, int is_hovered,
                           int can_afford)
{
    const ToolInfo *info = &TOOL_INFO[id];
    Color col = TOOL_COLORS[id];

    Color bg = is_selected ? (Color){40, 25,  8, 255} :
               is_hovered  ? (Color){30, 20,  6, 255} :
                             (Color){18, 12,  4, 255};
    DrawRectangleRec(*r, bg);

    Color border = is_selected ? col :
                   is_hovered  ? (Color){col.r/2, col.g/2, col.b/2, 255} :
                                 (Color){50, 35, 15, 255};
    float bw = is_selected ? 2.0f : 1.0f;
    DrawRectangleLinesEx(*r, bw, border);

    if (!can_afford) col = (Color){80, 60, 40, 255};

    // Icône
    int icon_fs = 20;
    DrawText(info->icon,
             (int)(r->x + r->width  / 2 - MeasureText(info->icon, icon_fs) / 2),
             (int)(r->y + 6),
             icon_fs, col);

    // Nom court
    int name_fs = 9;
    DrawText(info->shortname,
             (int)(r->x + r->width  / 2 - MeasureText(info->shortname, name_fs) / 2),
             (int)(r->y + 29),
             name_fs,
             can_afford ? (Color){160, 140, 110, 255} : (Color){80, 60, 40, 255});

    // Coût
    int needed = snprintf(NULL, 0, "%dor", info->cost) + 1;
    char *cost_str = malloc(needed);

    snprintf(cost_str, needed, "%dor", info->cost);

    DrawText(cost_str,
             (int)(r->x + r->width  / 2 - MeasureText(cost_str, name_fs) / 2),
             (int)(r->y + 40),
             name_fs,
             can_afford ? (Color){239, 159, 39, 255} : (Color){150, 60, 40, 255});

    free(cost_str);
    // Glow si sélectionné
    if (is_selected)
        DrawRectangleLinesEx(
            (Rectangle){r->x - 2, r->y - 2, r->width + 4, r->height + 4},
            1, (Color){col.r, col.g, col.b, 80});
}

// ════════════════════════════════════════════════════
// RENDU — tout en coordonnées virtuelles (0…1120, 0…770)
// ════════════════════════════════════════════════════
void ui_render(const UIState *ui, const GameState *gs) {
    const int VIRT_W  = MAP_W * TILE_SIZE;          // 1120
    const int HUD_Y   = MAP_H * TILE_SIZE;          // 640
    const int HUD_H   = UI_HUD_HEIGHT;              // 130
    const int PAD     = 10;

    const Theme *th = theme_get(gs->map.theme);

    // ── Fond HUD ──────────────────────────────────────────
    DrawRectangle(0, HUD_Y, VIRT_W, HUD_H, (Color){10, 6, 2, 255});
    DrawLine(0, HUD_Y, VIRT_W, HUD_Y, (Color){80, 50, 10, 220});

    // Séparateurs verticaux
    DrawLine(UI_PANEL_W, HUD_Y, UI_PANEL_W,
             HUD_Y + HUD_H, (Color){50, 32, 10, 180});
    DrawLine(VIRT_W - UI_PANEL_W, HUD_Y, VIRT_W - UI_PANEL_W,
             HUD_Y + HUD_H, (Color){50, 32, 10, 180});

    // ════════════════════════════════════════════════
    // PANNEAU GAUCHE — stats
    // ════════════════════════════════════════════════
    {
        int px = PAD;
        int py = HUD_Y + PAD;
        int bar_x  = px + 26;
        int bar_w  = UI_PANEL_W - 26 - 36;
        int val_x  = UI_PANEL_W - 32;

        // OR
        DrawText("OR",  px, py, 11, (Color){139, 94,  0, 255});
        draw_bar(bar_x, py + 2, bar_w, 9,
                 fminf((float)gs->gold / 300.0f, 1.0f),
                 (Color){239, 159, 39, 255}, (Color){30, 15, 0, 255});
        DrawText(TextFormat("%d", gs->gold),
                 val_x, py, 11, (Color){239, 159, 39, 255});
        py += 20;

        // VIES
        float lives_ratio = gs->bonuses.start_lives > 0
            ? (float)gs->lives / (float)gs->bonuses.start_lives : 0.0f;
        Color lcol = lives_ratio > 0.5f  ? (Color){46, 204, 113, 255}
                   : lives_ratio > 0.25f ? (Color){243, 156, 18, 255}
                                         : (Color){231,  76, 60, 255};
        DrawText("VIE", px, py, 11, (Color){139, 30, 30, 255});
        draw_bar(bar_x, py + 2, bar_w, 9,
                 fmaxf(lives_ratio, 0.0f), lcol, (Color){30, 5, 5, 255});
        DrawText(TextFormat("%d", gs->lives),
                 val_x, py, 11, lcol);
        py += 20;

        // FERRAILLE
        DrawText("SCR", px, py, 11, (Color){50, 100, 50, 255});
        draw_bar(bar_x, py + 2, bar_w, 9,
                 fminf((float)gs->meta.scrap / 200.0f, 1.0f),
                 (Color){127, 200, 50, 255}, (Color){5, 20, 5, 255});
        DrawText(TextFormat("%d", gs->meta.scrap),
                 val_x, py, 11, (Color){127, 200, 50, 255});
        py += 20;

        // Vague + thème
        DrawText(TextFormat("VAGUE %d", gs->wave_manager.number),
                 px, py, 11, (Color){180, 160, 110, 255});
        py += 16;
        DrawText(th->name, px, py, 10, (Color){80, 120, 80, 255});
    }

    // ════════════════════════════════════════════════
    // PANNEAU CENTRAL — boutons outils
    // ════════════════════════════════════════════════
    {
        int lx = UI_PANEL_W + PAD + 8;
        DrawText("TOURS",
                 lx, HUD_Y + 4, 9, (Color){100, 70, 30, 255});
        DrawText("UNITES",
                 lx, HUD_Y + 4 + UI_BTN_H + 6, 9, (Color){50, 100, 50, 255});

        // Boutons
        for (int i = 0; i < TOOL_COUNT; i++) {
            int can_afford = gs->gold >= TOOL_INFO[i].cost;
            draw_tool_btn(&ui->tool_btns[i], (ToolID)i,
                          ui->selected_tool == i,
                          ui->hovered_tool  == i,
                          can_afford);
        }

        // Tooltip
        if (ui->hovered_tool != -1) {
            const ToolInfo  *info = &TOOL_INFO[ui->hovered_tool];
            const Rectangle *r   = &ui->tool_btns[ui->hovered_tool];
            int tx = (int)r->x;
            int ty = (int)r->y - 58;
            if (ty < 0) ty = (int)r->y + UI_BTN_H + 4;
            DrawRectangle(tx, ty, 162, 56, (Color){10, 6, 2, 240});
            DrawRectangleLines(tx, ty, 162, 56, TOOL_COLORS[ui->hovered_tool]);
            DrawText(info->name,
                     tx + 6, ty + 4, 11, TOOL_COLORS[ui->hovered_tool]);
            DrawText(TextFormat("Dmg:%.0f  Port:%.1ft", info->dmg, info->range),
                     tx + 6, ty + 18, 10, (Color){160, 140, 110, 255});
            DrawText(TextFormat("Cad:%.1f/s  Cout:%dor", info->rate, info->cost),
                     tx + 6, ty + 30, 10, (Color){160, 140, 110, 255});
            DrawText(info->desc,
                     tx + 6, ty + 42, 9, (Color){100, 80, 50, 255});
        }
    }

    // ════════════════════════════════════════════════
    // BOUTON LANCER VAGUE
    // ════════════════════════════════════════════════
    {
        const Rectangle *wb    = &ui->wave_btn;
        int   in_prep          = (gs->phase == PHASE_PREP);
        float ratio            = gs->wave_manager.prep_timer / 20.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        Color wbg = in_prep ? (Color){8, 30, 12, 255} : (Color){8, 8, 8, 255};
        DrawRectangleRec(*wb, wbg);

        Color tcol = ratio > 0.5f  ? (Color){46,  204, 113, 255}
                   : ratio > 0.2f  ? (Color){243, 156,  18, 255}
                                   : (Color){231,  76,  60, 255};
        draw_bar((int)wb->x + 4, (int)wb->y + (int)wb->height - 12,
                 (int)wb->width - 8, 8, ratio, tcol, (Color){20, 20, 20, 255});

        Color wlabel = in_prep ? (Color){46, 204, 113, 255}
                               : (Color){80, 80, 80, 255};
        if (in_prep) {
            int bonus = (int)(ratio * 30.0f);
            DrawText("LANCER",
                     (int)(wb->x + 8), (int)(wb->y + 8),  13, wlabel);
            DrawText(TextFormat("+%dor bonus", bonus),
                     (int)(wb->x + 8), (int)(wb->y + 26), 10,
                     (Color){239, 159, 39, 255});
            DrawText(TextFormat("%.0fs", gs->wave_manager.prep_timer),
                     (int)(wb->x + 8), (int)(wb->y + 40), 10,
                     (Color){100, 80, 50, 255});
        } else {
            DrawText("EN COURS",
                     (int)(wb->x + 8), (int)(wb->y + 20), 11, wlabel);
        }
        DrawRectangleLinesEx(*wb, 1.0f,
            in_prep ? (Color){27, 94, 46, 255} : (Color){40, 40, 40, 255});
    }

    // ════════════════════════════════════════════════
    // PANNEAU DROIT — info outil / tour sélectionnée
    // ════════════════════════════════════════════════
    {
        int rx = VIRT_W - UI_PANEL_W + PAD;
        int py = HUD_Y + PAD;

        if (ui->selection.active) {
            const Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
            if (tw->active) {
                const TowerStats *st = &TOWER_BASE_STATS[tw->type];
                Color col = TOWER_FILL[tw->type];

                DrawText(st->name,                           rx, py, 13, col);  py += 18;
                DrawText(TextFormat("Dmg   : %.0f", tw->damage),  rx, py, 10, (Color){160,140,110,255}); py += 14;
                DrawText(TextFormat("Port  : %.1ft", tw->range),  rx, py, 10, (Color){160,140,110,255}); py += 14;
                DrawText(TextFormat("Cad   : %.1f/s", tw->fire_rate), rx, py, 10, (Color){160,140,110,255}); py += 14;
                DrawText(TextFormat("Niveau: %d", tw->level),     rx, py, 10, (Color){239,159,39,255});  py += 18;

                // Bouton vendre
                DrawRectangleRec(ui->sell_btn, (Color){30, 8, 8, 255});
                DrawRectangleLinesEx(ui->sell_btn, 1, (Color){139, 30, 30, 255});
                int refund = (int)(st->cost * 0.6f);
                DrawText(TextFormat("Vendre +%dor", refund),
                         (int)(ui->sell_btn.x + 5),
                         (int)(ui->sell_btn.y + 5),
                         10, (Color){192, 57, 43, 255});
            }
        } else if (ui->selected_tool != TOOL_NONE) {
            const ToolInfo *info = &TOOL_INFO[ui->selected_tool];
            Color col = TOOL_COLORS[ui->selected_tool];

            DrawText(info->name,                                  rx, py, 13, col);  py += 18;
            DrawText(TextFormat("Dmg   : %.0f",  info->dmg),     rx, py, 10, (Color){160,140,110,255}); py += 14;
            DrawText(TextFormat("Port  : %.1ft", info->range),   rx, py, 10, (Color){160,140,110,255}); py += 14;
            DrawText(TextFormat("Cad   : %.1f/s",info->rate),    rx, py, 10, (Color){160,140,110,255}); py += 14;
            DrawText(TextFormat("Cout  : %dor",  info->cost),    rx, py, 10, (Color){239,159,39,255});  py += 18;
            DrawText(info->desc,                                  rx, py,  9, (Color){100, 80, 50, 255});
        } else {
            DrawText("Clique sur",  rx, py,      10, (Color){60, 50, 35, 255});
            DrawText("un outil",    rx, py + 14, 10, (Color){60, 50, 35, 255});
            DrawText("ou une tour", rx, py + 28, 10, (Color){60, 50, 35, 255});
            DrawText("posee.",      rx, py + 42, 10, (Color){60, 50, 35, 255});
        }
    }

    // ════════════════════════════════════════════════
    // PRÉVISUALISATION SUR LA CARTE
    // ════════════════════════════════════════════════
    if (ui->selected_tool != TOOL_NONE &&
        ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0) {
        if (ui_tool_is_tower(ui->selected_tool)) {
            render_tower_preview(&gs->map, &gs->towers,
                                 ui_tool_to_tower(ui->selected_tool),
                                 ui->hovered_tile_x,
                                 ui->hovered_tile_y);
        } else {
            DrawCircleLines((int)gs->units.base_px, (int)gs->units.base_py,
                            5.0f * TILE_SIZE, (Color){39, 174, 96, 100});
        }
    }
}