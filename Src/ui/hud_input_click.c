/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_input_click.c ─ Clics gauches sur les panneaux du HUD.
 *
 *  Extrait de hud_input.c (règle projet : fichiers < 500 lignes) ; le code
 *  est inchangé, « goto end_click » y devient « return 1 » (clic consommé).
 */

#include "hud_internal.h"

/* Démarrage du glisser d'un overlay (prioritaire). Retourne 1 si le clic est consommé. */
int hud_click_overlay_drag(UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y) {
    (void)ui; (void)gs; (void)mouse; (void)HUD_Y;
    // Démarrage du drag sur un overlay (priorité sur tout autre clic)
    {
        Rectangle tr_r = {ui->overlay_tr_pos.x, ui->overlay_tr_pos.y,
                           OVERLAY_W, OVERLAY_TR_H};
        if (CheckCollisionPointRec(mouse, tr_r)) {
            // Bouton vitesse (coin bas-droit de l'overlay) : intercepté avant
            // le glissement. Rectangle partagé avec le rendu.
            if (CheckCollisionPointRec(mouse, hud_speed_btn_rect(ui))) {
                ui->speed_mult = (ui->speed_mult % 3) + 1;
                audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                return 1;
            }
            ui->dragging_overlay = 1;
            ui->drag_grab = (Vector2){mouse.x - tr_r.x, mouse.y - tr_r.y};
            return 1;
        }
        /* ── Overlay BL : bases HP + réparation ─────────────── */
        if (ui->overlay_bl_pos.x >= 0.0f && hud_show_bases_panel(gs)) {
            int _bh_bl = overlay_bl_h(gs);
            Rectangle bl_r = {ui->overlay_bl_pos.x, ui->overlay_bl_pos.y,
                              OVERLAY_BL_W, (float)_bh_bl};
            if (CheckCollisionPointRec(mouse, bl_r)) {
                /* Vérifie d'abord les boutons de réparation à l'intérieur */
                for (int _b = 0; _b < gs->map.base_count && _b < MAX_BASES; _b++) {
                    if (ui->repair_base_btn[_b].width <= 0.0f) continue;
                    if (!CheckCollisionPointRec(mouse, ui->repair_base_btn[_b])) continue;
                    BaseInfo *_base = &gs->map.bases[_b];
                    if (_base->active && _base->hp > 0 && _base->hp < _base->max_hp) {
                        int _cost = base_repair_cost(_base->repair_count);
                        if (gs->gold >= _cost) {
                            gs->gold -= _cost;
                            int _restore = BASE_REPAIR_RESTORE;
                            if (_base->hp + _restore > _base->max_hp)
                                _restore = _base->max_hp - _base->hp;
                            _base->hp += _restore;
                            gs->lives += _restore;
                            _base->repair_count++;
                            audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
                            char _nbuf[48];
                            snprintf(_nbuf, sizeof(_nbuf), "+%d PV (base réparée)", _restore);
                            ui_push_notif(ui, _nbuf, (Color){46, 204, 113, 255});
                        } else {
                            ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
                        }
                    }
                    return 1;
                }
                /* Clic hors bouton → déplacement du panneau */
                ui->dragging_overlay = 2;
                ui->drag_grab = (Vector2){mouse.x - bl_r.x, mouse.y - bl_r.y};
                return 1;
            }
        }
    }
    return 0;
}

