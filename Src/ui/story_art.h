/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  story_art.h ─ Illustrations pixel-art PROCÉDURALES du récit de campagne.
 *
 *  Deux familles, toutes deux dessinées au moteur (aucune texture externe,
 *  donc aucun asset à livrer ni à charger) :
 *    • PORTRAITS  : le buste de chaque personnage (story_portraits.c),
 *      sur une grille de STORY_PORTRAIT_CELLS cellules, animé (clignement,
 *      bouche pendant que la réplique défile, œil de NEXUS).
 *    • VIGNETTES  : le décor du chapitre (story_vignettes.c), avec une
 *      variante par nœud pour les lieux clés (convoi, Reine, parlement…).
 *
 *  Utilisé par la mise en scène des dialogues (story_scene.c).
 */
#pragma once
#include "raylib.h"
#include "../map/theme.h"

// ── Personnages ──────────────────────────────────────────────────
typedef enum {
    SPEAKER_RADIO = 0,   // voix non identifiée / technicien
    SPEAKER_VOSS,        // commandant du bastion
    SPEAKER_KIRA,        // sergent éclaireur
    SPEAKER_VARELA,      // ingénieure
    SPEAKER_STEEL,       // chef de la Faction Acier
    SPEAKER_NEXUS,       // l'IA centrale
    SPEAKER_GENERAL,     // le général machine (Ch.3, IA d'artillerie)
    SPEAKER_COUNT
} SpeakerId;

// Identifie le personnage depuis l'en-tête d'une réplique ("SERGENT KIRA").
SpeakerId   story_speaker_from_name(const char *name);
// Couleur d'accent du personnage (plaque de nom, cadre du portrait).
Color       story_speaker_color(SpeakerId id);
// Rôle affiché sous le nom ("Commandant du bastion").
const char *story_speaker_role(SpeakerId id);
float       story_speaker_voice(SpeakerId id);   // hauteur du déclic radio (1 = neutre)

// Côté du buste en cellules : le portrait occupe un carré de
// STORY_PORTRAIT_CELLS × px pixels.
#define STORY_PORTRAIT_CELLS 24

// Dessine le buste (fond compris), coin haut-gauche (x,y), cellule de `px`
// pixels. t = horloge d'animation (s) ; talk = 1 tant que la réplique
// défile (bouche animée) ; alpha 0..1 (fondu d'entrée).
void story_portrait_draw(SpeakerId id, int x, int y, int px,
                         float t, int talk, float alpha);

// ── Décors ───────────────────────────────────────────────────────
// Paysage du thème dans le rectangle (x,y,w,h), sur une grille de `px`
// pixels. node_id sélectionne la variante (lieu clé de l'acte) ;
// t = horloge d'animation (s).
void story_vignette_draw(ThemeID theme, int node_id,
                         int x, int y, int w, int h, int px, float t);
