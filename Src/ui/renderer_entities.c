/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  renderer_entities.c ─ Rendu 2D des ennemis et des unités.
 *
 *  Extrait de renderer.c (règle projet : fichiers < 500 lignes) ; fonctions
 *  déplacées à l'identique.
 */

#include "renderer.h"
#include "tile_art.h"   // pixel-art procédural : routes, spawns, bunkers
#include "render3d.h"   // rendu 3D des tours (textures pré-rendues)
#include "render3d_units.h"   // rendu 3D des unités (mode de vue 3D)
#include "render3d_enemies.h" // rendu 3D des ennemis (mode de vue 3D)
#include "entity_fx.h"        // apparition, fente d'attaque (pose des entités)
#include "../engine/assets.h"  // splash arts tours/unités
#include "raylib.h"
#include "ui_utils.h"   // DrawText/MeasureText → g_font (support accents)
#include "ui_palette.h" // palette UI (source unique) : UI_DANGER…
#include "../map/pathfinding.h"
#include "../map/map_gen.h"
#include "../map/theme.h"
#include "../combat/enemy.h"
#include "../combat/tower.h"
#include <math.h>
#include "../combat/unit.h"
#include "../game/game_state.h"
#include "hud.h"
#include "../combat/material.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   /* memset : grille d'occupation des tours */

// ── Facteurs d'échelle des sprites (× le rayon logique de l'entité) ──
// SPRITE = côté de la box de rendu ; HALF = demi-côté (rayon visuel).
#define UNIT_SPRITE_SCALE   4.2f
#define UNIT_SPRITE_HALF    (UNIT_SPRITE_SCALE * 0.5f)
#define ENEMY_SPRITE_SCALE  3.8f
#define ENEMY_SPRITE_HALF   (ENEMY_SPRITE_SCALE * 0.5f)

/* Opacité du nom d'ennemi (option « noms des entités ») : en dessous de
   l'opaque pour ne pas concurrencer la barre de vie et les sprites. */
#define ENEMY_NAME_ALPHA    205

/* Vue 3D (F4) : ombre de contact au pied du modèle, anneaux au sol,
   éclair de dégât (× size). */
#define ENT_SHADOW_RX       1.15f
#define ENT_SHADOW_RY       0.45f
#define ENT_SHADOW_A        95
#define ENT_RING_R3D        1.7f
#define ENT_FLASH_K3D       0.85f   /* hit_flash → blanchiment de la silhouette */

// ── Dessine une texture centrée dans une box, ratio conservé ──
// Retourne 1 si une image a été dessinée, 0 si texture absente.
int renderer_sprite_fit(Texture2D tex, float cx, float cy,
                        float box_w, float box_h, Color tint) {
    if (tex.id == 0) return 0;
    float scale = fminf(box_w / (float)tex.width, box_h / (float)tex.height);
    float dw = (float)tex.width * scale, dh = (float)tex.height * scale;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){cx - dw/2.0f, cy - dh/2.0f, dw, dh},
        (Vector2){0, 0}, 0.0f, tint);
    return 1;
}

