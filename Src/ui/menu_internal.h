/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_internal.h ─ Constantes et helpers internes du système de menus.
 *
 *  Ce fichier n'est PAS destiné à être inclus depuis l'extérieur.
 *  À inclure uniquement dans menu.c et les fichiers menu_*.c.
 *
 *  Contient :
 *    • La palette de couleurs (#define C_*)
 *    • Les constantes de layout (#define M_PAD, BTN_H, etc.)
 *    • Les déclarations des helpers partagés (définis dans menu.c)
 *    • Les déclarations des fonctions d'écran (définies dans menu_*.c)
 */
#pragma once

#include "menu.h"
#include "renderer.h"
#include "ui_utils.h"
#include "../game/campaign_data.h"
#include "../engine/audio.h"
#include "../engine/assets.h"
#include "../game/meta.h"
#include "../game/save.h"
#include "../game/game_init.h"
#include "../combat/enemy.h"
#include "../combat/material.h"
#include "../combat/tower.h"
#include "../combat/unit.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ════════════════════════════════════════════════════
// PALETTE — couleurs communes à tout le menu
// ════════════════════════════════════════════════════
#define C_BG      ((Color){  8,   5,   3, 255})
#define C_PANEL   ((Color){ 14,   9,   4, 255})
#define C_BORDER  ((Color){ 55,  36,  12, 255})
#define C_GOLD    ((Color){232, 152,  32, 255})
#define C_GREEN   ((Color){ 42, 190, 105, 255})
#define C_RED     ((Color){218,  68,  52, 255})
#define C_DIM     ((Color){ 72,  58,  38, 255})
#define C_TEXT    ((Color){168, 148, 102, 255})
#define C_HOV     ((Color){ 26,  17,   5, 255})
#define C_BLUE    ((Color){ 48, 140, 205, 255})
#define C_DARK    ((Color){  9,   5,   2, 255})
#define C_ORANGE  ((Color){215, 118,  28, 255})

// Couleur d'identité de chaque chapitre (source unique, défini dans menu.c).
extern const Color CHAPTER_COLORS[CAMPAIGN_CHAPTERS];

// ── Constantes de layout ─────────────────────────────────────────
#define M_PAD     16    // marge extérieure des panneaux
#define M_IN       8    // marge intérieure entre éléments
#define M_LINE    12    // espacement entre lignes de texte
#define M_SECT    18    // espacement entre sections
#define BTN_H     36    // hauteur standard des boutons
#define BTN_R      5    // rayon coins arrondis boutons
#define PANEL_R    6    // rayon coins arrondis panneaux

// ════════════════════════════════════════════════════
// HELPERS PARTAGÉS — définis dans menu.c
// ════════════════════════════════════════════════════

// Souris virtuelle
Vector2 vmouse(void);
int     vhov(int x, int y, int w, int h);
int     vhov_r(Rectangle r);
int     vclick_r(Rectangle r);
void    push_back_screen(MenuState *m);
void    pop_back_screen(MenuState *m);

// Dessin texte
void    txt_c(const char *s, int cx, int y, int fs, Color col);
void    draw_sep(int x, int y, int w, Color col);
void    txt_c_boxed(const char *s, int cx, int y, int fs, Color col);
void    draw_text_boxed(const char *s, int x, int y, int fs, Color col);

// Dessin panneaux / boutons
void    draw_bg(const MenuState *m, int vw, int vh);
void    draw_header(const char *subtitle, int vw);
void    draw_panel(int cx, int cy, int pw, int ph, Color border);
int     draw_btn(const char *label, int x, int y, int w, int h,
                 Color col, int active);
int     draw_volume_slider(const char *label, int x, int y, int w,
                           int value, int *out_value);
void    draw_dropdown_arrow(int bx, int by, int bw, int bh, Color col);
int     draw_nav_btn(const char *icon, const char *title, const char *desc,
                     Color col, int x, int y, int w, int h);
int     draw_back_btn(int vw, int vh);

// Message temporaire
void    draw_msg(MenuState *m, int vw, int vh);
void    set_msg(MenuState *m, const char *s);

// ════════════════════════════════════════════════════
// FONCTIONS D'ÉCRAN — déclarées ici pour le dispatch (menu.c)
// Définies dans les fichiers menu_*.c correspondants.
// ════════════════════════════════════════════════════
MenuAction draw_title         (MenuState *m, int vw, int vh);
MenuAction draw_play_hub      (MenuState *m, const MetaProgress *meta, int vw, int vh);
MenuAction draw_slot_list     (MenuState *m, int vw, int vh, int is_campaign);
MenuAction draw_new_campaign  (MenuState *m, const MetaProgress *meta, int vw, int vh);
MenuAction draw_new_arcade    (MenuState *m, int vw, int vh);
MenuAction draw_confirm_del   (MenuState *m, int vw, int vh);
MenuAction draw_pause         (MenuState *m, int vw, int vh);
MenuAction draw_world_map     (MenuState *m, const MetaProgress *meta, int vw, int vh);
MenuAction draw_options       (MenuState *m, int vw, int vh);
MenuAction draw_upgrades      (MenuState *m, const MetaProgress *meta, int vw, int vh);
MenuAction draw_bestiary      (MenuState *m, const MetaProgress *meta, int vw, int vh);
MenuAction draw_custom_config (MenuState *m, int vw, int vh);
MenuAction draw_mp_hub        (MenuState *m, int vw, int vh);
MenuAction draw_mp_lobby      (MenuState *m, int vw, int vh);
