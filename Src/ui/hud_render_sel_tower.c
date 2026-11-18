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

    /* ── Ligne d'astuce : synergie > minerai > rappel général ── */
    {
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

    /* ── Boutons d'amélioration (sous le portrait) ─ */
    {
        const int GAP2 = 6;
        const int ubw  = (max_w - GAP2 * 2) / 3;
        int uy = HUD_Y + M + PORTRAIT_SZ + GAP2;

        DrawLine(rx, uy - 3, rx + max_w, uy - 3, (Color){40, 25, 6, 120});

        /* Données des 3 boutons */
        typedef struct { const char *lbl; int cost; int lvl; Color col; } UB;
        UB ubs[3] = {
            { "DÉGÂTS", tower_upg_next_cost_dmg  (tw), tw->upg_dmg,   {231, 100,  60, 255} },
            { "PORTÉE", tower_upg_next_cost_range(tw), tw->upg_range,  { 82, 155, 200, 255} },
            { "CADENCE",tower_upg_next_cost_rate (tw), tw->upg_rate,   {155,  89, 182, 255} },
        };
        const Rectangle *upg_rects[3] = {
            &ui->upg_dmg_btn, &ui->upg_range_btn, &ui->upg_rate_btn
        };
        Vector2 vm2 = virt_mouse();
        for (int _u = 0; _u < 3; _u++) {
            const Rectangle *ur = upg_rects[_u];
            int _hov    = CheckCollisionPointRec(vm2, *ur);
            int _is_max = (ubs[_u].cost < 0);
            int _afford = !_is_max && (gs->gold >= ubs[_u].cost);
            Color _bg   = _is_max ? (Color){6, 6, 3, 220}
                        : _hov    ? (Color){20, 12, 4, 235}
                                  : (Color){10, 6, 2, 220};
            Color _brd  = _is_max ? (Color){50, 40, 15, 120}
                        : _afford ? ubs[_u].col
                                  : (Color){60, 45, 15, 140};
            DrawRectangleRounded(*ur, 0.15f, 4, _bg);
            DrawRectangleRoundedLinesEx(*ur, 0.15f, 4, 1.2f, hud_sel_col(_brd));

            /* Ligne 1 : étiquette type (y+2, fs=9) */
            char _ltxt[12];
            snprintf(_ltxt, sizeof(_ltxt), "%s", ubs[_u].lbl);
            int _lw = mtxt(_ltxt, 9);
            Color _lc = _is_max ? (Color){50, 40, 15, 180} : ubs[_u].col;
            dtxt(_ltxt,
                 (int)(ur->x + ur->width/2 - _lw/2),
                 (int)(ur->y + 2), 9, _lc);

            if (_is_max) {
                /* MAX sur 2 lignes fusionnées */
                int _mw = mtxt("MAX", 9);
                dtxt("MAX",
                     (int)(ur->x + ur->width/2 - _mw/2),
                     (int)(ur->y + 18), 9,
                     (Color){100, 80, 25, 200});
            } else {
                /* Ligne 2 : niveau X/5 (y+16, fs=8) */
                char _lvltxt[10];
                snprintf(_lvltxt, sizeof(_lvltxt), "niv %d/%d",
                         ubs[_u].lvl, TOWER_UPG_MAX);
                int _nlw = mtxt(_lvltxt, 8);
                dtxt(_lvltxt,
                     (int)(ur->x + ur->width/2 - _nlw/2),
                     (int)(ur->y + 16), 8,
                     (Color){148, 128, 95, 220});

                /* Ligne 3 : coût (y+27, fs=8) */
                char _ctxt[16];
                snprintf(_ctxt, sizeof(_ctxt), "%d or", ubs[_u].cost);
                int _cw = mtxt(_ctxt, 8);
                dtxt(_ctxt,
                     (int)(ur->x + ur->width/2 - _cw/2),
                     (int)(ur->y + 27), 8,
                     _afford ? (Color){220, 145, 30, 255}
                             : (Color){100, 55, 25, 200});
            }
        }
        (void)ubw; (void)uy;
    }

    /* ── Bouton RÉPARER TOUR + bouton VENDRE ─────────── */
    {
        /* Réparer */
        Rectangle rb   = ui->repair_tower_btn;
        float     rrnd = (float)UI_RADIUS / rb.height;
        Vector2   vm3  = virt_mouse();
        int _hov_r = CheckCollisionPointRec(vm3, rb);
        int _needs_rep = (tw->hp < TOWER_MAX_HP - 0.5f);
        int _can_rep   = _needs_rep && (gs->gold >= TOWER_REPAIR_COST);
        Color _rbg = _can_rep  ? (_hov_r ? (Color){3, 20, 8, 235} : (Color){3, 12, 6, 210})
                               : (Color){8, 6, 3, 190};
        Color _rbd = _can_rep  ? (Color){28, 130, 55, 200}
                               : (Color){45, 35, 12, 120};
        DrawRectangleRounded(rb, rrnd, 4, _rbg);
        DrawRectangleRoundedLinesEx(rb, rrnd, 4, 1.2f, hud_sel_col(_rbd));
        /* Tour intacte : un seul libellé (l'ancien « OK » en 2e ligne
           chevauchait « Rep. N or » : le bouton fait 1 ligne de haut). */
        char _rptxt[24];
        if (_needs_rep)
            snprintf(_rptxt, sizeof(_rptxt), "Réparer %d or", TOWER_REPAIR_COST);
        else
            snprintf(_rptxt, sizeof(_rptxt), "Tour intacte");
        int _rpw = mtxt(_rptxt, 10);
        Color _rpc = _can_rep   ? (Color){42, 185, 90, 255}
                   : _needs_rep ? (Color){55, 42, 22, 200}
                                : (Color){42, 120, 70, 170};
        dtxt(_rptxt,
             (int)(rb.x + rb.width/2 - _rpw/2),
             (int)(rb.y + rb.height/2 - fh(10)/2), 10, _rpc);
    }

    // Bouton VENDRE
    {
        Rectangle sb   = ui->sell_btn;
        float     srnd = (float)UI_RADIUS / sb.height;
        Vector2   m    = virt_mouse();
        int hov = CheckCollisionPointRec(m, sb);
        DrawRectangleRounded(sb, srnd, 6,
            hov ? (Color){48,8,8,255} : (Color){20,4,4,255});
        DrawRectangleRoundedLinesEx(sb, srnd, 6, 1.5f,
            hud_sel_col(hov ? (Color){192,48,48,255} : (Color){90,18,18,255}));
        int refund = tower_sell_refund(tw, &gs->map);
        snprintf(buf, sizeof(buf), "Vendre  +%d or", refund);
        int bw = mtxt(buf, 13);
        dtxt(buf, (int)(sb.x + sb.width/2 - bw/2),
                 (int)(sb.y + sb.height/2 - 7),
                 13, (Color){192,58,42,255});
    }

    // Bouton APPLIQUER MATÉRIAU
    if (ui->apply_mat_visible && gs->inventory_count > 0) {
        /* Borne de sécurité sur l'index sélectionné */
        int _sidx = ui->sel_mat_idx;
        if (_sidx < 0 || _sidx >= gs->inventory_count) _sidx = 0;
        MaterialType mat = gs->inventory[_sidx];

        Rectangle ab   = ui->apply_mat_btn;
        float     arnd = (float)UI_RADIUS / ab.height;
        Vector2   mv   = virt_mouse();
        int hov = CheckCollisionPointRec(mv, ab);
        DrawRectangleRounded(ab, arnd, 6,
            hov ? (Color){4,26,36,255} : (Color){3,14,20,255});
        DrawRectangleRoundedLinesEx(ab, arnd, 6, 1.5f,
            hud_sel_col(hov ? (Color){55,165,195,255} : (Color){24,82,100,255}));

        /* Label : matériau + index si plusieurs en inventaire */
        if (gs->inventory_count > 1) {
            /* Flèches de cycle */
            dtxt("<", (int)(ab.x + 5),
                 (int)(ab.y + ab.height/2 - 8), 14,
                 (Color){62,175,200,200});
            dtxt(">", (int)(ab.x + ab.width - 12),
                 (int)(ab.y + ab.height/2 - 8), 14,
                 (Color){62,175,200,200});
            clip_text(TextFormat("[%d/%d] %s",
                                _sidx + 1, gs->inventory_count,
                                MATERIAL_NAMES[mat]),
                      max_w - 40, 11, buf, sizeof(buf));
            int bw2 = mtxt(buf, 11);
            dtxt(buf, (int)(ab.x + ab.width/2 - bw2/2),
                     (int)(ab.y + ab.height/2 - 7),
                     11, UI_INFO);
        } else {
            clip_text(TextFormat("+ Appliquer  %s", MATERIAL_NAMES[mat]),
                      max_w - M, 13, buf, sizeof(buf));
            int bw2 = mtxt(buf, 13);
            dtxt(buf, (int)(ab.x + ab.width/2 - bw2/2),
                     (int)(ab.y + ab.height/2 - 7),
                     13, UI_INFO);
        }

        /* P0.3 — Conseil de contre au moment de CHOISIR le
           matériau (ligne au-dessus du bouton APPLIQUER). */
        char adv[80];
        if (hud_counter_advice(gs, adv, sizeof(adv))) {
            char advc[80];
            clip_text(adv, max_w, 10, advc, sizeof(advc));
            dtxt(advc, (int)ab.x,
                 (int)(ab.y - fh(10) - 2), 10, UI_INFO);
        }
    }
    return 1;
}
