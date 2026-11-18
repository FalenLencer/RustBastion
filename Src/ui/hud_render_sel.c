/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_sel.c ─ Panneau droit du HUD : fiche de la sélection.
 *
 *  Extrait de hud_render.c (règle projet : fichiers < 500 lignes) ; le code
 *  des sections est inchangé, ui_render() les appelle dans le même ordre.
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "../game/campaign_data.h"   // campaign_difficulty_stage (aperçu de vague)
#include "ui_plate.h"                // boutons en plaque de métal
// ── Animation d'entrée du panneau droit (crossfade de sélection) ──
// Quand le sujet affiché change (tour / unité / outil), le contenu
// glisse de +10px et fond en 0.18 s. Les TEXTES passent par hud_sel_txt()
// (offset + alpha) ; les BOUTONS gardent positions et hitbox fixes,
// seule leur bordure est fondue via hud_sel_col(). Pas de BeginScissorMode :
// il opère en coordonnées écran et se désalignerait du canvas virtuel.
#define SEL_FADE_DUR     0.18f   // entrée du contenu (s)
#define SEL_PORTRAIT_DUR 0.22f   // pop du portrait (s)
#define SEL_SLIDE_PX     10.0f   // glissement horizontal du texte (px)
#define SEL_FS           12      // lignes de la fiche d'unité (PV, état…)
#define SEL_BTN_FS        8      // libellés des boutons de comportement
#define SEL_BTN_IDLE     ((Color){175, 158, 125, 255})   /* libellé d'un bouton au repos */
#define SEL_RED          ((Color){200,  70,  50, 255})   /* vendre / renvoyer            */
#define SEL_RED_TXT      ((Color){240, 130, 100, 255})

static int   g_sel_dx = 0;       // offset x courant des textes du panneau
static float g_sel_al = 1.0f;    // facteur alpha courant du contenu

// ── Portrait splash art ───────────────────────────────────────────
// Dessine une texture dans un carré psz×psz avec :
//   • fit proportionnel centré (pas de déformation)
//   • fond sombre + bordure colorée
// Retourne 1 si une image a bien été dessinée, 0 sinon.
static int draw_portrait(Texture2D tex, int px, int py, int psz, Color border) {
    if (tex.id == 0) return 0;

    // Fond sombre
    DrawRectangle(px, py, psz, psz, (Color){6, 3, 1, 220});
    DrawRectangleLinesEx((Rectangle){(float)px,(float)py,(float)psz,(float)psz},
                         1.5f, border);

    // Image redimensionnée proportionnellement
    float scale = fminf((float)psz / (float)tex.width,
                        (float)psz / (float)tex.height);
    int dw = (int)((float)tex.width  * scale);
    int dh = (int)((float)tex.height * scale);
    int ox = px + (psz - dw) / 2;
    int oy = py + (psz - dh) / 2;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){(float)ox, (float)oy, (float)dw, (float)dh},
        (Vector2){0, 0}, 0.0f, WHITE);
    return 1;
}

void hud_sel_txt(const char *s, int x, int y, int fs, Color c) {
    c.a = (unsigned char)((float)c.a * g_sel_al);
    dtxt(s, x + g_sel_dx, y, fs, c);
}
Color hud_sel_col(Color c) {
    c.a = (unsigned char)((float)c.a * g_sel_al);
    return c;
}

/* Bouton du panneau droit en plaque de métal (fondu avec le contenu) ;
   `active` = état retenu (comportement courant) : corps éclairé. */
/* Corps du bouton (fondu avec le contenu du panneau). */
static void btn_body(const Rectangle *r, Color acc, int enabled, int active, int grips) {
    int hov = enabled && CheckCollisionPointRec(virt_mouse(), *r);
    plate_alpha(g_sel_al);
    plate_button_ex(*r, acc, hov || active, enabled, grips);
    plate_alpha(1.0f);
}

void hud_plate_btn(const Rectangle *r, Color acc, int enabled, int active,
                   const char *lbl, int fs, Color lc) {
    btn_body(r, acc, enabled, active, 1);
    if (active)                     /* témoin allumé : l'état retenu se lit d'un coup d'œil */
        DrawRectangle((int)r->x + 6, (int)(r->y + r->height) - 5, (int)r->width - 12, 2,
                      hud_sel_col(acc));
    if (lbl) {
        int w = mtxt(lbl, fs);
        dtxt(lbl, (int)(r->x + r->width / 2) - w / 2, (int)(r->y + (r->height - fh(fs)) / 2),
             fs, hud_sel_col(lc));
    }
}

