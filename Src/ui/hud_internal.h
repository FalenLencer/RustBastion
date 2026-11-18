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
#define OVERLAY_GRIP_H   4   /* poignée : le contenu commence à OV_P + GRIP_H */
#define OVERLAY_LAST_H  16   /* dernière ligne (taille 10 → fh(10) = 16 px)  */
/* Hauteur = poignée + lignes + dernière ligne À SA HAUTEUR RÉELLE (sans la
   poignée ni fh, « Vitesse » touchait le bord du cadre). Le panneau haut-
   gauche (or / vies / tours / unités) a disparu : tout est dans le panneau
   bas ; celui de droite garde vague (progression d'acte), kills, vitesse. */
#define OVERLAY_TR_ROW_WAVE   (17 + 3)
#define OVERLAY_TR_ROW_KILLS  (14 + 3)
#define OVERLAY_TR_H   (OVERLAY_OV_P + OVERLAY_GRIP_H + OVERLAY_TR_ROW_WAVE + OVERLAY_TR_ROW_KILLS \
                        + OVERLAY_LAST_H + OVERLAY_OV_P)   /* vague + kills + vitesse */
/* Bouton de vitesse du panneau haut-droit — SOURCE UNIQUE rendu ↔ clic. */
Rectangle hud_speed_btn_rect(const UIState *ui);
/* Hauteur dynamique de l'overlay BL — dépend de l'état des bases en jeu */
int overlay_bl_h(const GameState *gs);
/* Décale un panneau flottant (dir ±1) tant qu'il masque une base ou un
   spawn — positions initiales des overlays (hud.c). */
void overlay_avoid_marks(Vector2 *pos, int ow, int oh,
                         const GameState *gs, int dir);

// ════════════════════════════════════════════════════
// PANNEAU BAS : TROIS PLAQUES (gauche / centre / droite)
// Mise en page = hud_layout_buttons (SOURCE UNIQUE des rectangles,
// lue par le rendu ET par les clics).
// ════════════════════════════════════════════════════
#define HUD_PLATE_GAP    4    /* jour entre deux plaques (px)                      */
#define HUD_PANEL_PAD   12    /* bord du panneau → contenu, en x (cadre + marge)   */
#define HUD_SEC_Y       10    /* haut du panneau → 1er titre de section            */
#define HUD_SEC_H       20    /* titre de section + filet, au-dessus d'une rangée  */
#define HUD_ROW_GAP      6    /* rangée de cartes → titre de la rangée suivante    */
#define HUD_CARD_GAP     6    /* écart entre deux cartes d'outil                   */
#define HUD_BLOCK_GAP   12    /* cartes → bouton de vague                          */
#define HUD_WAVE_W     124    /* bouton LANCER LA VAGUE                            */
#define HUD_PAUSE_W     40    /* bouton PAUSE                                      */
#define HUD_SLOT_BTN_H  22    /* achats de slots (bas du panneau gauche)           */
#define HUD_TP_GAP       4    /* fiche de tour : écart entre rangées de boutons    */
#define HUD_TP_UPG_H    34    /* … améliorations (libellé + niveau·coût)           */
#define HUD_TP_ROW_H    24    /* … [CIBLE] [RÉPARER] [VENDRE]                      */
#define HUD_TP_MAT_H    22    /* … appliquer un minerai                            */
/* Zone de contenu du panneau droit (fiche de sélection / renseignement). */
#define HUD_RX(vw)      ((vw) - UI_PANEL_W + HUD_PANEL_PAD)
#define HUD_RW          (UI_PANEL_W - HUD_PANEL_PAD * 2)
/* Zone de contenu du panneau gauche. */
#define HUD_LX          HUD_PANEL_PAD
#define HUD_LW          (UI_LEFT_PANEL_W - HUD_PANEL_PAD * 2)
/* Rectangles de tous les boutons fixes du panneau bas (cartes, vague,
   pause, panneau droit, achats de slots) pour un HUD débutant à hud_y. */
