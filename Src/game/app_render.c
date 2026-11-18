/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  app_render.c ─ Rendu de la partie (carte, entités, HUD, interludes, MP).
 *
 *  Extrait d'app.c (règle projet : fichiers < 500 lignes) ; fonction
 *  déplacée à l'identique, appelée par app_update().
 */

#include "app.h"
#include "game_init.h"   // inclut menu.h pour CustomConfig
#include "save.h"
#include "meta.h"
#include "../engine/audio.h"
#include "../engine/canvas.h"
#include "../engine/window.h"
#include "../ui/renderer.h"
#include "../ui/tile_art.h"
#include "../ui/entity_fx.h"  // vie des unités/ennemis : apparitions, attaques, tirs
#include "../ui/ambient.h"    // météo d'ambiance 2D par thème
#include "../ui/hud.h"
#include "../ui/interlude.h"
#include "../ui/story_scene.h"   /* briefing : consigne retenue */
#include "../ui/ui_palette.h"   /* UI_ACCENT (notification d'ordre) */
#include "campaign_data.h"
#include "../ui/ui_utils.h"   // DrawText/MeasureText → g_font (support accents)
#include "../map/theme.h"
#include "../combat/fx.h"     // effets de jus (particules, secousse, pops)
#include "app_tutorial.h"     // tutoriel guidé (module extrait)
#include "app_mp.h"           // sous-système multijoueur (module extrait)
#include "app_flow.h"         // fins d'acte/partie (module extrait)
#include "hazard.h"           // hasards de terrain (rendu sol + infobulle)
#include "../net/net_upnp.h"  // UPNP_HOOK : ouverture auto du port (module isolé)
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ════════════════════════════════════════════════════════════════
// ÉCRAN JEU — RENDU
// ════════════════════════════════════════════════════════════════
int app_game_render(AppContext *ctx) {
    GameState *gs = &ctx->gs;

    ClearBackground(theme_get(gs->map.theme)->palette.bg);

    /* Décale la carte horizontalement pour centrer dans le canvas large.
       + secousse caméra (jus) : décalage purement visuel (le mappage souris
       reste sur g_map_x_off non secoué). */
    Camera2D map_cam = {0};
    Vector2 _mo = map_origin();   // inclut le zoom joueur + pan (source unique)
    map_cam.offset = (Vector2){_mo.x + fx_shake_dx(), _mo.y + fx_shake_dy()};
    map_cam.zoom   = map_eff_scale();
    BeginMode2D(map_cam);
        render_map(&gs->map, &gs->towers, 1);   // tour sur ruine → sol déblayé, socles
        hazard_render(&gs->map);          // tronçons piégés (braises, mare…)
        tile_art_draw_spawns(&gs->map);   // portails d'invasion
        render_spawn_exclusion_zones(&gs->map);
        render_bases(&gs->map);
        render_deposits(&gs->map, &gs->towers, gs->ui.hovered_tile_x,
                        gs->ui.hovered_tile_y, gs->wave_manager.number);
        render_dropped_mats(gs->dropped_mats, gs->dropped_mat_count);
        /* Vie des entités : apparitions + coups portés, poussière au sol */
        ent_fx_update(&gs->units, &gs->enemies, &gs->towers, &gs->map);
        ent_fx_draw_ground(&gs->units, &gs->enemies);
        render_towers(&gs->towers);
        render_units(&gs->units);
        render_enemies(&gs->enemies);
        render_projectiles(&gs->towers);
        render_combat_fx();   // tirs des unités et des ennemis
        fx_render_world();   // particules + popups d'or (jus)
        hazard_render_label(&gs->map, gs->ui.hovered_tile_x, gs->ui.hovered_tile_y);
    EndMode2D();

    ambient_render(gs->map.theme);   // météo d'ambiance (avant le HUD)

    // Bulle du tutoriel amarrée en haut → les bandeaux du haut passent dessous.
    gs->ui.top_reserved_y = 0;
    if (ctx->tutorial_active) {
        Rectangle tp = tutorial_panel_rect(ctx);
        if (tp.y < (float)(g_canvas_virt_h / 2))
            gs->ui.top_reserved_y = (int)(tp.y + tp.height) + 8;
    }
    ui_render(&gs->ui, gs);

    /* ── Indicateur de PAUSE TACTIQUE (solo) ─────────────────── */
    if (ctx->tactical_pause && !ctx->menu.paused) {
        int cxp = g_map_x_off + g_canvas_virt_w_base / 2;
        const char *pt = "PAUSE  -  ESPACE pour reprendre";
        int tw = mtxt(pt, 11);
        int bw = tw + 28, bx = cxp - bw/2, by = 40, bh = fh(11) + 12;
        if (gs->ui.top_reserved_y > by) by = gs->ui.top_reserved_y;  // sous la bulle tuto
        Rectangle pr = {(float)bx, (float)by, (float)bw, (float)bh};
        DrawRectangleRounded(pr, 0.4f, 6, (Color){10, 20, 40, 230});
        DrawRectangleRoundedLinesEx(pr, 0.4f, 6, 1.8f, (Color){90, 150, 220, 240});
        dtxt(pt, cxp - tw/2, by + (bh - fh(11))/2, 11, (Color){175, 215, 255, 255});
    }

    /* ── Tutoriel guidé (bulle d'aide par-dessus le jeu) ─────── */
    tutorial_render(ctx);

    /* ── Aide des commandes (touche H) + petit indice de découverte ─ */
    if (!ctx->menu.paused && !ctx->tutorial_active) {
        int cxh = g_map_x_off + g_canvas_virt_w_base / 2;
        if (ctx->show_help) {
            static const char *HL[] = {
                "ESC      menu pause",
                "ESPACE   pause tactique (solo) : geler pour réfléchir",
                "X        vitesse  x1 / x2 / x3",
                "1-9      choisir une tour (1-4) ou une unité (5-9)",
                "Molette  zoomer la carte    Clic-molette  déplacer",
                "Flèches  déplacer la carte zoomée",
                "Clic G   placer / sélectionner",
                "Glisser G  sélection de groupe d'unités",
                "Clic D   déplacer le groupe sélectionné",
                "G        envoyer de l'or au partenaire (Co-op)",
                "F        afficher / masquer les FPS",
                "H        fermer cette aide",
            };
            int n = (int)(sizeof(HL)/sizeof(HL[0]));
            int pw = 520, ph = 40 + n * (fh(10) + 4) + 10;
            int px = cxh - pw/2, py = (g_canvas_virt_h - UI_HUD_HEIGHT)/2 - ph/2;
            if (py < 8) py = 8;
            Rectangle pr = {(float)px,(float)py,(float)pw,(float)ph};
            DrawRectangleRounded(pr, 0.08f, 6, (Color){6, 10, 18, 240});
            DrawRectangleRoundedLinesEx(pr, 0.08f, 6, 2.0f, (Color){90, 150, 210, 235});
            const char *ht = "COMMANDES";
            dtxt(ht, cxh - mtxt(ht,13)/2, py + 10, 13, (Color){170, 215, 255, 255});
            int ly = py + 10 + fh(13) + 8;
            for (int i = 0; i < n; i++) {
                dtxt(HL[i], px + 22, ly, 10, (Color){210, 220, 232, 255});
                ly += fh(10) + 4;
            }
        } else {
            /* Coin bas-droit de la carte : en haut à droite, il chevauchait
               le panneau flottant « Vague ». */
            const char *hint = "[H] Aide";
            int hw = mtxt(hint, 9);
            dtxt(hint, g_map_x_off + g_canvas_virt_w_base - hw - 12,
                 g_canvas_virt_h - UI_HUD_HEIGHT - fh(9) - 4, 9,
                 (Color){120, 140, 165, 200});
        }
    }

    /* ── Bannière (mode de jeu + infos de carte) ─────────────── */
    /* • Démarrage : grand overlay centré sur la carte, fade-in 0.3 s,
         plein 4.2 s, fade-out 0.5 s  (banner_timer compte de 5 → 0).
       • Pause      : bandeau compact fixe en haut de la carte.      */
    /* Variables de titre hoistées au scope fonction : réutilisées par le
       bandeau de pause dessiné plus bas (après le voile du menu pause). */
    char big_title[80]  = {0};   /* titre principal (gros, centré)  */
    char line1    [120] = {0};   /* sous-titre / détails            */
    char line2    [80]  = {0};   /* info secondaire                 */
    Color ctitle = {255, 210,  80, 255};
    Color c1     = {200, 170,  60, 255};
    Color c2     = {150, 215, 110, 255};
    {
        if (gs->is_campaign && ctx->interlude == INTER_NONE) {
            const ActData *ad = campaign_act_get(gs->campaign_stage);
            const char *mut = campaign_mutator_name(campaign_mutator_for_stage(
                                  campaign_difficulty_stage(gs->campaign_stage)));
            snprintf(big_title, sizeof(big_title), "%s", ad->title);
            /* line1 : chapitre/acte + mutateur signature s'il y en a un */
            if (mut)
                snprintf(line1, sizeof(line1), "Ch.%d Acte %d   ·   %s",
                         ad->chapter + 1, ad->act + 1, mut);
            else
                snprintf(line1, sizeof(line1), "Campagne  —  Ch.%d  Acte %d",
                         ad->chapter + 1, ad->act + 1);
            snprintf(line2, sizeof(line2), "Objectif : %s",
                     ad->objective.description);
            ctitle = (Color){255, 220, 100, 255};
            c1     = (Color){180, 160,  80, 255};
        } else if (gs->is_endless) {
            int score = (int)((float)gs->wave_manager.number
                              * gs->endless_multiplier * 10.0f);
            snprintf(big_title, sizeof(big_title), "ENDLESS");
            snprintf(line1, sizeof(line1), "Série %d  ·  x%.1f  ·  Score %d",
                     gs->endless_series + 1, gs->endless_multiplier, score);
            int next_extr = 10 - (gs->wave_manager.number % 10);
            if (next_extr == 10 && gs->wave_manager.number > 0) next_extr = 0;
            if (next_extr > 0)
                snprintf(line2, sizeof(line2),
                         "Extraction dans %d vague(s)", next_extr);
            ctitle = (Color){160, 225, 255, 255};
            c1     = (Color){120, 185, 225, 255};
            c2     = (Color){ 90, 155, 200, 255};
        } else if (gs->is_custom) {
            const Theme *th = theme_get(gs->map.theme);
            snprintf(big_title, sizeof(big_title), "PARTIE PERSO");
            snprintf(line1, sizeof(line1), "%s  ·  %d spawn(s)  ·  %d base(s)",
                     th->name, gs->enemy_paths.count, gs->map.base_count);
            ctitle = (Color){200, 130, 255, 255};
            c1     = (Color){165, 100, 220, 255};
        } else {
            const Theme *th = theme_get(gs->map.theme);
            snprintf(big_title, sizeof(big_title), "ARCADE");
            snprintf(line1, sizeof(line1), "%s", th->name);
        }

        /* ── A) Overlay centré — intro au démarrage ───────────── */
        /* Pas par-dessus une fiche de découverte (rendue dans ui_render,
           AVANT ce point) : la bannière en masquait le nom et le sous-titre. */
        if (!ctx->menu.paused && ctx->banner_timer > 0.0f &&
            gs->ui.disc_count == 0) {
            float t  = ctx->banner_timer;   /* 5.0 → 0.0 */
            float af;
            if      (t > 4.7f) af = (5.0f - t) / 0.3f;  /* fade-in  0.3 s */
            else if (t < 0.5f) af = t / 0.5f;             /* fade-out 0.5 s */
            else               af = 1.0f;
            if (af < 0.0f) af = 0.0f;
            if (af > 1.0f) af = 1.0f;
            unsigned char a = (unsigned char)(af * 255.0f);

            if (a > 0 && big_title[0]) {
                const int FS_T = 26, FS_1 = 13, FS_2 = 10;
                const int PAD_H = 26, PAD_V = 18, GAP = 9;

                int cx = g_map_x_off + g_canvas_virt_w_base / 2;
                int cy = (g_canvas_virt_h - UI_HUD_HEIGHT) / 2;

                int twt = mtxt(big_title, FS_T);
                int tw1 = line1[0] ? mtxt(line1, FS_1) : 0;
                int tw2 = line2[0] ? mtxt(line2, FS_2) : 0;
                int mxw = twt > tw1 ? twt : tw1;
                if (tw2 > mxw) mxw = tw2;

                int ch = fh(FS_T);
                if (line1[0]) ch += GAP + 1 + GAP + fh(FS_1); /* +1 = séparateur */
                if (line2[0]) ch += GAP + fh(FS_2);

                int pw = mxw + PAD_H * 2;
                int ph = ch  + PAD_V * 2;
                float px = (float)(cx - pw / 2);
                float py = (float)(cy - ph / 2);

                /* Fond + cadre à équerres (identité militaire) */
                Rectangle _br = {px, py, (float)pw, (float)ph};
                DrawRectangleRounded(_br, 0.16f, 6,
                    (Color){3, 2, 0, (unsigned char)((int)a * 225 / 255)});
                DrawRectangleRoundedLinesEx(_br, 0.16f, 6, 2.0f,
                    (Color){(unsigned char)(ctitle.r/2), (unsigned char)(ctitle.g/2),
                            (unsigned char)(ctitle.b/2),
                            (unsigned char)((int)a * 190 / 255)});
                /* Filet d'accent supérieur */
                DrawRectangle((int)px + 10, (int)py + 6, pw - 20, 2,
                    (Color){ctitle.r, ctitle.g, ctitle.b,
                            (unsigned char)((int)a * 200 / 255)});
                /* Équerres d'angle */
                {
                    int L = 10;
                    unsigned char ca = (unsigned char)((int)a * 220 / 255);
                    Color cc = {ctitle.r, ctitle.g, ctitle.b, ca};
                    int x0 = (int)px + 4, y0 = (int)py + 4;
                    int x1 = (int)px + pw - 4, y1 = (int)py + ph - 4;
                    DrawRectangle(x0,     y0,     L, 2, cc); DrawRectangle(x0,     y0,     2, L, cc);
                    DrawRectangle(x1 - L, y0,     L, 2, cc); DrawRectangle(x1 - 2, y0,     2, L, cc);
                    DrawRectangle(x0,     y1 - 2, L, 2, cc); DrawRectangle(x0,     y1 - L, 2, L, cc);
                    DrawRectangle(x1 - L, y1 - 2, L, 2, cc); DrawRectangle(x1 - 2, y1 - L, 2, L, cc);
                }

                int ly = (int)py + PAD_V;

                /* Titre principal */
                dtxt(big_title, cx - twt / 2, ly, FS_T,
                     (Color){ctitle.r, ctitle.g, ctitle.b, a});
                ly += fh(FS_T);

                /* Séparateur fin + line1 */
                if (line1[0]) {
                    ly += GAP;
                    DrawLineEx(
                        (Vector2){px + 12.0f, (float)ly},
                        (Vector2){px + (float)pw - 12.0f, (float)ly},
                        1.0f,
                        (Color){ctitle.r / 3, ctitle.g / 3, ctitle.b / 3,
                                (unsigned char)((int)a * 150 / 255)});
                    ly += 1 + GAP;
                    dtxt(line1, cx - tw1 / 2, ly, FS_1,
                         (Color){c1.r, c1.g, c1.b, a});
                    ly += fh(FS_1);
                }

                /* line2 */
                if (line2[0]) {
                    ly += GAP;
                    dtxt(line2, cx - tw2 / 2, ly, FS_2,
                         (Color){c2.r, c2.g, c2.b,
                                 (unsigned char)((int)a * 200 / 255)});
                }
            }
        }

        /* Le bandeau de pause est dessiné plus bas, APRÈS le voile du
           menu pause, pour qu'il reste bien lisible (voir game_do_render). */
    }

    /* ── Interludes ───────────────────────────────────────────── */
    if (gs->phase == PHASE_GAMEOVER) {
        /* P0.2 : récap avec boutons — REJOUER seulement hors campagne
           (la campagne repart de la carte) et hors tuto/MP. */
        int allow_replay = !gs->is_campaign && !ctx->tutorial_active &&
                           !ctx->mp_in_game;
        int gc = interlude_render_gameover(gs, virt_mouse(),
                                           g_canvas_virt_w, g_canvas_virt_h,
                                           allow_replay);
        if (gc) ctx->gameover_choice = gc;
    }

    if (ctx->interlude == INTER_DIALOG_BEFORE) {
        /* Input ignoré le frame d'entrée (anti-bleed depuis butin/boutique). */
        int go = interlude_render_dialog_before(gs, virt_mouse(),
                     ctx->interlude == ctx->interlude_prev,
                     g_canvas_virt_w, g_canvas_virt_h);
        if (go) {
            /* ORDRE DU JOUR : appliqué ICI, au lancement — jamais au clic,
               sinon quitter pendant le briefing puis recharger le cumulerait. */
            int pk = story_scene_pick();
            const CampaignOrder *o = pk >= 0 ? campaign_order(gs->campaign_stage, pk) : NULL;
            if (o && campaign_order_affordable(o, gs->gold, gs->lives, gs->run.renfort)) {
                char eb[64], nb[96];
                game_apply_order(gs, o);
                snprintf(nb, sizeof(nb), "Ordre du jour : %s",
                         campaign_order_effect(o, eb, sizeof(eb)));
                ui_push_notif(&gs->ui, nb, UI_ACCENT);
            }
            ctx->interlude = INTER_NONE;
        }
    }
    if (ctx->interlude == INTER_DIALOG_AFTER) {
        interlude_render_dialog_after(gs, ctx->interlude_stars, ctx->interlude_scrap,
                                      ctx->interlude_last, virt_mouse(),
                                      ctx->interlude == ctx->interlude_prev,
                                      g_canvas_virt_w, g_canvas_virt_h);
    }
    if (ctx->interlude == INTER_EXTRACT) {
        /* Souris en coordonnées virtuelles (offset + échelle uniforme) */
        Vector2 vm = virt_mouse();
        interlude_render_extract(gs, g_canvas_virt_w, g_canvas_virt_h, vm);
    }
    if (ctx->interlude == INTER_DRAFT)
        interlude_render_draft(&gs->run, virt_mouse(),
                               g_canvas_virt_w, g_canvas_virt_h);
    if (ctx->interlude == INTER_SHOP)
        interlude_render_shop(&gs->run, SHOP_REROLL_COST, virt_mouse(),
                              g_canvas_virt_w, g_canvas_virt_h);

    /* ── Menu pause superposé ─────────────────────────────────── */
    if (ctx->menu.paused) {
        MenuAction pact = menu_render_and_act(&ctx->menu, &ctx->gs.meta,
                                              g_canvas_virt_w, g_canvas_virt_h);

        /* ── Bandeau d'info en haut — dessiné APRÈS le voile du menu ──
           pour rester parfaitement lisible (avant il était noyé sous le
           rectangle d'assombrissement plein écran du menu pause).        */
        if (big_title[0]) {
            char compact[256];   // assez large pour "titre — sous-titre"
            if (line1[0])
                snprintf(compact, sizeof(compact), "%s  —  %s", big_title, line1);
            else
                snprintf(compact, sizeof(compact), "%s", big_title);

            int fs = 12;
            int cx = g_map_x_off + g_canvas_virt_w_base / 2;
            int tw = mtxt(compact, fs);
            int bw = tw + 48;          /* place pour l'icône pause + marges */
            int bx = cx - bw / 2;
            int by = 10;
            int bh = fh(fs) + 16;
            Rectangle pr = {(float)bx, (float)by, (float)bw, (float)bh};

            /* Panneau opaque + filet d'accent + bordure vive */
            DrawRectangleRounded(pr, 0.35f, 6, (Color){12, 8, 3, 248});
            DrawRectangle(bx + 8, by + 5, bw - 16, 2,
                          (Color){ctitle.r, ctitle.g, ctitle.b, 235});
            DrawRectangleRoundedLinesEx(pr, 0.35f, 6, 2.0f,
                          (Color){ctitle.r, ctitle.g, ctitle.b, 240});

            /* Icône pause (deux barres) dans la marge gauche */
            int icy = by + bh/2 - 5;
            DrawRectangle(bx + 13, icy, 3, 10, (Color){ctitle.r, ctitle.g, ctitle.b, 240});
            DrawRectangle(bx + 19, icy, 3, 10, (Color){ctitle.r, ctitle.g, ctitle.b, 240});

            /* Texte clair et contrasté */
            dtxt(compact, cx - tw/2 + 12, by + (bh - fh(fs))/2, fs,
                 (Color){255, 244, 210, 255});

            /* line2 (objectif campagne, etc.) sous le bandeau */
            if (line2[0]) {
                int fs2 = 10;
                int tw2 = mtxt(line2, fs2);
                int by2 = by + bh + 5;
                Rectangle pr2 = {(float)(cx - tw2/2 - 10), (float)by2,
                                 (float)(tw2 + 20), (float)(fh(fs2) + 8)};
                DrawRectangleRounded(pr2, 0.4f, 5, (Color){12, 8, 3, 240});
                DrawRectangleRoundedLinesEx(pr2, 0.4f, 5, 1.2f,
                              (Color){c2.r, c2.g, c2.b, 170});
                dtxt(line2, cx - tw2/2, by2 + 4, fs2,
                     (Color){c2.r, c2.g, c2.b, 240});
            }
        }
        /* Sauvegarde rapide (sans quitter) */
        if (pact.save_and_quit == 2 && ctx->active_slot >= 0) {
            if (ctx->gs.is_campaign)
                campaign_save_write(&ctx->gs, ctx->active_slot,
                                    (int)ctx->interlude,
                                    ctx->interlude_scrap,
                                    ctx->interlude_stars,
                                    ctx->interlude_last);
            else
                save_write(&ctx->gs, ctx->active_slot);
        }
        /* Sauvegarder et retour au menu */
        if (pact.save_and_quit == 1) {
            if (ctx->active_slot >= 0) {
                if (ctx->gs.is_campaign)
                    campaign_save_write(&ctx->gs, ctx->active_slot,
                                        (int)ctx->interlude,
                                        ctx->interlude_scrap,
                                        ctx->interlude_stars,
                                        ctx->interlude_last);
                else
                    save_write(&ctx->gs, ctx->active_slot);
            }
            menu_refresh_slots(&ctx->menu);
            ctx->menu.paused = 0;
            ctx->menu.screen = MENU_TITLE;
            ctx->screen      = SCREEN_MENU;
            ctx->interlude   = INTER_NONE;
            audio_play_menu_music();
        }
        /* Quitter l'application — sauvegarde d'urgence */
        if (pact.quit_app) {
            if (ctx->active_slot >= 0) {
                if (ctx->gs.is_campaign)
                    campaign_save_write(&ctx->gs, ctx->active_slot,
                                        (int)ctx->interlude,
                                        ctx->interlude_scrap,
                                        ctx->interlude_stars,
                                        ctx->interlude_last);
                else
                    save_write(&ctx->gs, ctx->active_slot);
            }
            return 0;   /* quitter l'application */
        }
        if (pact.toggle_fs == 1) {
            ToggleFullscreen();
            ctx->menu.opts.fullscreen = IsWindowFullscreen();
        }
        if (pact.toggle_fs == 2 && !IsWindowFullscreen())
            window_apply_size(ctx->menu.opts.win_width,
                              ctx->menu.opts.win_height);
    }

    mp_render_overlay(ctx);   // mini-HUD rival + résultat (par-dessus tout)
    return 1;
}