/* Boutons de comportement de l'unité sélectionnée. Retourne 1 si le clic est consommé. */
int hud_click_unit_panel(UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y) {
    (void)ui; (void)gs; (void)mouse; (void)HUD_Y;
    /* ── Comportement de l'unité sélectionnée ─────────────── */
    if (ui->sell_unit_idx >= 0) {
        Unit *_cu = &gs->units.units[ui->sell_unit_idx];
        if (_cu->active && _cu->type != UNIT_WORKER) {
            static const UnitBehavior BEH_MAP[4] = {
                UBEH_PATROL, UBEH_GUARD_TOWER, UBEH_ESCORT_WORKER, UBEH_MANUAL
            };
            for (int _b = 0; _b < 4; _b++) {
                if (!CheckCollisionPointRec(mouse, ui->unit_beh_btns[_b])) continue;
                UnitBehavior nb = BEH_MAP[_b];
                if (nb == UBEH_PATROL) {
                    _cu->behavior        = UBEH_PATROL;
                    _cu->guard_tower_idx = -1;
                    _cu->escort_idx      = -1;
                    _cu->manual_moving   = 0;
                    ui->behavior_pending      = -1;
                    ui->behavior_pending_unit = -1;
                    audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                } else if (nb == UBEH_MANUAL) {
                    _cu->behavior      = UBEH_MANUAL;
                    _cu->manual_x      = _cu->x;
                    _cu->manual_y      = _cu->y;
                    _cu->manual_moving = 0;
                    ui->behavior_pending      = -1;
                    ui->behavior_pending_unit = -1;
                    ui_push_notif(ui, "Clic sur la carte pour déplacer",
                                  (Color){82, 155, 200, 255});
                    audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                } else {
                    // GUARD ou ESCORT : en attente de clic de cible
                    // NE PAS changer le behavior ici — on attend la confirmation de la cible
                    ui->behavior_pending      = (int)nb;
                    ui->behavior_pending_unit = ui->sell_unit_idx;
                    if (nb == UBEH_GUARD_TOWER)
                        ui_push_notif(ui, "Cliquez sur une tourelle",
                                      (Color){192, 57, 43, 255});
                    else
                        ui_push_notif(ui, "Cliquez sur un ouvrier",
                                      (Color){200, 200, 50, 255});
                    audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                }
                return 1;
            }
            /* 5e bouton : SUIVRE (médic uniquement) */
            if (_cu->type == UNIT_MEDIC &&
                CheckCollisionPointRec(mouse, ui->unit_beh_btns[4])) {
                ui->behavior_pending      = (int)UBEH_FOLLOW_UNIT;
                ui->behavior_pending_unit = ui->sell_unit_idx;
                ui_push_notif(ui, "Cliquez sur une unité",
                              (Color){231, 76, 60, 255});
                audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                return 1;
            }
        }
    }
    return 0;
}

/* Une amélioration (coût < 0 = niveau max). */
static void tower_upgrade_try(UIState *ui, GameState *gs, Tower *tw, int cost,
                              void (*apply)(Tower *), const int *lvl,
                              const char *what, const char *gain, Color col) {
    char nb[64];
    if (cost < 0) {
        snprintf(nb, sizeof(nb), "%s : niveau max !", what);
        ui_push_notif(ui, nb, (Color){130, 90, 40, 255});
    } else if (gs->gold >= cost) {
        gs->gold -= cost;
        apply(tw);
        snprintf(nb, sizeof(nb), "%s %s (niv %d/%d)", what, gain, *lvl, TOWER_UPG_MAX);
        ui_push_notif(ui, nb, col);
    } else {
        ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
    }
}

int hud_tower_action(UIState *ui, GameState *gs, int idx, TowerAction a) {
    if (idx < 0 || idx >= MAX_TOWERS || !gs->towers.towers[idx].active) return 0;
    Tower *tw = &gs->towers.towers[idx];
    switch (a) {
    case TACT_DMG:
        tower_upgrade_try(ui, gs, tw, tower_upg_next_cost_dmg(tw), tower_upgrade_dmg,
                          &tw->upg_dmg, "Dégâts", "+10%", (Color){231, 100, 60, 255});
        break;
    case TACT_RANGE:
        tower_upgrade_try(ui, gs, tw, tower_upg_next_cost_range(tw), tower_upgrade_range,
                          &tw->upg_range, "Portée", "+10%", (Color){82, 155, 200, 255});
        break;
    case TACT_RATE:
        tower_upgrade_try(ui, gs, tw, tower_upg_next_cost_rate(tw), tower_upgrade_rate,
                          &tw->upg_rate, "Cadence", "+20%", (Color){155, 89, 182, 255});
        break;
    case TACT_REPAIR:
        if (tw->hp >= TOWER_MAX_HP) {
            ui_push_notif(ui, "Tour déjà intacte !", (Color){130, 90, 40, 255});
        } else if (gs->gold >= TOWER_REPAIR_COST) {
            gs->gold -= TOWER_REPAIR_COST;
            tower_do_repair(tw);
            ui_push_notif(ui, "Tour réparée !", (Color){46, 204, 113, 255});
        } else {
            ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
        }
        break;
    case TACT_TARGET: {
        tw->target_mode = (tw->target_mode + 1) % TGT_COUNT;
        char nb[48];
        snprintf(nb, sizeof(nb), "Cible : %s", TARGET_MODE_NAMES[tw->target_mode]);
        ui_push_notif(ui, nb, (Color){210, 190, 140, 255});
        break;
    }
    case TACT_SELL: {
        int fresh  = tower_is_fresh(&gs->towers, idx);
        int refund = tower_refund(&gs->towers, idx, &gs->map);   // pose + améliorations
        gs->gold += refund;
        // Rend le MINERAI appliqué à l'inventaire (s'il reste de la place).
        if (tw->material != MAT_NONE && gs->inventory_count < MAX_INVENTORY) {
            gs->inventory[gs->inventory_count++] = tw->material;
            ui_push_notif(ui, "Minerai récupéré", (Color){62, 175, 200, 255});
        }
        gs->map.tiles[tw->tile_y][tw->tile_x].buildable = 1;
        tw->active = 0;
        gs->towers.tower_count--;
        char nb[48];
        snprintf(nb, sizeof(nb), fresh ? "Pose annulée : +%d or" : "+%d or récupérés", refund);
        ui_push_notif(ui, nb, (Color){230, 155, 35, 255});
        if (ui->selection.tower_idx == idx) ui->selection.active = 0;
        break;
    }
    }
    audio_play_sfx(AUDIO_SFX_MENU_CLICK);
    return 1;
}

