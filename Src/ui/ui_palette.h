/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  ui_palette.h ─ SOURCE UNIQUE de la palette du chrome UI.
 *
 *  Valeurs canoniques extraites des littéraux qui s'étaient dupliqués
 *  (avec de légères dérives) dans menu.c / menu_screens.c /
 *  hud_render.c / interlude.c. Toute nouvelle couleur d'interface se
 *  définit ICI — ne plus écrire de (Color){...} de chrome en dur.
 *
 *  NB : pour une variante d'alpha ponctuelle, dériver localement :
 *      Color c = UI_DANGER; c.a = 120;
 */
#include "raylib.h"

/* ── États / sémantique ─────────────────────────────────────── */
#define UI_DANGER      (Color){231,  76,  60, 255}  /* rouge alerte/PV bas   */
#define UI_SUCCESS     (Color){ 46, 204, 113, 255}  /* vert validation/gain  */
#define UI_WARN        (Color){243, 156,  18, 255}  /* orange avertissement  */
#define UI_INFO        (Color){ 62, 175, 200, 255}  /* bleu information      */

/* ── Accents or (identité visuelle) ─────────────────────────── */
#define UI_ACCENT      (Color){232, 152,  32, 255}  /* or vif (titres/CTA)   */
#define UI_ACCENT_DIM  (Color){212, 138,  25, 255}  /* or atténué            */

/* ── Textes ─────────────────────────────────────────────────── */
#define UI_TEXT        (Color){250, 245, 230, 255}  /* primaire (hover/fort) */
#define UI_TEXT_SEC    (Color){168, 148, 102, 255}  /* secondaire beige      */
#define UI_TEXT_DIM    (Color){148, 128,  95, 255}  /* discret/désactivé     */

/* ── Panneaux / bordures ────────────────────────────────────── */
#define UI_PANEL_BG      (Color){ 10,   6,   2, 252}  /* fond de panneau     */
#define UI_PANEL_BG_SOFT (Color){  8,   5,   2, 200}  /* fond léger/overlay  */
#define UI_BORDER        (Color){ 82,  65,  40, 255}  /* bordure standard    */
