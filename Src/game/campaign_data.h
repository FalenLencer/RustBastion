/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "../map/theme.h"

#define CAMPAIGN_CHAPTERS  5
#define CAMPAIGN_ACTS      3   // actes par chapitre
#define CAMPAIGN_TOTAL     (CAMPAIGN_CHAPTERS * CAMPAIGN_ACTS)  // 15 actes "principaux" (étoiles méta)

// Capacité du GRAPHE de campagne : les nœuds 0..CAMPAIGN_TOTAL-1 sont la
// trame principale (suivie par la méta / étoiles) ; les nœuds au-delà sont
// des branches alternatives et des nœuds de repli (pas d'étoiles méta).
// Augmenter cette valeur ne change PAS le format de sauvegarde méta.
#define CAMPAIGN_NODES     32

// Comportement en cas de défaite, décidé par nœud.
typedef enum {
    DEFEAT_GAMEOVER = 0,   // vraie fin (nœud critique) → retour menu
    DEFEAT_RETREAT,        // repli vers un nœud alternatif (campaign_defeat_node)
    DEFEAT_RETRY_WEAK,     // rejoue le même nœud avec un handicap
} DefeatMode;

// ── Objectif de l'acte ───────────────────────────────────────
typedef enum {
    OBJ_SURVIVE_WAVES = 0,  // survivre N vagues
    OBJ_RECRUIT_UNITS,      // recruter N unités avant la vague N
    OBJ_KILL_ENEMIES,       // tuer N ennemis
    OBJ_DEFEND_BASES,       // défendre 2 bases simultanément
    OBJ_COLLECT_MATERIALS,  // collecter N matériaux
    OBJ_NO_UNIT_LOST,       // ne perdre aucune unité
} ObjectiveType;

// ── Mutateur de chapitre ──────────────────────────────────────
// Une condition spéciale signature par chapitre, qui modifie les vagues.
typedef enum {
    MUT_NONE = 0,    // aucun (chapitre d'introduction)
    MUT_TOXIC,       // marais  : ennemis plus résistants (+PV)
    MUT_SANDSTORM,   // désert  : ennemis plus rapides (charge aveuglante)
    MUT_AMBUSH,      // ville   : essaims urbains plus nombreux
    MUT_OVERLOAD,    // usine   : production en surrégime (+vitesse, +PV)
} CampaignMutator;

// Mutateur signature d'un acte (déterminé par son chapitre).
CampaignMutator campaign_mutator_for_stage(int stage_index);
// Nom court affichable du mutateur (NULL si MUT_NONE).
const char     *campaign_mutator_name(CampaignMutator m);

// ── Routage du graphe de campagne ─────────────────────────────
// Nœud suivant après un acte réussi (objective_done) ou raté (= bifurcation).
// choice_idx : branche choisie par le joueur (0/1) si le nœud propose un choix.
// Retourne -1 = fin de campagne.
int        campaign_next_node (int stage_index, int objective_done, int choice_idx);
// Comportement en cas de défaite sur ce nœud.
DefeatMode campaign_defeat_mode(int stage_index);
// Nœud de repli si DEFEAT_RETREAT (-1 sinon).
int        campaign_defeat_node(int stage_index);

// ── Choix narratif présenté après un nœud (bifurcation pilotée) ──
int         campaign_has_choice   (int stage_index);          // 1 = ce nœud propose un choix
const char *campaign_choice_prompt(int stage_index);          // question (NULL si aucun)
const char *campaign_choice_label (int stage_index, int idx); // libellé option 0/1

// Position narrative (chapitre×3+acte) d'un nœud → pilote l'intensité de
// difficulté indépendamment de l'index brut (les nœuds de branche ont un
// index élevé mais peuvent appartenir narrativement à un chapitre antérieur).
int         campaign_difficulty_stage(int node_id);

// ── Drapeaux narratifs (choix & événements marquants) ────────
// Persistent toute la campagne (sauvegardés) ; actes, dialogues ET RÈGLES DU
// JEU y réagissent → les décisions du joueur ont un coût et un bénéfice réels.
// Chaque bifurcation pose un drapeau sur LES DEUX branches (une décision
// prudente est une décision : elle se paie et se souvient, elle aussi).
#define CFLAG_AMBUSH       (1 << 0)  // Ch1 : embuscade   (imprévisible)
#define CFLAG_TRACK        (1 << 1)  // Ch2 : traque      (imprévisible)
#define CFLAG_FAST_STRIKE  (1 << 2)  // Ch3 : frappe      (imprévisible)
#define CFLAG_SABOTAGE     (1 << 3)  // Ch5 : sabotage    (imprévisible)
#define CFLAG_LOST_QUEEN   (1 << 4)  // a perdu la Reine puis s'est replié
#define CFLAG_LOST_GENERAL (1 << 5)  // a perdu le Général puis s'est replié
#define CFLAG_ESCORT       (1 << 6)  // Ch1 : escorte     (méthodique)
#define CFLAG_LAB          (1 << 7)  // Ch2 : laboratoire (méthodique)
#define CFLAG_ANCHOR       (1 << 8)  // Ch3 : ancrages    (méthodique)
#define CFLAG_HACK         (1 << 9)  // Ch5 : piratage    (méthodique)

