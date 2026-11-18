/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_world.c ─ Marques dessinées dans l'espace de la carte.
 *
 *  Extrait de hud_render.c (règle projet : fichiers < 500 lignes) ; le code
 *  des sections est inchangé, ui_render() les appelle dans le même ordre.
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "../game/campaign_data.h"   // campaign_difficulty_stage (aperçu de vague)

/* ── Marqueur « amélioration disponible » (au-dessus d'une tour) ──
   Flèche VERTE montante, et non plus un point jaune : le jaune se
   confondait avec les pastilles de niveau et avec l'or, et un point ne
   dit pas ce qu'il propose. Vert = action bénéfique dispo, flèche
   montante = « monter d'un cran ». */
#define UPG_ARROW_HW   5.0f   /* demi-largeur de la pointe (px)        */
#define UPG_ARROW_H    6.0f   /* hauteur de la pointe (px)             */
#define UPG_STEM_HW    2.0f   /* demi-largeur de la hampe (px)         */
#define UPG_STEM_H     4.0f   /* hauteur de la hampe (px)              */
#define UPG_EDGE_A     210    /* alpha max du contour sombre           */
static void draw_upgrade_arrow(float cx, float cy, unsigned char a) {
    Color fill = {90, 230, 120, a};
    Color edge = {24, 120,  64, (unsigned char)(a < UPG_EDGE_A ? a : UPG_EDGE_A)};
    /* Sommets dans le même ordre que perk_art.c (bas-gauche, bas-droit,
       pointe) : winding valide pour raylib en repère écran (y vers le bas). */
    DrawTriangle((Vector2){cx - UPG_ARROW_HW, cy},
                 (Vector2){cx + UPG_ARROW_HW, cy},
                 (Vector2){cx,                cy - UPG_ARROW_H}, fill);
    DrawRectangleV((Vector2){cx - UPG_STEM_HW, cy},
                   (Vector2){UPG_STEM_HW * 2.0f, UPG_STEM_H}, fill);
    /* Contour : la flèche doit rester lisible sur un toit de tour clair. */
    DrawLineEx((Vector2){cx - UPG_ARROW_HW, cy},
               (Vector2){cx, cy - UPG_ARROW_H}, 1.0f, edge);
    DrawLineEx((Vector2){cx + UPG_ARROW_HW, cy},
               (Vector2){cx, cy - UPG_ARROW_H}, 1.0f, edge);
    DrawLineEx((Vector2){cx - UPG_STEM_HW, cy + UPG_STEM_H},
               (Vector2){cx + UPG_STEM_HW, cy + UPG_STEM_H}, 1.0f, edge);
}

