#include "game_state.h"

void game_state_init(GameState *gs) {
    if (!meta_load(&gs->meta)) meta_init(&gs->meta);
    meta_compute(&gs->meta, &gs->bonuses);
    gs->phase = PHASE_PREP;
    gs->gold  = gs->bonuses.start_gold;
    gs->lives = gs->bonuses.start_lives;
    gs->kills = 0;
    enemy_pool_init(&gs->enemies);
    wave_init(&gs->wave_manager);
    tower_pool_init(&gs->towers);
    // ui_init() et unit_pool_init() appelés après InitWindow dans main.c
}

void game_state_update(GameState *gs, float dt) {
    if (gs->lives <= 0) {
        if (gs->phase != PHASE_GAMEOVER) {
            // Déclenche la fin de partie une seule fois
            meta_end_of_run(&gs->meta,
                            gs->wave_manager.number,
                            gs->kills,
                            gs->gold);
        }
        gs->phase = PHASE_GAMEOVER;
        return;
    }

    const Theme *th = theme_get(gs->map.theme);

    tower_pool_update(&gs->towers, &gs->enemies, dt);
    unit_pool_update(&gs->units, &gs->enemies, dt);

    // ← CORRECTION bug #3 : gs->kills est incrémenté directement dans
    //   enemy_pool_update() (appelé depuis wave_update), via le pointeur &gs->kills.
    //   Plus besoin du double-comptage fragile avec prev_kills.
    wave_update(&gs->wave_manager, &gs->enemies,
                &gs->enemy_paths, th, dt, &gs->lives, &gs->gold,
                &gs->kills);

    gs->phase = (gs->wave_manager.state == WAVE_IDLE ||
                 gs->wave_manager.state == WAVE_COMPLETE)
                ? PHASE_PREP : PHASE_WAVE;
}