// ── DOCTRINES ────────────────────────────────────────────────
// Un choix ne se contente plus de router le graphe : il installe une
// DOCTRINE permanente, appliquée à CHAQUE acte restant, avec un vrai
// arbitrage (un axe renforcé, un axe affaibli). Les doctrines
// « méthodiques » renforcent la STRUCTURE (slots, vies) et rendent le
// joueur lisible pour NEXUS ; les « imprévisibles » jouent le TEMPO
// (moins d'ennemis, plus d'or) et dégradent son modèle.
#define CAMPAIGN_DOCTRINES 8

typedef struct {
    int         flag;         // CFLAG_* correspondant
    const char *name;         // "BUTIN DES CENDRES"
    const char *tag;          // "Butin" — affichage compact
    const char *effect;       // "+35 or/acte, +8% d'ennemis"
    int         methodical;   // 1 = prévisible (NEXUS affine), 0 = imprévisible
    // Modificateurs permanents appliqués à chaque acte
    int   act_gold;           // + or au début de l'acte (peut être négatif)
    int   act_lives;          // + vies au début de l'acte (peut être négatif)
    int   tower_slots;        // + limite de tours
    int   unit_slots;         // + limite d'unités
    float count_mult;         // × nombre d'ennemis par vague
    float scale_mult;         // × PV/dégâts des ennemis
    float speed_mult;         // × vitesse des ennemis
} DoctrineDef;

extern const DoctrineDef CAMPAIGN_DOCTRINE[CAMPAIGN_DOCTRINES];

// Doctrine correspondant à un drapeau (NULL si aucune).
const DoctrineDef *campaign_doctrine_by_flag(int flag);
// Remplit `out` avec les doctrines actives dans `flags`. Retourne le nombre.
int  campaign_doctrines_active(int flags, const DoctrineDef **out, int max);
// Somme des slots accordés par les doctrines actives (kind 0 = tours, 1 = unités).
int  campaign_doctrine_slots(int flags, int kind);

// ── INDICE DE PRÉDICTION NEXUS (0-100) ───────────────────────
// Le cœur thématique : NEXUS est un moteur de prédiction, et les choix du
// joueur en sont l'entrée. Méthodique → il vous modélise (indice haut,
// hordes mieux coordonnées à l'acte final) ; imprévisible → son modèle
// se délite (indice bas). Pilote la difficulté de NEXUS, son dialogue
// (le pourcentage annoncé est CALCULÉ) et l'épilogue.
int         campaign_nexus_prediction(int flags);

// Drapeau posé par un choix (stage, branche 0/1) ; 0 = aucun.
int         campaign_choice_flag(int stage_index, int choice_idx);
// Conséquence mécanique d'une option, affichée SOUS son libellé (NULL = aucune).
const char *campaign_choice_effect(int stage_index, int idx);
// Dialogue d'introduction d'un acte — dynamique pour NEXUS (indice calculé).
const char *campaign_dialog_before(int node_id, int flags);
// Ligne de rappel narratif pour l'intro d'un acte selon les drapeaux (NULL = aucune).
const char *campaign_echo(int stage_index, int flags);
// Épilogue de fin de campagne, variant selon le parcours (drapeaux).
const char *campaign_epilogue(int flags);

typedef struct {
    ObjectiveType type;
    int           target;        // valeur cible (N vagues, N unités...)
    int           before_wave;   // contrainte "avant la vague X" (0 = pas de contrainte)
    const char   *description;   // texte affiché au joueur
} Objective;

// ── Données d'un acte ────────────────────────────────────────
typedef struct {
    int         chapter;         // 0-4
    int         act;             // 0-2
    const char *title;           // ex: "Premier contact"
    const char *subtitle;        // ex: "Chapitre 1 — Acte 1"
    Objective   objective;
    const char *dialog_before;   // dialogue affiché AVANT l'acte
    const char *dialog_after;    // dialogue affiché APRÈS l'acte (victoire)
    ThemeID     theme;           // thème de carte imposé
    int         min_waves;       // vagues minimum pour valider l'acte
    const char *unlock_msg;      // message de débloquage si acte terminé (peut être NULL)
    int         forced_base_count; // 0 = 1 seule base (défaut), >0 = nombre imposé
                                   // garantit la cohérence entre le texte et la carte
} ActData;

// ── API ──────────────────────────────────────────────────────
// Retourne les données d'un acte (stage_index 0..14)
const ActData *campaign_act_get(int stage_index);

// Retourne l'index global d'un acte (chapter*3 + act)
int campaign_act_index(int chapter, int act);

// Vérifie si un objectif est accompli
int campaign_objective_check(const ActData *act,
                              int waves_done, int kills,
                              int units_alive, int materials_collected,
                              int no_unit_lost);