int hud_btn_keycap(const Rectangle *r, int key) {
    char kn[16];
    opts_key_name(key, kn, sizeof(kn));
    int kh = fh(HUD_KEY_FS) + 4;
    return plate_keycap_fs((int)r->x + 5, (int)(r->y + (r->height - kh) / 2), kn,
                           hud_sel_col(UI_ACCENT), HUD_KEY_FS);
}

void hud_plate_btn_key(const Rectangle *r, Color acc, int enabled, int key,
                       const char *lbl, int fs, Color lc) {
    btn_body(r, acc, enabled, 0, 0);                   /* sans prises : la touche les remplace */
    int kw = hud_btn_keycap(r, key);
    int x0 = (int)r->x + 5 + kw, x1 = (int)(r->x + r->width) - 5;   /* reste du bouton */
    while (mtxt(lbl, fs) > x1 - x0 - 4 && fs > HUD_BTN_FS_MIN) fs--;   /* réduire avant de couper */
    char b[48];
    clip_text(lbl, x1 - x0 - 4, fs, b, sizeof(b));
    dtxt(b, (x0 + x1) / 2 - mtxt(b, fs) / 2, (int)(r->y + (r->height - fh(fs)) / 2), fs,
         hud_sel_col(lc));
}

// Portrait avec pop d'entrée : scale 0.85 → 1.0 (ea_out_back) + bordure
// en fondu, centré dans la boîte psz × psz d'origine.
int hud_sel_portrait(Texture2D tex, int px, int py, int psz,
                     Color border, float t) {
    float k  = (t < SEL_PORTRAIT_DUR) ? ea_out_back(t / SEL_PORTRAIT_DUR) : 1.0f;
    float af = (t < SEL_PORTRAIT_DUR) ? (t / SEL_PORTRAIT_DUR) : 1.0f;
    int   sz = (int)((float)psz * (0.85f + 0.15f * k) + 0.5f);
    border.a = (unsigned char)((float)border.a * af);
    return draw_portrait(tex, px + (psz - sz)/2, py + (psz - sz)/2, sz, border);
}

