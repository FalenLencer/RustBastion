/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_input_map.c ─ Clic gauche sur la carte : pose, sélection, ordres.
 *
 *  Extrait de hud_input.c (règle projet : fichiers < 500 lignes) ; le code
 *  est inchangé, « goto end_click » y devient « return 1 » (clic consommé).
 */

#include "hud_internal.h"

// Efface la sélection de groupe courante.
static void group_clear(UIState *ui) {
    if (ui->group_count == 0) return;
    for (int j = 0; j < MAX_UNITS; j++) ui->group_sel[j] = 0;
    ui->group_count = 0;
}

/* Clic gauche sur une tuile de la carte. Retourne 1 si le clic est
   consommé ; 0 = carte vide (la sélection de groupe a pu être amorcée). */
int hud_click_map(UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y) {
    (void)ui; (void)gs; (void)mouse; (void)HUD_Y;
    int tx = ui->hovered_tile_x;
    int ty = ui->hovered_tile_y;
    float wx_m = map_screen_to_world(mouse).x;
    float wy_m = map_screen_to_world(mouse).y;

    // ── Attribution d'une cible de comportement en attente ───
    if (ui->behavior_pending == (int)UBEH_GUARD_TOWER) {
        int punit = ui->behavior_pending_unit;
        int found = 0;
        for (int _t = 0; _t < MAX_TOWERS && !found; _t++) {
            Tower *_tw = &gs->towers.towers[_t];
            if (!_tw->active) continue;
            if (_tw->tile_x == tx && _tw->tile_y == ty) {
                if (punit >= 0 && punit < MAX_UNITS &&
                    gs->units.units[punit].active) {
                    gs->units.units[punit].behavior        = UBEH_GUARD_TOWER;
                    gs->units.units[punit].guard_tower_idx = _t;
                    ui_push_notif(ui, "Unité en garde de tourelle",
                                  (Color){192, 57, 43, 255});
                }
                found = 1;
            }
        }
        if (!found) {
            // Clic raté → revenir en patrouille
            if (punit >= 0 && punit < MAX_UNITS &&
                gs->units.units[punit].active) {
                gs->units.units[punit].behavior        = UBEH_PATROL;
                gs->units.units[punit].guard_tower_idx = -1;
            }
        }
        ui->behavior_pending      = -1;
        ui->behavior_pending_unit = -1;
        return 1;
    }
    if (ui->behavior_pending == (int)UBEH_ESCORT_WORKER) {
        int punit = ui->behavior_pending_unit;
        int found = 0;
        for (int _j = 0; _j < MAX_UNITS && !found; _j++) {
            Unit *_wu = &gs->units.units[_j];
            if (!_wu->active || _wu->type != UNIT_WORKER) continue;
            float _dx = wx_m - _wu->x, _dy = wy_m - _wu->y;
            if (_dx*_dx + _dy*_dy <= (_wu->size+8.0f)*(_wu->size+8.0f)) {
                if (punit >= 0 && punit < MAX_UNITS &&
                    gs->units.units[punit].active) {
                    gs->units.units[punit].behavior   = UBEH_ESCORT_WORKER;
                    gs->units.units[punit].escort_idx = _j;
                    ui_push_notif(ui, "Unité en escorte d'ouvrier",
                                  (Color){200, 200, 50, 255});
                }
                found = 1;
            }
        }
        if (!found) {
            if (punit >= 0 && punit < MAX_UNITS &&
                gs->units.units[punit].active) {
                gs->units.units[punit].behavior   = UBEH_PATROL;
                gs->units.units[punit].escort_idx = -1;
            }
        }
        ui->behavior_pending      = -1;
        ui->behavior_pending_unit = -1;
        return 1;
    }

    if (ui->behavior_pending == (int)UBEH_FOLLOW_UNIT) {
        int punit = ui->behavior_pending_unit;
        int found = 0;
        for (int _j = 0; _j < MAX_UNITS && !found; _j++) {
            Unit *_wu = &gs->units.units[_j];
            if (!_wu->active || _j == punit) continue; // pas soi-même
            float _dx = wx_m - _wu->x, _dy = wy_m - _wu->y;
            if (_dx*_dx + _dy*_dy <= (_wu->size+8.0f)*(_wu->size+8.0f)) {
                if (punit >= 0 && punit < MAX_UNITS &&
                    gs->units.units[punit].active) {
                    gs->units.units[punit].behavior   = UBEH_FOLLOW_UNIT;
                    gs->units.units[punit].escort_idx = _j;
                    ui_push_notif(ui, "Medic en suivi d'unité",
                                  (Color){231, 76, 60, 255});
                }
                found = 1;
            }
        }
        if (!found) {
            if (punit >= 0 && punit < MAX_UNITS &&
                gs->units.units[punit].active)
                gs->units.units[punit].behavior = UBEH_PATROL;
        }
        ui->behavior_pending      = -1;
        ui->behavior_pending_unit = -1;
        return 1;
    }

    // ── Destination manuelle pour une unité MANUEL sélectionnée ─
    if (ui->sell_unit_idx >= 0) {
        Unit *_mu = &gs->units.units[ui->sell_unit_idx];
        if (_mu->active && _mu->type != UNIT_WORKER &&
            _mu->behavior == UBEH_MANUAL) {
            // Vérifier que la destination n'est pas sur une zone de spawn
            int in_spawn = 0;
            for (int _sp = 0; _sp < gs->map.path_count; _sp++) {
                const PathDef *_pd = &gs->map.paths[_sp];
                int spx = _pd->spawn.x, spy = _pd->spawn.y;
                int _dx = tx - spx, _dy = ty - spy;
                if (_dx*_dx + _dy*_dy <= SPAWN_EXCLUSION_RADIUS*SPAWN_EXCLUSION_RADIUS) {
                    in_spawn = 1; break;
                }
            }
            if (!in_spawn) {
                /* Destination ramenée sur une case atteignable
                   (clic sur un mur → point libre le plus proche,
                   comme un clic minicarte dans un MOBA). */
                float _fx = wx_m, _fy = wy_m;
                move_free_spot(&gs->map, &gs->towers, wx_m, wy_m,
                               _mu->size, MOVE_F_ALLY, &_fx, &_fy);
                _mu->manual_x      = _fx;
                _mu->manual_y      = _fy;
                _mu->manual_moving = 1;
                return 1;
            } else {
                ui_push_notif(ui, "Zone de spawn interdite !",
                              (Color){231, 76, 60, 255});
                return 1;
            }
        }
    }

    // Ouvrier sélectionné → dépôt, ou DÉBLAIEMENT d'un obstacle
    if (ui->worker_selected_idx >= 0) {
        int assigned = 0, locked_dep = 0;
        for (int d = 0; d < gs->map.deposit_count; d++) {
            MaterialDeposit *dep = &gs->map.deposits[d];
            if (dep->tile_x != tx || dep->tile_y != ty) continue;
            if (dep->active) {
                unit_assign_deposit(&gs->units,
                                    ui->worker_selected_idx, d);
                ui->worker_selected_idx = -1;
                gs->units.selected_unit = -1;
                assigned = 1;
            } else if (!dep->mined && dep->spawn_wave > 0) {
                /* Filon pas encore ouvert : on le DIT. Avant, le clic
                   tombait sur la ruine en dessous → déblaiement payant
                   silencieux au lieu du minage attendu. */
                ui_push_notif(ui, TextFormat("Gisement fermé : s'ouvre à la vague %d",
                                             dep->spawn_wave),
                              (Color){243, 156, 18, 255});
                locked_dep = 1;
            }
            break;
        }
        /* Pas un gisement : une ruine ou une roche minée sur la case ?
           L'ouvrier peut la déblayer — le terrain redevient
           franchissable ET constructible. */
        int keep_sel = locked_dep;   // filon fermé : l'ouvrier reste sélectionné
        if (!assigned && !locked_dep &&
            unit_tile_clearable(&gs->map, &gs->towers, tx, ty)) {
            char cbuf[52];
            if (gs->gold >= UNIT_WORKER_CLEAR_COST) {
                gs->gold -= UNIT_WORKER_CLEAR_COST;
                unit_assign_clear(&gs->units,
                                  ui->worker_selected_idx, tx, ty);
                snprintf(cbuf, sizeof(cbuf),
                         "Déblaiement en cours (-%d or)",
                         UNIT_WORKER_CLEAR_COST);
                ui_push_notif(ui, cbuf, (Color){168, 148, 102, 255});
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            } else {
                snprintf(cbuf, sizeof(cbuf),
                         "Déblaiement : %d or requis !",
                         UNIT_WORKER_CLEAR_COST);
                ui_push_notif(ui, cbuf, (Color){243, 156, 18, 255});
                /* Refus pour or insuffisant : on GARDE l'ouvrier
                   sélectionné, le joueur doit pouvoir réessayer
                   sans avoir à le re-cliquer. */
                keep_sel = 1;
            }
        }
        /* Ordre donné OU clic dans le vide → on désélectionne ;
           seul un refus pour or insuffisant conserve la sélection. */
        if (!keep_sel) {
            ui->worker_selected_idx = -1;
            gs->units.selected_unit = -1;
        }
    }

    // Tour existante ?
    int clicked_tower = -1;
    for (int i = 0; i < MAX_TOWERS; i++) {
        Tower *tw = &gs->towers.towers[i];
        if (!tw->active) continue;
        if (tw->tile_x == tx && tw->tile_y == ty) {
            clicked_tower = i; break;
        }
    }

    if (clicked_tower != -1) {
        ui->selection.active    = 1;
        ui->selection.tower_idx = clicked_tower;
        ui->selected_tool       = TOOL_NONE;
        ui->worker_selected_idx = -1;
        gs->units.selected_unit = -1;
        group_clear(ui);
    } else if (ui->selected_tool != TOOL_NONE) {
        if (ui_tool_is_tower(ui->selected_tool)) {
            if (gs->towers.tower_count < gs->towers.tower_limit) {
                TowerType _tt = ui_tool_to_tower(ui->selected_tool);
                if (tower_place(&gs->towers, _tt, tx, ty,
                                &gs->map, &gs->gold, &gs->bonuses))
                {
                    audio_play_sfx(AUDIO_SFX_TOWER_PLACE_GUN + (int)_tt);
                    if (gs->is_campaign &&
                        !gs->meta.tower_discovered[(int)_tt]) {
                        gs->meta.tower_discovered[(int)_tt] = 1;
                        meta_save(&gs->meta);
                        ui_disc_push(ui, DISC_TOWER, (int)_tt);
                    }
                } else {
                    /* Un clic qui ne fait RIEN est la pire réponse
                       possible : on dit pourquoi. Ordre = celui des
                       refus dans tower_place (tuile, puis or). */
                    char _fb[52];
                    switch (tower_place_fail(&gs->towers, &gs->map,
                                             tx, ty)) {
                        case TPF_SPAWN:
                            ui_push_notif(ui,
                                "Trop près d'un point d'apparition !",
                                (Color){243, 156, 18, 255});
                            break;
                        case TPF_OCCUPIED:
                            ui_push_notif(ui, "Case déjà occupée !",
                                (Color){243, 156, 18, 255});
                            break;
                        case TPF_TILE:
                            ui_push_notif(ui,
                                "Terrain non constructible ici",
                                (Color){243, 156, 18, 255});
                            break;
                        default:   /* tuile OK → il manque l'or */
                            snprintf(_fb, sizeof(_fb),
                                "Or insuffisant : %d or requis",
                                tower_cost_on_tile(_tt, &gs->map,
                                                   tx, ty));
                            ui_push_notif(ui, _fb,
                                (Color){243, 156, 18, 255});
                            break;
                    }
                }
            } else {
                ui_push_notif(ui, "Limite de tours atteinte !",
                              (Color){231, 76, 60, 255});
            }
        } else if (ui_tool_is_unit(ui->selected_tool)) {
            if (gs->units.count >= gs->units.unit_limit) {
                ui_push_notif(ui, "Limite d'unités atteinte !",
                              (Color){231, 76, 60, 255});
            } else {
                // Cherche la base active la plus proche du clic dans le rayon
                int   near_any_base = 0;
                float spawn_bpx = gs->units.base_px;
                float spawn_bpy = gs->units.base_py;
                float deploy_r  = UNIT_DEPLOY_RADIUS_TILES * TILE_SIZE;
                float best_dist = deploy_r + 1.0f; // seuil + 1 pour init
                for (int b = 0; b < gs->map.base_count; b++) {
                    if (!gs->map.bases[b].active) continue;
                    float bpx = gs->map.bases[b].pos.x * TILE_SIZE
                                + TILE_SIZE / 2.0f;
                    float bpy = gs->map.bases[b].pos.y * TILE_SIZE
                                + TILE_SIZE / 2.0f;
                    /* Convertit la souris en espace monde (zoom carte) */
                    float wx = map_screen_to_world(mouse).x;
                    float wy = map_screen_to_world(mouse).y;
                    float dx = wx - bpx;
                    float dy = wy - bpy;
                    float dist = sqrtf(dx*dx + dy*dy);
                    if (dist <= deploy_r && dist < best_dist) {
                        best_dist     = dist;
                        spawn_bpx     = bpx;
                        spawn_bpy     = bpy;
                        near_any_base = 1;
                    }
                }
                if (near_any_base) {
                    UnitType _put = ui_tool_to_unit(ui->selected_tool);

                    /* Limite médics par base — vérifiée avant l'appel
                       pour afficher une notification spécifique.       */
                    if (_put == UNIT_MEDIC) {
                        int _mc = unit_medic_count_at_base(
                                      &gs->units, spawn_bpx, spawn_bpy);
                        if (_mc >= MAX_MEDICS_PER_BASE) {
                            ui_push_notif(ui, "Max 4 medics par base !",
                                          (Color){231, 76, 60, 255});
                            return 1;
                        }
                    }

                    if (!unit_spawn_at(&gs->units, _put,
                                       &gs->gold, &gs->bonuses,
                                       spawn_bpx, spawn_bpy)) {
                        ui_push_notif(ui, "Or insuffisant !",
                                      (Color){243, 156, 18, 255});
                    } else if (gs->is_campaign &&
                               !gs->meta.unit_discovered[(int)_put]) {
                        gs->meta.unit_discovered[(int)_put] = 1;
                        meta_save(&gs->meta);
                        ui_disc_push(ui, DISC_UNIT, (int)_put);
                    }
                } else {
                    ui_push_notif(ui, "Spawn près d'une base !",
                                  (Color){243, 156, 18, 255});
                }
            }
        }
    } else {
        // Unité cliquée ? (un clic simple remplace toute sélection de groupe)
        ui->sell_unit_idx       = -1;
        ui->worker_selected_idx = -1;
        gs->units.selected_unit = -1;
        group_clear(ui);
        for (int j = 0; j < MAX_UNITS; j++) {
            Unit *u = &gs->units.units[j];
            if (!u->active) continue;
            float wx = map_screen_to_world(mouse).x;
            float wy = map_screen_to_world(mouse).y;
            float dx = wx - u->x;
            float dy = wy - u->y;
            if (sqrtf(dx*dx + dy*dy) <= u->size + 6.0f) {
                gs->units.selected_unit  = j;
                ui->sell_unit_idx        = j;
                ui->selection.active     = 0;
                if (u->type == UNIT_WORKER)
                    ui->worker_selected_idx = j;
                break;
            }
        }
        /* Carte vide (aucune unité/tour/outil) → amorce une sélection
           de groupe par glisser. Ce chemin n'est atteint qu'une fois
           tous les éléments d'UI écartés (ils font goto end_click), donc
           aucun faux départ possible. Validée à la relâche (bloc plus
           haut) ; un clic sans glisser ne fait que désélectionner. */
        if (gs->units.selected_unit < 0) {
            ui->box_dragging = 1;
            ui->box_start    = mouse;
            ui->box_cur      = mouse;
        }
    }
    return 0;
}
