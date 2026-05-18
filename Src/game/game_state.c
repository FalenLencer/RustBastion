/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "game_state.h"
#include "../engine/audio.h"

/* ── Fonctions de gestion des bases ─────────────────────────── */
int game_all_bases_fallen(const GameState *gs) {
    for (int i = 0; i < gs->map.base_count; i++)
        if (gs->map.bases[i].active && gs->map.bases[i].hp > 0) return 0;
    return 1;
}

void game_damage_base(GameState *gs, int base_id, int dmg) {
    if (base_id < 0 || base_id >= gs->map.base_count) return;
    BaseInfo *b = &gs->map.bases[base_id];
    if (!b->active) return;
    b->hp -= dmg;
    if (b->hp <= 0) {
        b->hp     = 0;
        b->active = 0;
    }
    gs->lives -= dmg;
    if (gs->lives < 0) gs->lives = 0;
}

void game_state_init(GameState *gs) {
    if (!meta_load(&gs->meta)) meta_init(&gs->meta);
    meta_compute(&gs->meta, &gs->bonuses);
    gs->phase               = PHASE_PREP;
    gs->gold                = gs->bonuses.start_gold;
    gs->lives               = gs->bonuses.start_lives;
    gs->kills               = 0;
    gs->is_campaign         = 0;
    gs->campaign_num        = 0;
    gs->campaign_stage      = 0;
    gs->campaign_order_seed = 0;
    gs->inventory_count     = 0;
    for (int i = 0; i < MAX_INVENTORY; i++)
        gs->inventory[i] = MAT_NONE;
    enemy_pool_init(&gs->enemies);
    wave_init(&gs->wave_manager);
    tower_pool_init(&gs->towers);
    gs->act_objective_done      = 0;
    gs->act_no_unit_lost        = 1;
    gs->act_materials_collected = 0;
}

void game_state_update(GameState *gs, float dt) {
    // Mise à jour des limites actives
    gs->towers.tower_limit = tower_active_limit(&gs->bonuses);
    gs->units.unit_limit   = unit_active_limit(&gs->bonuses, gs->map.base_count);

    // Vérification game over — toutes les bases doivent tomber
    if (game_all_bases_fallen(gs)) {
        if (gs->phase != PHASE_GAMEOVER) {
            // En endless, la ferraille est gérée par meta_endless_end() depuis main.c
            // En campagne, la ferraille est gérée par meta_end_of_campaign_stage()
            if (!gs->is_endless && !gs->is_campaign)
                meta_end_of_run(&gs->meta,
                                gs->wave_manager.number,
                                gs->kills, gs->gold);
            audio_play_sfx(AUDIO_SFX_GAME_OVER);
        }
        gs->phase = PHASE_GAMEOVER;
        return;
    }

    const Theme *th       = theme_get(gs->map.theme);
    float effective_dt    = dt * (float)gs->ui.speed_mult;

    // Snapshot avant mise à jour (pour tracking objectifs campagne)
    int unit_cnt_before = gs->units.count;
    int inv_cnt_before  = gs->inventory_count;

    // Mise à jour ennemis — passe towers pour Artillery
    enemy_pool_update(&gs->enemies, &gs->enemy_paths,
                      &gs->units, &gs->towers,
                      &gs->map,
                      effective_dt,
                      &gs->lives, &gs->gold, &gs->kills);

    // Libère les tuiles des tours détruites par Artillery
    for (int i = 0; i < MAX_TOWERS; i++) {
        Tower *tw = &gs->towers.towers[i];
        if (!tw->active) continue;
        if (tw->hp <= 0.0f) {
            gs->map.tiles[tw->tile_y][tw->tile_x].buildable = 1;
            tw->active = 0;
            if (gs->towers.tower_count > 0) gs->towers.tower_count--;
            ui_push_notif(&gs->ui, "Tour detruite par l'artillerie !",
                          (Color){231, 76, 60, 255});
        }
    }

    tower_pool_update(&gs->towers, &gs->enemies, effective_dt);
    unit_pool_update(&gs->units, &gs->enemies, &gs->map,
                     effective_dt, gs->inventory, &gs->inventory_count);
    wave_update(&gs->wave_manager, &gs->enemies,
                &gs->enemy_paths, &gs->map, th, effective_dt);

    // ── Suivi des objectifs de l'acte (campagne uniquement) ───────
    if (gs->is_campaign) {
        // OBJ_NO_UNIT_LOST : une unité est morte si le count a diminué
        if (gs->act_no_unit_lost && gs->units.count < unit_cnt_before)
            gs->act_no_unit_lost = 0;
        // OBJ_COLLECT_MATERIALS : cumule les matériaux livrés ce stage
        if (gs->inventory_count > inv_cnt_before)
            gs->act_materials_collected += gs->inventory_count - inv_cnt_before;
    }

    gs->phase = (gs->wave_manager.state == WAVE_IDLE ||
                 gs->wave_manager.state == WAVE_COMPLETE)
                ? PHASE_PREP : PHASE_WAVE;
}