// ════════════════════════════════════════════════════
// ENNEMIS
// ════════════════════════════════════════════════════
void render_enemies(const EnemyPool *pool) {
    float t = (float)GetTime();

    /* Tri par profondeur (y croissant) — même principe que render_towers :
       les sprites 3D hauts (ancres par le bas) d'ennemis proches se
       chevauchent correctement (celui de devant dessine en dernier). */
    int order[MAX_ENEMIES], nord = 0;
    for (int k = 0; k < MAX_ENEMIES; k++) {
        const Enemy *ek = &pool->enemies[k];
        if (ek->active && !ek->dead && ek->spawn_delay <= 0.0f) order[nord++] = k;
    }
    for (int a = 1; a < nord; a++) {           /* insertion sort (nord petit) */
        int   key = order[a];
        float ky  = pool->enemies[key].y;
        int   b   = a - 1;
        while (b >= 0 && pool->enemies[order[b]].y > ky) { order[b+1] = order[b]; b--; }
        order[b+1] = key;
    }

    for (int o = 0; o < nord; o++) {
        int i = order[o];
        const Enemy *e = &pool->enemies[i];

        /* Vue 3D (F4) : modèle pré-rendu, pied posé au sol (render3d_skin.h). */
        Texture2D t3d = (g_units_3d && !e->is_boss && render3d_enemy_has_model(e->type))
                      ? render3d_enemy_tex(i) : (Texture2D){0};
        int use3d = (t3d.id != 0);
        EntBody eb;
        ent_fx_enemy_body(i, &eb);

        // Ombre : de contact au pied du modèle 3D, sous le sprite en 2D
        if (use3d)
            DrawEllipse((int)e->x, (int)(e->y + e->size * R3D_ENT_FOOT_DY),
                        e->size * ENT_SHADOW_RX * eb.sx, e->size * ENT_SHADOW_RY * eb.sx,
                        (Color){0, 0, 0, ENT_SHADOW_A});
        else
            DrawEllipse((int)e->x + 2, (int)e->y + (int)e->size,
                        (int)(e->size * 0.8f), (int)(e->size * 0.3f),
                        (Color){0,0,0,80});

        // ════════════════════════════════════════════════
        // BOSS — rendu dédié (corps massif, couronne, aura, grande barre)
        // ════════════════════════════════════════════════
        if (e->is_boss) {
            float ratio = (e->max_hp > 0.0f) ? e->hp / e->max_hp : 1.0f;
            float pulse = (sinf(t * 4.0f) + 1.0f) * 0.5f;
            int   R     = (int)e->size;

            // Aura pulsée (rouge intense en furie)
            Color aura = (ratio < 0.33f) ? (Color){230, 40, 40, 0}
                                         : (Color){150, 40, 120, 0};
            aura.a = (unsigned char)(45 + pulse * 75);
            DrawCircle((int)e->x, (int)e->y, R + 8 + pulse * 5.0f, aura);

            // Corps : teinte selon la phase (violet → magenta → rouge)
            Color body = (ratio < 0.33f) ? (Color){205, 40, 40, 255}
                       : (ratio < 0.66f) ? (Color){175, 40, 95, 255}
                                         : (Color){115, 45, 125, 255};
            if (e->slow_timer > 0.0f) body = (Color){95, 155, 230, 255};
            DrawCircle((int)e->x, (int)e->y, R, body);
            DrawCircleLines((int)e->x, (int)e->y, R,     (Color){255, 255, 255, 130});
            DrawCircleLines((int)e->x, (int)e->y, R - 3, (Color){0, 0, 0, 90});

            // Couronne (3 piques dorées)
            for (int k = -1; k <= 1; k++)
                DrawRectangle((int)e->x + k*8 - 1, (int)e->y - R - 6, 3, 8,
                              (Color){250, 210, 60, 255});
            // Yeux luisants
            DrawCircle((int)e->x - 5, (int)e->y - 2, 2.0f, (Color){255, 240, 120, 255});
            DrawCircle((int)e->x + 5, (int)e->y - 2, 2.0f, (Color){255, 240, 120, 255});

            // Éclair blanc de dégât (feedback de jus)
            if (e->hit_flash > 0.0f)
                DrawCircle((int)e->x, (int)e->y, R,
                           (Color){255, 255, 255,
                                   (unsigned char)(e->hit_flash * 150.0f)});

            // ── Bouclier actif (invulnérable) : bulle cyan ─────────
            if (e->boss_shield > 0.0f) {
                float sp = (sinf(t * 6.0f) + 1.0f) * 0.5f;
                DrawCircle((int)e->x, (int)e->y, R + 10.0f,
                           (Color){90, 200, 255, (unsigned char)(28 + sp * 34)});
                DrawCircleLines((int)e->x, (int)e->y, R + 10.0f + sp * 3.0f,
                                (Color){120, 215, 255, 220});
            }
            // ── Télégraphe de capacité : anneau d'avertissement ────
            if (e->telegraph_timer > 0.0f) {
                float frac = e->telegraph_timer / BOSS_TELEGRAPH_TIME;   // 1 → 0
                Color wc = (e->boss_ability == BOSS_STUN)   ? (Color){120, 180, 255, 255}
                         : (e->boss_ability == BOSS_SUMMON) ? (Color){120, 230, 140, 255}
                                                            : (Color){ 90, 200, 255, 255};
                // STUN : l'anneau montre la portée réelle de l'onde EMP
                float rr = (e->boss_ability == BOSS_STUN)
                         ? R + frac * (BOSS_STUN_RADIUS * TILE_SIZE)
                         : R + 6.0f + frac * 22.0f;
                DrawCircleLines((int)e->x, (int)e->y, rr,        (Color){wc.r, wc.g, wc.b, 230});
                DrawCircleLines((int)e->x, (int)e->y, rr + 1.0f, (Color){wc.r, wc.g, wc.b, 110});
                const char *al = (e->boss_ability == BOSS_STUN)   ? "EMP !"
                               : (e->boss_ability == BOSS_SUMMON) ? "RENFORTS !"
                                                                  : "BOUCLIER !";
                dtxt(al, (int)e->x - mtxt(al, 8)/2, (int)e->y - R - 32, 8, wc);
            }

            // Grande barre de vie + label
            int bw = R * 3, bx = (int)e->x - bw/2, by = (int)e->y - R - 18;
            DrawRectangle(bx - 1, by - 1, bw + 2, 7, (Color){0, 0, 0, 210});
            DrawRectangle(bx, by, (int)(bw * ratio), 5, (Color){222, 40, 40, 255});
            dtxt("BOSS", bx, by - 12, 9, (Color){255, 210, 80, 255});
            continue;
        }

        // Couleur de base par type (source unique partagée avec l'aperçu HUD)
        Color col = renderer_enemy_color(e->type);
 
        // ── Teinte du sprite ──────────────────────────────────
        // WHITE = couleurs natives ; bleutée si ralenti ; alpha réduit
        // et clignotant pour le spectre.
        Color tint = WHITE;
        if (e->slow_timer > 0.0f) {
            tint = (Color){130, 185, 255, 255};   // gel → bleu
            col  = (Color){100, 180, 255, 255};   // (repli cercle)
        }
        unsigned char body_alpha = 255;
        if (e->type == ENEMY_GHOST) {
            float flicker = sinf(t * 8.0f + (float)i) * 0.5f + 0.5f;
            body_alpha = (unsigned char)(70 + (int)(flicker * 110));
        }
        tint.a = body_alpha;

        // ── Auras / cercles de portée (sous le sprite) ────────
        if (e->type == ENEMY_GHOST) {
            float flicker = sinf(t * 8.0f + (float)i) * 0.5f + 0.5f;
            DrawCircleLines((int)e->x, (int)e->y, (int)(e->size + 3),
                            (Color){180, 180, 255,
                                    (unsigned char)(40 + (int)(flicker * 50))});
        } else if (e->type == ENEMY_HEALER) {
            DrawCircle((int)e->x, (int)e->y, (int)e->heal_range,
                       (Color){46, 204, 113, 18});
            DrawCircleLines((int)e->x, (int)e->y, (int)e->heal_range,
                            (Color){46, 204, 113, 60});
        } else if (e->type == ENEMY_ARTILLERY && e->arty_target >= 0) {
            DrawCircleLines((int)e->x, (int)e->y, (int)e->arty_range,
                            (Color){231, 76, 60, 80});
        }

        // ── Corps : modèle 3D animé (mode vue 3D) sinon splash art 2D,
        //    transformé par l'apparition (surgit du sol) et la fente d'attaque.
        tint.a = (unsigned char)((float)tint.a * eb.alpha);
        float ex = e->x + eb.ox, ey = e->y + eb.oy;
        float head_y  = e->y - e->size * ENEMY_SPRITE_HALF;   // sommet (barre de vie)
        float body_cy = ey;                                   // centre (éclair de dégât)
        int   e_drawn = 0;
        if (use3d) {
            float     fy   = ey + e->size * R3D_ENT_FOOT_DY;
            Rectangle body = renderer_squash_rect(render3d_enemy_dst(i, ex, ey, e->size),
                                                  ex, fy, eb.dy, eb.sx, eb.sy);
            /* tint = gel/spectre ; l'éclair de dégât blanchit la silhouette */
            render3d_blit_outlined(t3d, body, tint, e->hit_flash * ENT_FLASH_K3D);
            head_y  = render3d_enemy_head_y(i, body);
            body_cy = (head_y + fy) * 0.5f;
            e_drawn = 1;
        }
        if (!e_drawn && e->type < ENEMY_TYPE_COUNT) {
            float     bs  = e->size * ENEMY_SPRITE_SCALE;
            Rectangle box = renderer_squash_rect((Rectangle){ex - bs*0.5f, ey - bs*0.5f, bs, bs},
                                                 ex, ey + bs*0.5f, eb.dy, eb.sx, eb.sy);
            e_drawn = renderer_sprite_fit(g_enemy_splash[e->type], box.x + box.width*0.5f,
                                          box.y + box.height*0.5f, box.width, box.height, tint);
        }
        if (!e_drawn) {
            DrawCircle((int)ex, (int)ey, (int)e->size,
                       (Color){col.r, col.g, col.b, tint.a});
            DrawCircleLines((int)ex, (int)ey, (int)e->size,
                            (Color){255,255,255,60});
        }

        // ── Éclair blanc de dégât (feedback de jus) — en 3D : dans le blit ──
        if (e->hit_flash > 0.0f && !use3d) {
            unsigned char fa = (unsigned char)(e->hit_flash * 170.0f);
            DrawCircle((int)ex, (int)body_cy, e->size + 1.0f, (Color){255, 255, 255, fa});
        }

        // ── Marqueurs (au-dessus du sprite) ───────────────────
        // Pathbreaker dévié : flèche de trajectoire
        if (e->type == ENEMY_PATHBREAKER && e->path_broken) {
            float dx   = e->target_x - e->x;
            float dy   = e->target_y - e->y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist > 1.0f) {
                float nx = dx/dist, ny = dy/dist;
                DrawLineEx((Vector2){e->x, e->y},
                           (Vector2){e->x + nx*20.0f, e->y + ny*20.0f},
                           2.0f, (Color){255, 80, 180, 180});
            }
        }
        // Hunter en traque : point jaune
        if (e->type == ENEMY_HUNTER && e->hunt_target >= 0)
            DrawCircle((int)e->x, (int)e->y, 3, (Color){255, 255, 0, 220});

        // ── Barre de vie ──────────────────────────────────────
        if (e->type != ENEMY_GHOST || e->hp < e->max_hp * 0.8f) {
            float ratio = e->hp / e->max_hp;
            int   bw    = (int)(e->size * 2.5f);
            int   bx    = (int)e->x - bw / 2;
            int   by    = (int)head_y - 6;   // au-dessus du sprite / du modèle 3D
            DrawRectangle(bx, by, bw, 3, (Color){30,10,10,200});
            Color hp_col = ratio > 0.6f ? (Color){46,204,113,255}
                         : ratio > 0.3f ? (Color){243,156,18,255}
                                        : (Color){231,76,60,255};
            DrawRectangle(bx, by, (int)(bw * ratio), 3, hp_col);
            /* Nom de l'espece : optionnel (defaut OFF — reduit le bruit).
               ROUGE = hostile : en blanc casse le nom se lisait comme une
               etiquette neutre, alors que la couleur est le seul repere
               immediat ami/ennemi sur la carte (vert = alliee). */
            if (g_show_entity_names) {
                Color nc = UI_DANGER;
                nc.a = ENEMY_NAME_ALPHA;
                dtxt(ENEMY_BASE_STATS[e->type].name, bx, by-12, 8, nc);
            }
        }
    }
}