/* Overlays dans l'espace de la carte + rectangle de sélection. */
void hud_render_world_marks(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    // ════════════════════════════════════════════════
    // OVERLAYS DANS L'ESPACE DE LA CARTE
    // ════════════════════════════════════════════════
    {
        Camera2D map_cam = {0};
        map_cam.offset = map_origin();      // zoom joueur + pan (aligné sur la carte)
        map_cam.zoom   = map_eff_scale();
        BeginMode2D(map_cam);

        // Prévisualisation placement
        if (ui->selected_tool != TOOL_NONE &&
            ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0) {
            if (ui_tool_is_tower(ui->selected_tool)) {
                render_tower_preview(&gs->map, &gs->towers,
                                     ui_tool_to_tower(ui->selected_tool),
                                     ui->hovered_tile_x,
                                     ui->hovered_tile_y);
            } else {
                for (int b = 0; b < gs->map.base_count; b++) {
                    if (!gs->map.bases[b].active) continue;
                    float bpx = gs->map.bases[b].pos.x * TILE_SIZE + TILE_SIZE/2.0f;
                    float bpy = gs->map.bases[b].pos.y * TILE_SIZE + TILE_SIZE/2.0f;
                    DrawCircleLines((int)bpx, (int)bpy,
                                    UNIT_DEPLOY_RADIUS_TILES * TILE_SIZE,
                                    (Color){39, 174, 96, 100});
                }
            }
        }

        // Highlight dépôts si ouvrier sélectionné
        if (ui->worker_selected_idx >= 0) {
            float t = (float)GetTime();
            for (int d = 0; d < gs->map.deposit_count; d++) {
                const MaterialDeposit *dep = &gs->map.deposits[d];
                if (!dep->active) continue;
                int cx = dep->tile_x * TILE_SIZE + TILE_SIZE/2;
                int cy = dep->tile_y * TILE_SIZE + TILE_SIZE/2;
                float pulse = (sinf(t * 5.0f + (float)d) + 1.0f) * 0.5f;
                DrawCircleLines(cx, cy, TILE_SIZE/2 + 3,
                    (Color){192,192,42,
                            (unsigned char)(95 + (int)(pulse*100))});
            }
            /* Obstacle survolé : cadre ambre + coût. Rend le déblaiement
               DÉCOUVRABLE — sans ce repère, rien n'indique qu'une ruine
               est autre chose qu'un décor définitif. */
            if (ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0 &&
                unit_tile_clearable(&gs->map, &gs->towers,
                                    ui->hovered_tile_x,
                                    ui->hovered_tile_y)) {
                int hx = ui->hovered_tile_x * TILE_SIZE;
                int hy = ui->hovered_tile_y * TILE_SIZE;
                int afford = (gs->gold >= UNIT_WORKER_CLEAR_COST);
                Color oc = afford ? (Color){225, 165, 70, 235}
                                  : (Color){150, 100, 60, 190};
                DrawRectangle(hx, hy, TILE_SIZE, TILE_SIZE,
                              (Color){oc.r, oc.g, oc.b, 45});
                DrawRectangleLinesEx(
                    (Rectangle){(float)hx, (float)hy,
                                (float)TILE_SIZE, (float)TILE_SIZE},
                    2.0f, oc);
                char lb[24];
                snprintf(lb, sizeof(lb), "%d or", UNIT_WORKER_CLEAR_COST);
                int lw = mtxt(lb, 10);
                dtxt_o(lb, hx + TILE_SIZE/2 - lw/2, hy - fh(10) - 2, 10, oc);
            }
        }

        // Indicateurs d'amélioration : point pulsant sur les tours qu'on peut
        // réellement améliorer MAINTENANT (axe non-max ET or suffisant).
        {
            float t_dot = (float)GetTime();
            float pulse_dot = (sinf(t_dot * 4.0f) + 1.0f) * 0.5f;
            unsigned char pa = (unsigned char)(140 + (int)(pulse_dot * 100.0f));
            for (int i = 0; i < MAX_TOWERS; i++) {
                const Tower *tw = &gs->towers.towers[i];
                if (!tw->active) continue;
                /* Vérifie qu'au moins un axe est abordable (coût > 0 = pas au max) */
                int c_dmg   = tower_upg_next_cost_dmg  (tw);
                int c_range = tower_upg_next_cost_range (tw);
                int c_rate  = tower_upg_next_cost_rate  (tw);
                int can_upg = (c_dmg   > 0 && gs->gold >= c_dmg)  ||
                              (c_range > 0 && gs->gold >= c_range) ||
                              (c_rate  > 0 && gs->gold >= c_rate);
                if (!can_upg) continue;
                /* Flèche verte pulsante en BAS-droit de la tuile : en haut,
                   elle couvrait la tourelle 3D (et masquait sa rotation). */
                float dot_x = tw->tile_x * TILE_SIZE + TILE_SIZE - 6.0f;
                float dot_y = tw->tile_y * TILE_SIZE + TILE_SIZE - 3.0f;
                draw_upgrade_arrow(dot_x, dot_y, pa);
            }
        }

        // Portée au survol d'une tour posée
        if (ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0 &&
            ui->selected_tool == TOOL_NONE && !ui->selection.active) {
            int htx = ui->hovered_tile_x, hty = ui->hovered_tile_y;
            for (int i = 0; i < MAX_TOWERS; i++) {
                const Tower *tw = &gs->towers.towers[i];
                if (!tw->active || tw->tile_x != htx || tw->tile_y != hty) continue;
                float cx = tw->tile_x * TILE_SIZE + TILE_SIZE / 2.0f;
                float cy = tw->tile_y * TILE_SIZE + TILE_SIZE / 2.0f;
                float rad = tw->range * TILE_SIZE;
                Color rc = TOWER_FILL[tw->type];
                DrawCircle((int)cx, (int)cy, rad,
                           (Color){rc.r, rc.g, rc.b, 18});
                DrawCircleLines((int)cx, (int)cy, rad,
                                (Color){rc.r, rc.g, rc.b, 100});
                break;
            }
        }

        // Portée de la tour sélectionnée
        if (ui->selection.active) {
            int tidx = ui->selection.tower_idx;
            if (tidx >= 0 && tidx < MAX_TOWERS) {
                const Tower *tw = &gs->towers.towers[tidx];
                if (tw->active) {
                    float cx = tw->tile_x * TILE_SIZE + TILE_SIZE / 2.0f;
                    float cy = tw->tile_y * TILE_SIZE + TILE_SIZE / 2.0f;
                    float rad = tw->range * TILE_SIZE;
                    Color rc = TOWER_FILL[tw->type];
                    DrawCircle((int)cx, (int)cy, rad,
                               (Color){rc.r, rc.g, rc.b, 28});
                    DrawCircleLines((int)cx, (int)cy, rad,
                                   (Color){rc.r, rc.g, rc.b, 160});
                }
            }
        }

        // Anneaux de sélection sous les unités du GROUPE (+ marqueur de destination)
        if (ui->group_count > 0) {
            float t   = (float)GetTime();
            float pul = (sinf(t * 4.0f) + 1.0f) * 0.5f;
            for (int j = 0; j < MAX_UNITS; j++) {
                if (!ui->group_sel[j]) continue;
                const Unit *u = &gs->units.units[j];
                if (!u->active) continue;
                float r = u->size + 5.0f + pul * 2.0f;
                DrawCircleLines((int)u->x, (int)u->y, r,        (Color){120, 230, 140, 235});
                DrawCircleLines((int)u->x, (int)u->y, r + 1.0f, (Color){ 50, 150,  85, 150});
                if (u->behavior == UBEH_MANUAL && u->manual_moving) {
                    DrawLine((int)u->x, (int)u->y,
                             (int)u->manual_x, (int)u->manual_y,
                             (Color){120, 230, 140, 70});
                    DrawCircleLines((int)u->manual_x, (int)u->manual_y, 4,
                                    (Color){120, 230, 140, 150});
                }
            }
        }

        EndMode2D();
    }

    // Rectangle de sélection de groupe (glisser en cours) — espace canvas
    if (ui->box_dragging) {
        float x0 = fminf(ui->box_start.x, ui->box_cur.x);
        float y0 = fminf(ui->box_start.y, ui->box_cur.y);
        float bw = fabsf(ui->box_cur.x - ui->box_start.x);
        float bh = fabsf(ui->box_cur.y - ui->box_start.y);
        DrawRectangle((int)x0, (int)y0, (int)bw, (int)bh, (Color){120, 230, 140, 38});
        DrawRectangleLinesEx((Rectangle){x0, y0, bw, bh}, 1.5f,
                             (Color){120, 230, 140, 210});
    }
}
