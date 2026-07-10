/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  ui_anim.h ─ Infrastructure d'animation de l'UI (menus + HUD).
 *
 *  L'UI du jeu est en immediate-mode : aucun widget n'a d'identité ni
 *  d'état persistant. Ce module fournit le socle commun à toutes les
 *  animations d'interface :
 *    • Courbes d'easing (entrée clampée sur [0,1]).
 *    • Horloge UI : dt de frame borné, partagé par toutes les anims.
 *      AUCUNE logique d'animation ne doit lire GetTime() — tout passe
 *      par ui_dt() (accumulation frame par frame, robuste aux hoquets).
 *    • Cache hover : état de survol persistant par widget, indexé par
 *      la position du widget (clé dérivée de x/y).
 *    • Timers génériques par slot (anims d'entrée d'écran, etc.).
 *
 *  Emploi : appeler ui_anim_tick() UNE fois en tête de chaque passe de
 *  rendu UI (menu_render_and_act et ui_render). Pendant le menu pause
 *  les DEUX passes tournent dans la même frame : le tick est conçu pour
 *  être inoffensif s'il est appelé plusieurs fois par frame (cf. .c).
 */
#pragma once

// ── Courbes d'easing ─────────────────────────────────────────────
// L'ENTRÉE t est clampée sur [0,1]. La sortie de ea_out_back et
// ea_out_elastic peut dépasser 1 temporairement : c'est l'overshoot
// voulu (rebond). ea_out_cubic / ea_in_cubic restent dans [0,1].
float ea_out_cubic  (float t);
float ea_in_cubic   (float t);
float ea_out_back   (float t);
float ea_out_elastic(float t);

// ── Horloge UI ──────────────────────────────────────────────────
// À appeler une fois en tête de menu_render_and_act et de ui_render.
void  ui_anim_tick(void);
// dt de la frame courante, borné (jamais un grand saut après un lag).
float ui_dt(void);

// ── Cache hover (widgets immediate-mode) ────────────────────────
// x, y : coin haut-gauche du widget (sert de clé d'identité).
// hovered : état de survol de CETTE frame (0/1).
// Retourne le survol lissé, déjà passé par ea_out_cubic : 0 = repos,
// 1 = survol installé. Montée en UIA_HOVER_IN s, descente en
// UIA_HOVER_OUT s. À appeler chaque frame où le widget est dessiné.
float ui_hover_t(int x, int y, int hovered);

// Progression du reflet balayant (0 = départ, 1 = terminé/inactif),
// relancée à chaque ENTRÉE en survol. Même clé que ui_hover_t : à
// appeler APRÈS ui_hover_t du même widget dans la même frame (c'est
// ui_hover_t qui détecte le front montant et relance le balayage).
float ui_hover_sweep(int x, int y, int hovered);

// ── Timers génériques ───────────────────────────────────────────
// 16 slots indépendants (UIA_TIMER_SLOTS). reset=1 remet le slot à 0.
// Retourne le temps écoulé (s) depuis le dernier reset. Le slot
// n'avance que lorsqu'il est lu (un écran non rendu reste figé).
float ui_timer(int slot, int reset);

// Nombre de ticks écoulés depuis la dernière lecture/reset du slot.
// Sert à détecter la réapparition d'un écran (pas rendu pendant au
// moins une frame). NB : pendant le menu pause, 2 ticks tombent par
// frame (passe HUD + passe menu) → un écran rendu en continu voit un
// écart ≤ 2 ; tester « > 2 » pour détecter un vrai trou de rendu.
// À appeler AVANT le ui_timer(slot, 0) de la frame.
unsigned ui_timer_gap(int slot);

// ── Ouverture des panneaux (« tôle soudée ») ─────────────────────
// Progress global partagé par tous les panneaux d'un écran : remis à 0
// par la transition d'écran (menu.c), monte linéairement vers 1 en
// UIA_PANEL_DUR (avance dans ui_anim_tick). Les équerres/filets des
// panneaux se dessinent avec ce k (cf. draw_panel).
void  ui_panel_reset(void);
float ui_panel_k(void);   // 0..1, linéaire (l'easing est chez l'appelant)

// Nombre de slots disponibles pour ui_timer (bornage des appelants).
#define UIA_TIMER_SLOTS 16
