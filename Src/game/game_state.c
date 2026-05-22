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
    gs->slots_tower_bought      = 0;
    gs->slots_unit_bought       = 0;
}

void game_state_update(GameState *gs, float dt) {
    // Mise à jour des limites actives (méta + slots achetés en jeu)
    {
        int tl = tower_active_limit(&gs->bonuses) + gs->slots_tower_bought;
        if (tl > MAX_TOWERS_HARD) tl = MAX_TOWERS_HARD;
        gs->towers.tower_limit = tl;

        int ul = unit_active_limit(&gs->bonuses, gs->map.base_count) + gs->slots_unit_bought;
        if (ul > MAX_UNITS) ul = MAX_UNITS;
        gs->units.unit_limit = ul;
    }

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
    {
        // Campagne : on passe l'acte courant-1 comme seuil de déblocage.
        // Acte 0 → max_stage=-1 (seul RAIDER), acte 1 → 0 (BRUTE arrive), etc.
        // Cela garantit une introduction progressive à chaque nouvelle run,
        // indépendamment de la progression méta globale du joueur.
        // Arcade : max_stage ignoré (pas de filtre, biais aléatoires utilisés).
        int max_stage = gs->is_campaign
            ? (gs->campaign_stage - 1)
            : meta_max_stage_completed(&gs->meta);
        wave_update(&gs->wave_manager, &gs->enemies,
                    &gs->enemy_paths, &gs->map, th, effective_dt,
                    gs->is_campaign, max_stage);
    }

    // ── Découverte bestiaire & minerais — campagne uniquement ────
    // En arcade rien n'est révélé : tout se débloque en campagne.
    if (gs->is_campaign) {
        for (int _bi = 0; _bi < MAX_ENEMIES; _bi++) {
            const Enemy *_be = &gs->enemies.enemies[_bi];
            if (_be->active && _be->spawn_delay <= 0.0f &&
                !gs->meta.bestiary_discovered[(int)_be->type]) {
                gs->meta.bestiary_discovered[(int)_be->type] = 1;
                ui_disc_push(&gs->ui, DISC_ENEMY, (int)_be->type);
            }
        }
        // Nouveaux matériaux livrés ce frame → révélation dans le bestiaire
        for (int _mi = inv_cnt_before; _mi < gs->inventory_count; _mi++) {
            MaterialType _mt = gs->inventory[_mi];
            if (_mt >= 0 && _mt < MAT_COUNT)
                gs->meta.material_discovered[(int)_mt] = 1;
        }
    }

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