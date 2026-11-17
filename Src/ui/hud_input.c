/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_input.c ─ Gestion des entrées du HUD en jeu.
 *
 *  Contient :
 *    ui_update  — Traitement souris / clavier par frame (orchestration).
 *                 Mise en page : hud_input_layout.c ; clics panneaux :
 *                 hud_input_click.c ; clic carte : hud_input_map.c.
 */

#include "hud_internal.h"

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

    // Hover tuile via le mappage centralisé (zoom/pan + grandes cartes inclus)
    {
        Vector2 w = map_screen_to_world(mouse);
        int tx = (int)(w.x / TILE_SIZE);
        int ty = (int)(w.y / TILE_SIZE);
        if (mouse.y >= 0 && mouse.y < HUD_Y && w.x >= 0 && w.y >= 0 &&
            tx >= 0 && tx < gs->map.w && ty >= 0 && ty < gs->map.h) {
            ui->hovered_tile_x = tx;
            ui->hovered_tile_y = ty;
        } else {
            ui->hovered_tile_x = -1;
            ui->hovered_tile_y = -1;
        }
    }

    hud_input_layout(ui, gs, mouse, HUD_Y);   // boutons, overlays, réparation


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
                Vector2 _w0 = map_screen_to_world((Vector2){
                    fminf(ui->box_start.x, ui->box_cur.x),
                    fminf(ui->box_start.y, ui->box_cur.y)});
                Vector2 _w1 = map_screen_to_world((Vector2){
                    fmaxf(ui->box_start.x, ui->box_cur.x),
                    fmaxf(ui->box_start.y, ui->box_cur.y)});
                float x0 = _w0.x, y0 = _w0.y, x1 = _w1.x, y1 = _w1.y;
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
                    snprintf(gb, sizeof(gb), "%d unité%s sélectionnée%s",
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
        if (hud_click_overlay_drag(ui, gs, mouse, HUD_Y)) goto end_click;
        if (hud_click_unit_panel  (ui, gs, mouse, HUD_Y)) goto end_click;
        if (hud_click_tower_panel (ui, gs, mouse, HUD_Y)) goto end_click;
        if (hud_click_slot_buys   (ui, gs, mouse, HUD_Y)) goto end_click;

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
        else if (!gs->wave_manager.lock_manual &&
                 CheckCollisionPointRec(mouse, ui->wave_btn)) {
            if (gs->phase == PHASE_PREP) {
                int bonus = wave_early_launch_bonus(&gs->wave_manager);
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
                int refund = tower_sell_refund(tw, &gs->map);   // pose + améliorations
                gs->gold += refund;
                // Rend le MINERAI appliqué à l'inventaire (s'il reste de la place).
                if (tw->material != MAT_NONE && gs->inventory_count < MAX_INVENTORY) {
                    gs->inventory[gs->inventory_count++] = tw->material;
                    ui_push_notif(ui, "Minerai récupéré", (Color){62, 175, 200, 255});
                }
                gs->map.tiles[tw->tile_y][tw->tile_x].buildable = 1;
                tw->active = 0;
                gs->towers.tower_count--;
                char nbuf[32];
                snprintf(nbuf, sizeof(nbuf), "+%d or récupérés", refund);
                ui_push_notif(ui, nbuf, (Color){230, 155, 35, 255});
            }
            ui->selection.active = 0;
        }
        else if (ui->sell_unit_idx >= 0 &&
                 CheckCollisionPointRec(mouse, ui->unit_sell_btn)) {
            Unit *u = &gs->units.units[ui->sell_unit_idx];
            if (u->active) {
                int base_cost = UNIT_BASE_STATS[u->type].cost;
                int refund    = (int)(base_cost * UNIT_SELL_REFUND);
                gs->gold += refund;
                u->active = 0;
                gs->units.count--;
                char nbuf[36];
                snprintf(nbuf, sizeof(nbuf), "+%d or (unité renvoyée)", refund);
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
                    tower_set_material(tw, mat);   // type de dégâts + gros bonus (remplace proprement)
                    /* Retire le matériau sélectionné de l'inventaire */
                    for (int k = _midx; k < gs->inventory_count - 1; k++)
                        gs->inventory[k] = gs->inventory[k + 1];
                    gs->inventory[gs->inventory_count - 1] = MAT_NONE;
                    gs->inventory_count--;
                    /* Recentre l'index si nécessaire */
                    if (ui->sel_mat_idx >= gs->inventory_count)
                        ui->sel_mat_idx = (gs->inventory_count > 0) ? gs->inventory_count - 1 : 0;
                    audio_play_sfx(AUDIO_SFX_MATERIAL_APPLY);
                    ui_push_notif(ui, "Matériau appliqué !",
                                  (Color){62, 175, 200, 255});
                    if (gs->inventory_count == 0)
                        ui->apply_mat_visible = 0;
                }
            }
        }
        else if (mouse.y < HUD_Y && ui->hovered_tile_x >= 0) {
            if (hud_click_map(ui, gs, mouse, HUD_Y)) goto end_click;
        }
        end_click:;
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        /* Groupe sélectionné + clic droit sur la carte (sans outil) →
           ORDRE DE DÉPLACEMENT EN FORMATION (on conserve la sélection). */
        int did_group_move = 0;
        if (ui->group_count > 0 && mouse.y < HUD_Y &&
            ui->selected_tool == TOOL_NONE) {
            float wx = map_screen_to_world(mouse).x;
            float wy = map_screen_to_world(mouse).y;
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
                    /* Chaque place de la formation est ramenée sur une
                       case atteignable (sinon les unités du bord poussaient
                       dans le décor). */
                    float fx = wx + ox, fy = wy + oy;
                    move_free_spot(&gs->map, &gs->towers, fx, fy,
                                   u->size, MOVE_F_ALLY, &fx, &fy);
                    u->behavior      = UBEH_MANUAL;
                    u->manual_x      = fx;
                    u->manual_y      = fy;
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

        if (!did_group_move) ui_clear_selection(ui, gs);
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

    /* ECHAP est traité EN AMONT (app.c game_do_input) : il annule d'abord la
       sélection, et ne met en pause que s'il n'y a rien à annuler. Le gérer
       aussi ici effaçait la sélection à la SORTIE de pause — game_do_update
       tourne dans la même frame, où la touche est encore « pressée ». */
}