// ════════════════════════════════════════════════════
// UNITÉS
// ════════════════════════════════════════════════════
void render_units(const UnitPool *up) {
    DrawCircleLines((int)up->base_px, (int)up->base_py,
                    UNIT_BASE_STATS[UNIT_SOLDIER].intercept_range * TILE_SIZE,
                    (Color){231,76,60,40});

    /* Tri par profondeur (y croissant) — cf. render_towers/render_enemies. */
    int order[MAX_UNITS], nord = 0;
    for (int k = 0; k < MAX_UNITS; k++)
        if (up->units[k].active) order[nord++] = k;
    for (int a = 1; a < nord; a++) {           /* insertion sort (nord petit) */
        int   key = order[a];
        float ky  = up->units[key].y;
        int   b   = a - 1;
        while (b >= 0 && up->units[order[b]].y > ky) { order[b+1] = order[b]; b--; }
        order[b+1] = key;
    }

    for (int o = 0; o < nord; o++) {
        int i = order[o];
        const Unit *u = &up->units[i];

        // Couleur selon type
        Color col = (u->type < UNIT_TYPE_COUNT) ? UNIT_FILL[u->type]
                  : (Color){200, 200, 50, 255};

        /* Vue 3D (F4) : modèle pré-rendu, pied posé au sol ; pose animée. */
        Texture2D t3d = (g_units_3d && render3d_unit_has_model(u->type))
                      ? render3d_unit_tex(i) : (Texture2D){0};
        int   use3d = (t3d.id != 0);
        float rad   = u->size * UNIT_SPRITE_HALF;   // demi-taille visuelle du sprite
        float fy    = u->y + u->size * R3D_ENT_FOOT_DY;   // pied du modèle 3D
        EntBody ub;
        ent_fx_unit_body(i, &ub);
        /* Sommet (barres) : calculé au dessin du corps ; défaut = sprite 2D. */
        float head_y = u->y - rad;

        // ── Ouvrier : halo et barre de collecte ──────────
        if (u->type == UNIT_WORKER) {
            // Note: ligne pointillée vers dépôt non implémentée
            // (Raylib n'a pas de DrawLineDashed natif)

            // Si porte un matériau : halo coloré (rayon = autour du sprite)
            if (u->has_material && u->carried_mat != MAT_NONE) {
                Color mc = MATERIAL_COLORS[u->carried_mat];
                DrawCircle((int)u->x, (int)u->y, (int)(u->size * UNIT_SPRITE_HALF + 5),
                           (Color){mc.r, mc.g, mc.b, 80});
            }

            // Barre de progression (collecte OU déblaiement d'obstacle)
            if ((u->state == USTATE_COLLECT || u->state == USTATE_CLEARING) &&
                u->collect_duration > 0.0f) {
                float ratio = 1.0f - (u->collect_timer / u->collect_duration);
                int   bw    = (int)(u->size * 3.0f);
                int   bx    = (int)u->x - bw/2;
                /* au-dessus de la barre de vie (sommet du sprite / du modèle 3D) */
                float top   = use3d ? render3d_unit_head_y(i, render3d_unit_dst(i, u->x, u->y, u->size))
                                    : u->y - rad;
                int   by    = (int)top - 10;
                /* Ambre pour le déblaiement, cyan pour la collecte : deux
                   chantiers distincts doivent se lire d'un coup d'oeil. */
                int   clearing = (u->state == USTATE_CLEARING);
                Color pc = clearing ? (Color){225, 165,  70, 255}
                                    : (Color){ 80, 200, 220, 255};
                DrawRectangle(bx, by, bw, 4, (Color){20, 20, 20, 200});
                DrawRectangle(bx, by, (int)(bw * ratio), 4, pc);
                dtxt(clearing ? "déblaie" : "...", bx, by - 10, 8,
                     (Color){pc.r, pc.g, pc.b, 200});
            }
        }

        // ── Auras visibles quand l'unité est sélectionnée ────────
        if (up->selected_unit == i && u->type != UNIT_WORKER) {
            Color uc = UNIT_FILL[u->type];
            // Portée d'attaque — uniquement pour les unités à longue portée
            if (u->atk_range > UNIT_MELEE_ATK_THRESHOLD) {
                int ar = (int)(u->atk_range * TILE_SIZE);
                DrawCircle((int)u->x, (int)u->y, ar,
                           (Color){uc.r, uc.g, uc.b, 14});
                DrawCircleLines((int)u->x, (int)u->y, ar,
                                (Color){uc.r, uc.g, uc.b, 100});
            }
            // Aura de soin du médic
            if (u->type == UNIT_MEDIC) {
                int hr = (int)(UNIT_MEDIC_HEAL_RANGE * TILE_SIZE);
                DrawCircle((int)u->x, (int)u->y, hr,
                           (Color){231, 76, 60, 20});
                DrawCircleLines((int)u->x, (int)u->y, hr,
                                (Color){231, 76, 60, 110});
            }
        }

        // ── Ombre au sol (de contact au pied du modèle 3D) ────────
        if (use3d)
            DrawEllipse((int)u->x, (int)fy, u->size * ENT_SHADOW_RX * ub.sx,
                        u->size * ENT_SHADOW_RY * ub.sx, (Color){0, 0, 0, ENT_SHADOW_A});
        else
            DrawEllipse((int)u->x+1, (int)(u->y + rad),
                        (int)(rad*0.8f), (int)(rad*0.3f),
                        (Color){0,0,0,70});

        // ── Corps : modèle 3D animé (mode vue 3D) sinon splash art 2D,
        //    transformé par le déploiement (chute) et la fente d'attaque. ──
        Color tint = {255, 255, 255, (unsigned char)(255.0f * ub.alpha)};
        float ux = u->x + ub.ox, uy = u->y + ub.oy;
        int drawn = 0;
        if (use3d) {
            Rectangle body = renderer_squash_rect(render3d_unit_dst(i, ux, uy, u->size),
                                                  ux, fy + ub.oy, ub.dy, ub.sx, ub.sy);
            render3d_blit_outlined(t3d, body, tint, 0.0f);
            head_y = render3d_unit_head_y(i, body);
            drawn  = 1;
        }
        if (!drawn && u->type < UNIT_TYPE_COUNT) {
            float     bs  = u->size * UNIT_SPRITE_SCALE;
            Rectangle box = renderer_squash_rect((Rectangle){ux - bs*0.5f, uy - bs*0.5f, bs, bs},
                                                 ux, uy + bs*0.5f, ub.dy, ub.sx, ub.sy);
            drawn = renderer_sprite_fit(g_unit_splash[u->type], box.x + box.width*0.5f,
                                        box.y + box.height*0.5f, box.width, box.height, tint);
        }
        if (!drawn) {
            DrawCircle((int)ux, (int)uy, (int)u->size, (Color){col.r, col.g, col.b, tint.a});
            DrawCircleLines((int)ux, (int)uy, (int)u->size,
                            (Color){255,255,255,80});
        }

        // ── Anneaux (DESSUS du sprite, encerclent le corps) ───────
        // En vue 3D : anneaux AU SOL autour du pied (ellipses, sol en 3/4).
        static const Color BCOLS[5] = {
            {0,0,0,0},{192,57,43,220},{200,200,50,220},
            {82,155,200,220},{231,76,60,220}
        };
        int   ring_b = (u->type != UNIT_WORKER && u->behavior != UBEH_PATROL);
        int   _bidx  = (int)u->behavior;
        if (_bidx < 0 || _bidx >= 5) _bidx = 0;
        if (use3d) {
            float rr = u->size * ENT_RING_R3D;
            if (up->selected_unit == i)
                DrawEllipseLines((int)u->x, (int)fy, rr, rr * 0.5f, (Color){255,255,255,200});
            if (ring_b)
                DrawEllipseLines((int)u->x, (int)fy, rr + 2.0f, rr * 0.5f + 1.0f, BCOLS[_bidx]);
        } else {
            // Sélection : anneau blanc ; comportement : anneau coloré
            if (up->selected_unit == i)
                DrawCircleLines((int)u->x, (int)u->y, rad + 2.0f,
                                (Color){255,255,255,200});
            if (ring_b)
                DrawCircleLines((int)u->x, (int)u->y, rad + 4.0f, BCOLS[_bidx]);
        }

        // ── Barre de vie (au-dessus du sprite / du modèle 3D) ─────
        int bw = (int)(u->size*2.5f);
        int bx = (int)u->x - bw/2;
        int by = (int)head_y - 6;
        DrawRectangle(bx, by, bw, 3, (Color){10,30,10,200});
        float ratio = u->hp / u->max_hp;
        DrawRectangle(bx, by, (int)(bw*ratio), 3,
                      ratio > 0.5f ? (Color){46,204,113,255}
                                   : (Color){231,76,60,255});
    }
}
