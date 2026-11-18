/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  ui_plate.h ─ Habillage « plaque de métal rivetée » (SOURCE UNIQUE).
 *
 *  Le style des écrans du récit : coins CHANFREINÉS, cadre biseauté (arête
 *  claire en haut à gauche), rivets, ombre portée, panneau sombre avec une
 *  lueur et un liseré intérieur à la couleur du contexte (personnage qui
 *  parle, consigne retenue, bouton survolé…). Utilisé par la boîte de
 *  dialogue, le bandeau illustré, et les panneaux / cartes / boutons du
 *  briefing et du bilan.
 */
#include "raylib.h"

#define PLATE_FRAME_D   ((Color){ 26,  20,  14, 255})   /* arête sombre      */
#define PLATE_FRAME     ((Color){ 70,  54,  38, 255})   /* métal du cadre    */
#define PLATE_FRAME_L   ((Color){124, 100,  70, 255})   /* arête éclairée    */
#define PLATE_PANEL     ((Color){ 17,  13,  10, 246})   /* fond des panneaux */
#define PLATE_WELL      ((Color){  6,   5,   4, 255})   /* feuillure         */
#define PLATE_CHAMFER   9.0f    /* coin coupé d'un grand panneau (px)       */
#define PLATE_CHAMFER_S 6.0f    /* … d'une carte ou d'un bouton (px)        */
#define PLATE_BORDER    4.0f    /* épaisseur du cadre (px)                  */

/* Opacité globale de tout ce que dessine ce module (fondu d'ouverture d'un
   écran). Remettre à 1 après usage. */
void      plate_alpha(float a);
Color     plate_fade (Color c, float a);
Rectangle plate_inset(Rectangle r, float d);
/* Triangle plein quel que soit l'ordre des sommets. */
void plate_tri(Vector2 a, Vector2 b, Vector2 c, Color col);
/* Rectangle aux coins chanfreinés (plein / contour d'1 px). */
void plate_chamfer_fill(Rectangle r, float c, Color col);
void plate_chamfer_line(Rectangle r, float c, Color col);

/* Plaque de métal : ombre portée, cadre biseauté, rivets (chanfrein c). */
void plate_draw (Rectangle box, float c);
/* Plaque + panneau sombre + lueur haute + liseré intérieur couleur `acc`
   (opacité `acc_a`). */
void plate_panel(Rectangle box, float c, Color acc, float acc_a);
/* Bouton : plaque, panneau teinté `acc`, prises latérales ; survolé = plus
   vif + halo ; désactivé = métal terne. */
void plate_button(Rectangle b, Color acc, int hov, int enabled);
/* … sans les prises latérales (grips = 0) : bouton portant une touche. */
void plate_button_ex(Rectangle b, Color acc, int hov, int enabled, int grips);
/* Touche de clavier en métal, coin haut-gauche (x, y) ; retourne sa largeur.
   _fs : taille de police au choix (petite touche sur une carte du HUD). */
int  plate_keycap(int x, int y, const char *lbl, Color col);
int  plate_keycap_fs(int x, int y, const char *lbl, Color col, int fs);
/* Titre de section : repère coloré, texte, filet dégradé jusqu'à x + w. */
void plate_section(int x, int y, int w, const char *txt, Color col);
/* Indication « [TOUCHE] action » ; draw = 0 : mesure seulement. Retourne
   la largeur totale. */
int  plate_key_hint(int x, int y, const char *key, const char *txt, int draw);
/* Rayures de danger diagonales (sombre / `col`) dans un rectangle. */
void plate_hazard(int x, int y, int w, int h, Color col);
