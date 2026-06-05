/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_input.c ─ Gestion des entrées du HUD en jeu.
 *
 *  Contient :
 *    ui_update  — Traitement souris / clavier par frame
 *                 (placement tours/unités, sélection, améliorations,
 *                  réparations, overlays déplaçables, touches raccourcis)
 */

#include "hud_internal.h"

// Efface la sélection de groupe courante.
static void group_clear(UIState *ui) {
    if (ui->group_count == 0) return;
    for (int j = 0; j < MAX_UNITS; j++) ui->group_sel[j] = 0;
    ui->group_count = 0;
}

// ════════════════════════════════════════════════════
// MISE À JOUR
// ════════════════════════════════════════════════════
void ui_update(UIState *ui, GameState *gs) {
    Vector2   mouse = virt_mouse();
    const int HUD_Y = g_canvas_virt_h - UI_HUD_HEIGHT;

    // ── Fiche de découverte — fermeture prioritaire ──────────────
    if (ui->disc_count > 0) {
        int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;
        int cw = 540, ch = 400;
        int card_x = cx - cw/2, card_y = cy - ch/2;
        // Bouton [✕]
        Rectangle xbtn = {(float)(card_x + cw - 10 - 28),
                          (float)(card_y + 10), 28.0f, 22.0f};
        // Bouton CONTINUER
        Rectangle cont = {(float)(cx - 80),
                          (float)(card_y + ch - 14 - 34), 160.0f, 34.0f};
        int dismiss = IsKeyPressed(KEY_SPACE)
                   || IsKeyPressed(KEY_ENTER)
                   || (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                       (CheckCollisionPointRec(mouse, xbtn) ||
                        CheckCollisionPointRec(mouse, cont)));
        if (dismiss) {
            disc_pop(ui);
            audio_play_sfx(AUDIO_SFX_MENU_CLICK);
        }
        return;  // bloque toute autre entrée pendant l'affichage de la fiche
    }

    // Nettoyage unité morte : désélectionner si l'unité n'est plus active
    if (ui->sell_unit_idx >= 0) {
        const Unit *su = &gs->units.units[ui->sell_unit_idx];
        if (!su->active) {
            ui->sell_unit_idx       = -1;
            ui->worker_selected_idx = -1;
            gs->units.selected_unit = -1;
        }
    }
    if (ui->worker_selected_idx >= 0) {
        const Unit *wu = &gs->units.units[ui->worker_selected_idx];
        if (!wu->active) {
            ui->worker_selected_idx = -1;
            gs->units.selected_unit = -1;
        }
    }
    // Groupe : retire les unités mortes et recompte
    if (ui->group_count > 0) {
        int gc = 0;
        for (int j = 0; j < MAX_UNITS; j++) {
            if (!ui->group_sel[j]) continue;
            if (!gs->units.units[j].active) { ui->group_sel[j] = 0; continue; }
            gc++;
        }
        ui->group_count = gc;
    }

    // Hover outil
    ui->hovered_tool = -1;
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (CheckCollisionPointRec(mouse, ui->tool_btns[i])) {
            ui->hovered_tool = i;
            break;
        }
    }

    // Hover tuile (tient compte du décalage horizontal et du zoom de carte)
    {
        int map_left  = g_map_x_off;
        int map_right = g_map_x_off + g_canvas_virt_w_base;
        if (mouse.y >= 0 && mouse.y < HUD_Y &&
            mouse.x >= map_left && mouse.x < map_right) {
            float eff_tile = TILE_SIZE * g_map_render_scale;
            ui->hovered_tile_x = (int)((mouse.x - map_left) / eff_tile);
            ui->hovered_tile_y = (int)(mouse.y / eff_tile);
        } else {
            ui->hovered_tile_x = -1;
            ui->hovered_tile_y = -1;
        }
    }

    // ── Repositionnement dynamique des boutons centraux (tours/unités/vague) ──
    // Centrés dans la zone entre le panneau gauche et le panneau droit
    // pour s'adapter à toutes les largeurs de fenêtre.
    {
        const int M    = UI_MARGIN;
        const int GAP  = 6;

        int mid_left  = UI_LEFT_PANEL_W;
        int mid_right = g_canvas_virt_w - UI_PANEL_W;
        int mid_w     = mid_right - mid_left;

        /* Empreinte totale : 5 boutons + M + bouton vague + M + pause */
        const int BTNS_SPAN = 5*(UI_BTN_W + GAP) + M + 108 + M + 36;

        int col0_x = mid_left + (mid_w - BTNS_SPAN) / 2;
        if (col0_x < mid_left + M) col0_x = mid_left + M;

        int row1_y = HUD_Y + 18;
        int row2_y = row1_y + UI_BTN_H + GAP + 14;

        for (int i = 0; i < 4; i++)
            ui->tool_btns[TOOL_TOWER_GUN + i] = (Rectangle){
                (float)(col0_x + i*(UI_BTN_W+GAP)),
                (float)row1_y, UI_BTN_W, UI_BTN_H
            };
        for (int i = 0; i < 5; i++)
            ui->tool_btns[TOOL_UNIT_SOLDIER + i] = (Rectangle){
                (float)(col0_x + i*(UI_BTN_W+GAP)),
                (float)row2_y, UI_BTN_W, UI_BTN_H
            };

        int wave_x = col0_x + 5*(UI_BTN_W + GAP) + M;
        int wave_h  = UI_BTN_H * 2 + GAP + 14;
        ui->wave_btn  = (Rectangle){(float)wave_x, (float)row1_y, 108.0f, (float)wave_h};
        ui->pause_btn = (Rectangle){(float)(wave_x + 108 + M), (float)row1_y,
                                    36.0f, (float)wave_h};
    }

    // ── Repositionnement dynamique des boutons du panneau droit ──
    {
        const int M       = UI_MARGIN;
        const int GAP     = 6;
        const int right_x = g_canvas_virt_w - UI_PANEL_W + M;
        const int max_w   = UI_PANEL_W - M * 2;
        const int ubh     = 38;   /* hauteur bouton upgrade (3 lignes visibles) */
        const int ubw     = (max_w - GAP * 2) / 3;
        const int rsh     = (max_w - GAP) / 2;
        const int btn_h   = 34;

        /* Boutons d'amélioration + réparation tour (sous le portrait) */
        int uy = HUD_Y + M + PORTRAIT_SZ + GAP;
        ui->upg_dmg_btn   = (Rectangle){(float)right_x,                     (float)uy, (float)ubw, (float)ubh};
        ui->upg_range_btn = (Rectangle){(float)(right_x + ubw + GAP),       (float)uy, (float)ubw, (float)ubh};
        ui->upg_rate_btn  = (Rectangle){(float)(right_x + (ubw+GAP)*2),     (float)uy, (float)ubw, (float)ubh};
        uy += ubh + 5;
        ui->repair_tower_btn = (Rectangle){(float)right_x,              (float)uy, (float)rsh,               26.0f};
        ui->sell_btn         = (Rectangle){(float)(right_x + rsh + GAP),(float)uy, (float)(max_w - rsh - GAP),26.0f};
        uy += 26 + 5;
        ui->apply_mat_btn    = (Rectangle){(float)right_x,              (float)uy, (float)max_w,              26.0f};

        /* Boutons achat de slots — bas du panneau GAUCHE (déplacés du panneau droit) */
        {
            const int bspx = M + 2;
            const int bsw  = UI_LEFT_PANEL_W - M * 2 - 4;
            /* Ancrés sur le bas du HUD : unités en bas, tours juste au-dessus */
            ui->buy_tower_slot_btn = (Rectangle){(float)bspx,
                                                 (float)(HUD_Y + UI_HUD_HEIGHT - 54),
                                                 (float)bsw, 22.0f};
            ui->buy_unit_slot_btn  = (Rectangle){(float)bspx,
                                                 (float)(HUD_Y + UI_HUD_HEIGHT - 28),
                                                 (float)bsw, 22.0f};
        }

        /* Vente unité (panneau droit, bas) */
        ui->unit_sell_btn = (Rectangle){(float)right_x,
                                        (float)(HUD_Y + UI_HUD_HEIGHT - M - btn_h),
                                        (float)max_w, (float)btn_h};

        /* Boutons comportement unité — 4 boutons au-dessus de RENVOYER */
        {
            const int beh_h = 22;
            const int beh_w = (max_w - GAP * 3) / 4;
            int beh_y = HUD_Y + UI_HUD_HEIGHT - M - btn_h - GAP - beh_h;
            for (int _b = 0; _b < 4; _b++) {
                ui->unit_beh_btns[_b] = (Rectangle){
                    (float)(right_x + _b * (beh_w + GAP)),
                    (float)beh_y, (float)beh_w, (float)beh_h
                };
            }
            /* 5e bouton : SUIVRE (médic uniquement) — rangée au-dessus des 4 boutons */
            int beh_y2 = beh_y - beh_h - GAP;
            ui->unit_beh_btns[4] = (Rectangle){
                (float)right_x, (float)beh_y2,
                (float)max_w, (float)beh_h
            };
        }

        (void)ubw; (void)uy;
    }

    // Visibilité bouton matériau (+ vérif que la tour est encore active)
    {
        int _mat_ok = 0;
        if (ui->selection.active && gs->inventory_count > 0) {
            int _tidx = ui->selection.tower_idx;
            if (_tidx >= 0 && _tidx < MAX_TOWERS &&
                gs->towers.towers[_tidx].active)
                _mat_ok = 1;
        }
        if (!_mat_ok) ui->sel_mat_idx = 0; // réinitialise l'index si le bouton disparaît
        /* Borne de sécurité */
        if (ui->sel_mat_idx >= gs->inventory_count)
            ui->sel_mat_idx = 0;
        ui->apply_mat_visible = _mat_ok;
    }

    // ── Overlays déplaçables ──────────────────────────────────
    {
        // Initialisation des positions par défaut au 1er frame
        if (ui->overlay_tl_pos.x < 0.0f) {
            ui->overlay_tl_pos = (Vector2){
                (float)(g_map_x_off + 8),
                8.0f
            };
            ui->overlay_tr_pos = (Vector2){
                (float)(g_map_x_off + g_canvas_virt_w_base - 8 - OVERLAY_W),
                8.0f
            };
        }
        if (ui->overlay_bl_pos.x < 0.0f) {
            int _bh = overlay_bl_h(gs);
            ui->overlay_bl_pos = (Vector2){
                (float)(g_map_x_off + 8),
                (float)(HUD_Y - _bh - 8)
            };
        }

        // Mise à jour de la position pendant le drag
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
            ui->dragging_overlay = -1;

        if (ui->dragging_overlay >= 0) {
            Vector2 *pos;
            float oh, ow;
            if (ui->dragging_overlay == 0) {
                pos = &ui->overlay_tl_pos; oh = OVERLAY_TL_H; ow = OVERLAY_W;
            } else if (ui->dragging_overlay == 1) {
                pos = &ui->overlay_tr_pos; oh = OVERLAY_TR_H; ow = OVERLAY_W;
            } else {
                pos = &ui->overlay_bl_pos;
                oh  = (float)overlay_bl_h(gs); ow = OVERLAY_BL_W;
            }
            pos->x = mouse.x - ui->drag_grab.x;
            pos->y = mouse.y - ui->drag_grab.y;
            // Clamp dans le canvas
            int canvas_h = g_canvas_virt_h;
            if (pos->x < 0)                    pos->x = 0;
            if (pos->y < 0)                    pos->y = 0;
            if (pos->x + ow > g_canvas_virt_w) pos->x = g_canvas_virt_w - ow;
            if (pos->y + oh > canvas_h)         pos->y = canvas_h - oh;
        }
    }

    /* ── Boutons réparation bases (overlay bas-gauche) ─────────── */
    /* Calculé APRÈS le drag pour que les rects suivent sans décalage */
    {
        if (ui->overlay_bl_pos.x >= 0.0f) {
            const int OV_P = OVERLAY_OV_P;
            int bx = (int)ui->overlay_bl_pos.x + OV_P;
            int by = (int)ui->overlay_bl_pos.y + OV_P + 4;  /* top + grip */
            const int bw = OVERLAY_BL_W - OV_P * 2;
            for (int b = 0; b < gs->map.base_count && b < MAX_BASES; b++) {
                const BaseInfo *base = &gs->map.bases[b];
                by += 18;   /* label(8) + gap(2) + barre(6) + gap(2) */
                if (base->active && base->hp > 0 && base->hp < base->max_hp) {
                    ui->repair_base_btn[b] = (Rectangle){
                        (float)bx, (float)(by + 1),
                        (float)bw, 14.0f
                    };
                    by += 16;   /* bouton (14) + gap (2) */
                } else {
                    ui->repair_base_btn[b] = (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
                }
                if (b < gs->map.base_count - 1) by += 4;   /* gap inter-bases */
            }
        } else {
            for (int b = 0; b < MAX_BASES; b++)
                ui->repair_base_btn[b] = (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
        }
    }

    /* ── Sélection de GROUPE par glisser-boîte : suivi + validation ──
       La boîte est AMORCÉE plus bas, dans le chemin « carte vide » du clic
       gauche (après que tout overlay / bouton / outil a déjà consommé le
       clic via goto end_click) → aucun faux départ possible.
       Ici on suit le glisser et on valide la sélection à la relâche.
       L'ordre de déplacement du groupe se donne ensuite au CLIC DROIT. */
    if (ui->box_dragging) {
        ui->box_cur = mouse;
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            float ddx = ui->box_cur.x - ui->box_start.x;
            float ddy = ui->box_cur.y - ui->box_start.y;
            if (ddx*ddx + ddy*ddy >= 64.0f) {            /* glisser réel ≥ 8 px */
                float x0 = (fminf(ui->box_start.x, ui->box_cur.x) - g_map_x_off)
                           / g_map_render_scale;
                float x1 = (fmaxf(ui->box_start.x, ui->box_cur.x) - g_map_x_off)
                           / g_map_render_scale;
                float y0 = fminf(ui->box_start.y, ui->box_cur.y) / g_map_render_scale;
                float y1 = fmaxf(ui->box_start.y, ui->box_cur.y) / g_map_render_scale;
                int cnt = 0;
                for (int j = 0; j < MAX_UNITS; j++) {
                    ui->group_sel[j] = 0;
                    Unit *u = &gs->units.units[j];
                    if (!u->active || u->type == UNIT_WORKER) continue;
                    if (u->x >= x0 && u->x <= x1 && u->y >= y0 && u->y <= y1) {
                        ui->group_sel[j] = 1; cnt++;
                    }
                }
                ui->group_count = cnt;
                if (cnt > 0) {
                    /* La sélection de groupe remplace la sélection simple */
                    ui->sell_unit_idx       = -1;
                    ui->worker_selected_idx = -1;
                    gs->units.selected_unit = -1;
                    ui->selection.active    = 0;
                    char gb[40];
                    snprintf(gb, sizeof(gb), "%d unite%s selectionnee%s",
                             cnt, cnt > 1 ? "s" : "", cnt > 1 ? "s" : "");
                    ui_push_notif(ui, gb, (Color){120, 200, 140, 255});
                    audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                }
            }
            ui->box_dragging = 0;
        } else if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            ui->box_dragging = 0;   /* sécurité : relâche manquée */
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui->mp_block_click) {
        // Démarrage du drag sur un overlay (priorité sur tout autre clic)
        {
            Rectangle tl_r = {ui->overlay_tl_pos.x, ui->overlay_tl_pos.y,
                               OVERLAY_W, OVERLAY_TL_H};
            Rectangle tr_r = {ui->overlay_tr_pos.x, ui->overlay_tr_pos.y,
                               OVERLAY_W, OVERLAY_TR_H};
            if (CheckCollisionPointRec(mouse, tl_r)) {
                ui->dragging_overlay = 0;
                ui->drag_grab = (Vector2){mouse.x - tl_r.x, mouse.y - tl_r.y};
                goto end_click; // ne pas propager le clic
            }
            if (CheckCollisionPointRec(mouse, tr_r)) {
                // Bouton vitesse dans le coin bas-droit de l'overlay TR :
                // intercepté avant le drag pour ne pas déclencher le glissement.
                // Position identique à celle calculée dans le rendu.
                {
                    const int _ovp = OVERLAY_OV_P;
                    const int _bw  = 26, _bh = 14;
                    // ty_speed = oy + OV_P+4 + (17+3) + (14+3) + (14+3) + (9+5)
                    const int _by = (int)tr_r.y + _ovp + 4 + 20 + 17 + 17 + 14;
                    Rectangle _sbr = {
                        tr_r.x + OVERLAY_W - _ovp - _bw,
                        (float)_by, (float)_bw, (float)_bh
                    };
                    if (CheckCollisionPointRec(mouse, _sbr)) {
                        ui->speed_mult = (ui->speed_mult % 3) + 1;
                        audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                        goto end_click;
                    }
                }
                ui->dragging_overlay = 1;
                ui->drag_grab = (Vector2){mouse.x - tr_r.x, mouse.y - tr_r.y};
                goto end_click;
            }
            /* ── Overlay BL : bases HP + réparation ─────────────── */
            if (ui->overlay_bl_pos.x >= 0.0f) {
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
                                snprintf(_nbuf, sizeof(_nbuf), "+%d HP (base reparee)", _restore);
                                ui_push_notif(ui, _nbuf, (Color){46, 204, 113, 255});
                            } else {
                                ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
                            }
                        }
                        goto end_click;
                    }
                    /* Clic hors bouton → déplacement du panneau */
                    ui->dragging_overlay = 2;
                    ui->drag_grab = (Vector2){mouse.x - bl_r.x, mouse.y - bl_r.y};
                    goto end_click;
                }
            }
        }

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
                        ui_push_notif(ui, "Clic sur la carte pour deplacer",
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
                    goto end_click;
                }
                /* 5e bouton : SUIVRE (médic uniquement) */
                if (_cu->type == UNIT_MEDIC &&
                    CheckCollisionPointRec(mouse, ui->unit_beh_btns[4])) {
                    ui->behavior_pending      = (int)UBEH_FOLLOW_UNIT;
                    ui->behavior_pending_unit = ui->sell_unit_idx;
                    ui_push_notif(ui, "Cliquez sur une unite",
                                  (Color){231, 76, 60, 255});
                    audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                    goto end_click;
                }
            }
        }

        /* ── Amélioration et réparation de la tour sélectionnée ── */
        if (ui->selection.active) {
            int _tidx = ui->selection.tower_idx;
            if (_tidx >= 0 && _tidx < MAX_TOWERS && gs->towers.towers[_tidx].active) {
                Tower *_tw = &gs->towers.towers[_tidx];

                if (CheckCollisionPointRec(mouse, ui->upg_dmg_btn)) {
                    int _c = tower_upg_next_cost_dmg(_tw);
                    if (_c < 0) {
                        ui_push_notif(ui, "Degats : niveau max !", (Color){130, 90, 40, 255});
                    } else if (gs->gold >= _c) {
                        gs->gold -= _c;
                        tower_upgrade_dmg(_tw);
                        char _nb[48];
                        snprintf(_nb, sizeof(_nb), "Degats +10%% (niv %d/%d)",
                                 _tw->upg_dmg, TOWER_UPG_MAX);
                        ui_push_notif(ui, _nb, (Color){231, 100, 60, 255});
                    } else {
                        ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
                    }
                    goto end_click;
                }
                if (CheckCollisionPointRec(mouse, ui->upg_range_btn)) {
                    int _c = tower_upg_next_cost_range(_tw);
                    if (_c < 0) {
                        ui_push_notif(ui, "Portee : niveau max !", (Color){130, 90, 40, 255});
                    } else if (gs->gold >= _c) {
                        gs->gold -= _c;
                        tower_upgrade_range(_tw);
                        char _nb[48];
                        snprintf(_nb, sizeof(_nb), "Portee +10%% (niv %d/%d)",
                                 _tw->upg_range, TOWER_UPG_MAX);
                        ui_push_notif(ui, _nb, (Color){82, 155, 200, 255});
                    } else {
                        ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
                    }
                    goto end_click;
                }
                if (CheckCollisionPointRec(mouse, ui->upg_rate_btn)) {
                    int _c = tower_upg_next_cost_rate(_tw);
                    if (_c < 0) {
                        ui_push_notif(ui, "Cadence : niveau max !", (Color){130, 90, 40, 255});
                    } else if (gs->gold >= _c) {
                        gs->gold -= _c;
                        tower_upgrade_rate(_tw);
                        char _nb[48];
                        snprintf(_nb, sizeof(_nb), "Cadence +20%% (niv %d/%d)",
                                 _tw->upg_rate, TOWER_UPG_MAX);
                        ui_push_notif(ui, _nb, (Color){155, 89, 182, 255});
                    } else {
                        ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
                    }
                    goto end_click;
                }
                if (CheckCollisionPointRec(mouse, ui->repair_tower_btn)) {
                    if (_tw->hp >= TOWER_MAX_HP) {
                        ui_push_notif(ui, "Tour deja a plein HP !", (Color){130, 90, 40, 255});
                    } else if (gs->gold >= TOWER_REPAIR_COST) {
                        gs->gold -= TOWER_REPAIR_COST;
                        tower_do_repair(_tw);
                        ui_push_notif(ui, "Tour reparee !", (Color){46, 204, 113, 255});
                    } else {
                        ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
                    }
                    goto end_click;
                }
            }
        }

        /* ── Achat de slots supplémentaires ───────────────────── */
        if (CheckCollisionPointRec(mouse, ui->buy_tower_slot_btn)) {
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
            goto end_click;
        }
        if (CheckCollisionPointRec(mouse, ui->buy_unit_slot_btn)) {
            int _bought = gs->slots_unit_bought;
            int _max_extra = MAX_UNITS - unit_active_limit(&gs->bonuses, gs->map.base_count);
            if (_bought >= SLOT_MAX_BUYS || _bought >= _max_extra) {
                ui_push_notif(ui, "Limite d'unites atteinte !", (Color){130, 90, 40, 255});
            } else {
                int _cost = SLOT_UNIT_COSTS[_bought];
                if (gs->gold >= _cost) {
                    gs->gold -= _cost;
                    gs->slots_unit_bought++;
                    char _nb[48];
                    snprintf(_nb, sizeof(_nb), "+1 slot unite (limite : %d)",
                             gs->units.unit_limit + 1);
                    ui_push_notif(ui, _nb, (Color){39, 174, 96, 255});
                } else {
                    ui_push_notif(ui, "Or insuffisant !", (Color){243, 156, 18, 255});
                }
            }
            goto end_click;
        }

        if (ui->hovered_tool != -1) {
            if (!tool_is_unlocked((ToolID)ui->hovered_tool, gs)) {
                /* Tour verrouillée : on ignore le clic */
            } else {
                ui->selected_tool       = (ToolID)ui->hovered_tool;
                ui->selection.active    = 0;
                ui->worker_selected_idx = -1;
                gs->units.selected_unit = -1;
            }
        }
        else if (CheckCollisionPointRec(mouse, ui->wave_btn)) {
            if (gs->phase == PHASE_PREP) {
                int bonus = (int)(gs->wave_manager.prep_timer / 20.0f * 15.0f);
                gs->gold += bonus;
                wave_start(&gs->wave_manager);
                gs->phase = PHASE_WAVE;
                if (bonus > 0) {
                    char nbuf[44];
                    snprintf(nbuf, sizeof(nbuf), "+%d or (lancement rapide)", bonus);
                    ui_push_notif(ui, nbuf, (Color){230, 155, 35, 255});
                }
            }
        }
        else if (ui->selection.active &&
                 CheckCollisionPointRec(mouse, ui->sell_btn)) {
            Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
            if (tw->active) {
                int real_cost = tower_cost_on_tile(tw->type, &gs->map,
                                                   tw->tile_x, tw->tile_y);
                float rfrac = TOWER_SELL_REFUND + g_run_mods.sell_refund_add;
                if (rfrac > 1.0f) rfrac = 1.0f;   // perk Recyclage (cap 100 %)
                int refund = (int)(real_cost * rfrac);
                gs->gold += refund;
                gs->map.tiles[tw->tile_y][tw->tile_x].buildable = 1;
                tw->active = 0;
                gs->towers.tower_count--;
                char nbuf[32];
                snprintf(nbuf, sizeof(nbuf), "+%d or recuperes", refund);
                ui_push_notif(ui, nbuf, (Color){230, 155, 35, 255});
            }
            ui->selection.active = 0;
        }
        else if (ui->sell_unit_idx >= 0 &&
                 CheckCollisionPointRec(mouse, ui->unit_sell_btn)) {
            Unit *u = &gs->units.units[ui->sell_unit_idx];
            if (u->active) {
                int base_cost = UNIT_BASE_STATS[u->type].cost;
                int refund    = (int)(base_cost * 0.5f);
                gs->gold += refund;
                u->active = 0;
                gs->units.count--;
                char nbuf[36];
                snprintf(nbuf, sizeof(nbuf), "+%d or (unite renvoyee)", refund);
                ui_push_notif(ui, nbuf, (Color){230, 155, 35, 255});
            }
            ui->sell_unit_idx       = -1;
            ui->worker_selected_idx = -1;
            gs->units.selected_unit = -1;
        }
        else if (ui->selection.active &&
                 ui->apply_mat_visible &&
                 CheckCollisionPointRec(mouse, ui->apply_mat_btn)) {
            /* Flèche gauche (<) = cycle vers l'arrière */
            Rectangle _ab = ui->apply_mat_btn;
            Rectangle _left_arrow  = {_ab.x, _ab.y, 20.0f, _ab.height};
            Rectangle _right_arrow = {_ab.x + _ab.width - 20.0f, _ab.y, 20.0f, _ab.height};
            if (gs->inventory_count > 1 && CheckCollisionPointRec(mouse, _left_arrow)) {
                ui->sel_mat_idx = (ui->sel_mat_idx - 1 + gs->inventory_count) % gs->inventory_count;
                audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                goto end_click;
            }
            if (gs->inventory_count > 1 && CheckCollisionPointRec(mouse, _right_arrow)) {
                ui->sel_mat_idx = (ui->sel_mat_idx + 1) % gs->inventory_count;
                audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                goto end_click;
            }
            int _tidx = ui->selection.tower_idx;
            if (_tidx >= 0 && _tidx < MAX_TOWERS) {
                Tower *tw = &gs->towers.towers[_tidx];
                if (tw->active && gs->inventory_count > 0) {
                    /* Utilise le matériau sélectionné (pas toujours [0]) */
                    int _midx = ui->sel_mat_idx;
                    if (_midx < 0 || _midx >= gs->inventory_count) _midx = 0;
                    MaterialType mat = gs->inventory[_midx];
                    switch (mat) {
                        case MAT_ACID:
                            tw->dmg_type = DMG_POISON;
                            tw->material = MAT_ACID;
                            break;
                        case MAT_PLASMA:
                            tw->dmg_type = DMG_ELECTRIC;
                            tw->material = MAT_PLASMA;
                            break;
                        case MAT_CRYO:
                            tw->dmg_type = DMG_CRYO;
                            tw->material = MAT_CRYO;
                            break;
                        case MAT_NANO:
                            tw->dmg_type = DMG_NANO;
                            tw->material = MAT_NANO;
                            break;
                        case MAT_IRON:
                            // Guard: n'empile pas le +10% si déjà appliqué
                            if (tw->material != MAT_IRON) {
                                tw->damage  *= 1.10f;
                                tw->material = MAT_IRON;
                            }
                            break;
                        default:
                            break;
                    }
                    /* Retire le matériau sélectionné de l'inventaire */
                    for (int k = _midx; k < gs->inventory_count - 1; k++)
                        gs->inventory[k] = gs->inventory[k + 1];
                    gs->inventory[gs->inventory_count - 1] = MAT_NONE;
                    gs->inventory_count--;
                    /* Recentre l'index si nécessaire */
                    if (ui->sel_mat_idx >= gs->inventory_count)
                        ui->sel_mat_idx = (gs->inventory_count > 0) ? gs->inventory_count - 1 : 0;
                    audio_play_sfx(AUDIO_SFX_MATERIAL_APPLY);
                    ui_push_notif(ui, "Materiau applique !",
                                  (Color){62, 175, 200, 255});
                    if (gs->inventory_count == 0)
                        ui->apply_mat_visible = 0;
                }
            }
        }
        else if (mouse.y < HUD_Y && ui->hovered_tile_x >= 0) {
            int tx = ui->hovered_tile_x;
            int ty = ui->hovered_tile_y;
            float wx_m = (mouse.x - g_map_x_off) / g_map_render_scale;
            float wy_m = mouse.y / g_map_render_scale;

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
                            ui_push_notif(ui, "Unite en garde de tourelle",
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
                goto end_click;
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
                            ui_push_notif(ui, "Unite en escorte d'ouvrier",
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
                goto end_click;
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
                            ui_push_notif(ui, "Medic en suivi d'unite",
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
                goto end_click;
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
                        _mu->manual_x      = wx_m;
                        _mu->manual_y      = wy_m;
                        _mu->manual_moving = 1;
                        goto end_click;
                    } else {
                        ui_push_notif(ui, "Zone de spawn interdite !",
                                      (Color){231, 76, 60, 255});
                        goto end_click;
                    }
                }
            }

            // Ouvrier sélectionné → dépôt
            if (ui->worker_selected_idx >= 0) {
                int assigned = 0;
                for (int d = 0; d < gs->map.deposit_count; d++) {
                    MaterialDeposit *dep = &gs->map.deposits[d];
                    if (dep->active &&
                        dep->tile_x == tx && dep->tile_y == ty) {
                        unit_assign_deposit(&gs->units,
                                            ui->worker_selected_idx, d);
                        ui->worker_selected_idx = -1;
                        gs->units.selected_unit = -1;
                        assigned = 1;
                        break;
                    }
                }
                if (!assigned) {
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
                        }
                    } else {
                        ui_push_notif(ui, "Limite de tours atteinte !",
                                      (Color){231, 76, 60, 255});
                    }
                } else if (ui_tool_is_unit(ui->selected_tool)) {
                    if (gs->units.count >= gs->units.unit_limit) {
                        ui_push_notif(ui, "Limite d'unites atteinte !",
                                      (Color){231, 76, 60, 255});
                    } else {
                        // Cherche la base active la plus proche du clic dans le rayon
                        int   near_any_base = 0;
                        float spawn_bpx = gs->units.base_px;
                        float spawn_bpy = gs->units.base_py;
                        float best_dist = 5.0f * TILE_SIZE + 1.0f; // seuil + 1 pour init
                        for (int b = 0; b < gs->map.base_count; b++) {
                            if (!gs->map.bases[b].active) continue;
                            float bpx = gs->map.bases[b].pos.x * TILE_SIZE
                                        + TILE_SIZE / 2.0f;
                            float bpy = gs->map.bases[b].pos.y * TILE_SIZE
                                        + TILE_SIZE / 2.0f;
                            /* Convertit la souris en espace monde (zoom carte) */
                            float wx = (mouse.x - g_map_x_off) / g_map_render_scale;
                            float wy = mouse.y / g_map_render_scale;
                            float dx = wx - bpx;
                            float dy = wy - bpy;
                            float dist = sqrtf(dx*dx + dy*dy);
                            if (dist <= 5.0f * TILE_SIZE && dist < best_dist) {
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
                                int _mc = 0;
                                for (int _j = 0; _j < MAX_UNITS; _j++) {
                                    const Unit *_u = &gs->units.units[_j];
                                    if (_u->active && _u->type == UNIT_MEDIC &&
                                        _u->home_base_px == spawn_bpx &&
                                        _u->home_base_py == spawn_bpy)
                                        _mc++;
                                }
                                if (_mc >= MAX_MEDICS_PER_BASE) {
                                    ui_push_notif(ui, "Max 4 medics par base !",
                                                  (Color){231, 76, 60, 255});
                                    goto end_click;
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
                            ui_push_notif(ui, "Spawn pres d'une base !",
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
                    float wx = (mouse.x - g_map_x_off) / g_map_render_scale;
                    float wy = mouse.y / g_map_render_scale;
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
        }
        end_click:;
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        /* Groupe sélectionné + clic droit sur la carte (sans outil) →
           ORDRE DE DÉPLACEMENT EN FORMATION (on conserve la sélection). */
        int did_group_move = 0;
        if (ui->group_count > 0 && mouse.y < HUD_Y &&
            ui->selected_tool == TOOL_NONE) {
            float wx = (mouse.x - g_map_x_off) / g_map_render_scale;
            float wy = mouse.y / g_map_render_scale;
            int   tx = (int)(wx / TILE_SIZE), ty = (int)(wy / TILE_SIZE);
            int   in_spawn = 0;
            for (int _sp = 0; _sp < gs->map.path_count; _sp++) {
                int _dx = tx - gs->map.paths[_sp].spawn.x;
                int _dy = ty - gs->map.paths[_sp].spawn.y;
                if (_dx*_dx + _dy*_dy <= SPAWN_EXCLUSION_RADIUS*SPAWN_EXCLUSION_RADIUS) {
                    in_spawn = 1; break;
                }
            }
            if (in_spawn) {
                ui_push_notif(ui, "Zone de spawn interdite !",
                              (Color){231, 76, 60, 255});
            } else {
                /* Disposition en grille autour du point cliqué (évite l'empilement). */
                int   n    = ui->group_count;
                int   cols = (int)ceilf(sqrtf((float)n)); if (cols < 1) cols = 1;
                int   rows = (n + cols - 1) / cols;
                float sp   = TILE_SIZE * 0.8f;
                int   placed = 0;
                for (int j = 0; j < MAX_UNITS; j++) {
                    if (!ui->group_sel[j]) continue;
                    Unit *u = &gs->units.units[j];
                    if (!u->active || u->type == UNIT_WORKER) { ui->group_sel[j] = 0; continue; }
                    int   gx = placed % cols, gy = placed / cols;
                    float ox = ((float)gx - (cols - 1) * 0.5f) * sp;
                    float oy = ((float)gy - (rows - 1) * 0.5f) * sp;
                    u->behavior      = UBEH_MANUAL;
                    u->manual_x      = wx + ox;
                    u->manual_y      = wy + oy;
                    u->manual_moving = 1;
                    placed++;
                }
                /* Recompte (des unités du groupe ont pu mourir entre-temps). */
                ui->group_count = placed;
                if (placed > 0) {
                    ui_push_notif(ui, "Groupe en mouvement",
                                  (Color){120, 200, 140, 255});
                    audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                }
            }
            did_group_move = 1;   /* le clic droit est consommé par l'ordre */
        }

        if (!did_group_move) {
            ui->selected_tool         = TOOL_NONE;
            ui->selection.active      = 0;
            ui->worker_selected_idx   = -1;
            ui->sell_unit_idx         = -1;
            gs->units.selected_unit   = -1;
            ui->behavior_pending      = -1;
            ui->behavior_pending_unit = -1;
            group_clear(ui);
        }
    }

    if (IsKeyPressed(KEY_ONE))   ui->selected_tool = TOOL_TOWER_GUN;
    if (IsKeyPressed(KEY_TWO)   && tool_is_unlocked(TOOL_TOWER_SNIPER, gs))
        ui->selected_tool = TOOL_TOWER_SNIPER;
    if (IsKeyPressed(KEY_THREE) && tool_is_unlocked(TOOL_TOWER_FLAME,  gs))
        ui->selected_tool = TOOL_TOWER_FLAME;
    if (IsKeyPressed(KEY_FOUR)  && tool_is_unlocked(TOOL_TOWER_TESLA,  gs))
        ui->selected_tool = TOOL_TOWER_TESLA;
    if (IsKeyPressed(KEY_FIVE))  ui->selected_tool = TOOL_UNIT_SOLDIER;
    if (IsKeyPressed(KEY_SIX))   ui->selected_tool = TOOL_UNIT_HEAVY;
    if (IsKeyPressed(KEY_SEVEN)) ui->selected_tool = TOOL_UNIT_MEDIC;
    if (IsKeyPressed(KEY_EIGHT)) ui->selected_tool = TOOL_UNIT_DOG;
    if (IsKeyPressed(KEY_NINE))  ui->selected_tool = TOOL_UNIT_WORKER;
    if (IsKeyPressed(KEY_X))     ui->speed_mult = (ui->speed_mult % 3) + 1;
    if (IsKeyPressed(KEY_F))     ui->show_fps  ^= 1;

    // Tick notifications flottantes
    {
        float dt = GetFrameTime();
        for (int i = 0; i < ui->notif_count; ) {
            FloatNotif *n = &ui->notifs[i];
            n->timer -= dt;
            n->y_off += 32.0f * dt;
            if (n->timer <= 0.0f) {
                for (int j = i; j < ui->notif_count - 1; j++)
                    ui->notifs[j] = ui->notifs[j + 1];
                ui->notif_count--;
            } else {
                i++;
            }
        }
    }

    /* ESCAPE : vide la sélection (la pause est gérée séparément dans game_do_input) */
    if (IsKeyPressed(KEY_ESCAPE)) {
        ui->selected_tool         = TOOL_NONE;
        ui->selection.active      = 0;
        ui->worker_selected_idx   = -1;
        ui->sell_unit_idx         = -1;
        gs->units.selected_unit   = -1;
        ui->behavior_pending      = -1;
        ui->behavior_pending_unit = -1;
    }
}
