/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_upgrades.c ─ Écran des améliorations méta-progression.
 *
 *  Contient :
 *    draw_upgrades  — Dépense de ferraille pour améliorer les stats globales
 */

#include "menu_internal.h"

// ════════════════════════════════════════════════════
// AMÉLIORATIONS
// ════════════════════════════════════════════════════
MenuAction draw_upgrades(MenuState *m, const MetaProgress *meta,
                         int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("AMELIORATIONS", vw);

    txt_c_boxed("La ferraille se gagne uniquement en completant des stages de campagne.",
                cx, M_PAD + 86, 10, C_TEXT);

    // Bandeau ferraille
    {
        int fw = 220, fhh = 28;
        int fx = cx - fw/2, fy = M_PAD + 104;
        Rectangle fr = {(float)fx,(float)fy,(float)fw,(float)fhh};
        DrawRectangleRounded(fr, (float)PANEL_R/fhh, 5, (Color){6,20,6,255});
        DrawRectangleRoundedLinesEx(fr, (float)PANEL_R/fhh, 5, 1.5f, C_GREEN);
        txt_c(TextFormat("Ferraille : %d", meta->scrap),
              cx, fy + fhh/2 - 6, 13, C_GREEN);
    }

    int lx = M_PAD * 3;
    int rw = vw - lx * 2;
    int y  = M_PAD + 140;
    draw_text_boxed(TextFormat("Campagnes terminees : %d     Meilleure vague : %d",
                 meta->campaigns_completed, meta->best_wave),
             lx, y, 10, C_TEXT);
    y += fh(10) + 3 + 5;
    draw_sep(lx, y, rw, C_BORDER);
    y += M_IN;

    for (int i = 0; i < UPGRADE_COUNT; i++) {
        int hov = vhov(lx, y, rw, 30);
        if (hov) m->sel_upg = i;
        int is_sel = (m->sel_upg == i);

        Rectangle row = {(float)lx,(float)y,(float)rw, 30};
        DrawRectangleRounded(row, (float)PANEL_R/30, 5,
            is_sel ? (Color){32, 20, 5, 255} : (Color){14, 9, 3, 200});

        draw_text_boxed(UPGRADE_NAMES[i], lx + M_IN, y + 9, 12,
                        is_sel ? C_GOLD : C_TEXT);

        int lvl = 0, maxlvl = 0;
        switch (i) {
            case UPGRADE_TOWER_DMG:   lvl=meta->lvl_tower_dmg;   maxlvl=MAX_LVL_TOWER_DMG;   break;
            case UPGRADE_TOWER_RANGE: lvl=meta->lvl_tower_range;  maxlvl=MAX_LVL_TOWER_RANGE;  break;
            case UPGRADE_TOWER_RATE:  lvl=meta->lvl_tower_rate;   maxlvl=MAX_LVL_TOWER_RATE;   break;
            case UPGRADE_UNIT_HP:     lvl=meta->lvl_unit_hp;      maxlvl=MAX_LVL_UNIT_HP;      break;
            case UPGRADE_UNIT_DMG:    lvl=meta->lvl_unit_dmg;     maxlvl=MAX_LVL_UNIT_DMG;     break;
            case UPGRADE_START_GOLD:  lvl=meta->lvl_start_gold;   maxlvl=MAX_LVL_START_GOLD;   break;
            case UPGRADE_LIVES:       lvl=meta->lvl_lives;         maxlvl=MAX_LVL_LIVES;        break;
            case UPGRADE_SCRAP_BONUS: lvl=meta->lvl_scrap_bonus;  maxlvl=MAX_LVL_SCRAP_BONUS;  break;
            case UPGRADE_TOWER_LIMIT: lvl=meta->lvl_tower_limit;  maxlvl=MAX_LVL_TOWER_LIMIT;  break;
            case UPGRADE_UNIT_LIMIT:  lvl=meta->lvl_unit_limit;   maxlvl=MAX_LVL_UNIT_LIMIT;   break;
            default: break;
        }

        // Pastilles de niveau — centrées
        int pip_w = 13, pip_h = 9, pip_gap = 3;
        int pip_total = maxlvl * (pip_w + pip_gap) - pip_gap;
        int pip_x = cx - pip_total/2;
        int pip_y = y + 11;
        for (int s = 0; s < maxlvl; s++) {
            Color pc = (s < lvl) ? C_GOLD : (Color){35,25,8,255};
            Rectangle pip = {(float)(pip_x + s*(pip_w+pip_gap)),
                             (float)pip_y, (float)pip_w, (float)pip_h};
            DrawRectangleRounded(pip, 0.4f, 3, pc);
            DrawRectangleRoundedLinesEx(pip, 0.4f, 3, 0.8f, (Color){65,48,15,160});
        }

        // Description clippée à droite du centre
        int desc_x  = cx + pip_total/2 + M_IN*2;
        int desc_max = lx + rw - 92 - desc_x - M_IN;
        if (desc_max > 30) {
            char udesc[48];
            clip_text(UPGRADE_DESC[i], desc_max, 9, udesc, sizeof(udesc));
            dtxt(udesc, desc_x, y + 11, 9, C_TEXT);
        }

        // Coût / MAX à droite
        int cost = meta_upgrade_cost(meta, i);
        int cost_x = lx + rw - 82;
        if (cost > 0) {
            int can = meta->scrap >= cost;
            draw_text_boxed(TextFormat("%d ferr.", cost), cost_x, y + 9, 11,
                            can ? C_GOLD : C_RED);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (meta_upgrade((MetaProgress*)meta, i))
                    set_msg(m, "Amelioration achetee !");
                else
                    set_msg(m, "Ferraille insuffisante.");
            }
        } else {
            draw_text_boxed("MAX", cost_x + 10, y + 9, 11, C_GREEN);
        }

        draw_sep(lx, y+30, rw, (Color){25,16,5,140});
        y += 31;
    }

    draw_msg(m, vw, vh);
    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}