void hud_layout_buttons(UIState *ui, int hud_y);
/* Actions sur une tour posée — SOURCE UNIQUE pour le clic sur son bouton
   et pour son raccourci clavier (hud_input_click.c). 1 = action traitée. */
typedef enum { TACT_DMG = 0, TACT_RANGE, TACT_RATE, TACT_REPAIR, TACT_SELL, TACT_TARGET } TowerAction;
int hud_tower_action(UIState *ui, GameState *gs, int idx, TowerAction a);
/* Raccourcis de la tour sélectionnée. Codes = POSITIONS physiques (US) :
   Q/W/E = A/Z/E sur AZERTY ; libellé affiché via opts_key_name(). */
#define HUD_KEY_UPG_DMG    KEY_Q
#define HUD_KEY_UPG_RANGE  KEY_W
#define HUD_KEY_UPG_RATE   KEY_E
#define HUD_KEY_REPAIR     KEY_R
#define HUD_KEY_TARGET     KEY_T
#define HUD_KEY_SELL       KEY_DELETE
#define HUD_KEY_NEXT_TOWER KEY_TAB
#define HUD_KEY_SHOW_RANGE KEY_LEFT_ALT   /* maintenue : portées de toutes les tours */
/* Couleurs du panneau bas partagées par ses sections. */
#define HUD_COL_TOWER   ((Color){214, 160,  60, 255})   /* tout ce qui est « tour »  */
#define HUD_COL_UNIT    ((Color){110, 200, 110, 255})   /* tout ce qui est « unité » */
#define HUD_COL_GREY    ((Color){110,  95,  75, 255})   /* texte d'un élément éteint */
#define HUD_KEY_FS       7    /* petite touche de raccourci (cartes, pause)        */
#define HUD_BTN_FS_MIN   8    /* libellé de bouton : plus petit lisible avant coupe */

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
/* Fiche d'outil avec les chiffres RÉELS (stats de tours / unités) : à
   utiliser pour tout affichage de coût, dégâts, portée, cadence (hud.c). */
ToolInfo hud_tool_info(ToolID id);
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
/* Panneau droit au repos : renseignement de vague (hud_render_intel.c). */
void  hud_render_intel(const UIState *ui, const GameState *gs, int rx, int max_w);
/* Bouton du panneau droit en plaque : libellé centré (taille fs). */
void  hud_plate_btn(const Rectangle *r, Color acc, int enabled, int active,
                    const char *lbl, int fs, Color lc);
/* … avec la touche de raccourci `key` à gauche (nom selon le clavier) et le
   libellé centré dans le reste. hud_btn_keycap : la touche seule (largeur). */
void  hud_plate_btn_key(const Rectangle *r, Color acc, int enabled, int key,
                        const char *lbl, int fs, Color lc);
int   hud_btn_keycap(const Rectangle *r, int key);
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

/* Types de dégâts auxquels un ennemi est FAIBLE (weak_side = 1 : ×dégâts ≥
   HUD_DMG_WEAK_MULT) ou qu'il RÉSISTE (0 : ≤ HUD_DMG_RESIST_MULT), en liste
   « Feu ×1.5, Électrique ×1.3 » (with_mult) ou « Feu, Électrique ». Retourne
   le nombre de types. Partagé : fiche de découverte, fiche au survol. */
#define HUD_DMG_WEAK_MULT    1.30f
#define HUD_DMG_RESIST_MULT  0.70f
int     hud_enemy_dmg_list(int etype, int weak_side, int with_mult, char *out, int out_sz);
/* Ennemi sous la souris (coords canvas) sur la carte, -1 = aucun. */
int     hud_enemy_pick(const GameState *gs, Vector2 mouse);
/* Fiche de l'ennemi survolé (hud_render_enemy.c). */
void    hud_render_enemy_tip(const UIState *ui, const GameState *gs);
