/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_sel_tower.c ─ Fiche de la tour sélectionnée (panneau droit).
 *
 *  Extrait de hud_render.c (règle projet : fichiers < 500 lignes) ; le code
 *  des sections est inchangé, ui_render() les appelle dans le même ordre.
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "../game/campaign_data.h"   // campaign_difficulty_stage (aperçu de vague)

#define TP_TALLY_FS  8   /* bilan de la tour, au pied du portrait */

/* Fiche de la tour sélectionnée (rx, max_w, py, sel_t : repères du panneau
   droit). Retourne 0 si la tour n'existe plus (panneau vide). */
int hud_sel_tower(const UIState *ui, const GameState *gs,
                  int rx, int max_w, int py, float sel_t) {
    HUD_FRAME_LOCALS;
    char buf[48];
    const Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
    if (!tw->active) return 0;   /* tour vendue : le panneau retombe à vide */

    const TowerStats *st = &TOWER_BASE_STATS[tw->type];
    Color col = TOWER_FILL[tw->type];

    // Portrait splash (pop d'entrée)
    int pdone = hud_sel_portrait(g_tower_splash[tw->type],
                              rx + max_w - PORTRAIT_SZ, HUD_Y + M,
                              PORTRAIT_SZ, col, sel_t);
    int text_w = pdone ? max_w - PORTRAIT_SZ - 6 : max_w;

    /* Bilan de la tour (depuis sa pose), en bandeau au pied du portrait */
    {
        const TowerTally *tt = tower_tally(ui->selection.tower_idx);
        char l1[24], l2[24];
        if (tt->dmg >= 1000.0f) snprintf(l1, sizeof(l1), "%.1fk dégâts", tt->dmg / 1000.0f);
        else                    snprintf(l1, sizeof(l1), "%.0f dégâts", tt->dmg);
        snprintf(l2, sizeof(l2), "%d kill%s", tt->kills, tt->kills > 1 ? "s" : "");
        int bx = rx + max_w - PORTRAIT_SZ, cx = bx + PORTRAIT_SZ / 2;
        int by = HUD_Y + M + PORTRAIT_SZ - 2 * fh(TP_TALLY_FS) - 2;
        DrawRectangle(bx + 2, by, PORTRAIT_SZ - 4, 2 * fh(TP_TALLY_FS) + 1,
                      hud_sel_col((Color){0, 0, 0, 185}));
        hud_sel_txt(l1, cx - mtxt(l1, TP_TALLY_FS) / 2, by, TP_TALLY_FS, (Color){240, 200, 120, 255});
        hud_sel_txt(l2, cx - mtxt(l2, TP_TALLY_FS) / 2, by + fh(TP_TALLY_FS), TP_TALLY_FS,
                    (Color){225, 215, 195, 255});
    }

    // Nom (fs=16)
    clip_text(st->name, text_w, 16, buf, sizeof(buf));
    hud_sel_txt(buf, rx, py, 16, col);
    py += fh(16) + 4;
    /* Trait s'arrête avant le portrait (text_w = zone de texte sans le splash) */
    DrawLine(rx, py, rx + text_w, py, hud_sel_col((Color){44, 28, 8, 140}));
    py += 7;

    // Stats — 2 colonnes (fs=13)
    Color sc   = UI_TEXT_DIM;
    Color typc = (Color){82,155,200,255};
    Color matc = (Color){62,172,192,255};

    /* Budget vertical : tout doit tenir AU-DESSUS des boutons
       d'amélioration (HUD_Y + M + PORTRAIT_SZ). L'ancienne pile
       (stats + « Niv » + Type + Mat + astuce) débordait dessous.
       « Niv » affichait Tower.level, jamais incrémenté (toujours 0). */
    char s_dmg[24], s_rng[24], s_rate[24], s_typ[24];
    snprintf(s_dmg,  sizeof(s_dmg),  "Dégâts %.0f",    tw->damage);
    snprintf(s_rng,  sizeof(s_rng),  "Portée %.1f",    tw->range);
    snprintf(s_rate, sizeof(s_rate), "Cadence %.1f/s", tw->fire_rate);
    snprintf(s_typ,  sizeof(s_typ),  "%s", DAMAGE_NAMES[tw->dmg_type]);
    /* 2e colonne calée à droite de la zone de texte (avant le portrait) */
    int w_c2 = mtxt(s_rng, 13);
    if (mtxt(s_typ, 13) > w_c2) w_c2 = mtxt(s_typ, 13);
    int col2 = rx + text_w - w_c2;

    hud_sel_txt(s_dmg,  rx,   py, 13, sc);
    hud_sel_txt(s_rng,  col2, py, 13, sc);   py += 17;
    hud_sel_txt(s_rate, rx,   py, 13, sc);
    hud_sel_txt(s_typ,  col2, py, 13, typc); py += 17;

    /* ── Ligne d'astuce : contre de la vague (minerai en main) > synergie >
          minerai > rappel général. Le contre vivait au-dessus du bouton de
          minerai, sur la rangée [CIBLE][RÉPARER][VENDRE]. ── */
    char adv[80];
    if (ui->apply_mat_visible && gs->inventory_count > 0 &&
        hud_counter_advice(gs, adv, sizeof(adv))) {
        clip_text(adv, text_w, 9, buf, sizeof(buf));
        hud_sel_txt(buf, rx, py, 9, UI_INFO);
        py += fh(9);
    } else {
        const char *hint = (tw->dmg_type == DMG_CRYO)
            ? "Synergie : gelé -> +30% dégâts subis"
            : (tw->dmg_type == DMG_POISON)
            ? "Synergie : corrodé -> +25% phys/feu"
            : (tw->material != MAT_NONE)
            ? TextFormat("Minerai : %s", MATERIAL_NAMES[tw->material])
            : "Frappe + fort les ennemis gelés / corrodés";
        Color hc = (tw->material != MAT_NONE) ? matc
                                              : (Color){120, 200, 140, 230};
        clip_text(hint, text_w, 9, buf, sizeof(buf));
        hud_sel_txt(buf, rx, py, 9, hc);
        py += fh(9);
    }

    /* ── Améliorations (touche + libellé, niveau · coût) ─ */
    {
        typedef struct { const char *lbl; int cost; int lvl; Color col; int key; } UB;
        UB ubs[3] = {
            { "DÉGÂTS", tower_upg_next_cost_dmg  (tw), tw->upg_dmg,  {231, 100,  60, 255}, HUD_KEY_UPG_DMG   },
            { "PORTÉE", tower_upg_next_cost_range(tw), tw->upg_range, { 82, 155, 200, 255}, HUD_KEY_UPG_RANGE },
            { "CADENCE",tower_upg_next_cost_rate (tw), tw->upg_rate,  {155,  89, 182, 255}, HUD_KEY_UPG_RATE  },
        };
        const Rectangle *upg_rects[3] = {
            &ui->upg_dmg_btn, &ui->upg_range_btn, &ui->upg_rate_btn
        };
        for (int _u = 0; _u < 3; _u++) {
            const Rectangle *ur = upg_rects[_u];
            int _is_max = (ubs[_u].cost < 0);
            int _afford = !_is_max && (gs->gold >= ubs[_u].cost);
            hud_plate_btn(ur, ubs[_u].col, _afford, 0, NULL, 0, WHITE);
            Rectangle kr = {ur->x, ur->y, ur->width, (float)(fh(9) + 8)};   /* 1re ligne */
            int kw = hud_btn_keycap(&kr, ubs[_u].key);

            /* Ligne 1 : étiquette (après la touche) ; ligne 2 : niveau · coût
               (ou MAX). Pas vertical = fh() (texte DANS le cadre de 3 px). */
            int cx = (int)(ur->x + ur->width / 2) + kw / 2;
            int ly = (int)ur->y + 4;
            Color _lc = _is_max ? (Color){95, 80, 50, 255} : ubs[_u].col;
            dtxt(ubs[_u].lbl, cx - mtxt(ubs[_u].lbl, 9) / 2, ly, 9, hud_sel_col(_lc));
            ly += fh(9) + 1;
            char _l2[24];
            if (_is_max) snprintf(_l2, sizeof(_l2), "MAX");
            else         snprintf(_l2, sizeof(_l2), "%d/%d · %d or",
                                  ubs[_u].lvl, TOWER_UPG_MAX, ubs[_u].cost);
            cx = (int)(ur->x + ur->width / 2);
            dtxt(_l2, cx - mtxt(_l2, 8) / 2, ly, 8,
                 hud_sel_col(_is_max ? (Color){120, 100, 60, 255}
                           : _afford ? (Color){240, 168, 45, 255}
                                     : (Color){200, 85, 60, 255}));
        }
    }

    /* ── [CIBLE] [RÉPARER] [VENDRE] ── */
    {
        snprintf(buf, sizeof(buf), "Cible : %s",
                 TARGET_MODE_NAMES[(unsigned)tw->target_mode < TGT_COUNT ? tw->target_mode : 0]);
        hud_plate_btn_key(&ui->target_btn, (Color){210, 190, 140, 255}, 1, HUD_KEY_TARGET,
                          buf, 10, (Color){235, 220, 185, 255});

        int _needs_rep = (tw->hp < TOWER_MAX_HP - 0.5f);
        int _can_rep   = _needs_rep && (gs->gold >= TOWER_REPAIR_COST);
        if (_needs_rep) snprintf(buf, sizeof(buf), "Réparer %d", TOWER_REPAIR_COST);
        else            snprintf(buf, sizeof(buf), "Intacte");
        hud_plate_btn_key(&ui->repair_tower_btn, (Color){60, 190, 100, 255}, _can_rep,
                          HUD_KEY_REPAIR, buf, 10,
                          _can_rep   ? (Color){120, 230, 150, 255}
                        : _needs_rep ? (Color){200, 85, 60, 255}
                                     : (Color){100, 150, 110, 255});

        /* Posée pendant cette préparation : remboursée à 100 % (annuler) */
        int fresh  = tower_is_fresh(&gs->towers, ui->selection.tower_idx);
        int refund = tower_refund(&gs->towers, ui->selection.tower_idx, &gs->map);
        snprintf(buf, sizeof(buf), fresh ? "Annuler +%d" : "Vendre +%d", refund);
        hud_plate_btn_key(&ui->sell_btn, fresh ? (Color){230, 170, 60, 255} : (Color){200, 70, 50, 255},
                          1, HUD_KEY_SELL, buf, 10,
                          fresh ? (Color){250, 210, 120, 255} : (Color){240, 130, 100, 255});
    }

    // Bouton APPLIQUER MATÉRIAU
    if (ui->apply_mat_visible && gs->inventory_count > 0) {
        /* Borne de sécurité sur l'index sélectionné */
        int _sidx = ui->sel_mat_idx;
        if (_sidx < 0 || _sidx >= gs->inventory_count) _sidx = 0;
        MaterialType mat = gs->inventory[_sidx];

        Rectangle ab   = ui->apply_mat_btn;
        hud_plate_btn(&ab, ((int)mat >= 0 && (int)mat < MAT_COUNT) ? MATERIAL_COLORS[mat] : UI_INFO,
                      1, 0, NULL, 0, WHITE);

        /* Label : matériau + index si plusieurs en inventaire */
        if (gs->inventory_count > 1) {
            /* Flèches de cycle (dans le cadre, après les prises) */
            dtxt("<", (int)(ab.x + 16),
                 (int)(ab.y + ab.height/2 - fh(14)/2), 14,
                 (Color){62,175,200,200});
            dtxt(">", (int)(ab.x + ab.width - 16 - mtxt(">", 14)),
                 (int)(ab.y + ab.height/2 - fh(14)/2), 14,
                 (Color){62,175,200,200});
            clip_text(TextFormat("[%d/%d] %s",
                                _sidx + 1, gs->inventory_count,
                                MATERIAL_NAMES[mat]),
                      max_w - 60, 11, buf, sizeof(buf));
            int bw2 = mtxt(buf, 11);
            dtxt(buf, (int)(ab.x + ab.width/2 - bw2/2),
                     (int)(ab.y + (ab.height - fh(11)) / 2),
                     11, UI_INFO);
        } else {
            clip_text(TextFormat("+ Appliquer  %s", MATERIAL_NAMES[mat]),
                      max_w - 40, 11, buf, sizeof(buf));
            int bw2 = mtxt(buf, 11);
            dtxt(buf, (int)(ab.x + ab.width/2 - bw2/2),
                     (int)(ab.y + (ab.height - fh(11)) / 2),
                     11, UI_INFO);
        }
        /* (Le conseil de contre est sur la ligne d'astuce, plus haut.) */
    }
    return 1;
}
