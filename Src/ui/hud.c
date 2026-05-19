/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "hud.h"
#include "renderer.h"
#include "ui_utils.h"
#include "../engine/audio.h"
#include "../engine/assets.h"
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
// DIMENSIONS DES OVERLAYS DÉPLAÇABLES
// ════════════════════════════════════════════════════
#define OVERLAY_W      164
#define OVERLAY_OV_P     6
#define OVERLAY_TL_H   (OVERLAY_OV_P + 17+3 + 14+3 + 14 + OVERLAY_OV_P)   // fs12→17, fs10→14 (FONT_SCALE 1.4)
#define OVERLAY_TR_H   (OVERLAY_OV_P + 17+3 + 14+3 + 14+3 + 9+5 + 14 + OVERLAY_OV_P)

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

static Vector2 virt_mouse(void) {
    Vector2 raw = GetMousePosition();
    return (Vector2){
        (raw.x - g_mouse_ox) / g_mouse_sx,
        (raw.y - g_mouse_oy) / g_mouse_sy,
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
// DÉBLOCAGE DES TOURS PAR PROGRESSION DE CAMPAGNE
// Seuil = index du dernier acte à compléter pour débloquer.
// -1 = toujours disponible. Arcade = tout débloqué.
// ════════════════════════════════════════════════════
// Ordre narratif : GUN (tjs) → SNIPER (acte 2) → TESLA (acte 3) → FLAME (acte 6)
static const int TOWER_UNLOCK_AT[TOWER_TYPE_COUNT] = {
    [TOWER_GUN]    = -1,  // toujours disponible
    [TOWER_SNIPER] =  1,  // après avoir complété l'acte index 1 (La route du nord)
    [TOWER_FLAME]  =  5,  // après avoir complété l'acte index 5 (La reine des marais)
    [TOWER_TESLA]  =  2,  // après avoir complété l'acte index 2 (Le convoi)
};

// Texte court identifiant l'acte de déblocage pour chaque tour
static const char *TOWER_UNLOCK_ACT_NAME[TOWER_TYPE_COUNT] = {
    [TOWER_GUN]    = NULL,
    [TOWER_SNIPER] = "Ch.1 — Acte 2",
    [TOWER_FLAME]  = "Ch.2 — Acte 3",
    [TOWER_TESLA]  = "Ch.1 — Acte 3",
};

static int tool_is_unlocked(ToolID id, const GameState *gs) {
    if (!gs->is_campaign) return 1;            // arcade : tout disponible
    if (!ui_tool_is_tower(id)) return 1;       // unités : toujours dispo
    TowerType tt = ui_tool_to_tower(id);
    int threshold = TOWER_UNLOCK_AT[tt];
    if (threshold < 0) return 1;
    return meta_max_stage_completed(&gs->meta) >= threshold;
}

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

// ── Portrait splash art ───────────────────────────────────────────
// Dessine une texture dans un carré psz×psz avec :
//   • fit proportionnel centré (pas de déformation)
//   • fond sombre + bordure colorée
// Retourne 1 si une image a bien été dessinée, 0 sinon.
static int draw_portrait(Texture2D tex, int px, int py, int psz, Color border) {
    if (tex.id == 0) return 0;

    // Fond sombre
    DrawRectangle(px, py, psz, psz, (Color){6, 3, 1, 220});
    DrawRectangleLinesEx((Rectangle){(float)px,(float)py,(float)psz,(float)psz},
                         1.5f, border);

    // Image redimensionnée proportionnellement
    float scale = fminf((float)psz / (float)tex.width,
                        (float)psz / (float)tex.height);
    int dw = (int)((float)tex.width  * scale);
    int dh = (int)((float)tex.height * scale);
    int ox = px + (psz - dw) / 2;
    int oy = py + (psz - dh) / 2;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){(float)ox, (float)oy, (float)dw, (float)dh},
        (Vector2){0, 0}, 0.0f, WHITE);
    return 1;
}

static void draw_tool_btn(const Rectangle *r, ToolID id,
                           int is_selected, int is_hovered,
                           int can_afford, int is_locked)
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

    // Splash art si disponible, icône texte sinon
    /* ── Bouton verrouillé : overlay sombre + cadenas ─────────── */
    if (is_locked) {
        DrawRectangleRounded(*r, rnd, 6, (Color){6, 4, 2, 240});
        DrawRectangleRoundedLinesEx(*r, rnd, 6, 1.0f, (Color){40, 28, 10, 160});
        // Symbole cadenas centré
        int cx2 = (int)(r->x + r->width  / 2);
        int cy2 = (int)(r->y + r->height / 2);
        DrawRectangle(cx2 - 7, cy2 - 2, 14, 10, (Color){55, 40, 15, 220});
        DrawRectangleLines(cx2 - 7, cy2 - 2, 14, 10, (Color){90, 65, 20, 255});
        DrawCircleLines(cx2, cy2 - 6, 6, (Color){90, 65, 20, 255});
        // Nom grisé en bas
        int nw2 = mtxt(info->shortname, 9);
        dtxt(info->shortname,
                 cx2 - nw2/2,
                 (int)(r->y + r->height - 13), 9,
                 (Color){55, 42, 22, 255});
        return;
    }

    Texture2D splash = {0};
    if (ui_tool_is_tower(id))
        splash = g_tower_splash[ui_tool_to_tower(id)];
    else if (ui_tool_is_unit(id))
        splash = g_unit_splash[ui_tool_to_unit(id)];

    int name_fs = 9;
    int name_y, cost_y;

    if (splash.id != 0) {
        // Image mise à l'échelle pour remplir le bouton (marge 2px)
        int pad = 2;
        float scale = fminf((float)((int)r->width  - pad*2) / (float)splash.width,
                            (float)((int)r->height - pad*2) / (float)splash.height);
        int dw = (int)(splash.width  * scale);
        int dh = (int)(splash.height * scale);
        int ox = (int)r->x + ((int)r->width  - dw) / 2;
        int oy = (int)r->y + ((int)r->height - dh) / 2;
        Color tint = can_afford ? WHITE : (Color){120, 100, 80, 200};
        DrawTexturePro(splash,
            (Rectangle){0, 0, (float)splash.width, (float)splash.height},
            (Rectangle){(float)ox, (float)oy, (float)dw, (float)dh},
            (Vector2){0, 0}, 0.0f, tint);

        // Bande sombre en bas pour lisibilité du texte (hauteur dynamique)
        int band_h = (int)(r->height * 0.38f);
        if (band_h < 20) band_h = 20;
        DrawRectangle((int)r->x, (int)(r->y + r->height) - band_h,
                      (int)r->width, band_h, (Color){0, 0, 0, 175});

        name_y = (int)(r->y + r->height) - band_h + 2;
        cost_y = (int)(r->y + r->height) - band_h/2 + 1;
    } else {
        // Fallback : icône texte
        int icon_fs = 20;
        int iw = mtxt(info->icon, icon_fs);
        dtxt(info->icon,
                 (int)(r->x + r->width/2 - iw/2),
                 (int)(r->y + 5), icon_fs, col);
        name_y = (int)(r->y + 29);
        cost_y = (int)(r->y + 41);
    }

    // Nom abrégé
    int nw = mtxt(info->shortname, name_fs);
    dtxt(info->shortname,
             (int)(r->x + r->width/2 - nw/2),
             name_y, name_fs,
             can_afford ? (Color){200, 185, 160, 255}
                        : (Color){75, 58, 38, 255});

    // Coût
    char cost_buf[20];
    snprintf(cost_buf, sizeof(cost_buf), "%dor", info->cost);
    int cw = mtxt(cost_buf, name_fs);
    dtxt(cost_buf,
             (int)(r->x + r->width/2 - cw/2),
             cost_y, name_fs,
             can_afford ? (Color){230, 150, 32, 255}
                        : (Color){130, 55, 35, 255});

    if (is_selected)
        DrawRectangleRoundedLinesEx(
            (Rectangle){r->x-2, r->y-2, r->width+4, r->height+4},
            rnd, 6, 1.0f,
            (Color){col.r, col.g, col.b, 60});
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

