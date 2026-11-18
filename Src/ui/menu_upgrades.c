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

/* Liste sur un panneau OPAQUE (le fond peint rendait les petits textes
   illisibles), colonnes FIXES : nom | pastilles | effet | coût. */
#define UPG_ROW_H     36    // hauteur d'une ligne (texte 12 → fh 19, centré)
#define UPG_ROW_GAP    4    // écart entre deux lignes
#define UPG_PIP_COL  210    // début des pastilles (depuis le bord gauche, px)
#define UPG_DESC_COL 330    // début de l'effet (px)
#define UPG_COST_W    90    // colonne du coût (bord droit)

// ════════════════════════════════════════════════════
// AMÉLIORATIONS
// ════════════════════════════════════════════════════
MenuAction draw_upgrades(MenuState *m, const MetaProgress *meta,
                         int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("AMÉLIORATIONS", vw);

    txt_c_boxed("La ferraille se gagne uniquement en complétant des stages de campagne.",
                cx, M_PAD + 86, 10, C_TEXT);

    // Bandeau ferraille
    {
        int fw = 220, fhh = 28;
        int fx = cx - fw/2, fy = M_PAD + 104;
        Rectangle fr = {(float)fx,(float)fy,(float)fw,(float)fhh};
        DrawRectangleRounded(fr, (float)PANEL_R/fhh, 5, (Color){6,20,6,255});
        DrawRectangleRoundedLinesEx(fr, (float)PANEL_R/fhh, 5, 1.5f, C_GREEN);
        {
            const char *sval = TextFormat("%d", meta->scrap);
            int icon_sz = fh(13);
            int tw      = mtxt(sval, 13);
            int gap     = 5;
            int total_w = icon_sz + gap + tw;
            int ix      = cx - total_w / 2;
            int iy      = fy + fhh/2 - icon_sz/2;
            draw_icon(g_icon_scrap, ix, iy, icon_sz, WHITE);
            dtxt(sval, ix + icon_sz + gap, iy, 13, C_GREEN);
        }
    }

    int lx = M_PAD * 3;
    int rw = vw - lx * 2;
    int y  = M_PAD + 140;
    draw_text_boxed(TextFormat("Campagnes terminées : %d     Meilleure vague : %d",
                 meta->campaigns_completed, meta->best_wave),
             lx, y, 10, C_TEXT);
    y += fh(10) + 3 + 5;
    {   /* panneau opaque derrière toute la liste */
        int lh = UPGRADE_COUNT * (UPG_ROW_H + UPG_ROW_GAP) + M_IN * 2;
        Rectangle lp = {(float)(lx - M_IN), (float)y, (float)(rw + M_IN * 2), (float)lh};
        DrawRectangleRounded(lp, (float)PANEL_R / lh, 6, (Color){8, 5, 2, 238});
        DrawRectangleRoundedLinesEx(lp, (float)PANEL_R / lh, 6, 1.2f, C_BORDER);
    }
    y += M_IN;

    for (int i = 0; i < UPGRADE_COUNT; i++) {
        int hov = vhov(lx, y, rw, UPG_ROW_H);
        if (hov) m->sel_upg = i;
        int is_sel = (m->sel_upg == i);
        int ty = y + UPG_ROW_H/2;               /* axe vertical de la ligne */

        Rectangle row = {(float)lx,(float)y,(float)rw, UPG_ROW_H};
        DrawRectangleRounded(row, (float)PANEL_R/UPG_ROW_H, 5,
            is_sel ? (Color){32, 20, 5, 255} : (Color){16, 10, 4, 255});
        if (is_sel)
            DrawRectangleRoundedLinesEx(row, (float)PANEL_R/UPG_ROW_H, 5, 1.0f, C_GOLD);

        dtxt(UPGRADE_NAMES[i], lx + M_IN + 4, ty - fh(12)/2, 12,
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

        // Pastilles de niveau — colonne fixe, alignées d'une ligne à l'autre
        int pip_w = 13, pip_h = 9, pip_gap = 3;
        int pip_x = lx + UPG_PIP_COL;
        for (int s = 0; s < maxlvl; s++) {
            Color pc = (s < lvl) ? C_GOLD : (Color){35,25,8,255};
            Rectangle pip = {(float)(pip_x + s*(pip_w+pip_gap)),
                             (float)(ty - pip_h/2), (float)pip_w, (float)pip_h};
            DrawRectangleRounded(pip, 0.4f, 3, pc);
            DrawRectangleRoundedLinesEx(pip, 0.4f, 3, 0.8f, (Color){65,48,15,160});
        }

        // Effet — colonne fixe, rogné avant la colonne du coût
        int desc_x   = lx + UPG_DESC_COL;
        int desc_max = lx + rw - UPG_COST_W - desc_x - M_IN;
        char udesc[48];
        clip_text(UPGRADE_DESC[i], desc_max, 10, udesc, sizeof(udesc));
        dtxt(udesc, desc_x, ty - fh(10)/2, 10, is_sel ? C_TEXT : (Color){140, 122, 84, 255});

        // Coût / MAX à droite
        int cost = meta_upgrade_cost(meta, i);
        int cost_x = lx + rw - UPG_COST_W + M_IN;
        if (cost > 0) {
            int can = meta->scrap >= cost;
            char cbuf[12];
            snprintf(cbuf, sizeof(cbuf), "%d", cost);
            int icon_sz = fh(12);
            draw_icon(g_icon_scrap, cost_x, ty - icon_sz/2, icon_sz, WHITE);
            dtxt(cbuf, cost_x + icon_sz + 4, ty - fh(12)/2, 12, can ? C_GOLD : C_RED);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (meta_upgrade((MetaProgress*)meta, i))
                    set_msg(m, "Amélioration achetée !");
                else
                    set_msg(m, "Ferraille insuffisante.");
            }
        } else {
            dtxt("MAX", cost_x + 10, ty - fh(12)/2, 12, C_GREEN);
        }
        y += UPG_ROW_H + UPG_ROW_GAP;
    }

    /* (le message temporaire est dessiné par menu_render_and_act) */
    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}
