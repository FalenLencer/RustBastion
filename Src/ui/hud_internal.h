/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_internal.h ─ Constantes, types et helpers internes du HUD.
 *
 *  Ce fichier n'est PAS destiné à être inclus depuis l'extérieur.
 *  À inclure uniquement dans hud.c, hud_input.c et hud_render.c.
 *
 *  Contient :
 *    • Les dimensions des overlays (#define OVERLAY_*)
 *    • Les constantes de coût (#define BASE_REPAIR_RESTORE, SLOT_MAX_BUYS)
 *    • Le type ToolInfo et les déclarations extern des tableaux partagés
 *    • Les déclarations des fonctions partagées (définies dans hud.c)
 */
#pragma once

#include "hud.h"
#include "renderer.h"
#include "ui_utils.h"
#include "../engine/audio.h"
#include "../engine/assets.h"
#include "../game/meta.h"
#include "../map/pathfinding.h"
#include "../combat/material.h"
#include "../game/game_state.h"
#include "menu.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// ════════════════════════════════════════════════════
// DIMENSIONS DES OVERLAYS DÉPLAÇABLES
// ════════════════════════════════════════════════════
#define OVERLAY_W      164
#define OVERLAY_BL_W   180   /* overlay bas-gauche (bases HP + réparation) */
#define PORTRAIT_SZ     82   /* taille carrée des portraits splash (px)    */
#define OVERLAY_OV_P     6
#define OVERLAY_TL_H   (OVERLAY_OV_P + 17+3 + 17+3 + 14+3 + 14 + OVERLAY_OV_P)  /* or + vies + tours + unites */
#define OVERLAY_TR_H   (OVERLAY_OV_P + 17+3 + 14+3 + 14+3 + 9+5 + 14 + OVERLAY_OV_P)
/* Hauteur dynamique de l'overlay BL — dépend de l'état des bases en jeu */
int overlay_bl_h(const GameState *gs);
/* Décale un panneau flottant (dir ±1) tant qu'il masque une base ou un
   spawn — positions initiales des overlays (hud.c). */
void overlay_avoid_marks(Vector2 *pos, int ow, int oh,
                         const GameState *gs, int dir);

// ════════════════════════════════════════════════════
// COÛTS DES ACHATS EN JEU
// ════════════════════════════════════════════════════
#define BASE_REPAIR_RESTORE   20   /* HP restaurés par réparation de base */
#define SLOT_MAX_BUYS          6   /* achats de slots max par partie       */

extern const int SLOT_TOWER_COSTS[SLOT_MAX_BUYS];
extern const int SLOT_UNIT_COSTS [SLOT_MAX_BUYS];

// ════════════════════════════════════════════════════
// TYPE ET DONNÉES DES OUTILS
// ════════════════════════════════════════════════════
typedef struct {
    const char *name;
    const char *shortname;
    const char *icon;
    const char *desc;
    int         cost;
    float       dmg;
    float       range;
    float       rate;
    int         is_unit;
} ToolInfo;

extern const ToolInfo  TOOL_INFO[TOOL_COUNT];
extern       Color     TOOL_COLORS[TOOL_COUNT];
extern const int       TOWER_UNLOCK_AT[TOWER_TYPE_COUNT];
extern const char     *TOWER_UNLOCK_ACT_NAME[TOWER_TYPE_COUNT];

// ════════════════════════════════════════════════════
// FONCTIONS PARTAGÉES — définies dans hud.c
// ════════════════════════════════════════════════════
Vector2 virt_mouse(void);
int     base_repair_cost(int n);
int     tool_is_unlocked(ToolID id, const GameState *gs);
void    disc_pop(UIState *ui);

/* P1.2 — HUD progressif : predicats PARTAGES rendu + input (un element
   masque ne garde jamais de hitbox). Voir hud.c. */
#define HUD_SLOTS_FROM_WAVE 3   /* vague d'apparition des achats de slots */
int     hud_show_bases_panel(const GameState *gs);
int     hud_show_slot_buys  (const GameState *gs);

/* Fiche de découverte (hud_render_disc.c) — dessinée en DERNIER par
   ui_render, au-dessus de tout le HUD. No-op si la file est vide. */
void    hud_render_discovery(const UIState *ui, const GameState *gs);

// ── Rendu du HUD découpé (hud_render*.c) ─────────────────────────
/* Repères communs, recalculés dans chaque morceau (ils dépendent du canvas
   courant). (void) : chaque morceau n'en lit qu'une partie. Exige `ui` et
   `gs` dans la portée. */
#define HUD_FRAME_LOCALS                                         \
    const int VIRT_W = g_canvas_virt_w;                          \
    const int HUD_Y  = g_canvas_virt_h - UI_HUD_HEIGHT;          \
    const int HUD_H  = UI_HUD_HEIGHT;                            \
    const int M      = UI_MARGIN;                                \
    const int GAP    = 6;                                        \
    const Theme *th  = theme_get(gs->map.theme);                 \
    (void)ui; (void)VIRT_W; (void)HUD_Y; (void)HUD_H; (void)M;   \
    (void)GAP; (void)th
/* Barre de progression générique (hud_render.c). */
void  hud_draw_bar(int x, int y, int w, int h, float ratio, Color fill, Color bg);
/* Panneau droit (hud_render_sel.c) : textes / couleurs suivant le crossfade
   de sélection, portrait avec pop d'entrée. */
void  hud_sel_txt(const char *s, int x, int y, int fs, Color c);
Color hud_sel_col(Color c);
int   hud_sel_portrait(Texture2D tex, int px, int py, int psz, Color border, float t);
int   hud_sel_tower(const UIState *ui, const GameState *gs,       // _sel_tower.c
                    int rx, int max_w, int py, float sel_t);
/* Sections de ui_render, dans l'ordre de dessin. */
void  hud_render_left_panel  (const UIState *ui, const GameState *gs);  // _info.c
void  hud_render_wave_button (const UIState *ui, const GameState *gs);  // _wave.c
void  hud_render_selection   (const UIState *ui, const GameState *gs);  // _sel.c
void  hud_render_overlays    (const UIState *ui, const GameState *gs);  // _ovl.c
void  hud_render_world_marks (const UIState *ui, const GameState *gs);  // _world.c
void  hud_render_build_ribbon(const UIState *ui, const GameState *gs);  // _info.c
void  hud_render_hint        (const UIState *ui, const GameState *gs);  // _info.c
void  hud_render_wave_banner (const UIState *ui, const GameState *gs);  // _wave.c
void  hud_render_tool_tooltip(const UIState *ui, const GameState *gs);  // _info.c

// ── Entrées du HUD découpées (hud_input*.c) ──────────────────────
/* Mise en page de la frame (boutons, overlays déplaçables, réparation). */
void  hud_input_layout(UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y);
/* Sous-blocs du clic gauche, dans l'ordre de priorité ; 1 = clic consommé. */
int   hud_click_overlay_drag(UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y);
int   hud_click_unit_panel  (UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y);
int   hud_click_tower_panel (UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y);
int   hud_click_slot_buys   (UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y);
int   hud_click_map         (UIState *ui, GameState *gs, Vector2 mouse, int HUD_Y);

/* P0.3 — Conseil de contre : écrit dans `out` (≤ 63 chars utiles) une
   ligne du type "V7 Brutes, Raiders : Cryo efficace, Feu resiste"
   basée sur la vague SUIVANTE. Retourne 0 si rien d'utile (préview
   vide, ennemis non découverts en campagne, ou multiplicateurs plats).
   Partagé : tooltip de pose 2D, bouton matériau 2D, invites héros. */
int     hud_counter_advice(const GameState *gs, char *out, int out_sz);
