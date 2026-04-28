#include "ui.h"
#include "renderer.h"
#include "meta.h"
#include "pathfinding.h"
#include <string.h>
#include <math.h>
#include "renderer.h"
#include "game_state.h"
// ════════════════════════════════════════════════════
// DONNÉES DES OUTILS
// ════════════════════════════════════════════════════



// ── 1. STRUCT ToolInfo EN PREMIER ────────────────────────────
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

// ── 2. TABLEAU TOOL_INFO ──────────────────────────────────────
static const ToolInfo TOOL_INFO[TOOL_COUNT] = {
    [TOOL_TOWER_GUN]    = {"TOURELLE",  "Gun",     "T", "Polyvalente.",  15, 20,  3.5f, 1.5f, 0},
    [TOOL_TOWER_SNIPER] = {"SNIPER",    "Sniper",  "S", "Longue portee.",25, 90,  6.5f, 0.5f, 0},
    [TOOL_TOWER_FLAME]  = {"FLAMMES",   "Flame",   "F", "Zone courte.",  30, 12,  2.5f, 3.0f, 0},
    [TOOL_TOWER_TESLA]  = {"TESLA",     "Tesla",   "E", "Chaine x3.",    40, 45,  4.0f, 0.8f, 0},
    [TOOL_UNIT_SOLDIER] = {"SOLDAT",    "Soldat",  "o", "Polyvalent.",   20, 25,  1.2f, 1.2f, 1},
    [TOOL_UNIT_HEAVY]   = {"LOURD",     "Lourd",   "H", "Tank.",         35, 50,  1.0f, 0.6f, 1},
    [TOOL_UNIT_MEDIC]   = {"MEDIC",     "Medic",   "+", "Soigneur.",     25,  8,  3.0f, 0.5f, 1},
    [TOOL_UNIT_DOG]     = {"CHIEN",     "Chien",   "d", "Rapide.",       10, 15,  0.8f, 2.0f, 1},
};

const char *ui_tool_name(ToolID id) {
    if (id < 0 || id >= TOOL_COUNT) return "Aucun";
    return TOOL_INFO[id].shortname;
}

// Couleurs par outil
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
int ui_tool_is_tower(ToolID id) { return id >= TOOL_TOWER_GUN && id <= TOOL_TOWER_TESLA; }
int ui_tool_is_unit (ToolID id) { return id >= TOOL_UNIT_SOLDIER && id <= TOOL_UNIT_DOG; }

TowerType ui_tool_to_tower(ToolID id) { return (TowerType)(id - TOOL_TOWER_GUN); }
UnitType  ui_tool_to_unit (ToolID id) { return (UnitType) (id - TOOL_UNIT_SOLDIER); }

// ════════════════════════════════════════════════════
// INIT — calcule les rectangles
// ════════════════════════════════════════════════════
void ui_init(UIState *ui) {
    memset(ui, 0, sizeof(UIState));
    ui->selected_tool  = TOOL_TOWER_GUN;
    ui->hovered_tool   = -1;
    ui->hovered_tile_x = -1;
    ui->hovered_tile_y = -1;

    int sw     = GetScreenWidth();
    int hud_y  = MAP_H * TILE_SIZE;
    int center_x = UI_PANEL_W + 8;
    int btn_y_towers = hud_y + 8;
    int btn_y_units  = hud_y + 8 + UI_BTN_H + 6;

    // 4 boutons tours
    for (int i = 0; i < 4; i++) {
        ui->tool_btns[TOOL_TOWER_GUN + i] = (Rectangle){
            center_x + i * (UI_BTN_W + 4),
            btn_y_towers,
            UI_BTN_W, UI_BTN_H
        };
    }
    // 4 boutons unités
    for (int i = 0; i < 4; i++) {
        ui->tool_btns[TOOL_UNIT_SOLDIER + i] = (Rectangle){
            center_x + i * (UI_BTN_W + 4),
            btn_y_units,
            UI_BTN_W, UI_BTN_H
        };
    }

    // Bouton vague
    ui->wave_btn = (Rectangle){
        center_x + 4*(UI_BTN_W+4) + 8,
        btn_y_towers,
        120, UI_BTN_H * 2 + 6
    };

    // Bouton vendre (panneau droit)
    ui->sell_btn = (Rectangle){
        sw - UI_PANEL_W + 8,
        hud_y + 90,
        80, 22
    };
}

