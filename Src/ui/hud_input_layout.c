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

Rectangle hud_speed_btn_rect(const UIState *ui) {
    const float bw = 26.0f, bh = 14.0f;
    float ox = ui->overlay_tr_pos.x, oy = ui->overlay_tr_pos.y;
    return (Rectangle){ox + OVERLAY_W - OVERLAY_OV_P - bw,
                       oy + OVERLAY_OV_P + OVERLAY_GRIP_H + OVERLAY_TR_ROW_WAVE + OVERLAY_TR_ROW_KILLS,
                       bw, bh};
}

/* Rectangles des boutons fixes du panneau bas (cf. hud_internal.h). */
void hud_layout_buttons(UIState *ui, int HUD_Y) {
    // ── Plaque centrale : cartes des tours / unités + LANCER + PAUSE ──
    // Bloc centré entre les panneaux gauche et droit (toutes largeurs).
    {
        const int M      = UI_MARGIN;
        const int STEP   = UI_BTN_W + HUD_CARD_GAP;
        const int CARDS  = 5 * STEP - HUD_CARD_GAP;          /* rangée la plus longue */
        const int SPAN   = CARDS + HUD_BLOCK_GAP + HUD_WAVE_W + M + HUD_PAUSE_W;

        int mid_left  = UI_LEFT_PANEL_W;
        int mid_w     = g_canvas_virt_w - UI_PANEL_W - mid_left;
        int col0_x    = mid_left + (mid_w - SPAN) / 2;
        if (col0_x < mid_left + HUD_PANEL_PAD) col0_x = mid_left + HUD_PANEL_PAD;

        /* Chaque rangée a son titre de section au-dessus (HUD_SEC_H). */
        int sec1_y = HUD_Y + HUD_SEC_Y;
        int row1_y = sec1_y + HUD_SEC_H;
        int row2_y = row1_y + UI_BTN_H + HUD_ROW_GAP + HUD_SEC_H;

        for (int i = 0; i < 4; i++)
            ui->tool_btns[TOOL_TOWER_GUN + i] = (Rectangle){
                (float)(col0_x + i * STEP), (float)row1_y, UI_BTN_W, UI_BTN_H
            };
        for (int i = 0; i < 5; i++)
            ui->tool_btns[TOOL_UNIT_SOLDIER + i] = (Rectangle){
                (float)(col0_x + i * STEP), (float)row2_y, UI_BTN_W, UI_BTN_H
            };

        /* LANCER et PAUSE : toute la hauteur des deux rangées, titres compris */
        int wave_x = col0_x + CARDS + HUD_BLOCK_GAP;
        int wave_h = row2_y + UI_BTN_H - sec1_y;
        ui->wave_btn  = (Rectangle){(float)wave_x, (float)sec1_y, HUD_WAVE_W, (float)wave_h};
        ui->pause_btn = (Rectangle){(float)(wave_x + HUD_WAVE_W + M), (float)sec1_y,
                                    HUD_PAUSE_W, (float)wave_h};
    }

    // ── Boutons du panneau droit ──
    {
        const int M       = UI_MARGIN;
        const int GAP     = 6;
        const int right_x = HUD_RX(g_canvas_virt_w);
        const int max_w   = HUD_RW;
        const int ubw     = (max_w - GAP * 2) / 3;   /* 3 boutons par rangée */
        const int btn_h   = 34;

        /* Fiche de tour, sous le portrait : améliorations (2 lignes de
           texte), puis [CIBLE] [RÉPARER] [VENDRE], puis le minerai. Tout
           tient au-dessus du cadre bas (l'ancienne pile débordait de 6 px
           SOUS le bas de l'écran pour le bouton de minerai). */
        int uy = HUD_Y + M + PORTRAIT_SZ + HUD_TP_GAP;
        ui->upg_dmg_btn   = (Rectangle){(float)right_x,                 (float)uy, (float)ubw, HUD_TP_UPG_H};
        ui->upg_range_btn = (Rectangle){(float)(right_x + ubw + GAP),   (float)uy, (float)ubw, HUD_TP_UPG_H};
        ui->upg_rate_btn  = (Rectangle){(float)(right_x + (ubw+GAP)*2), (float)uy, (float)ubw, HUD_TP_UPG_H};
        uy += HUD_TP_UPG_H + HUD_TP_GAP;
        ui->target_btn       = (Rectangle){(float)right_x,                 (float)uy, (float)ubw, HUD_TP_ROW_H};
        ui->repair_tower_btn = (Rectangle){(float)(right_x + ubw + GAP),   (float)uy, (float)ubw, HUD_TP_ROW_H};
        ui->sell_btn         = (Rectangle){(float)(right_x + (ubw+GAP)*2), (float)uy,
                                           (float)(max_w - (ubw + GAP) * 2), HUD_TP_ROW_H};
        uy += HUD_TP_ROW_H + HUD_TP_GAP;
        ui->apply_mat_btn    = (Rectangle){(float)right_x, (float)uy, (float)max_w, HUD_TP_MAT_H};

        /* Boutons achat de slots — bas du panneau GAUCHE */
        {
            /* Ancrés sur le bas du HUD : unités en bas, tours juste au-dessus */
            const int uy0 = HUD_Y + UI_HUD_HEIGHT - HUD_SEC_Y - HUD_SLOT_BTN_H;
            ui->buy_unit_slot_btn  = (Rectangle){(float)HUD_LX, (float)uy0,
                                                 (float)HUD_LW, HUD_SLOT_BTN_H};
            ui->buy_tower_slot_btn = (Rectangle){(float)HUD_LX,
                                                 (float)(uy0 - HUD_SLOT_BTN_H - GAP),
                                                 (float)HUD_LW, HUD_SLOT_BTN_H};
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
}

/* Recalcule la mise en page des boutons du HUD et suit le glisser des
   overlays (avant tout clic de la frame). */
void hud_input_layout(UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y) {
    hud_layout_buttons(ui, HUD_Y);

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
        if (ui->overlay_tr_pos.x < 0.0f) {
            ui->overlay_tr_pos = (Vector2){
                (float)(g_map_x_off + g_canvas_virt_w_base - 8 - OVERLAY_W),
                8.0f
            };
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
            if (ui->dragging_overlay == 1) {
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
