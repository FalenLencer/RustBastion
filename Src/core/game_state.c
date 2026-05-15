#include "game_state.h"

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
}

void game_state_update(GameState *gs, float dt) {
    // Mise à jour des limites actives
    gs->towers.tower_limit = tower_active_limit(&gs->bonuses);
    gs->units.unit_limit   = unit_active_limit(&gs->bonuses, gs->map.base_count);

    // Vérification game over
    if (gs->lives <= 0) {
        if (gs->phase != PHASE_GAMEOVER) {
            meta_end_of_run(&gs->meta,
                            gs->wave_manager.number,
                            gs->kills, gs->gold);
        }
        gs->phase = PHASE_GAMEOVER;
        return;
    }

    const Theme *th       = theme_get(gs->map.theme);
    float effective_dt    = dt * (float)gs->ui.speed_mult;

    // Mise à jour ennemis — passe towers pour Artillery
    enemy_pool_update(&gs->enemies, &gs->enemy_paths,
                      &gs->units, &gs->towers,
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
        }
    }

    tower_pool_update(&gs->towers, &gs->enemies, effective_dt);
    unit_pool_update(&gs->units, &gs->enemies, &gs->map,
                     effective_dt, gs->inventory, &gs->inventory_count);
    wave_update(&gs->wave_manager, &gs->enemies,
                &gs->enemy_paths, th, effective_dt,
                &gs->lives, &gs->gold, &gs->kills);

    gs->phase = (gs->wave_manager.state == WAVE_IDLE ||
                 gs->wave_manager.state == WAVE_COMPLETE)
                ? PHASE_PREP : PHASE_WAVE;
}