/* Panneau droit : tour / unité / outil sélectionné (crossfade au changement). */
void hud_render_selection(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    // ════════════════════════════════════════════════
    // PANNEAU DROIT — sélection tour / outil / unité
    // ════════════════════════════════════════════════
    {
        const int rx    = HUD_RX(VIRT_W);
        const int max_w = HUD_RW;
        int py = HUD_Y + M;
        char buf[48];

        /* ── Crossfade de sélection : détection du sujet affiché ── */
        int subject = ui->selection.active   ? 1000 + ui->selection.tower_idx
                    : ui->sell_unit_idx >= 0 ? 2000 + ui->sell_unit_idx
                    : ui->selected_tool != TOOL_NONE ? 3000 + ui->selected_tool
                    : 0;
        static int   prev_subject = -1;
        static float sel_t        = 1e9f;
        static int   hp_snap      = 0;   // barre HP : snap au changement
        if (subject != prev_subject) {
            prev_subject = subject;
            hp_snap      = 1;
            if (subject != 0) sel_t = 0.0f;   // pas d'anim vers l'état vide
        }
        sel_t += ui_dt();
        {
            float sk = (sel_t < SEL_FADE_DUR)
                     ? ea_out_cubic(sel_t / SEL_FADE_DUR) : 1.0f;
            g_sel_dx = (int)(SEL_SLIDE_PX * (1.0f - sk));
            g_sel_al = sk;
        }

        if (ui->selection.active) {
            /* Fiche de la tour : hud_render_sel_tower.c (0 = tour vendue). */
            if (!hud_sel_tower(ui, gs, rx, max_w, py, sel_t)) goto panel_right_empty;
        } else if (ui->sell_unit_idx >= 0) {
            const Unit *u = &gs->units.units[ui->sell_unit_idx];
            if (!u->active) goto panel_right_empty;

            // Nom et couleur selon le type
            static const char *UNAMES[UNIT_TYPE_COUNT] = {
                "SOLDAT", "LOURD", "MEDIC", "CHIEN", "OUVRIER"
            };
            static const Color UCOLS[UNIT_TYPE_COUNT] = {
                {39,174,96,255},{41,128,185,255},{231,76,60,255},
                {243,156,18,255},{200,200,50,255}
            };
            Color ucol = (u->type < UNIT_TYPE_COUNT)
                       ? UCOLS[u->type] : UI_TEXT_DIM;
            const char *uname = (u->type < UNIT_TYPE_COUNT)
                              ? UNAMES[u->type] : "UNITÉ";

            hud_sel_txt(uname, rx, py, 16, ucol); py += fh(16) + 4;
            DrawLine(rx, py, rx+max_w, py, hud_sel_col((Color){44,44,8,140})); py += 7;

            // HP bar — la barre « coule » vers la vraie valeur (2.5×/s)
            float hr = u->max_hp > 0.0f ? u->hp / u->max_hp : 0.0f;
            Color hc = hr > 0.6f ? UI_SUCCESS
                     : hr > 0.3f ? UI_WARN
                                 : UI_DANGER;
            static float disp_hr = 0.0f;
            if (hp_snap) { disp_hr = hr; hp_snap = 0; }
            disp_hr += (hr - disp_hr) * fminf(1.0f, ui_dt() * 2.5f);
            /* Pas verticaux = fh() (un pas de 14 px sous un texte de 21 px
               posait la barre de PV sur le libellé). */
            snprintf(buf, sizeof(buf), "PV   %.0f / %.0f", u->hp, u->max_hp);
            hud_sel_txt(buf, rx, py, SEL_FS, hc); py += fh(SEL_FS);
            hud_draw_bar(rx, py, max_w, 6, fmaxf(disp_hr, 0.0f), hc, (Color){16,16,16,200});
            py += 6 + 4;

            // État
            if (u->type == UNIT_WORKER) {
                const char *ss;
                switch (u->state) {
                    case USTATE_GOTO_DEPOSIT: ss = "-> Dépôt";     break;
                    case USTATE_COLLECT:      ss = "Collecte...";  break;
                    case USTATE_GOTO_BASE:    ss = "<- Base";      break;
                    case USTATE_GOTO_CLEAR:   ss = "-> Obstacle";  break;
                    case USTATE_CLEARING:     ss = "Déblaiement..."; break;
                    default:                  ss = "En attente";   break;
                }
                hud_sel_txt(ss, rx, py, SEL_FS, UI_TEXT_DIM); py += fh(SEL_FS);
                /* Ligne de statut : avance py (sinon la ligne d'aide suivante
                   se dessinait par-dessus). Pas vertical = fh(11). */
                if (!gs->units.mining_enabled) {
                    hud_sel_txt("Minage : reprend pendant la vague", rx, py, 11,
                         (Color){200,180,50,200});
                    py += fh(11);
                } else if (u->state == USTATE_COLLECT) {
                    // Indicateur de ralentissement par ennemis
                    for (int _ej = 0; _ej < MAX_ENEMIES; _ej++) {
                        const Enemy *_e = &gs->enemies.enemies[_ej];
                        if (!_e->active || _e->dead || _e->spawn_delay > 0.0f) continue;
                        float _dx = _e->x - u->x, _dy = _e->y - u->y;
                        float _d2 = UNIT_WORKER_ENEMY_SLOW_RANGE * TILE_SIZE;
                        if (_dx*_dx + _dy*_dy <= _d2*_d2) {
                            hud_sel_txt("! Ennemi proche : collecte ralentie", rx, py, 11,
                                 (Color){231,76,60,220});
                            py += fh(11);
                            break;
                        }
                    }
                }
            } else {
                // Unité de combat : état + comportement
                static const char *BNAMES[5] = {
                    "Patrouille", "Garde tourelle", "Escorte", "Manuel", "Suit unité"
                };
                static const Color BCOLS[5] = {
                    {148,128,95,255},{192,57,43,255},{200,200,50,255},
                    {82,155,200,255},{231,76,60,255}
                };
                int beh = (int)u->behavior;
                if (beh < 0 || beh > 4) beh = 0;
                hud_sel_txt(BNAMES[beh], rx, py, SEL_FS, BCOLS[beh]); py += fh(SEL_FS);
            }

            if (u->has_material && u->carried_mat != MAT_NONE) {
                hud_sel_txt(TextFormat("Porte  %s", MATERIAL_NAMES[u->carried_mat]),
                         rx, py, SEL_FS, UI_INFO); py += fh(SEL_FS);
            }

            /* Ligne d'aide seulement si elle tient au-dessus de RENVOYER */
            if (u->type == UNIT_WORKER &&
                py + fh(11) <= (int)ui->unit_sell_btn.y - 2) {
                /* Une seule ligne d'aide (le minage est automatique ; cliquer
                   un gisement ne sert qu'à en CHOISIR un autre). */
                char _cb[64];
                snprintf(_cb, sizeof(_cb), "Clic : autre gisement / ruine (%d or)",
                         UNIT_WORKER_CLEAR_COST);
                hud_sel_txt(_cb, rx, py, 11, (Color){150,150,70,200});
            }

            // ── Boutons de comportement (unités de combat seulement) ──
            if (u->type != UNIT_WORKER) {
                static const char *BLBL[4] = {"PATROL","GARDE","ESCORT","MANUEL"};
                static const Color BCOL[4] = {
                    {175,150,100,255},{192,57,43,255},{200,200,50,255},{82,155,200,255}
                };
                for (int _b = 0; _b < 4; _b++) {
                    int _active = ((int)u->behavior == _b);
                    hud_plate_btn(&ui->unit_beh_btns[_b], BCOL[_b], 1, _active,
                                  BLBL[_b], SEL_BTN_FS,
                                  _active ? (Color){245, 235, 215, 255} : SEL_BTN_IDLE);
                }
                /* 5e bouton : SUIVRE (médic uniquement) */
                if (u->type == UNIT_MEDIC) {
                    int _act5 = (u->behavior == UBEH_FOLLOW_UNIT);
                    hud_plate_btn(&ui->unit_beh_btns[4], (Color){231, 76, 60, 255}, 1, _act5,
                                  "SUIVRE UNITÉ", SEL_BTN_FS,
                                  _act5 ? (Color){245, 235, 215, 255} : SEL_BTN_IDLE);
                }
            }

            // Bouton RENVOYER
            {
                int refund = (int)(UNIT_BASE_STATS[u->type].cost * UNIT_SELL_REFUND);
                snprintf(buf, sizeof(buf), "Renvoyer  +%d or", refund);
                hud_plate_btn(&ui->unit_sell_btn, SEL_RED, 1, 0, buf, 13, SEL_RED_TXT);
            }

        } else if (ui->selected_tool != TOOL_NONE) {
            const ToolInfo  ti   = hud_tool_info(ui->selected_tool);   /* chiffres réels */
            const ToolInfo *info = &ti;
            Color col = TOOL_COLORS[ui->selected_tool];

            // Portrait splash
            Texture2D ptex = ui_tool_is_tower(ui->selected_tool)
                ? g_tower_splash[ui_tool_to_tower(ui->selected_tool)]
                : g_unit_splash [ui_tool_to_unit (ui->selected_tool)];
            int pdone = hud_sel_portrait(ptex, rx + max_w - PORTRAIT_SZ,
                                           HUD_Y + M, PORTRAIT_SZ, col, sel_t);
            int text_w = pdone ? max_w - PORTRAIT_SZ - 6 : max_w;

            clip_text(info->name, text_w, 16, buf, sizeof(buf));
            hud_sel_txt(buf, rx, py, 16, col);
            py += 26;
            DrawLine(rx, py, rx+max_w, py, hud_sel_col((Color){44,28,8,140}));
            py += 7;

            // Stats 2 colonnes (fs=13) — mêmes libellés que le panneau de tour
            Color sc2 = UI_TEXT_DIM;
            char s_rng2[24];
            snprintf(s_rng2, sizeof(s_rng2), "Portée %.1f", info->range);
            int cx2 = rx + text_w - mtxt(s_rng2, 13);   /* calée avant le portrait */

            if (info->dmg > 0.0f) {
                hud_sel_txt(TextFormat("Dégâts %.0f",  info->dmg),
                         rx,  py, 13, sc2);
                hud_sel_txt(s_rng2, cx2, py, 13, sc2);   py += fh(13);
                hud_sel_txt(TextFormat("Cadence %.1f/s",info->rate),
                         rx,  py, 13, sc2);  py += fh(13);
            } else {                                   /* ouvrier : ne combat pas */
                hud_sel_txt("Non combatif", rx, py, 13, sc2);  py += fh(13) * 2;
            }

            if (ui_tool_is_tower(ui->selected_tool) &&
                ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0) {
                TowerType tt = ui_tool_to_tower(ui->selected_tool);
                int real_cost = tower_cost_on_tile(tt, &gs->map,
                                    ui->hovered_tile_x,
                                    ui->hovered_tile_y);
                int is_ruin = (gs->map.tiles[ui->hovered_tile_y]
                                            [ui->hovered_tile_x].type
                               == TILE_RUIN);
                if (is_ruin) {
                    hud_sel_txt(TextFormat("Coût  %d or (x2)", real_cost),
                             rx, py, 13, (Color){205,108,22,255}); py += fh(13);
                } else {
                    hud_sel_txt(TextFormat("Coût  %d or", real_cost),
                             rx, py, 13, UI_ACCENT_DIM); py += fh(13);
                }
            } else {
                hud_sel_txt(TextFormat("Coût  %d or", info->cost),
                         rx, py, 13, UI_ACCENT_DIM); py += fh(13);
            }
            py += 3;
            clip_text(info->desc, max_w, 12, buf, sizeof(buf));
            hud_sel_txt(buf, rx, py, 12, UI_BORDER);

        } else {
            panel_right_empty:
            /* Au repos : renseignement de vague (composition, contre, raccourcis) */
            hud_render_intel(ui, gs, rx, max_w);
        }

        /* Fin du panneau droit : helpers redevenus neutres pour le reste */
        g_sel_dx = 0;
        g_sel_al = 1.0f;
    }
}