/* Boutons de la tour sélectionnée. Retourne 1 si le clic est consommé. */
int hud_click_tower_panel(UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y) {
    (void)HUD_Y;
    if (!ui->selection.active) return 0;
    const Rectangle *r[] = {&ui->upg_dmg_btn, &ui->upg_range_btn, &ui->upg_rate_btn,
                            &ui->repair_tower_btn, &ui->target_btn};
    const TowerAction a[] = {TACT_DMG, TACT_RANGE, TACT_RATE, TACT_REPAIR, TACT_TARGET};
    for (int k = 0; k < (int)(sizeof(a) / sizeof(a[0])); k++)
        if (CheckCollisionPointRec(mouse, *r[k]))
            return hud_tower_action(ui, gs, ui->selection.tower_idx, a[k]) || 1;
    return 0;
}

/* Achat de slots supplémentaires (tours / unités). Retourne 1 si le clic est consommé. */
int hud_click_slot_buys(UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y) {
    (void)ui; (void)gs; (void)mouse; (void)HUD_Y;
    /* ── Achat de slots supplémentaires ───────────────────── */
    if (hud_show_slot_buys(gs) &&
        CheckCollisionPointRec(mouse, ui->buy_tower_slot_btn)) {
        int _bought = gs->slots_tower_bought;
        int _base_lim = tower_active_limit(&gs->bonuses);
        int _max_extra = MAX_TOWERS_HARD - _base_lim;
        if (_bought >= SLOT_MAX_BUYS || _bought >= _max_extra) {
            ui_push_notif(ui, "Limite de tours atteinte !", (Color){130, 90, 40, 255});
        } else {
            int _cost = SLOT_TOWER_COSTS[_bought];
            if (gs->gold >= _cost) {
                gs->gold -= _cost;
                gs->slots_tower_bought++;
                char _nb[48];
                snprintf(_nb, sizeof(_nb), "+1 slot tour (limite : %d)",
                         gs->towers.tower_limit + 1);
                ui_push_notif(ui, _nb, (Color){148, 128, 95, 255});
            } else {
                ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
            }
        }
        return 1;
    }
    if (hud_show_slot_buys(gs) &&
        CheckCollisionPointRec(mouse, ui->buy_unit_slot_btn)) {
        int _bought = gs->slots_unit_bought;
        int _max_extra = MAX_UNITS - unit_active_limit(&gs->bonuses, gs->map.base_count);
        if (_bought >= SLOT_MAX_BUYS || _bought >= _max_extra) {
            ui_push_notif(ui, "Limite d'unités atteinte !", (Color){130, 90, 40, 255});
        } else {
            int _cost = SLOT_UNIT_COSTS[_bought];
            if (gs->gold >= _cost) {
                gs->gold -= _cost;
                gs->slots_unit_bought++;
                char _nb[48];
                snprintf(_nb, sizeof(_nb), "+1 slot unité (limite : %d)",
                         gs->units.unit_limit + 1);
                ui_push_notif(ui, _nb, (Color){39, 174, 96, 255});
            } else {
                ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
            }
        }
        return 1;
    }
    return 0;
}
