/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  story_scene.h ─ Moteur de mise en scène des dialogues de campagne.
 *
 *  Une SCÈNE = une suite de répliques (« beats ») lues dans un script :
 *      NOM :
 *      Texte de la réplique…
 *      <ligne vide>
 *      AUTRE NOM : texte sur la même ligne (forme courte, pour les échos)
 *  Chaque réplique s'affiche avec le portrait du personnage et un texte
 *  qui défile ; ESPACE/clic complète la réplique puis passe à la suivante.
 *
 *  L'état est unique (un seul dialogue à l'écran à la fois) et
 *  immediate-mode friendly : l'écran appelle story_scene_sync() à chaque
 *  frame ; la scène ne repart de zéro que si sa clé change ou si elle
 *  n'a pas été rendue à la frame précédente.
 */
#pragma once
#include "raylib.h"
#include "story_art.h"
#include "../map/theme.h"

// Prépare / entretient la scène `key`. Les deux scripts sont lus à la
// suite (prelude peut être NULL). Avance aussi l'horloge de la scène.
void  story_scene_sync(int key, const char *prelude, const char *script);

int   story_scene_done(void);        // dernière réplique affichée en entier
int   story_scene_done_once(void);   // le script a été lu jusqu'au bout au moins 1×
float story_scene_since_open(void);  // s écoulées depuis done_once (0 avant)
float story_scene_time(void);        // horloge de la scène (s) — animations

void  story_scene_advance(void);     // complète la réplique ou passe à la suivante
void  story_scene_skip_all(void);    // saute à la fin du script (TAB)

// Réplique de RÉACTION à une décision du joueur : remplace la précédente
// réaction éventuelle et l'affiche aussitôt. (NULL, NULL) la retire.
void  story_scene_say(const char *name, const char *text);

// Décision du joueur dans la scène (carte cliquée / touche) : -1 = aucune.
int   story_scene_pick(void);
void  story_scene_set_pick(int idx);
int   story_scene_take_pick(void);   // la consomme (-1 si rien de neuf)

// Boîte de dialogue : portrait + nom + rôle + texte qui défile.
void  story_scene_draw_dialogue(Rectangle box);
// Bandeau illustré du lieu, animé par l'horloge de la scène.
void  story_scene_draw_vignette(Rectangle r, ThemeID theme, int node_id);

// Côté d'une cellule du décor (px) : la hauteur du bandeau en est un multiple.
#define STORY_VIGNETTE_PX 4
