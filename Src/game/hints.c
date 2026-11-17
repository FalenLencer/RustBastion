/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hints.c ─ Indices contextuels (cf. hints.h).
 *
 *  Les conditions sont évaluées dans l'ordre d'URGENCE : une base en danger
 *  passe avant un conseil d'économie. Le texte est affiché par le HUD
 *  (hud_render_hint, hud_render_info.c) via ui.hint_text / ui.hint_t.
 */
#include "hints.h"
#include "game_state.h"
#include "hazard.h"
#include "../engine/audio.h"

#define HINT_SFX_PITCH     1.50f  // déclic aigu à l'apparition d'un conseil
#define HINT_GAP         6.0f   // silence minimal entre deux conseils (s)
#define HINT_IDLE_GOLD     200    // or « qui dort » en préparation
#define HINT_IDLE_WAVE     2      // …à partir de cette vague
#define HINT_MAT_DELAY     25.0f  // minerai en stock depuis N s (temps de jeu)
#define HINT_BASE_PCT      40     // base sous N % de ses PV
#define HINT_UNITS_WAVE    3      // aucune unité à partir de cette vague

typedef enum {
    HINT_BASE_LOW = 0,
    HINT_COVERAGE,
    HINT_MATERIAL,
    HINT_IDLE_GOLD_ID,
    HINT_UNITS,
    HINT_TERRAIN,
    HINT_COUNT
} HintId;

static const char *HINT_TEXT[HINT_COUNT] = {
    [HINT_BASE_LOW]     = "Base en danger : réparez-la depuis le panneau des bases (en bas à gauche).",
    [HINT_COVERAGE]     = "Vos tours ne couvrent pas le chemin : posez-les au bord de la route.",
    [HINT_MATERIAL]     = "Minerai en stock : sélectionnez une tour, puis « Appliquer ».",
    [HINT_IDLE_GOLD_ID] = "De l'or dort : posez une tour ou améliorez-en une avant la vague.",
    [HINT_UNITS]        = "Les unités (touches 5 à 9) interceptent ce qui passe entre les tours.",
    [HINT_TERRAIN]      = "Le terrain piégé use l'ennemi : gardez vos tours à portée de ces tronçons.",
};

static int   g_shown[HINT_COUNT];
static int   g_key = -1;          // graine de la carte des conseils déjà vus
static float g_gap, g_mat_wait;

static int base_low(const GameState *gs) {
    for (int b = 0; b < gs->map.base_count && b < MAX_BASES; b++) {
        const BaseInfo *bi = &gs->map.bases[b];
        if (bi->active && bi->max_hp > 0 && bi->hp * 100 <= bi->max_hp * HINT_BASE_PCT)
            return 1;
    }
    return 0;
}

static int towers_miss_path(const GameState *gs) {
    if (gs->towers.tower_count <= 0) return 0;
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *t = &gs->towers.towers[i];
        if (!t->active) continue;
        if (tower_path_coverage(&gs->map, t->tile_x, t->tile_y,
                                TOWER_BASE_STATS[t->type].range) > 0)
            return 0;   // au moins une tour mord sur le chemin
    }
    return 1;
}

/* Premier conseil pertinent, dans l'ordre d'urgence ; -1 = aucun. */
static int pick(const GameState *gs) {
    int prep = (gs->phase == PHASE_PREP), wave = gs->wave_manager.number;
    if (!g_shown[HINT_BASE_LOW] && base_low(gs))                  return HINT_BASE_LOW;
    if (!g_shown[HINT_COVERAGE] && prep && towers_miss_path(gs)) return HINT_COVERAGE;
    if (!g_shown[HINT_MATERIAL] && g_mat_wait >= HINT_MAT_DELAY)  return HINT_MATERIAL;
    if (!g_shown[HINT_IDLE_GOLD_ID] && prep && wave >= HINT_IDLE_WAVE &&
        gs->gold >= HINT_IDLE_GOLD)                               return HINT_IDLE_GOLD_ID;
    if (!g_shown[HINT_UNITS] && prep && wave >= HINT_UNITS_WAVE &&
        gs->units.count == 0)                                     return HINT_UNITS;
    if (!g_shown[HINT_TERRAIN] && gs->phase == PHASE_WAVE &&
        hazard_tile_count(&gs->map) > 0)                          return HINT_TERRAIN;
    return -1;
}

void hints_update(GameState *gs, float dt) {
    if (gs->ui.hint_t > 0.0f) gs->ui.hint_t -= dt;      // conseil à l'écran
    if (gs->meta.campaigns_completed > 0) return;        // vétéran : rien
    if (gs->map.seed != g_key) {                         // nouvelle carte
        for (int i = 0; i < HINT_COUNT; i++) g_shown[i] = 0;
        g_key = gs->map.seed; g_gap = HINT_GAP; g_mat_wait = 0.0f;
    }
    g_mat_wait = (gs->inventory_count > 0) ? g_mat_wait + dt : 0.0f;
    if (gs->ui.hint_t > 0.0f) return;
    if (g_gap > 0.0f) { g_gap -= dt; return; }

    int h = pick(gs);
    if (h < 0) return;
    g_shown[h]       = 1;
    gs->ui.hint_text = HINT_TEXT[h];
    gs->ui.hint_t    = HINT_SHOW_TIME;
    g_gap            = HINT_GAP;
    audio_play_sfx_pitch(AUDIO_SFX_MENU_CLICK, HINT_SFX_PITCH);   // discret : attire l'œil
}