// ════════════════════════════════════════════════════
// MISE À JOUR
// ════════════════════════════════════════════════════
void ui_update(UIState *ui, GameState *gs) {
    Vector2   mouse = virt_mouse();
    const int HUD_Y = MAP_H * TILE_SIZE;

    // Nettoyage unité morte : désélectionner si l'unité n'est plus active
    if (ui->sell_unit_idx >= 0) {
        const Unit *su = &gs->units.units[ui->sell_unit_idx];
        if (!su->active) {
            ui->sell_unit_idx       = -1;
            ui->worker_selected_idx = -1;
            gs->units.selected_unit = -1;
        }
    }
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

    // Hover tuile (tient compte du décalage horizontal de la carte)
    {
        int map_left  = g_map_x_off;
        int map_right = g_map_x_off + MAP_W * TILE_SIZE;
        if (mouse.y >= 0 && mouse.y < HUD_Y &&
            mouse.x >= map_left && mouse.x < map_right) {
            ui->hovered_tile_x = (int)((mouse.x - map_left) / TILE_SIZE);
            ui->hovered_tile_y = (int)(mouse.y / TILE_SIZE);
        } else {
            ui->hovered_tile_x = -1;
            ui->hovered_tile_y = -1;
        }
    }

    // Repositionnement dynamique des boutons du panneau droit
    {
        const int M       = UI_MARGIN;
        const int right_x = g_canvas_virt_w - UI_PANEL_W + M;
        const int right_w = UI_PANEL_W - M * 2;
        const int btn_h   = 34;
        ui->sell_btn = (Rectangle){
            right_x,
            HUD_Y + UI_HUD_HEIGHT - M - btn_h * 2 - 6,
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
        ui->pause_btn = (Rectangle){
            (float)(UI_LEFT_PANEL_W + M + 6 + 5*(UI_BTN_W+6) + M + 108 + M),
            (float)(HUD_Y + 18),
            36.0f,
            (float)(UI_BTN_H * 2 + 6 + 14)
        };
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

    // ── Overlays déplaçables ──────────────────────────────────
    {
        // Initialisation des positions par défaut au 1er frame
        if (ui->overlay_tl_pos.x < 0.0f) {
            ui->overlay_tl_pos = (Vector2){
                (float)(g_map_x_off + 8),
                8.0f
            };
            ui->overlay_tr_pos = (Vector2){
                (float)(g_map_x_off + MAP_W * TILE_SIZE - 8 - OVERLAY_W),
                8.0f
            };
        }

        // Mise à jour de la position pendant le drag
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
            ui->dragging_overlay = -1;

        if (ui->dragging_overlay >= 0) {
            Vector2 *pos = (ui->dragging_overlay == 0)
                         ? &ui->overlay_tl_pos : &ui->overlay_tr_pos;
            float oh = (ui->dragging_overlay == 0) ? OVERLAY_TL_H : OVERLAY_TR_H;
            pos->x = mouse.x - ui->drag_grab.x;
            pos->y = mouse.y - ui->drag_grab.y;
            // Clamp dans le canvas
            int canvas_h = MAP_H * TILE_SIZE + UI_HUD_HEIGHT;
            if (pos->x < 0) pos->x = 0;
            if (pos->y < 0) pos->y = 0;
            if (pos->x + OVERLAY_W > g_canvas_virt_w) pos->x = g_canvas_virt_w - OVERLAY_W;
            if (pos->y + oh > canvas_h)                pos->y = canvas_h - oh;
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Démarrage du drag sur un overlay (priorité sur tout autre clic)
        {
            Rectangle tl_r = {ui->overlay_tl_pos.x, ui->overlay_tl_pos.y,
                               OVERLAY_W, OVERLAY_TL_H};
            Rectangle tr_r = {ui->overlay_tr_pos.x, ui->overlay_tr_pos.y,
                               OVERLAY_W, OVERLAY_TR_H};
            if (CheckCollisionPointRec(mouse, tl_r)) {
                ui->dragging_overlay = 0;
                ui->drag_grab = (Vector2){mouse.x - tl_r.x, mouse.y - tl_r.y};
                goto end_click; // ne pas propager le clic
            }
            if (CheckCollisionPointRec(mouse, tr_r)) {
                ui->dragging_overlay = 1;
                ui->drag_grab = (Vector2){mouse.x - tr_r.x, mouse.y - tr_r.y};
                goto end_click;
            }
        }

        if (ui->hovered_tool != -1) {
            if (!tool_is_unlocked((ToolID)ui->hovered_tool, gs)) {
                /* Tour verrouillée : on ignore le clic */
            } else {
                ui->selected_tool       = (ToolID)ui->hovered_tool;
                ui->selection.active    = 0;
                ui->worker_selected_idx = -1;
                gs->units.selected_unit = -1;
            }
        }
        else if (CheckCollisionPointRec(mouse, ui->wave_btn)) {
            if (gs->phase == PHASE_PREP) {
                int bonus = (int)(gs->wave_manager.prep_timer / 20.0f * 15.0f);
                gs->gold += bonus;
                wave_start(&gs->wave_manager);
                gs->phase = PHASE_WAVE;
                if (bonus > 0) {
                    char nbuf[44];
                    snprintf(nbuf, sizeof(nbuf), "+%d or (lancement rapide)", bonus);
                    ui_push_notif(ui, nbuf, (Color){230, 155, 35, 255});
                }
            }
        }
        else if (ui->selection.active &&
                 CheckCollisionPointRec(mouse, ui->sell_btn)) {
            Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
            if (tw->active) {
                int real_cost = tower_cost_on_tile(tw->type, &gs->map,
                                                   tw->tile_x, tw->tile_y);
                int refund = (int)(real_cost * 0.6f);
                gs->gold += refund;
                gs->map.tiles[tw->tile_y][tw->tile_x].buildable = 1;
                tw->active = 0;
                gs->towers.tower_count--;
                char nbuf[32];
                snprintf(nbuf, sizeof(nbuf), "+%d or recuperes", refund);
                ui_push_notif(ui, nbuf, (Color){230, 155, 35, 255});
            }
            ui->selection.active = 0;
        }
        else if (ui->sell_unit_idx >= 0 &&
                 CheckCollisionPointRec(mouse, ui->unit_sell_btn)) {
            Unit *u = &gs->units.units[ui->sell_unit_idx];
            if (u->active) {
                int base_cost = UNIT_BASE_STATS[u->type].cost;
                int refund    = (int)(base_cost * 0.5f);
                gs->gold += refund;
                u->active = 0;
                gs->units.count--;
                char nbuf[36];
                snprintf(nbuf, sizeof(nbuf), "+%d or (unite renvoyee)", refund);
                ui_push_notif(ui, nbuf, (Color){230, 155, 35, 255});
            }
            ui->sell_unit_idx       = -1;
            ui->worker_selected_idx = -1;
            gs->units.selected_unit = -1;
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
                    audio_play_sfx(AUDIO_SFX_MATERIAL_APPLY);
                    ui_push_notif(ui, "Materiau applique !",
                                  (Color){62, 175, 200, 255});
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
                            audio_play_sfx(AUDIO_SFX_TOWER_PLACE_GUN +
                                           (int)ui_tool_to_tower(ui->selected_tool));
                        }
                    } else {
                        ui_push_notif(ui, "Limite de tours atteinte !",
                                      (Color){231, 76, 60, 255});
                    }
                } else if (ui_tool_is_unit(ui->selected_tool)) {
                    if (gs->units.count >= gs->units.unit_limit) {
                        ui_push_notif(ui, "Limite d'unites atteinte !",
                                      (Color){231, 76, 60, 255});
                    } else {
                        // Cherche la base active la plus proche du clic dans le rayon
                        int   near_any_base = 0;
                        float spawn_bpx = gs->units.base_px;
                        float spawn_bpy = gs->units.base_py;
                        float best_dist = 5.0f * TILE_SIZE + 1.0f; // seuil + 1 pour init
                        for (int b = 0; b < gs->map.base_count; b++) {
                            if (!gs->map.bases[b].active) continue;
                            float bpx = gs->map.bases[b].pos.x * TILE_SIZE
                                        + TILE_SIZE / 2.0f;
                            float bpy = gs->map.bases[b].pos.y * TILE_SIZE
                                        + TILE_SIZE / 2.0f;
                            float dx = (mouse.x - g_map_x_off) - bpx;
                            float dy = mouse.y - bpy;
                            float dist = sqrtf(dx*dx + dy*dy);
                            if (dist <= 5.0f * TILE_SIZE && dist < best_dist) {
                                best_dist     = dist;
                                spawn_bpx     = bpx;
                                spawn_bpy     = bpy;
                                near_any_base = 1;
                            }
                        }
                        if (near_any_base) {
                            if (!unit_spawn_at(&gs->units,
                                               ui_tool_to_unit(ui->selected_tool),
                                               &gs->gold, &gs->bonuses,
                                               spawn_bpx, spawn_bpy)) {
                                ui_push_notif(ui, "Or insuffisant !",
                                              (Color){243, 156, 18, 255});
                            }
                        } else {
                            ui_push_notif(ui, "Spawn pres d'une base !",
                                          (Color){243, 156, 18, 255});
                        }
                    }
                }
            } else {
                // Unité cliquée ?
                ui->sell_unit_idx       = -1;
                ui->worker_selected_idx = -1;
                gs->units.selected_unit = -1;
                for (int j = 0; j < MAX_UNITS; j++) {
                    Unit *u = &gs->units.units[j];
                    if (!u->active) continue;
                    float dx = (mouse.x - g_map_x_off) - u->x;
                    float dy = mouse.y - u->y;
                    if (sqrtf(dx*dx + dy*dy) <= u->size + 6.0f) {
                        gs->units.selected_unit  = j;
                        ui->sell_unit_idx        = j;
                        ui->selection.active     = 0;
                        if (u->type == UNIT_WORKER)
                            ui->worker_selected_idx = j;
                        break;
                    }
                }
            }
        }
        end_click:;
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        ui->selected_tool       = TOOL_NONE;
        ui->selection.active    = 0;
        ui->worker_selected_idx = -1;
        ui->sell_unit_idx       = -1;
        gs->units.selected_unit = -1;
    }

    if (IsKeyPressed(KEY_ONE))   ui->selected_tool = TOOL_TOWER_GUN;
    if (IsKeyPressed(KEY_TWO)   && tool_is_unlocked(TOOL_TOWER_SNIPER, gs))
        ui->selected_tool = TOOL_TOWER_SNIPER;
    if (IsKeyPressed(KEY_THREE) && tool_is_unlocked(TOOL_TOWER_FLAME,  gs))
        ui->selected_tool = TOOL_TOWER_FLAME;
    if (IsKeyPressed(KEY_FOUR)  && tool_is_unlocked(TOOL_TOWER_TESLA,  gs))
        ui->selected_tool = TOOL_TOWER_TESLA;
    if (IsKeyPressed(KEY_FIVE))  ui->selected_tool = TOOL_UNIT_SOLDIER;
    if (IsKeyPressed(KEY_SIX))   ui->selected_tool = TOOL_UNIT_HEAVY;
    if (IsKeyPressed(KEY_SEVEN)) ui->selected_tool = TOOL_UNIT_MEDIC;
    if (IsKeyPressed(KEY_EIGHT)) ui->selected_tool = TOOL_UNIT_DOG;
    if (IsKeyPressed(KEY_NINE))  ui->selected_tool = TOOL_UNIT_WORKER;
    if (IsKeyPressed(KEY_X))     ui->speed_mult = (ui->speed_mult % 3) + 1;
    if (IsKeyPressed(KEY_F))     ui->show_fps  ^= 1;

    // Tick notifications flottantes
    {
        float dt = GetFrameTime();
        for (int i = 0; i < ui->notif_count; ) {
            FloatNotif *n = &ui->notifs[i];
            n->timer -= dt;
            n->y_off += 32.0f * dt;
            if (n->timer <= 0.0f) {
                for (int j = i; j < ui->notif_count - 1; j++)
                    ui->notifs[j] = ui->notifs[j + 1];
                ui->notif_count--;
            } else {
                i++;
            }
        }
    }

    /* ESCAPE : vide la sélection (la pause est gérée séparément dans game_do_input) */
    if (IsKeyPressed(KEY_ESCAPE)) {
        ui->selected_tool       = TOOL_NONE;
        ui->selection.active    = 0;
        ui->worker_selected_idx = -1;
        ui->sell_unit_idx       = -1;
        gs->units.selected_unit = -1;
    }
}

