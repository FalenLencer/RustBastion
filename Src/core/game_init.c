#include "game_init.h"
#include "game_state.h"
#include "../map/map_gen.h"
#include "../map/pathfinding.h"
#include "../combat/unit.h"
#include "../ui/ui.h"
#include "../meta/meta.h"
#include <stdio.h>

void game_init_map(GameState *gs, ThemeID theme) {
    int seed;
    do {
        seed = GetRandomValue(1, 99999);
        generate_map(&gs->map, seed, 20, theme);
        astar_all(&gs->map, &gs->enemy_paths);
        pathset_apply(&gs->map, &gs->enemy_paths);
    } while (gs->enemy_paths.count == 0 || gs->map.path_count == 0);
    
    float bpx = gs->map.paths[0].base.x * TILE_SIZE + TILE_SIZE/2.0f;
    float bpy = gs->map.paths[0].base.y * TILE_SIZE + TILE_SIZE/2.0f;
    unit_pool_init(&gs->units, bpx, bpy);
    ui_init(&gs->ui);
    printf("Map seed=%d chemins=%d theme=%d\n",
           seed, gs->enemy_paths.count, (int)gs->map.theme);
}

void game_init_arcade(GameState *gs, ThemeID theme, int slot) {
    game_state_init(gs);
    gs->is_campaign = 0;
    gs->campaign_num = 0;
    gs->campaign_stage = 0;
    game_init_map(gs, theme);
    printf("Arcade slot=%d theme=%d\n", slot, (int)theme);
}

void game_init_campaign(GameState *gs, int campaign_num, int slot) {
    game_state_init(gs);
    gs->is_campaign    = 1;
    gs->campaign_num   = campaign_num;
    gs->campaign_stage = 0;
    int themes[CAMPAIGN_STAGES];
    meta_campaign_theme_order(campaign_num, themes);
    game_init_map(gs, (ThemeID)themes[0]);
    printf("Campagne %d stage 0 slot=%d theme=%d\n",
           campaign_num, slot, themes[0]);
}

void game_next_campaign_stage(GameState *gs) {
    int camp_num   = gs->campaign_num;
    int camp_stage = gs->campaign_stage + 1;
    MetaProgress meta_bak = gs->meta;
    game_state_init(gs);
    gs->is_campaign    = 1;
    gs->campaign_num   = camp_num;
    gs->campaign_stage = camp_stage;
    gs->meta           = meta_bak;
    meta_compute(&gs->meta, &gs->bonuses);
    gs->gold  = gs->bonuses.start_gold;
    gs->lives = gs->bonuses.start_lives;
    int themes[CAMPAIGN_STAGES];
    meta_campaign_theme_order(camp_num, themes);
    game_init_map(gs, (ThemeID)themes[camp_stage]);
    printf("Campagne %d stage %d theme=%d\n", camp_num, camp_stage, themes[camp_stage]);
}