// ════════════════════════════════════════════════════
// MISE À JOUR — gestion des clics
// ════════════════════════════════════════════════════
void ui_update(UIState *ui, GameState *gs) {
    Vector2 mouse = GetMousePosition();
    int hud_y     = MAP_H * TILE_SIZE;

    // ── Hover outil ───────────────────────────────────────
    ui->hovered_tool = -1;
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (CheckCollisionPointRec(mouse, ui->tool_btns[i])) {
            ui->hovered_tool = i;
            break;
        }
    }

    // ── Hover tuile ───────────────────────────────────────
    if (mouse.y < hud_y) {
        ui->hovered_tile_x = (int)(mouse.x / TILE_SIZE);
        ui->hovered_tile_y = (int)(mouse.y / TILE_SIZE);
    } else {
        ui->hovered_tile_x = -1;
        ui->hovered_tile_y = -1;
    }

    // ── Clic gauche ───────────────────────────────────────
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

        // Clic sur un bouton outil
        if (ui->hovered_tool != -1) {
            ui->selected_tool = (ToolID)ui->hovered_tool;
            ui->selection.active = 0;   // désélectionne tour
        }

        // Clic sur bouton vague
        else if (CheckCollisionPointRec(mouse, ui->wave_btn)) {
            if (gs->phase == PHASE_PREP) {
                // Bonus or proportionnel au temps restant
                int bonus = (int)(gs->wave_manager.prep_timer / 20.0f * 30.0f);
                gs->gold += bonus;
                wave_start(&gs->wave_manager);
                gs->phase = PHASE_WAVE;
            }
        }

        // Clic sur la carte
        else if (mouse.y < hud_y) {
            int tx = ui->hovered_tile_x;
            int ty = ui->hovered_tile_y;

            // Vérifie si on clique sur une tour existante
            int clicked_tower = -1;
            for (int i = 0; i < MAX_TOWERS; i++) {
                Tower *tw = &gs->towers.towers[i];
                if (!tw->active) continue;
                if (tw->tile_x == tx && tw->tile_y == ty) {
                    clicked_tower = i;
                    break;
                }
            }

            if (clicked_tower != -1) {
                // Sélectionne la tour
                ui->selection.active    = 1;
                ui->selection.tower_idx = clicked_tower;
                ui->selected_tool       = TOOL_NONE;
            } else if (ui->selected_tool != TOOL_NONE) {
                // Place tour ou unité
                if (ui_tool_is_tower(ui->selected_tool)) {
                    tower_place(&gs->towers,
                                ui_tool_to_tower(ui->selected_tool),
                                tx, ty, &gs->map, &gs->gold,
                                &gs->bonuses);
                    ui->selection.active = 0;
                } else if (ui_tool_is_unit(ui->selected_tool)) {
                    float base_px = gs->units.base_px;
                    float base_py = gs->units.base_py;
                    float d = sqrtf(
                        (mouse.x-base_px)*(mouse.x-base_px)+
                        (mouse.y-base_py)*(mouse.y-base_py));
                    if (d <= 5.0f * TILE_SIZE)
                        unit_spawn(&gs->units,
                                   ui_tool_to_unit(ui->selected_tool),
                                   &gs->gold, &gs->bonuses);
                }
            }
        }

        // Clic sur bouton vendre
        else if (ui->selection.active &&
                 CheckCollisionPointRec(mouse, ui->sell_btn)) {
            Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
            if (tw->active) {
                // Rembourse 60% du coût
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

    // ── Raccourcis clavier conservés ─────────────────────
    if (IsKeyPressed(KEY_ONE))   ui->selected_tool = TOOL_TOWER_GUN;
    if (IsKeyPressed(KEY_TWO))   ui->selected_tool = TOOL_TOWER_SNIPER;
    if (IsKeyPressed(KEY_THREE)) ui->selected_tool = TOOL_TOWER_FLAME;
    if (IsKeyPressed(KEY_FOUR))  ui->selected_tool = TOOL_TOWER_TESLA;
    if (IsKeyPressed(KEY_FIVE))  ui->selected_tool = TOOL_UNIT_SOLDIER;
    if (IsKeyPressed(KEY_SIX))   ui->selected_tool = TOOL_UNIT_HEAVY;
    if (IsKeyPressed(KEY_SEVEN)) ui->selected_tool = TOOL_UNIT_MEDIC;
    if (IsKeyPressed(KEY_EIGHT)) ui->selected_tool = TOOL_UNIT_DOG;
    if (IsKeyPressed(KEY_ESCAPE)){ ui->selected_tool = TOOL_NONE; ui->selection.active = 0; }
}

// ════════════════════════════════════════════════════
// RENDU
// ════════════════════════════════════════════════════

static void draw_bar(int x, int y, int w, int h,
                     float ratio, Color fill, Color bg) {
    DrawRectangle(x, y, w, h, bg);
    DrawRectangle(x, y, (int)(w * ratio), h, fill);
    DrawRectangleLines(x, y, w, h, (Color){60,40,20,180});
}

static void draw_tool_btn(const Rectangle *r, ToolID id,
                           int is_selected, int is_hovered,
                           int can_afford)
{
    const ToolInfo *info = &TOOL_INFO[id];
    Color col = TOOL_COLORS[id];

    // Fond
    Color bg = is_selected ? (Color){40,25,8,255} :
               is_hovered  ? (Color){30,20,6,255} :
                             (Color){18,12,4,255};
    DrawRectangleRec(*r, bg);

    // Bordure
    Color border = is_selected ? col :
                   is_hovered  ? (Color){col.r/2,col.g/2,col.b/2,255} :
                                 (Color){50,35,15,255};
    float bw = is_selected ? 2.0f : 1.0f;
    DrawRectangleLinesEx(*r, bw, border);

    // Icône (grand caractère coloré)
    if (!can_afford) col = (Color){80,60,40,255};
    DrawText(info->icon,
             (int)(r->x + r->width/2  - 5),
             (int)(r->y + 6), 20, col);

    // Nom
    DrawText(info->shortname,
             (int)(r->x + r->width/2 - MeasureText(info->shortname,9)/2),
             (int)(r->y + 28), 9,
             can_afford ? (Color){160,140,110,255}
                        : (Color){80,60,40,255});

    // Coût
    Color cost_col = can_afford ? (Color){239,159,39,255}
                                : (Color){150,60,40,255};
    DrawText(TextFormat("%dor", info->cost),
             (int)(r->x + r->width/2 - MeasureText(TextFormat("%dor",info->cost),9)/2),
             (int)(r->y + 39), 9, cost_col);

    // Glow si sélectionné
    if (is_selected)
        DrawRectangleLinesEx(
            (Rectangle){r->x-2,r->y-2,r->width+4,r->height+4},
            1, (Color){col.r,col.g,col.b,80});
}

void ui_render(const UIState *ui, const GameState *gs) {
    int sw    = GetScreenWidth();
    int hud_y = MAP_H * TILE_SIZE;
    const Theme *th = theme_get(gs->map.theme);

    // ════════════════════════════════════════════════
    // FOND HUD
    // ════════════════════════════════════════════════
    DrawRectangle(0, hud_y, sw, UI_HUD_HEIGHT, (Color){10,6,2,255});
    DrawLine(0, hud_y, sw, hud_y, (Color){80,50,10,200});

    // Séparateurs de colonnes
    DrawLine(UI_PANEL_W, hud_y, UI_PANEL_W,
             hud_y+UI_HUD_HEIGHT, (Color){40,25,8,180});
    DrawLine(sw-UI_PANEL_W, hud_y, sw-UI_PANEL_W,
             hud_y+UI_HUD_HEIGHT, (Color){40,25,8,180});

    // ════════════════════════════════════════════════
    // PANNEAU GAUCHE — stats
    // ════════════════════════════════════════════════
    int px = 8, py = hud_y + 8;

    // OR
    DrawText("OR", px, py, 11, (Color){139,94,0,255});
    draw_bar(px+24, py+2, UI_PANEL_W-60, 9,
             fminf((float)gs->gold/300.0f,1.0f),
             (Color){239,159,39,255}, (Color){30,15,0,255});
    DrawText(TextFormat("%d", gs->gold),
             px+UI_PANEL_W-32, py, 11, (Color){239,159,39,255});
    py += 20;

    // VIES
    DrawText("VIE", px, py, 11, (Color){139,30,30,255});
    float lives_ratio = (float)gs->lives / (float)gs->bonuses.start_lives;
    Color lives_col = lives_ratio > 0.5f ? (Color){46,204,113,255}
                    : lives_ratio > 0.25f? (Color){243,156,18,255}
                                         : (Color){231,76,60,255};
    draw_bar(px+24, py+2, UI_PANEL_W-60, 9,
             fmaxf(lives_ratio,0.0f), lives_col, (Color){30,5,5,255});
    DrawText(TextFormat("%d", gs->lives),
             px+UI_PANEL_W-32, py, 11, lives_col);
    py += 20;

    // FERRAILLE
    DrawText("SCR", px, py, 11, (Color){50,100,50,255});
    draw_bar(px+24, py+2, UI_PANEL_W-60, 9,
             fminf((float)gs->meta.scrap/200.0f,1.0f),
             (Color){127,200,50,255}, (Color){5,20,5,255});
    DrawText(TextFormat("%d", gs->meta.scrap),
             px+UI_PANEL_W-32, py, 11, (Color){127,200,50,255});
    py += 20;

    // Vague + thème
    DrawText(TextFormat("VAGUE %d", gs->wave_manager.number),
             px, py, 11, (Color){180,160,110,255});
    py += 16;
    DrawText(th->name, px, py, 10, (Color){80,120,80,255});

    // ════════════════════════════════════════════════
    // PANNEAU CENTRAL — boutons outils
    // ════════════════════════════════════════════════

    // Label "TOURS"
    int lx = UI_PANEL_W + 8;
    DrawText("TOURS",  lx, hud_y+4,  9, (Color){100,70,30,255});
    DrawText("UNITES", lx, hud_y+4+UI_BTN_H+6, 9, (Color){50,100,50,255});

    // Boutons
    for (int i = 0; i < TOOL_COUNT; i++) {
        int can_afford = gs->gold >= TOOL_INFO[i].cost;
        draw_tool_btn(&ui->tool_btns[i], (ToolID)i,
                      ui->selected_tool == i,
                      ui->hovered_tool  == i,
                      can_afford);
    }

    // Tooltip au survol d'un outil
    if (ui->hovered_tool != -1) {
        const ToolInfo *info = &TOOL_INFO[ui->hovered_tool];
        const Rectangle *r  = &ui->tool_btns[ui->hovered_tool];
        int tx = (int)r->x;
        int ty = (int)r->y - 58;
        if (ty < 0) ty = (int)r->y + UI_BTN_H + 4;
        int tw2 = 160;
        DrawRectangle(tx, ty, tw2, 54, (Color){10,6,2,240});
        DrawRectangleLines(tx, ty, tw2, 54,
                           TOOL_COLORS[ui->hovered_tool]);
        DrawText(info->name, tx+6, ty+4,  11,
                 TOOL_COLORS[ui->hovered_tool]);
        DrawText(TextFormat("Dmg:%.0f  Port:%.1ft",
                     info->dmg, info->range),
                 tx+6, ty+18, 10, (Color){160,140,110,255});
        DrawText(TextFormat("Cad:%.1f/s  Cout:%dor",
                     info->rate, info->cost),
                 tx+6, ty+30, 10, (Color){160,140,110,255});
        DrawText(info->desc, tx+6, ty+42, 9,
                 (Color){100,80,50,255});
    }

    // Bouton LANCER VAGUE
    {
        const Rectangle *wb = &ui->wave_btn;
        int in_prep = (gs->phase == PHASE_PREP);
        float ratio = gs->wave_manager.prep_timer / 20.0f;

        // Fond
        Color wbg = in_prep ? (Color){8,30,12,255}
                            : (Color){8,8,8,255};
        DrawRectangleRec(*wb, wbg);

        // Barre timer
        Color tcol = ratio > 0.5f ? (Color){46,204,113,255}
                   : ratio > 0.2f ? (Color){243,156,18,255}
                                  : (Color){231,76,60,255};
        draw_bar((int)wb->x+4, (int)wb->y+(int)wb->height-12,
                 (int)wb->width-8, 8, ratio, tcol,
                 (Color){20,20,20,255});

        // Texte
        Color wlabel = in_prep ? (Color){46,204,113,255}
                               : (Color){80,80,80,255};
        if (in_prep) {
            int bonus = (int)(ratio * 30.0f);
            DrawText("▶ LANCER",
                     (int)(wb->x+8), (int)(wb->y+8), 13, wlabel);
            DrawText(TextFormat("+%dor bonus", bonus),
                     (int)(wb->x+8), (int)(wb->y+26), 10,
                     (Color){239,159,39,255});
            DrawText(TextFormat("%.0fs", gs->wave_manager.prep_timer),
                     (int)(wb->x+8), (int)(wb->y+40), 10,
                     (Color){100,80,50,255});
        } else {
            DrawText("EN COURS",
                     (int)(wb->x+8), (int)(wb->y+20), 11, wlabel);
        }
        DrawRectangleLinesEx(*wb, 1.0f,
            in_prep ? (Color){27,94,46,255}
                    : (Color){40,40,40,255});
    }

    // ════════════════════════════════════════════════
    // PANNEAU DROIT — info outil ou tour sélectionnée
    // ════════════════════════════════════════════════
    int rx = sw - UI_PANEL_W + 8;
    py = hud_y + 8;

    if (ui->selection.active) {
        // Tour sélectionnée sur la carte
        const Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
        if (tw->active) {
            const TowerStats *st = &TOWER_BASE_STATS[tw->type];
            Color col = TOWER_FILL[tw->type];

            DrawText(st->name, rx, py, 13, col); py += 18;
            DrawText(TextFormat("Dmg   : %.0f", tw->damage),
                     rx, py, 10, (Color){160,140,110,255}); py += 14;
            DrawText(TextFormat("Port  : %.1ft", tw->range),
                     rx, py, 10, (Color){160,140,110,255}); py += 14;
            DrawText(TextFormat("Cad   : %.1f/s", tw->fire_rate),
                     rx, py, 10, (Color){160,140,110,255}); py += 14;
            DrawText(TextFormat("Niveau: %d", tw->level),
                     rx, py, 10, (Color){239,159,39,255}); py += 18;

            // Bouton vendre
            DrawRectangleRec(ui->sell_btn, (Color){30,8,8,255});
            DrawRectangleLinesEx(ui->sell_btn,1,(Color){139,30,30,255});
            int refund = (int)(st->cost * 0.6f);
            DrawText(TextFormat("Vendre +%dor", refund),
                     (int)(ui->sell_btn.x+4),
                     (int)(ui->sell_btn.y+5), 10,
                     (Color){192,57,43,255});
        }
    } else if (ui->selected_tool != TOOL_NONE) {
        // Info de l'outil sélectionné
        const ToolInfo *info = &TOOL_INFO[ui->selected_tool];
        Color col = TOOL_COLORS[ui->selected_tool];

        DrawText(info->name, rx, py, 13, col); py += 18;
        DrawText(TextFormat("Dmg   : %.0f", info->dmg),
                 rx, py, 10, (Color){160,140,110,255}); py += 14;
        DrawText(TextFormat("Port  : %.1ft", info->range),
                 rx, py, 10, (Color){160,140,110,255}); py += 14;
        DrawText(TextFormat("Cad   : %.1f/s", info->rate),
                 rx, py, 10, (Color){160,140,110,255}); py += 14;
        DrawText(TextFormat("Cout  : %dor", info->cost),
                 rx, py, 10, (Color){239,159,39,255}); py += 18;
        DrawText(info->desc, rx, py, 9, (Color){100,80,50,255});
    } else {
        DrawText("Clique sur", rx, py,   10, (Color){60,50,35,255});
        DrawText("un outil",   rx, py+14, 10, (Color){60,50,35,255});
        DrawText("ou une tour", rx, py+28, 10, (Color){60,50,35,255});
        DrawText("posee.",      rx, py+42, 10, (Color){60,50,35,255});
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
            // Zone de déploiement unité
            float bpx = gs->units.base_px;
            float bpy = gs->units.base_py;
            DrawCircleLines((int)bpx, (int)bpy,
                            5.0f*TILE_SIZE,
                            (Color){39,174,96,100});
        }
    }
}