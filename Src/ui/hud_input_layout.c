/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_input_layout.c ─ Mise en page dynamique des boutons + overlays déplaçables.
 *
 *  Extrait de hud_input.c (règle projet : fichiers < 500 lignes) ; le code
 *  est inchangé, « goto end_click » y devient « return 1 » (clic consommé).
 */

#include "hud_internal.h"

/* Recalcule la mise en page des boutons du HUD et suit le glisser des
   overlays (avant tout clic de la frame). */
void hud_input_layout(UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y) {
    (void)ui; (void)gs; (void)mouse; (void)HUD_Y;
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
            overlay_avoid_marks(&ui->overlay_tl_pos, OVERLAY_W, OVERLAY_TL_H, gs, +1);
            overlay_avoid_marks(&ui->overlay_tr_pos, OVERLAY_W, OVERLAY_TR_H, gs, -1);
        }
        if (ui->overlay_bl_pos.x < 0.0f) {
            int _bh = overlay_bl_h(gs);
            ui->overlay_bl_pos = (Vector2){
                (float)(g_map_x_off + 8),
                (float)(HUD_Y - _bh - 8)
            };
            overlay_avoid_marks(&ui->overlay_bl_pos, OVERLAY_BL_W, _bh, gs, +1);
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
        if (ui->overlay_bl_pos.x >= 0.0f && hud_show_bases_panel(gs)) {
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
}
