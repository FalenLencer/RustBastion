/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "game_init.h"
#include "game_state.h"
#include "../map/map_gen.h"
#include "../map/pathfinding.h"
#include "../combat/unit.h"
#include "../ui/hud.h"
#include "../ui/campaign_data.h"
#include "meta.h"
#include <stdio.h>

// Vérifie que chaque base active a au moins 1 chemin A* valide
static int all_bases_reachable(const GameState *gs) {
    for (int b = 0; b < gs->map.base_count; b++) {
        if (!gs->map.bases[b].active) continue;
        int found = 0;
        for (int p = 0; p < gs->enemy_paths.count; p++) {
            if (gs->enemy_paths.paths[p].found &&
                gs->enemy_paths.paths[p].base_id == b) {
                found = 1; break;
            }
        }
        if (!found) return 0;
    }
    return 1;
}

void game_init_map(GameState *gs, ThemeID theme) {
    int seed;
    int attempts = 0;
    int ok       = 0;

    do {
        seed = GetRandomValue(1, 99999);
        generate_map(&gs->map, seed, 10, theme);
        astar_all(&gs->map, &gs->enemy_paths);
        pathset_apply(&gs->map, &gs->enemy_paths);
        attempts++;

        ok = (gs->enemy_paths.count > 0 &&
              gs->map.path_count    > 0 &&
              all_bases_reachable(gs));

        if (!ok && attempts == 100) {
            generate_map(&gs->map, seed, 4, theme);
            astar_all(&gs->map, &gs->enemy_paths);
            pathset_apply(&gs->map, &gs->enemy_paths);
            ok = (gs->enemy_paths.count > 0 &&
                  gs->map.path_count    > 0 &&
                  all_bases_reachable(gs));
        }
    } while (!ok && attempts < 300);

    if (!ok) {
        generate_map(&gs->map, 42, 4, theme);
        astar_all(&gs->map, &gs->enemy_paths);
        pathset_apply(&gs->map, &gs->enemy_paths);
        fprintf(stderr, "game_init_map: fallback seed 42 theme=%d\n",
                (int)theme);
    }

    // lives = somme HP de toutes les bases
    {
        int total = 0;
        for (int b = 0; b < gs->map.base_count; b++)
            total += gs->map.bases[b].hp;
        gs->lives = total;
    }

    // UnitPool ancré sur la base principale
    float bpx, bpy;
    if (gs->map.base_count > 0) {
        bpx = gs->map.bases[0].pos.x * TILE_SIZE + TILE_SIZE / 2.0f;
        bpy = gs->map.bases[0].pos.y * TILE_SIZE + TILE_SIZE / 2.0f;
    } else {
        bpx = MAP_W * TILE_SIZE * 0.5f;
        bpy = MAP_H * TILE_SIZE * 0.5f;
    }
    unit_pool_init(&gs->units, bpx, bpy);
    ui_init(&gs->ui);

    printf("Map seed=%d spawns=%d bases=%d theme=%d attempts=%d\n",
           seed, gs->enemy_paths.count,
           gs->map.base_count, (int)gs->map.theme, attempts);
}

void game_init_arcade(GameState *gs, ThemeID theme, int slot) {
    game_state_init(gs);
    gs->is_campaign    = 0;
    gs->campaign_num   = 0;
    gs->campaign_stage = 0;
    gs->is_endless          = 1;
    gs->endless_series      = 0;
    gs->endless_multiplier  = 1.0f;
    gs->endless_pending_extract = 0;
    game_init_map(gs, theme);
    printf("Arcade slot=%d theme=%d\n", slot, (int)theme);
}

void game_init_campaign(GameState *gs, int campaign_num, int slot, int seed) {
    game_state_init(gs);
    gs->is_campaign         = 1;
    gs->campaign_num        = campaign_num;
    gs->campaign_stage      = 0;
    gs->campaign_order_seed = seed;

    ThemeID theme = campaign_act_get(0)->theme;
    game_init_map(gs, theme);

    printf("Campagne %d acte 0 slot=%d theme=%d\n",
           campaign_num, slot, (int)theme);
    (void)seed;
}

void game_next_campaign_stage(GameState *gs) {
    int camp_num   = gs->campaign_num;
    int camp_stage = gs->campaign_stage + 1;
    int camp_seed  = gs->campaign_order_seed;
    MetaProgress meta_bak = gs->meta;

    game_state_init(gs);
    gs->is_campaign         = 1;
    gs->campaign_num        = camp_num;
    gs->campaign_stage      = camp_stage;
    gs->campaign_order_seed = camp_seed;
    gs->meta                = meta_bak;

    meta_compute(&gs->meta, &gs->bonuses);
    gs->gold = gs->bonuses.start_gold;

    ThemeID theme = campaign_act_get(camp_stage)->theme;
    game_init_map(gs, theme);

    printf("Campagne %d acte %d theme=%d\n",
           camp_num, camp_stage, (int)theme);
}