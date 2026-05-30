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
#include "../combat/wave.h"
#include "../ui/hud.h"
#include "../ui/renderer.h"
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

void game_init_map_full(GameState *gs, ThemeID theme,
                        int forced_bases, int forced_spawns, int min_dist,
                        int forced_deposits, int map_w, int map_h) {
    if (min_dist <= 0) min_dist = 10;

    int seed;
    int attempts = 0;
    int ok       = 0;

    do {
        seed = GetRandomValue(1, 99999);
        generate_map(&gs->map, seed, min_dist, theme, forced_bases, forced_spawns,
                     forced_deposits, map_w, map_h);
        astar_all(&gs->map, &gs->enemy_paths);
        pathset_apply(&gs->map, &gs->enemy_paths);
        attempts++;

        /* En arcade (forced_bases == 0), certaines bases peuvent ne pas
           avoir de chemin ennemi — elles servent de "vies bonus" non
           attaquées. On vérifie seulement qu'il existe AU MOINS un chemin.
           En campagne / custom (forced_bases > 0), toutes les bases doivent
           être atteignables pour que le scénario soit cohérent. */
        int paths_ok = (gs->enemy_paths.count > 0 && gs->map.path_count > 0);
        ok = paths_ok && (forced_bases <= 0 || all_bases_reachable(gs));

        if (!ok && attempts == 100) {
            int md2 = min_dist > 4 ? min_dist / 2 : 4;
            generate_map(&gs->map, seed, md2, theme, forced_bases, forced_spawns,
                         forced_deposits, map_w, map_h);
            astar_all(&gs->map, &gs->enemy_paths);
            pathset_apply(&gs->map, &gs->enemy_paths);
            paths_ok = (gs->enemy_paths.count > 0 && gs->map.path_count > 0);
            ok = paths_ok && (forced_bases <= 0 || all_bases_reachable(gs));
        }
    } while (!ok && attempts < 300);

    if (!ok) {
        generate_map(&gs->map, 42, 4, theme, forced_bases, forced_spawns,
                     forced_deposits, map_w, map_h);
        astar_all(&gs->map, &gs->enemy_paths);
        pathset_apply(&gs->map, &gs->enemy_paths);
        fprintf(stderr, "game_init_map: fallback seed 42 theme=%d bases=%d spawns=%d\n",
                (int)theme, forced_bases, forced_spawns);
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
        bpx = gs->map.w * TILE_SIZE * 0.5f;
        bpy = gs->map.h * TILE_SIZE * 0.5f;
    }
    unit_pool_init(&gs->units, bpx, bpy);
    ui_init(&gs->ui);

    /* Zoom de rendu : on choisit le plus petit des deux ratios (largeur et hauteur)
       pour que la carte tienne dans les deux dimensions sans déborder sur le HUD.
       Le HUD reste à taille fixe quelle que soit la taille de la carte. */
    {
        float sx = (gs->map.w > 0) ? (float)MAP_W / (float)gs->map.w : 1.0f;
        float sy = (gs->map.h > 0) ? (float)MAP_H / (float)gs->map.h : 1.0f;
        g_map_render_scale = (sx < sy) ? sx : sy;
        if (g_map_render_scale > 1.0f) g_map_render_scale = 1.0f;
    }

    printf("Map seed=%d spawns=%d bases=%d theme=%d size=%dx%d scale=%.3f attempts=%d\n",
           seed, gs->enemy_paths.count,
           gs->map.base_count, (int)gs->map.theme,
           gs->map.w, gs->map.h, g_map_render_scale, attempts);

    /* Bonus d'or de départ : chaque base supplémentaire ajoute 60 or.
       Appliqué uniquement en début de partie (wave 0), pas en regen endless. */
    if (gs->map.base_count > 1 && gs->wave_manager.number == 0)
        gs->gold += (gs->map.base_count - 1) * 60;
}

void game_init_map(GameState *gs, ThemeID theme, int forced_bases) {
    game_init_map_full(gs, theme, forced_bases, 0, 10, 0, 0, 0);
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
    // Tire les biais de répartition ennemis propres à cette partie
    wave_arcade_bias_init(&gs->wave_manager);
    game_init_map(gs, theme, 0);   // 0 = nombre de bases aléatoire en arcade
    printf("Arcade slot=%d theme=%d\n", slot, (int)theme);
}

void game_init_campaign(GameState *gs, int campaign_num, int slot,
                        int seed, int start_stage) {
    if (start_stage < 0 || start_stage >= CAMPAIGN_TOTAL) start_stage = 0;

    game_state_init(gs);
    gs->is_campaign         = 1;
    gs->campaign_num        = campaign_num;
    gs->campaign_stage      = start_stage;
    gs->campaign_order_seed = seed;

    const ActData *act = campaign_act_get(start_stage);
    ThemeID theme = act->theme;
    game_init_map(gs, theme, act->forced_base_count);

    printf("Campagne %d acte %d slot=%d theme=%d bases=%d\n",
           campaign_num, start_stage, slot, (int)theme, act->forced_base_count);
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

    const ActData *act = campaign_act_get(camp_stage);
    ThemeID theme = act->theme;
    game_init_map(gs, theme, act->forced_base_count);

    printf("Campagne %d acte %d theme=%d bases=%d\n",
           camp_num, camp_stage, (int)theme, act->forced_base_count);
}

// ════════════════════════════════════════════════════
// PARTIE PERSONNALISÉE
// ════════════════════════════════════════════════════
void game_init_custom(GameState *gs, const CustomConfig *cfg) {
    game_state_init(gs);
    gs->is_campaign = 0;
    gs->is_custom   = 1;
    gs->is_endless  = 0;
    wave_arcade_bias_init(&gs->wave_manager);

    // Scalabilité personnalisée
    gs->wave_manager.scale_cap  = cfg->scale_cap  > 0.0f ? cfg->scale_cap  : 6.0f;
    gs->wave_manager.count_mult = cfg->count_mult > 0.0f ? cfg->count_mult : 1.0f;

    int fb = cfg->forced_bases  > 0 ? cfg->forced_bases  : 1;
    int fs = cfg->forced_spawns > 0 ? cfg->forced_spawns : 2;
    int md = cfg->min_dist      > 0 ? cfg->min_dist      : 10;
    int fd = cfg->forced_deposits;   // 0 = aléatoire
    int mw = cfg->map_w > 0 ? cfg->map_w : MAP_W;
    int mh = cfg->map_h > 0 ? cfg->map_h : MAP_H;

    game_init_map_full(gs, (ThemeID)cfg->theme, fb, fs, md, fd, mw, mh);
    printf("Custom theme=%d bases=%d spawns=%d min_dist=%d deposits=%d size=%dx%d cap=%.1f mult=%.1f\n",
           cfg->theme, fb, fs, md, fd, mw, mh,
           gs->wave_manager.scale_cap, gs->wave_manager.count_mult);
}