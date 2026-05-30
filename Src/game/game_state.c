/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "game_state.h"
#include "../engine/audio.h"
#include "../map/pathfinding.h"

/* ═══════════════════════════════════════════════════════════════
   HELPER INTERNE — redirige les chemins orphelins quand une base
   est détruite. Tous les spawns qui ciblaient destroyed_base_id
   sont redirigés vers la première base encore active.
   Les ennemis déjà en route sur ces chemins sont recalés sur le
   nouveau tracé (path_index clampé).
   ═══════════════════════════════════════════════════════════════ */
static void game_redirect_paths_for_base(GameState *gs, int destroyed_base_id) {
    /* Cherche la première base encore vivante comme cible de repli */
    int fallback_base = -1;
    for (int b = 0; b < gs->map.base_count; b++) {
        if (b != destroyed_base_id && gs->map.bases[b].active) {
            fallback_base = b;
            break;
        }
    }
    if (fallback_base < 0) return; /* Plus aucune base → game over imminent */

    Point new_base_pos = gs->map.bases[fallback_base].pos;

    /* ── Redirection des chemins ennemis ─────────────────────── */
    for (int p = 0; p < gs->enemy_paths.count; p++) {
        Path *path = &gs->enemy_paths.paths[p];
        if (!path->found || path->base_id != destroyed_base_id) continue;

        /* Met à jour la PathDef correspondante dans la carte */
        int pid = path->path_id; /* index dans map.paths[] */
        if (pid >= 0 && pid < MAX_PATHS) {
            gs->map.paths[pid].base_id = fallback_base;
            gs->map.paths[pid].base    = new_base_pos;
        }

        /* Recalcule le chemin A* depuis le même spawn vers la nouvelle base */
        Path rebuilt;
        astar_single(&gs->map,
                     path->steps[0],   /* spawn (premier pas) */
                     new_base_pos,
                     path->path_id,
                     fallback_base,
                     &rebuilt);

        if (!rebuilt.found) continue; /* pas de chemin → on laisse tel quel */

        /* Redirige les ennemis déjà actifs sur ce slot de chemin */
        for (int e = 0; e < MAX_ENEMIES; e++) {
            Enemy *en = &gs->enemies.enemies[e];
            if (!en->active || en->path_id != p) continue;
            /* Clamp path_index dans les limites du nouveau chemin */
            if (en->path_index >= rebuilt.len)
                en->path_index = rebuilt.len > 0 ? rebuilt.len - 1 : 0;
            /* Pathbreaker : recale sa cible en ligne droite vers la nouvelle base
               (sinon il continue de foncer vers les décombres de l'ancienne). */
            if (en->type == ENEMY_PATHBREAKER) {
                en->target_x = new_base_pos.x * TILE_SIZE + TILE_SIZE / 2.0f;
                en->target_y = new_base_pos.y * TILE_SIZE + TILE_SIZE / 2.0f;
            }
        }

        /* Écrase le chemin en place dans le PathSet */
        *path = rebuilt;
    }

    /* ── Redirection des unités alliées ──────────────────────── */
    /* Les unités ancrées sur la base détruite sont redirigées vers
       la base de repli : elles patrouillent et déposent les matériaux
       au bon endroit dès le frame suivant.                         */
    {
        float dead_px = (float)gs->map.bases[destroyed_base_id].pos.x * TILE_SIZE
                        + TILE_SIZE * 0.5f;
        float dead_py = (float)gs->map.bases[destroyed_base_id].pos.y * TILE_SIZE
                        + TILE_SIZE * 0.5f;
        float new_px  = (float)new_base_pos.x * TILE_SIZE + TILE_SIZE * 0.5f;
        float new_py  = (float)new_base_pos.y * TILE_SIZE + TILE_SIZE * 0.5f;

        /* Pool entier */
        if (gs->units.base_px == dead_px && gs->units.base_py == dead_py) {
            gs->units.base_px = new_px;
            gs->units.base_py = new_py;
        }

        /* Chaque unité active */
        for (int u = 0; u < MAX_UNITS; u++) {
            Unit *un = &gs->units.units[u];
            if (!un->active) continue;
            if (un->home_base_px == dead_px && un->home_base_py == dead_py) {
                un->home_base_px = new_px;
                un->home_base_py = new_py;
            }
        }
    }
}

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
        /* Redirige immédiatement tous les spawns qui ciblaient cette base
           vers la première base encore vivante, et recale les ennemis
           déjà en route sur les chemins reconstruits. */
        game_redirect_paths_for_base(gs, base_id);
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
    gs->dropped_mat_count       = 0;
    for (int i = 0; i < MAX_DROPPED_MATS; i++)
        gs->dropped_mats[i].active = 0;
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

    // Activation/désactivation du minage selon la phase
    gs->units.mining_enabled = (gs->phase == PHASE_WAVE) ? 1 : 0;

    // Snapshot ouvriers AVANT update (pour détecter morts avec matériau)
    typedef struct { int active; int has_mat; MaterialType mat; float x, y; } WSnap;
    WSnap wsnap[MAX_UNITS];
    for (int _w = 0; _w < MAX_UNITS; _w++) {
        const Unit *u = &gs->units.units[_w];
        wsnap[_w].active  = u->active;
        wsnap[_w].has_mat = (u->type == UNIT_WORKER) ? u->has_material : 0;
        wsnap[_w].mat     = u->carried_mat;
        wsnap[_w].x       = u->x;
        wsnap[_w].y       = u->y;
    }

    // Snapshot de l'état des bases AVANT l'update ennemis : permet de détecter
    // celles qui tombent ce frame (les ennemis les désactivent directement).
    int _base_was_active[MAX_BASES];
    for (int _b = 0; _b < gs->map.base_count && _b < MAX_BASES; _b++)
        _base_was_active[_b] = gs->map.bases[_b].active;

    // Mise à jour ennemis — passe towers pour Artillery
    enemy_pool_update(&gs->enemies, &gs->enemy_paths,
                      &gs->units, &gs->towers,
                      &gs->map,
                      effective_dt,
                      &gs->lives, &gs->gold, &gs->kills);

    // Une base est-elle tombée ce frame ? → redirige immédiatement les chemins
    // (et les ennemis déjà en route) vers une base encore vivante, pour ne pas
    // laisser les ennemis converger vers une base détruite pendant la vague.
    for (int _b = 0; _b < gs->map.base_count && _b < MAX_BASES; _b++) {
        if (_base_was_active[_b] && !gs->map.bases[_b].active)
            game_redirect_paths_for_base(gs, _b);
    }

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
    int _inv_before = gs->inventory_count;
    unit_pool_update(&gs->units, &gs->enemies, &gs->map,
                     effective_dt, gs->inventory, &gs->inventory_count,
                     &gs->towers);
    /* Récompense or pour chaque matériau livré ce frame */
    {
        int _delivered = gs->inventory_count - _inv_before;
        if (_delivered > 0)
            gs->gold += _delivered * 20;
    }

    // ── Ouvriers tués en portant → lâchent le matériau ───────────
    for (int _w = 0; _w < MAX_UNITS; _w++) {
        if (wsnap[_w].active && wsnap[_w].has_mat &&
            !gs->units.units[_w].active) {
            if (gs->dropped_mat_count < MAX_DROPPED_MATS) {
                DroppedMat *dm = &gs->dropped_mats[gs->dropped_mat_count++];
                dm->x        = wsnap[_w].x;
                dm->y        = wsnap[_w].y;
                dm->type     = wsnap[_w].mat;
                dm->active   = 1;
                dm->lifetime = 15.0f;
            }
        }
    }

    // ── Matériaux au sol : expiration et absorption par ennemis ──
    {
        for (int _d = 0; _d < gs->dropped_mat_count; _d++) {
            DroppedMat *dm = &gs->dropped_mats[_d];
            if (!dm->active) continue;
            dm->lifetime -= effective_dt;
            if (dm->lifetime <= 0.0f) { dm->active = 0; continue; }
            // Ennemi passe dessus ?
            for (int _e = 0; _e < MAX_ENEMIES; _e++) {
                const Enemy *en = &gs->enemies.enemies[_e];
                if (!en->active || en->dead || en->spawn_delay > 0.0f) continue;
                float dx = en->x - dm->x, dy = en->y - dm->y;
                if (dx*dx + dy*dy <= (float)(TILE_SIZE * TILE_SIZE)) {
                    dm->active = 0; // absorbé
                    break;
                }
            }
        }
        // Compactage
        int _w2 = 0;
        for (int _d = 0; _d < gs->dropped_mat_count; _d++)
            if (gs->dropped_mats[_d].active)
                gs->dropped_mats[_w2++] = gs->dropped_mats[_d];
        gs->dropped_mat_count = _w2;
    }

    // ── Activation des dépôts selon la vague ─────────────────────
    for (int _dep = 0; _dep < gs->map.deposit_count; _dep++) {
        MaterialDeposit *dep = &gs->map.deposits[_dep];
        if (!dep->active && dep->spawn_wave > 0 &&
            gs->wave_manager.number >= dep->spawn_wave) {
            dep->active = 1;
            ui_push_notif(&gs->ui, "Nouveau gisement accessible !",
                          (Color){62, 175, 200, 255});
        }
    }
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
                meta_save(&gs->meta);
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