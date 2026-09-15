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

/* Hauteur dynamique de l'overlay bas-gauche (bases HP + boutons réparation).
   Recomputed each frame : varie selon le nombre de bases et celles qui ont
   un bouton réparation visible. */
int overlay_bl_h(const GameState *gs) {
    const int OV_P = OVERLAY_OV_P;
    int h = OV_P + 4 + OV_P;   /* top padding + espace poignée + bottom padding */
    for (int b = 0; b < gs->map.base_count && b < MAX_BASES; b++) {
        h += 18;   /* label(8) + gap(2) + barre(6) + gap(2) */
        const BaseInfo *base = &gs->map.bases[b];
        if (base->active && base->hp > 0 && base->hp < base->max_hp)
            h += 16;   /* bouton réparation (14) + gap (2) */
        if (b < gs->map.base_count - 1)
            h += 4;    /* gap inter-bases */
    }
    if (h < OV_P * 2 + 20) h = OV_P * 2 + 20;
    return h;
}

/* ── Sélection : annulation et interrogation (source unique) ──────── */
int ui_has_selection(const UIState *ui) {
    return (ui->selected_tool != TOOL_NONE ||
            ui->selection.active          ||
            ui->worker_selected_idx >= 0  ||
            ui->sell_unit_idx      >= 0   ||
            ui->group_count         > 0   ||
            ui->behavior_pending   >= 0);
}

void ui_clear_selection(UIState *ui, GameState *gs) {
    ui->selected_tool         = TOOL_NONE;
    ui->selection.active      = 0;
    ui->worker_selected_idx   = -1;
    ui->sell_unit_idx         = -1;
    ui->behavior_pending      = -1;
    ui->behavior_pending_unit = -1;
    for (int j = 0; j < MAX_UNITS; j++) ui->group_sel[j] = 0;
    ui->group_count = 0;
    if (gs) gs->units.selected_unit = -1;
}

/* ── P1.2 : HUD progressif (moins d'UI tant qu'elle n'est pas UTILE) ──
   Predicats PARTAGES rendu + input : un element masque ne doit JAMAIS
   garder une hitbox cliquable. */

/* Panneau bases (overlay BL) : visible seulement si plusieurs bases,
   ou si une base est abimee/detruite (= il y a quelque chose a y faire). */
int hud_show_bases_panel(const GameState *gs) {
    if (gs->map.base_count > 1) return 1;
    for (int b = 0; b < gs->map.base_count && b < MAX_BASES; b++) {
        const BaseInfo *base = &gs->map.bases[b];
        if (!base->active || base->hp < base->max_hp) return 1;
    }
    return 0;
}

/* Achats de slots : caches en tout debut de partie (bruit pour un nouveau
   joueur), visibles des HUD_SLOTS_FROM_WAVE ou si deja achetes (continuite
   apres chargement d'une sauvegarde). */
int hud_show_slot_buys(const GameState *gs) {
    if (gs->slots_tower_bought > 0 || gs->slots_unit_bought > 0) return 1;
    return gs->wave_manager.number >= HUD_SLOTS_FROM_WAVE;
}

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
    [TOWER_FLAME]  =  5,  // après avoir complété l'acte index 5 (Ch.2-Act.3)
};

const char *TOWER_UNLOCK_ACT_NAME[TOWER_TYPE_COUNT] = {
    [TOWER_GUN]    = NULL,
    [TOWER_SNIPER] = "Ch.1 — Acte 2",
    [TOWER_TESLA]  = "Ch.1 — Acte 3",
    [TOWER_FLAME]  = "Ch.2 — Acte 3",
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
    ui->overlay_bl_pos      = (Vector2){-1.0f, -1.0f};  // sentinel : init au 1er frame
    ui->dragging_overlay      = -1;
    ui->behavior_pending      = -1;
    ui->behavior_pending_unit = -1;

    const int HUD_Y  = g_canvas_virt_h - UI_HUD_HEIGHT;
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
    int right_x = g_canvas_virt_w_base - UI_PANEL_W + M;
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
// CONSEIL DE CONTRE (P0.3) — helper PARTAGÉ
// Analyse la vague suivante (wave_preview_types) contre la table
// ENEMY_DMG_MULT et désigne le type de dégâts le plus efficace /
// le plus résisté. Utilisé par : tooltip de pose de tour (2D),
// bouton APPLIQUER MATÉRIAU (2D), invite [M]/[N] du mode héros.
// ════════════════════════════════════════════════════
#define ADVICE_TYPES   3      /* ennemis les plus probables considérés */
#define ADVICE_STRONG  1.15f  /* seuil « efficace » (multiplicateur)   */
#define ADVICE_WEAK    0.85f  /* seuil « résiste »                     */

int hud_counter_advice(const GameState *gs, char *out, int out_sz) {
    EnemyType pv[ENEMY_TYPE_COUNT];
    int max_stage = gs->is_campaign
        ? campaign_difficulty_stage(gs->campaign_stage) - 1 : 9999;
    int n = wave_preview_types(gs->wave_manager.number + 1,
                               gs->map.theme, gs->is_campaign, max_stage,
                               gs->wave_manager.arcade_bias,
                               pv, ENEMY_TYPE_COUNT);
    if (n > ADVICE_TYPES) n = ADVICE_TYPES;

    /* Campagne : ne conseille que sur les ennemis DÉCOUVERTS (pas de
       spoil — même règle que les « ? » de l'aperçu de vague). */
    EnemyType known[ADVICE_TYPES];
    int kn = 0;
    for (int i = 0; i < n; i++) {
        if (gs->is_campaign && !gs->meta.bestiary_discovered[pv[i]]) continue;
        known[kn++] = pv[i];
    }
    if (kn == 0) return 0;

    float avg[DAMAGE_TYPE_COUNT];
    int best = 0, worst = 0;
    for (int d = 0; d < DAMAGE_TYPE_COUNT; d++) {
        float s = 0.0f;
        for (int i = 0; i < kn; i++) s += ENEMY_DMG_MULT[known[i]][d];
        avg[d] = s / (float)kn;
        if (avg[d] > avg[best])  best  = d;
        if (avg[d] < avg[worst]) worst = d;
    }
    int has_best  = (avg[best]  >= ADVICE_STRONG);
    int has_worst = (avg[worst] <= ADVICE_WEAK);
    if (!has_best && !has_worst) return 0;   /* multiplicateurs plats */

    /* Noms d'ennemis : 2 max, repli sur 1 seul si trop long. */
    char names[28];
    if (kn >= 2) {
        snprintf(names, sizeof(names), "%s, %s",
                 ENEMY_BASE_STATS[known[0]].name,
                 ENEMY_BASE_STATS[known[1]].name);
        if ((int)strlen(names) > 24)
            snprintf(names, sizeof(names), "%s...",
                     ENEMY_BASE_STATS[known[0]].name);
    } else {
        snprintf(names, sizeof(names), "%s",
                 ENEMY_BASE_STATS[known[0]].name);
    }

    if (has_best && has_worst)
        snprintf(out, (size_t)out_sz, "V%d %s : %s efficace, %s resiste",
                 gs->wave_manager.number + 1, names,
                 DAMAGE_NAMES[best], DAMAGE_NAMES[worst]);
    else if (has_best)
        snprintf(out, (size_t)out_sz, "V%d %s : %s tres efficace",
                 gs->wave_manager.number + 1, names, DAMAGE_NAMES[best]);
    else
        snprintf(out, (size_t)out_sz, "V%d %s : evitez %s",
                 gs->wave_manager.number + 1, names, DAMAGE_NAMES[worst]);
    return 1;
}