// ════════════════════════════════════════════════════
// RENDU
// ════════════════════════════════════════════════════
void ui_render(const UIState *ui, const GameState *gs) {
    const int VIRT_W = g_canvas_virt_w;
    const int HUD_Y  = MAP_H * TILE_SIZE;
    const int HUD_H  = UI_HUD_HEIGHT;
    const int M      = UI_MARGIN;
    const int GAP    = 6;

    const Theme *th = theme_get(gs->map.theme);

    // ── Fond HUD ──────────────────────────────────────────────
    DrawRectangle(0, HUD_Y, VIRT_W, HUD_H, (Color){10, 6, 2, 255});
    DrawLine(0, HUD_Y, VIRT_W, HUD_Y, (Color){70, 44, 0, 200});

    DrawRectangle(0,                   HUD_Y, UI_LEFT_PANEL_W, HUD_H, (Color){15, 8, 3, 235});
    DrawRectangle(VIRT_W - UI_PANEL_W, HUD_Y, UI_PANEL_W,      HUD_H, (Color){15, 8, 3, 235});
    DrawLine(UI_LEFT_PANEL_W,     HUD_Y, UI_LEFT_PANEL_W,
             HUD_Y + HUD_H, (Color){50, 32, 8, 160});
    DrawLine(VIRT_W - UI_PANEL_W, HUD_Y, VIRT_W - UI_PANEL_W,
             HUD_Y + HUD_H, (Color){50, 32, 8, 160});

    // ════════════════════════════════════════════════
    // PANNEAU GAUCHE — vies, ferraille, thème, inventaire
    // ════════════════════════════════════════════════
    {
        const int px    = M + 2;
        const int bar_x = px + 32;
        const int bar_w = UI_LEFT_PANEL_W - 32 - 30 - M;
        const int val_x = UI_LEFT_PANEL_W - 30;
        int py = HUD_Y + M;

        // BASES — une barre HP par base, pulsante si critique
        {
            float t = (float)GetTime();
            float pulse = (sinf(t * 6.0f) + 1.0f) * 0.5f;

            for (int b = 0; b < gs->map.base_count; b++) {
                const BaseInfo *base = &gs->map.bases[b];
                float ratio = (base->max_hp > 0)
                    ? (float)base->hp / (float)base->max_hp : 0.0f;

                // Code couleur explicite : vert > jaune > rouge selon HP restant
                Color bc;
                if (!base->active || base->hp <= 0) {
                    bc = (Color){100, 35, 35, 255};
                } else if (ratio > 0.60f) {
                    bc = (Color){46, 204, 113, 255};   // vert = sain
                } else if (ratio > 0.30f) {
                    bc = (Color){243, 156,  18, 255};  // orange = endommagé
                } else {
                    bc = (Color){231,  76,  60, 255};  // rouge = critique
                }

                // Pulsation si critique
                if (ratio > 0.0f && ratio <= 0.30f && base->active) {
                    unsigned char r = (unsigned char)(140 + (int)(91.0f * pulse));
                    unsigned char g = (unsigned char)(20  + (int)(56.0f * pulse));
                    bc = (Color){r, g, 40, 255};
                }

                // Étiquette base + HP fraction explicites
                const char *bname = base->is_primary ? "BASE PRINC." : TextFormat("BASE SEC.%d", b + 1);
                char hp_str[16];
                snprintf(hp_str, sizeof(hp_str), "%d/%d",
                         base->hp > 0 ? base->hp : 0, base->max_hp);

                int bw2 = UI_LEFT_PANEL_W - M * 2;

                // Fond de ligne coloré pour renforcer le code couleur
                Color bg_line = {bc.r/5, bc.g/5, bc.b/5, 80};
                DrawRectangle(px - 2, py - 1, bw2 + 4, 9, bg_line);

                dtxt(bname,  px,    py, 8, bc);

                // HP fraction alignée à droite
                int hw = mtxt(hp_str, 8);
                dtxt(hp_str, px + bw2 - hw, py, 8, bc);

                draw_bar(px, py + 10, bw2, 6, fmaxf(ratio, 0.0f), bc,
                         (Color){18, 10, 4, 200});

                if (!base->active || base->hp <= 0)
                    dtxt("DETRUITE", px + bw2/2 - 22, py + 10, 7,
                             (Color){180, 60, 60, 255});
                py += 22;
            }
        }

        // FERRAILLE — uniquement en campagne
        if (gs->is_campaign) {
            float sr = fminf((float)gs->meta.scrap / 200.0f, 1.0f);
            dtxt("SCR", px, py + 1, 10, (Color){30, 65, 30, 255});
            draw_bar(bar_x, py + 2, bar_w, 7, sr,
                     (Color){127, 200, 50, 255}, (Color){4, 16, 4, 200});
            dtxt(TextFormat("%d", gs->meta.scrap),
                     val_x, py + 1, 10, (Color){127, 200, 50, 255});
            py += 16;
        }

        draw_sep(px, py, UI_LEFT_PANEL_W - M * 2, (Color){40, 25, 6, 130});
        py += 5;

        // Thème
        {
            char tbuf[26];
            clip_text(th->name, UI_LEFT_PANEL_W - M * 2, 9, tbuf, sizeof(tbuf));
            dtxt(tbuf, px, py, 9, (Color){48, 82, 48, 255});
            py += 12;
        }

        // Inventaire
        if (gs->inventory_count > 0) {
            dtxt(TextFormat("Mat: %d", gs->inventory_count),
                     px, py, 9, (Color){62, 165, 185, 255});
        }
    }

    // ════════════════════════════════════════════════
    // PANNEAU CENTRAL — boutons
    // ════════════════════════════════════════════════
    {
        int lx    = UI_LEFT_PANEL_W + M + 6;
        int row1y = (int)ui->tool_btns[TOOL_TOWER_GUN].y;
        int row2y = (int)ui->tool_btns[TOOL_UNIT_SOLDIER].y;

        dtxt("TOURS",  lx, row1y - 12, 9, (Color){100, 70, 22, 255});
        dtxt("UNITES", lx, row2y - 12, 9, (Color){38, 100, 38, 255});

        for (int i = 0; i < TOOL_COUNT; i++) {
            int locked     = !tool_is_unlocked((ToolID)i, gs);
            int can_afford = !locked && gs->gold >= TOOL_INFO[i].cost;

            // Griser si limite atteinte
            if (!locked && ui_tool_is_tower((ToolID)i) &&
                gs->towers.tower_count >= gs->towers.tower_limit)
                can_afford = 0;
            if (!locked && ui_tool_is_unit((ToolID)i) &&
                gs->units.count >= gs->units.unit_limit)
                can_afford = 0;

            draw_tool_btn(&ui->tool_btns[i], (ToolID)i,
                          ui->selected_tool == i,
                          ui->hovered_tool  == i,
                          can_afford, locked);
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
            dtxt(l1, wx - mtxt(l1,13)/2, (int)wb->y + M, 13, wlbl);

            char b2[14];
            snprintf(b2, sizeof(b2), "+%dor", (int)(ratio*15.0f));
            dtxt(b2, wx - mtxt(b2,10)/2, (int)wb->y+27, 10,
                     (Color){225,145,28,255});

            char b3[10];
            snprintf(b3, sizeof(b3), "%.0fs", gs->wave_manager.prep_timer);
            dtxt(b3, wx - mtxt(b3,10)/2, (int)wb->y+40, 10,
                     (Color){82,65,40,255});
        } else {
            const char *l1 = "EN COURS";
            dtxt(l1, wx - mtxt(l1,11)/2,
                     (int)(wb->y + wb->height/2 - 8), 11, wlbl);
        }
    }

    // ════════════════════════════════════════════════
    // BOUTON PAUSE
    // ════════════════════════════════════════════════
    {
        const Rectangle *pb  = &ui->pause_btn;
        float prnd = (float)UI_RADIUS / pb->height;
        int phov = CheckCollisionPointRec(virt_mouse(), *pb);
        DrawRectangleRounded(*pb, prnd, 6,
            phov ? (Color){28, 22, 8, 255} : (Color){14, 10, 3, 255});
        DrawRectangleRoundedLinesEx(*pb, prnd, 6, 1.5f,
            phov ? (Color){140, 105, 40, 255} : (Color){60, 44, 14, 180});
        // Deux barres verticales (icône pause)
        int pcx = (int)(pb->x + pb->width  / 2);
        int pcy = (int)(pb->y + pb->height / 2);
        DrawRectangle(pcx - 7, pcy - 9, 5, 18, (Color){160, 120, 45, 255});
        DrawRectangle(pcx + 2, pcy - 9, 5, 18, (Color){160, 120, 45, 255});
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

            // Portrait 82×82
            const int PSZ = 82;
            int pdone = draw_portrait(g_tower_splash[tw->type],
                                      rx + max_w - PSZ, HUD_Y + M, PSZ, col);
            int text_w = pdone ? max_w - PSZ - 6 : max_w;

            // Nom (fs=16)
            clip_text(st->name, text_w, 16, buf, sizeof(buf));
            dtxt(buf, rx, py, 16, col);
            py += 19;
            DrawLine(rx, py, rx + max_w, py, (Color){44, 28, 8, 140});
            py += 7;

            // Stats — 2 colonnes (fs=13)
            Color sc   = (Color){148,128,95,255};
            Color nlc  = (Color){212,138,25,255};
            Color typc = (Color){82,155,200,255};
            Color matc = (Color){62,172,192,255};
            int   cx2  = rx + max_w / 2;

            dtxt(TextFormat("Dmg   %.0f",   tw->damage),
                     rx,  py, 13, sc);
            dtxt(TextFormat("Port  %.1ft",  tw->range),
                     cx2, py, 13, sc);   py += 17;

            dtxt(TextFormat("Cad   %.1f/s", tw->fire_rate),
                     rx,  py, 13, sc);
            dtxt(TextFormat("Niv   %d",     tw->level),
                     cx2, py, 13, nlc);  py += 17;

            dtxt(TextFormat("Type  %s", DAMAGE_NAMES[tw->dmg_type]),
                     rx, py, 13, typc); py += 17;

            if (tw->material != MAT_NONE) {
                clip_text(TextFormat("Mat: %s", MATERIAL_NAMES[tw->material]),
                          max_w, 13, buf, sizeof(buf));
                dtxt(buf, rx, py, 13, matc);
                py += 16;
            }

            py += 3;
            DrawLine(rx, py, rx + max_w, py, (Color){40, 25, 6, 100});

            // Bouton VENDRE
            {
                Rectangle sb   = ui->sell_btn;
                float     srnd = (float)UI_RADIUS / sb.height;
                Vector2   m    = virt_mouse();
                int hov = CheckCollisionPointRec(m, sb);
                DrawRectangleRounded(sb, srnd, 6,
                    hov ? (Color){48,8,8,255} : (Color){20,4,4,255});
                DrawRectangleRoundedLinesEx(sb, srnd, 6, 1.5f,
                    hov ? (Color){192,48,48,255} : (Color){90,18,18,255});
                int refund = (int)(tower_cost_on_tile(tw->type, &gs->map,
                                       tw->tile_x, tw->tile_y) * 0.6f);
                snprintf(buf, sizeof(buf), "Vendre  +%d or", refund);
                int bw = mtxt(buf, 13);
                dtxt(buf, (int)(sb.x + sb.width/2 - bw/2),
                         (int)(sb.y + sb.height/2 - 7),
                         13, (Color){192,58,42,255});
            }

            // Bouton APPLIQUER MATÉRIAU
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
                clip_text(TextFormat("+ Appliquer  %s", MATERIAL_NAMES[mat]),
                          max_w - M, 13, buf, sizeof(buf));
                int bw = mtxt(buf, 13);
                dtxt(buf, (int)(ab.x + ab.width/2 - bw/2),
                         (int)(ab.y + ab.height/2 - 7),
                         13, (Color){62,175,200,255});
            }

        } else if (ui->sell_unit_idx >= 0) {
            const Unit *u = &gs->units.units[ui->sell_unit_idx];
            if (!u->active) goto panel_right_empty;

            // Nom et couleur selon le type
            static const char *UNAMES[UNIT_TYPE_COUNT] = {
                "SOLDAT", "LOURD", "MEDIC", "CHIEN", "OUVRIER"
            };
            static const Color UCOLS[UNIT_TYPE_COUNT] = {
                {39,174,96,255},{41,128,185,255},{231,76,60,255},
                {243,156,18,255},{200,200,50,255}
            };
            Color ucol = (u->type < UNIT_TYPE_COUNT)
                       ? UCOLS[u->type] : (Color){148,128,95,255};
            const char *uname = (u->type < UNIT_TYPE_COUNT)
                              ? UNAMES[u->type] : "UNITE";

            dtxt(uname, rx, py, 16, ucol); py += 19;
            DrawLine(rx, py, rx+max_w, py, (Color){44,44,8,140}); py += 7;

            // HP bar
            float hr = u->max_hp > 0.0f ? u->hp / u->max_hp : 0.0f;
            Color hc = hr > 0.6f ? (Color){46,204,113,255}
                     : hr > 0.3f ? (Color){243,156,18,255}
                                 : (Color){231,76,60,255};
            snprintf(buf, sizeof(buf), "HP   %.0f / %.0f", u->hp, u->max_hp);
            dtxt(buf, rx, py, 13, hc); py += 14;
            draw_bar(rx, py, max_w, 6, fmaxf(hr, 0.0f), hc, (Color){16,16,16,200});
            py += 13;

            // État
            const char *ss;
            switch (u->state) {
                case USTATE_GOTO_DEPOSIT: ss = "-> Depot";    break;
                case USTATE_COLLECT:      ss = "Collecte..."; break;
                case USTATE_GOTO_BASE:    ss = "<- Base";     break;
                default:                  ss = "Patrouille";  break;
            }
            dtxt(ss, rx, py, 13, (Color){148,128,95,255}); py += 17;

            if (u->has_material && u->carried_mat != MAT_NONE) {
                dtxt(TextFormat("Porte  %s", MATERIAL_NAMES[u->carried_mat]),
                         rx, py, 13, (Color){62,175,200,255}); py += 17;
            }

            if (u->type == UNIT_WORKER)
                dtxt("Clic depot = mission", rx, py, 11, (Color){92,92,35,175});

            // Bouton RENVOYER
            {
                Rectangle ub   = ui->unit_sell_btn;
                float     urnd = (float)UI_RADIUS / ub.height;
                Vector2   m    = virt_mouse();
                int hov = CheckCollisionPointRec(m, ub);
                DrawRectangleRounded(ub, urnd, 6,
                    hov ? (Color){30,10,4,255} : (Color){14,4,2,255});
                DrawRectangleRoundedLinesEx(ub, urnd, 6, 1.5f,
                    hov ? (Color){192,80,48,255} : (Color){80,30,16,255});
                int refund = (int)(UNIT_BASE_STATS[u->type].cost * 0.5f);
                snprintf(buf, sizeof(buf), "Renvoyer  +%d or", refund);
                int bw = mtxt(buf, 13);
                dtxt(buf, (int)(ub.x + ub.width/2 - bw/2),
                         (int)(ub.y + ub.height/2 - 7),
                         13, (Color){192,90,58,255});
            }

        } else if (ui->selected_tool != TOOL_NONE) {
            const ToolInfo *info = &TOOL_INFO[ui->selected_tool];
            Color col = TOOL_COLORS[ui->selected_tool];

            // Portrait 82×82
            const int PSZ = 82;
            Texture2D ptex = ui_tool_is_tower(ui->selected_tool)
                ? g_tower_splash[ui_tool_to_tower(ui->selected_tool)]
                : g_unit_splash [ui_tool_to_unit (ui->selected_tool)];
            int pdone = draw_portrait(ptex, rx + max_w - PSZ, HUD_Y + M, PSZ, col);
            int text_w = pdone ? max_w - PSZ - 6 : max_w;

            clip_text(info->name, text_w, 16, buf, sizeof(buf));
            dtxt(buf, rx, py, 16, col);
            py += 19;
            DrawLine(rx, py, rx+max_w, py, (Color){44,28,8,140});
            py += 7;

            // Stats 2 colonnes (fs=13)
            Color sc2 = (Color){148,128,95,255};
            int   cx2 = rx + max_w / 2;

            dtxt(TextFormat("Dmg   %.0f",  info->dmg),
                     rx,  py, 13, sc2);
            dtxt(TextFormat("Port  %.1ft", info->range),
                     cx2, py, 13, sc2);  py += 17;
            dtxt(TextFormat("Cad   %.1f/s",info->rate),
                     rx,  py, 13, sc2);  py += 17;

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
                    dtxt(TextFormat("Cout  %d or (x2)", real_cost),
                             rx, py, 13, (Color){205,108,22,255}); py += 17;
                } else {
                    dtxt(TextFormat("Cout  %d or", real_cost),
                             rx, py, 13, (Color){212,138,25,255}); py += 17;
                }
            } else {
                dtxt(TextFormat("Cout  %d or", info->cost),
                         rx, py, 13, (Color){212,138,25,255}); py += 17;
            }
            py += 3;
            clip_text(info->desc, max_w, 12, buf, sizeof(buf));
            dtxt(buf, rx, py, 12, (Color){82,65,40,255});

        } else {
            panel_right_empty:
            dtxt("Clic sur",    rx, py, 13, (Color){50,40,25,255}); py += 17;
            dtxt("un outil",    rx, py, 13, (Color){50,40,25,255}); py += 17;
            dtxt("ou une tour", rx, py, 13, (Color){50,40,25,255}); py += 17;
            dtxt("posee.",      rx, py, 13, (Color){50,40,25,255});
        }
    }

    // ════════════════════════════════════════════════
    // OVERLAYS CARTE
    // ════════════════════════════════════════════════
    {
        const int OV_M = 8;   // marge depuis le bord de la carte
        const int OV_P = 6;   // padding interne des panneaux

        // ── Haut-gauche : OR / Tours / Unités ─────────────────
        {
            const int ow = OVERLAY_W, line1_h = 17, line2_h = 14;  // hauteurs réelles après FONT_SCALE=1.4
            const int oh = OVERLAY_TL_H;
            const int ox = (ui->overlay_tl_pos.x >= 0.0f)
                         ? (int)ui->overlay_tl_pos.x : g_map_x_off + OV_M;
            const int oy = (ui->overlay_tl_pos.y >= 0.0f)
                         ? (int)ui->overlay_tl_pos.y : OV_M;

            int dragging_tl = (ui->dragging_overlay == 0);
            Color border_tl = dragging_tl ? (Color){140, 100, 30, 255}
                                          : (Color){65, 46, 14, 200};
            DrawRectangleRounded(
                (Rectangle){ox, oy, ow, oh}, 0.2f, 4,
                (Color){4, 3, 1, 235});
            DrawRectangleRoundedLinesEx(
                (Rectangle){ox, oy, ow, oh}, 0.2f, 4,
                dragging_tl ? 1.8f : 1.2f, border_tl);
            // Poignée de déplacement (3 points)
            for (int d = 0; d < 3; d++)
                DrawRectangle(ox + ow/2 - 9 + d*9, oy + 3, 5, 2,
                              (Color){80, 58, 20, 160});

            int tx = ox + OV_P;
            int ty = oy + OV_P + 4;  // +4 pour la poignée

            // Or
            char gbuf[20];
            if (gs->gold >= 10000)
                snprintf(gbuf, sizeof(gbuf), "%dk or", gs->gold / 1000);
            else
                snprintf(gbuf, sizeof(gbuf), "%d or", gs->gold);
            dtxt(gbuf, tx, ty, 12, (Color){230, 155, 35, 255});
            ty += line1_h + 3;

            // Tours
            {
                Color tc = gs->towers.tower_count >= gs->towers.tower_limit
                    ? (Color){231, 76, 60, 255} : (Color){148, 128, 95, 255};
                dtxt(TextFormat("Tours  %d / %d",
                             gs->towers.tower_count, gs->towers.tower_limit),
                         tx, ty, 10, tc);
                ty += line2_h + 3;
            }

            // Unités
            {
                Color uc = gs->units.count >= gs->units.unit_limit
                    ? (Color){231, 76, 60, 255} : (Color){148, 128, 95, 255};
                dtxt(TextFormat("Unites %d / %d",
                             gs->units.count, gs->units.unit_limit),
                         tx, ty, 10, uc);
            }
        }

        // ── Haut-droit : Vague / Kills / Ennemis / Progression / Vitesse ─
        {
            int in_wave  = (gs->phase == PHASE_WAVE);
            int alive    = enemy_pool_alive(&gs->enemies);
            int to_spawn = gs->wave_manager.total_to_spawn
                         - gs->wave_manager.total_spawned;
            if (to_spawn < 0) to_spawn = 0;
            int total_left = alive + to_spawn;

            const int ow = OVERLAY_W;
            const int oh = OVERLAY_TR_H;
            const int ox = (ui->overlay_tr_pos.x >= 0.0f)
                         ? (int)ui->overlay_tr_pos.x
                         : g_map_x_off + MAP_W * TILE_SIZE - OV_M - ow;
            const int oy = (ui->overlay_tr_pos.y >= 0.0f)
                         ? (int)ui->overlay_tr_pos.y : OV_M;

            int dragging_tr = (ui->dragging_overlay == 1);
            Color border_tr = dragging_tr ? (Color){140, 100, 30, 255}
                                          : (Color){65, 46, 14, 200};
            DrawRectangleRounded(
                (Rectangle){ox, oy, ow, oh}, 0.2f, 4,
                (Color){4, 3, 1, 235});
            DrawRectangleRoundedLinesEx(
                (Rectangle){ox, oy, ow, oh}, 0.2f, 4,
                dragging_tr ? 1.8f : 1.2f, border_tr);
            // Poignée de déplacement (3 points)
            for (int d = 0; d < 3; d++)
                DrawRectangle(ox + ow/2 - 9 + d*9, oy + 3, 5, 2,
                              (Color){80, 58, 20, 160});

            int tx = ox + OV_P, ty = oy + OV_P + 4;
            int inner_w = ow - OV_P * 2;

            // Vague
            dtxt(TextFormat("Vague %d", gs->wave_manager.number),
                     tx, ty, 12, (Color){185, 145, 60, 255});
            ty += 17 + 3;  // fh(12)=17

            // Kills
            dtxt(TextFormat("Kills  %d", gs->kills),
                     tx, ty, 10, (Color){148, 128, 95, 255});
            ty += 14 + 3;  // fh(10)=14

            // Ennemis restants
            dtxt(in_wave ? TextFormat("Ennemis  %d", total_left)
                             : "Ennemis  --",
                     tx, ty, 10,
                     in_wave ? (Color){218, 90, 70, 255}
                             : (Color){60, 48, 30, 255});
            ty += 14 + 3;  // fh(10)=14

            // Barre de progression de vague
            {
                float prog = 0.0f;
                if (in_wave && gs->wave_manager.total_to_spawn > 0)
                    prog = 1.0f - (float)total_left
                                / (float)gs->wave_manager.total_to_spawn;
                prog = prog < 0.0f ? 0.0f : prog > 1.0f ? 1.0f : prog;
                Color pc = prog > 0.7f ? (Color){46, 204, 113, 255}
                         : prog > 0.3f ? (Color){243, 156,  18, 255}
                                       : (Color){218,  90,  70, 255};
                draw_bar(tx, ty, inner_w, 7,
                         in_wave ? prog : 0.0f,
                         pc, (Color){18, 10, 4, 200});
            }
            ty += 9 + 5;

            // Vitesse + touche [X]
            {
                const char *sl[] = {"x1", "x2", "x3"};
                const Color sc[] = {
                    {80, 118, 80, 255}, {239, 159, 39, 255}, {231, 76, 60, 255}
                };
                int sidx = (ui->speed_mult >= 1 && ui->speed_mult <= 3)
                         ? ui->speed_mult - 1 : 0;
                char vbuf[20];
                snprintf(vbuf, sizeof(vbuf), "Vitesse %s", sl[sidx]);
                dtxt(vbuf, tx, ty, 10, sc[sidx]);
                dtxt("(X)", tx + mtxt(vbuf, 10) + 5, ty, 9,
                         (Color){82, 65, 40, 210});
            }
        }

        // (barres HP des bases déplacées dans le panneau gauche du HUD)
    }

    // ════════════════════════════════════════════════
    // OVERLAYS CARTE (dans l'espace de la carte)
    // ════════════════════════════════════════════════
    {
        Camera2D map_cam = {0};
        map_cam.offset = (Vector2){(float)g_map_x_off, 0.0f};
        map_cam.zoom   = 1.0f;
        BeginMode2D(map_cam);

        // Prévisualisation
        if (ui->selected_tool != TOOL_NONE &&
            ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0) {
            if (ui_tool_is_tower(ui->selected_tool)) {
                render_tower_preview(&gs->map, &gs->towers,
                                     ui_tool_to_tower(ui->selected_tool),
                                     ui->hovered_tile_x,
                                     ui->hovered_tile_y);
            } else {
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

        // Portée au survol d'une tour posée
        if (ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0 &&
            ui->selected_tool == TOOL_NONE && !ui->selection.active) {
            int htx = ui->hovered_tile_x, hty = ui->hovered_tile_y;
            for (int i = 0; i < MAX_TOWERS; i++) {
                const Tower *tw = &gs->towers.towers[i];
                if (!tw->active || tw->tile_x != htx || tw->tile_y != hty) continue;
                float cx = tw->tile_x * TILE_SIZE + TILE_SIZE / 2.0f;
                float cy = tw->tile_y * TILE_SIZE + TILE_SIZE / 2.0f;
                float rad = tw->range * TILE_SIZE;
                Color rc = TOWER_FILL[tw->type];
                DrawCircle((int)cx, (int)cy, rad,
                           (Color){rc.r, rc.g, rc.b, 18});
                DrawCircleLines((int)cx, (int)cy, rad,
                                (Color){rc.r, rc.g, rc.b, 100});
                break;
            }
        }

        EndMode2D();
    }

    // ════════════════════════════════════════════════
    // NOTIFICATIONS FLOTTANTES
    // ════════════════════════════════════════════════
    {
        int nx = g_map_x_off + MAP_W * TILE_SIZE / 2;
        int base_y = HUD_Y - 12;
        for (int i = 0; i < ui->notif_count; i++) {
            const FloatNotif *n = &ui->notifs[i];
            float alpha_f = n->timer > 0.5f ? 1.0f : n->timer / 0.5f;
            unsigned char alpha = (unsigned char)(alpha_f * 235.0f);
            int ny = base_y - (int)n->y_off - i * 22;
            if (ny < 4) continue;
            int tw2 = mtxt(n->text, 12);
            DrawRectangleRounded(
                (Rectangle){nx - tw2/2 - 9, ny - 4, tw2 + 18, 19},
                0.3f, 4,
                (Color){4, 3, 1, (unsigned char)(alpha_f * 185.0f)});
            dtxt(n->text, nx - tw2/2, ny, 12,
                     (Color){n->col.r, n->col.g, n->col.b, alpha});
        }
    }

    // ════════════════════════════════════════════════
    // TOOLTIP OUTIL (au-dessus de tout le reste)
    // ════════════════════════════════════════════════
    if (ui->hovered_tool != -1) {
        const ToolInfo  *info   = &TOOL_INFO[ui->hovered_tool];
        const Rectangle *rb     = &ui->tool_btns[ui->hovered_tool];
        int              locked = !tool_is_unlocked((ToolID)ui->hovered_tool, gs);

        /* Hauteur du tooltip : plus grand si verrouillé (2 lignes de texte) */
        const int TW = 170;
        const int TH = locked ? 52 : 62;

        int tx = (int)rb->x;
        int ty = (int)rb->y - TH - GAP;
        if (ty < HUD_Y + 2) ty = HUD_Y + 2;
        if (ty + TH > HUD_Y + HUD_H - 2) ty = HUD_Y + HUD_H - TH - 2;
        if (tx < UI_LEFT_PANEL_W + M) tx = UI_LEFT_PANEL_W + M;
        if (tx + TW > VIRT_W - UI_PANEL_W - M)
            tx = VIRT_W - UI_PANEL_W - M - TW;

        float trnd = (float)UI_RADIUS / TH;
        Color border = locked ? (Color){80, 55, 20, 200} : TOOL_COLORS[ui->hovered_tool];
        DrawRectangleRounded(
            (Rectangle){(float)tx,(float)ty,(float)TW,(float)TH},
            trnd, 6, (Color){10, 6, 2, 252});
        DrawRectangleRoundedLinesEx(
            (Rectangle){(float)tx,(float)ty,(float)TW,(float)TH},
            trnd, 6, 1.5f, border);

        char dbuf[48];
        clip_text(info->name, TW - M*2, 11, dbuf, sizeof(dbuf));
        dtxt(dbuf, tx+M, ty+M, 11,
             locked ? (Color){80, 62, 35, 255} : TOOL_COLORS[ui->hovered_tool]);

        if (locked) {
            /* Tour verrouillée : affiche uniquement le déblocage requis */
            TowerType tt = ui_tool_to_tower((ToolID)ui->hovered_tool);
            const char *when = TOWER_UNLOCK_ACT_NAME[tt];
            clip_text(TextFormat("Deblocage : %s", when ? when : "?"),
                      TW-M*2, 9, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ty+30, 9, (Color){130, 95, 40, 255});
        } else {
            /* Tour disponible : stats complètes */
            clip_text(TextFormat("Dmg:%.0f  Port:%.1ft", info->dmg, info->range),
                      TW-M*2, 10, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ty+23, 10, (Color){145,125,92,255});
            clip_text(TextFormat("Cad:%.1f/s  Cout:%dor", info->rate, info->cost),
                      TW-M*2, 10, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ty+35, 10, (Color){145,125,92,255});
            clip_text(info->desc, TW-M*2, 9, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ty+49, 9, (Color){82,65,40,255});

            int at_tower_limit = ui_tool_is_tower((ToolID)ui->hovered_tool) &&
                                 gs->towers.tower_count >= gs->towers.tower_limit;
            int at_unit_limit  = ui_tool_is_unit((ToolID)ui->hovered_tool) &&
                                 gs->units.count >= gs->units.unit_limit;
            if (at_tower_limit || at_unit_limit) {
                dtxt(TextFormat("LIMITE (%d bases)", gs->map.base_count),
                         tx + M, ty + TH - 14, 9,
                         (Color){231, 76, 60, 255});
            }
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
        dtxt(fb, VIRT_W - 66, HUD_Y + M, 11, fc);
    }
}