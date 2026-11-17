/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "campaign_data.h"
#include <stddef.h>
#include <stdio.h>   /* snprintf : dialogues dynamiques (indice NEXUS) */

// ════════════════════════════════════════════════════
// DONNÉES DES 15 ACTES (5 chapitres × 3 actes)
// ════════════════════════════════════════════════════
static const ActData ACT_DATA[CAMPAIGN_NODES] = {

    // ── CHAPITRE 1 — LES TERRES BRULEES ──────────────────────
    // Introduction : raiders humains, Voss etablit la situation.
    // Personnages : Voss (commandant), Kira (sergent terrain).

    [0] = {
        .chapter  = 0, .act = 0,
        .title    = "Premier contact",
        .subtitle = "Chapitre 1 — Les Terres Brûlées",
        .theme    = THEME_WASTELAND,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 8,
            .before_wave = 0,
            .description = "Survivre 8 vagues",
        },
        .min_waves  = 8,
        .unlock_msg = NULL,
    },

    [1] = {
        .chapter  = 0, .act = 1,
        .title    = "La route du nord",
        .subtitle = "Chapitre 1 — Les Terres Brûlées",
        .theme    = THEME_WASTELAND,
        .objective = {
            .type        = OBJ_DEFEND_BASES,
            .target      = 2,
            .before_wave = 0,
            .description = "Défendre 2 positions simultanément",
        },
        .min_waves  = 9,
        .unlock_msg = "DÉBLOQUÉ : tourelle Sniper",
        .forced_base_count = 2,
    },

    [2] = {
        .chapter  = 0, .act = 2,
        .title    = "Le convoi",
        .subtitle = "Chapitre 1 — Les Terres Brûlées",
        .theme    = THEME_WASTELAND,
        .objective = {
            .type        = OBJ_KILL_ENEMIES,
            .target      = 30,
            .before_wave = 0,
            .description = "Éliminer 30 ennemis",
        },
        .min_waves  = 10,
        .unlock_msg = "DÉBLOQUÉ : tourelle Tesla",
    },

    // ── CHAPITRE 2 — LE MARAIS TOXIQUE ───────────────────────
    // Escalade : mutants, spectres invisibles, coordination ennemie.
    // Personnages : Voss, Kira, Varela.

    [3] = {
        .chapter  = 1, .act = 0,
        .title    = "Brumes vertes",
        .subtitle = "Chapitre 2 — Le Marais Toxique",
        .theme    = THEME_SWAMP,
        .objective = {
            .type        = OBJ_RECRUIT_UNITS,
            .target      = 3,
            .before_wave = 0,    // contrainte non verifiable : objectif simplifie
            .description = "Avoir 3 unités combattantes en vie",
        },
        .min_waves  = 8,
        .unlock_msg = NULL,
    },

    [4] = {
        .chapter  = 1, .act = 1,
        .title    = "Le laboratoire englouti",
        .subtitle = "Chapitre 2 — Le Marais Toxique",
        .theme    = THEME_SWAMP,
        .objective = {
            .type        = OBJ_COLLECT_MATERIALS,
            .target      = 2,
            .before_wave = 0,
            .description = "Collecter 2 minerais avec des ouvriers",
        },
        .min_waves  = 9,
        .unlock_msg = "DÉBLOQUÉ : minerais pour les tourelles",
    },

    [5] = {
        .chapter  = 1, .act = 2,
        .title    = "La reine des marais",
        .subtitle = "Chapitre 2 — Le Marais Toxique",
        .theme    = THEME_SWAMP,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 10,
            .before_wave = 0,
            .description = "Survivre 10 vagues",
        },
        .min_waves  = 10,
        .unlock_msg = "DÉBLOQUÉ : tourelle Lance-flammes",
    },

    // ── CHAPITRE 3 — LE DESERT IRRADIE ───────────────────────
    // Revelation : une IA militaire coordonne les hordes.
    // Personnages : Voss, Kira, Varela.

    [6] = {
        .chapter  = 2, .act = 0,
        .title    = "Tempête de sable",
        .subtitle = "Chapitre 3 — Le Désert Irradié",
        .theme    = THEME_DESERT,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 9,
            .before_wave = 0,
            .description = "Survivre 9 vagues dans le désert irradié",
        },
        .min_waves  = 9,
        .unlock_msg = NULL,
    },

    [7] = {
        .chapter  = 2, .act = 1,
        .title    = "Les ruines de Vegas",
        .subtitle = "Chapitre 3 — Le Désert Irradié",
        .theme    = THEME_DESERT,
        .objective = {
            .type        = OBJ_DEFEND_BASES,
            .target      = 2,
            .before_wave = 0,
            .description = "Protéger 2 positions réparties sur la carte",
        },
        .min_waves  = 10,
        .unlock_msg = NULL,
        .forced_base_count = 2,
    },

    [8] = {
        .chapter  = 2, .act = 2,
        .title    = "Le général machine",
        .subtitle = "Chapitre 3 — Le Désert Irradié",
        .theme    = THEME_DESERT,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 11,
            .before_wave = 0,
            .description = "Survivre 11 vagues face à l'IA militaire",
        },
        .min_waves  = 11,
        .unlock_msg = NULL,
    },

    // ── CHAPITRE 4 — LA VILLE EN RUINE ───────────────────────
    // Confrontation humaine : la Faction Acier, ennemis organises.
    // Personnages : Voss, Kira, Chef de la Faction Acier.

    [9] = {
        .chapter  = 3, .act = 0,
        .title    = "Quartiers morts",
        .subtitle = "Chapitre 4 — La Ville en Ruine",
        .theme    = THEME_CITY,
        .objective = {
            .type        = OBJ_NO_UNIT_LOST,
            .target      = 0,
            .before_wave = 0,
            .description = "Ne perdre aucune unité alliée",
        },
        .min_waves  = 10,
        .unlock_msg = NULL,
    },

    [10] = {
        .chapter  = 3, .act = 1,
        .title    = "La Faction Acier",
        .subtitle = "Chapitre 4 — La Ville en Ruine",
        .theme    = THEME_CITY,
        .objective = {
            .type        = OBJ_COLLECT_MATERIALS,
            .target      = 1,
            .before_wave = 0,
            .description = "Collecter au moins 1 minerai",
        },
        .min_waves  = 11,
        .unlock_msg = NULL,
    },

    [11] = {
        .chapter  = 3, .act = 2,
        .title    = "Siège du parlement",
        .subtitle = "Chapitre 4 — La Ville en Ruine",
        .theme    = THEME_CITY,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 12,
            .before_wave = 0,
            .description = "Tenir 12 vagues et prendre le parlement",
        },
        .min_waves  = 12,
        .unlock_msg = NULL,
        .forced_base_count = 2,
    },

    // ── CHAPITRE 5 — L'USINE ABANDONNEE ──────────────────────
    // Climax : NEXUS, l'IA centrale qui orchestre tout.
    // Personnages : Voss, Varela, NEXUS.

    [12] = {
        .chapter  = 4, .act = 0,
        .title    = "La source",
        .subtitle = "Chapitre 5 — L'Usine Abandonnée",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_COLLECT_MATERIALS,
            .target      = 1,
            .before_wave = 0,
            .description = "Collecter 1 minerai dans l'usine",
        },
        .min_waves  = 12,   /* lissé : 15 était un pic isolé (tous les autres 8-12) */
        .unlock_msg = NULL,
    },

    [13] = {
        .chapter  = 4, .act = 1,
        .title    = "Cœur de l'usine",
        .subtitle = "Chapitre 5 — L'Usine Abandonnée",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_COLLECT_MATERIALS,
            .target      = 3,
            .before_wave = 0,
            .description = "Désactiver 3 générateurs avec des ouvriers",
        },
        .min_waves  = 10,
        .unlock_msg = NULL,
    },

    [14] = {
        .chapter  = 4, .act = 2,
        .title    = "NEXUS",
        .subtitle = "Chapitre 5 — L'Usine Abandonnée",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 14,
            .before_wave = 0,
            .description = "Survivre 14 vagues et résister à NEXUS",
        },
        .min_waves  = 14,   /* était 20 : 2× la norme, au pire moment (le climax).
                               14 reste l'acte le plus long (finales 10→10→11→12)
                               et les doctrines méthodiques y ajoutent jusqu'à
                               +18 % d'ennemis via l'indice NEXUS. */
        .unlock_msg = "FIN DE CAMPAGNE — Le monde peut recommencer.",
    },

    // ══════════════════════════════════════════════════════════════
    // NŒUDS DE BRANCHE (hors trame principale — pas d'étoiles méta).
    // Leur chapter/act fixe leur difficulté narrative (campaign_difficulty_stage).
    // ══════════════════════════════════════════════════════════════

    // [15] Branche "traque" du Chapitre 2 (choix au nœud 3) → rejoint la Reine (5)
    [15] = {
        .chapter  = 1, .act = 1,      // difficulté ≈ milieu Ch.2
        .title    = "La piste dans la brume",
        .subtitle = "Chapitre 2 — Le Marais Toxique",
        .theme    = THEME_SWAMP,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 9,
            .before_wave = 0,
            .description = "Survivre 9 vagues en terrain découvert",
        },
        .min_waves  = 9,
        .unlock_msg = NULL,
    },

    // [16] Détour "réacteur" du Chapitre 5 (échec collecte au nœud 13) → NEXUS (14)
    [16] = {
        .chapter  = 4, .act = 1,      // difficulté ≈ milieu Ch.5
        .title    = "Le réacteur s'emballe",
        .subtitle = "Chapitre 5 — L'Usine Abandonnée",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 9,
            .before_wave = 0,
            .description = "Tenir 9 vagues malgré la surchauffe",
        },
        .min_waves  = 9,
        .unlock_msg = NULL,
    },

    // [17] Ch1 — branche "embuscade" (choix au nœud 1) → rejoint le convoi (2)
    [17] = {
        .chapter  = 0, .act = 1,
        .title    = "L'embuscade des cendres",
        .subtitle = "Chapitre 1 — Les Terres Brûlées",
        .theme    = THEME_WASTELAND,
        .objective = {
            .type = OBJ_KILL_ENEMIES, .target = 25, .before_wave = 0,
            .description = "Anéantir 25 pillards dans l'embuscade",
        },
        .min_waves  = 9,
        .unlock_msg = NULL,
    },

    // [18] Ch3 — branche "frappe rapide" (choix au nœud 6) → rejoint le Général (8)
    [18] = {
        .chapter  = 2, .act = 1,
        .title    = "Course contre la machine",
        .subtitle = "Chapitre 3 — Le Désert Irradié",
        .theme    = THEME_DESERT,
        .objective = {
            .type = OBJ_SURVIVE_WAVES, .target = 8, .before_wave = 0,
            .description = "Percer vers l'IA en 8 vagues, à découvert",
        },
        .min_waves  = 8,
        .unlock_msg = NULL,
    },

    // [19] Ch4 — détour "sous le joug" (échec du plasma au nœud 10) → parlement (11)
    [19] = {
        .chapter  = 3, .act = 1,
        .title    = "Sous le joug de l'Acier",
        .subtitle = "Chapitre 4 — La Ville en Ruine",
        .theme    = THEME_CITY,
        .objective = {
            .type = OBJ_SURVIVE_WAVES, .target = 11, .before_wave = 0,
            .description = "Survivre 11 vagues, armures intactes",
        },
        .min_waves  = 11,
        .unlock_msg = NULL,
    },

    // [20] Ch5 — branche "sabotage" (choix au nœud 12) → rejoint le cœur (13)
    [20] = {
        .chapter  = 4, .act = 0,
        .title    = "Sabotage de la chaîne",
        .subtitle = "Chapitre 5 — L'Usine Abandonnée",
        .theme    = THEME_FACTORY,
        .objective = {
            .type = OBJ_KILL_ENEMIES, .target = 35, .before_wave = 0,
            .description = "Détruire 35 robots sortis des chaînes",
        },
        .min_waves  = 10,
        .unlock_msg = NULL,
    },

    // [21] Repli après la Reine (défaite nœud 5) → entre dans le desert (6)
    [21] = {
        .chapter  = 1, .act = 1,
        .title    = "La dernière ligne",
        .subtitle = "Chapitre 2 — Le Marais Toxique",
        .theme    = THEME_SWAMP,
        .objective = {
            .type = OBJ_SURVIVE_WAVES, .target = 10, .before_wave = 0,
            .description = "Couvrir la retraite : tenir 10 vagues",
        },
        .min_waves  = 10,
        .unlock_msg = NULL,
    },

    // [22] Repli après le Général (défaite nœud 8) → entre dans la ville (9)
    [22] = {
        .chapter  = 2, .act = 1,
        .title    = "Repli dans les ruines",
        .subtitle = "Chapitre 3 — Le Désert Irradié",
        .theme    = THEME_DESERT,
        .objective = {
            .type = OBJ_SURVIVE_WAVES, .target = 9, .before_wave = 0,
            .description = "Survivre 9 vagues en retraite ordonnée",
        },
        .min_waves  = 9,
        .unlock_msg = NULL,
    },
};

// ════════════════════════════════════════════════════
// API
// ════════════════════════════════════════════════════
const ActData *campaign_act_get(int stage_index) {
    if (stage_index < 0 || stage_index >= CAMPAIGN_NODES) return &ACT_DATA[0];
    return &ACT_DATA[stage_index];
}

// ════════════════════════════════════════════════════
// ROUTAGE DU GRAPHE
// Phase actuelle : trame principale linéaire (0→1→…→14→fin).
// Les bifurcations (succès/échec d'objectif, choix) et les nœuds de
// repli seront ajoutés ici, nœud par nœud, sans toucher au moteur.
// ════════════════════════════════════════════════════
int campaign_next_node(int stage_index, int objective_done, int choice_idx) {
    switch (stage_index) {
        // ── Ch1 : choix d'approche du convoi (reconverge sur le convoi) ──
        case 1:  return (choice_idx == 1) ? 17 : 2;   // 1 = embuscade (17)
        case 17: return 2;

        // ── Ch2 : choix labo / traque (reconverge sur la Reine, nœud 5) ──
        case 3:  return (choice_idx == 1) ? 15 : 4;   // 1 = traque (15)
        case 15: return 5;

        // ── Ch3 : choix défense / frappe rapide (reconverge sur le Général) ──
        case 6:  return (choice_idx == 1) ? 18 : 7;   // 1 = course (18)
        case 18: return 8;

        // ── Ch4 : échec d'emploi du plasma → détour sous le joug de l'Acier ──
        case 10: return objective_done ? 11 : 19;
        case 19: return 11;

        // ── Ch5 : choix piratage / sabotage (reconverge sur le cœur, nœud 13) ──
        case 12: return (choice_idx == 1) ? 20 : 13;  // 1 = sabotage (20)
        case 20: return 13;

        // ── Ch5 : échec des générateurs → détour réacteur instable → NEXUS ──
        case 13: return objective_done ? 14 : 16;
        case 16: return 14;

        // ── Replis (après défaite de finale) → reconvergent sur la suite ──
        case 21: return 6;   // "La dernière ligne" (repli Reine)   → désert (6)
        case 22: return 9;   // "Repli dans les ruines" (repli Général) → ville (9)

        default:
            if (stage_index < 0)                   return 0;
            if (stage_index >= CAMPAIGN_TOTAL - 1) return -1;  // fin de la trame
            return stage_index + 1;
    }
}

DefeatMode campaign_defeat_mode(int stage_index) {
    if (stage_index >= CAMPAIGN_TOTAL - 1) return DEFEAT_GAMEOVER;   // NEXUS = vraie fin
    // Perdre une grande finale n'arrête pas l'histoire : on bat en retraite
    // vers une situation alternative (plus dure) au lieu de tout reprendre.
    if (stage_index == 5 || stage_index == 8) return DEFEAT_RETREAT;
    return DEFEAT_RETRY_WEAK;   // ailleurs : reprise affaiblie
}

int campaign_defeat_node(int stage_index) {
    if (stage_index == 5) return 21;   // défaite contre la Reine → "La dernière ligne"
    if (stage_index == 8) return 22;   // défaite contre le Général → "Repli dans les ruines"
    return -1;
}

// ════════════════════════════════════════════════════
// DOCTRINES — la contrepartie MÉCANIQUE des choix narratifs
// ════════════════════════════════════════════════════
// Règle de conception : chaque doctrine renforce un axe et en affaiblit un
// autre. Aucune n'est strictement meilleure — elles proposent deux façons
// de gagner. Les MÉTHODIQUES bâtissent (slots, vies) mais rendent le joueur
// lisible ; les IMPRÉVISIBLES jouent le tempo (or, hordes réduites) au prix
// de la solidité. Voir campaign_nexus_prediction pour la conséquence finale.
/* SÉPARATION STRICTE DES DEUX FAMILLES — sans elle, un build mixte cumule
   les baisses de pression des deux camps et écrase les deux identités
   (mesuré : count ×0,748 en mixte contre ×0,81 en imprévisible pur).
     • MÉTHODIQUE  : gagne de la STRUCTURE (slots, vies) ; ne réduit JAMAIS
                     la pression ennemie — au contraire, il la paie.
     • IMPRÉVISIBLE: réduit la PRESSION (nombre, PV) ou enrichit ; le paie
                     en structure (slots, vies, vitesse ennemie).
   Aucune doctrine ne doit offrir à la fois structure et pression réduite. */
const DoctrineDef CAMPAIGN_DOCTRINE[CAMPAIGN_DOCTRINES] = {
    /* ── Ch1 : le convoi blindé ───────────────────────────── */
    { CFLAG_ESCORT, "CONVOI SAUVÉ", "Convoi",
      "+1 unité déployable, +5 % d'ennemis", 1,
        0, 0, 0, 1,   1.05f, 1.00f, 1.00f },
    { CFLAG_AMBUSH, "BUTIN DES CENDRES", "Butin",
      "+30 or par acte, -8 vies par acte", 0,
       30, -8, 0, 0,  1.00f, 1.00f, 1.00f },

    /* ── Ch2 : la source du signal ────────────────────────── */
    { CFLAG_LAB, "ARSENAL CHIMIQUE", "Arsenal",
      "+1 tour déployable, ennemis +6 % de PV", 1,
        0, 0, 1, 0,   1.00f, 1.06f, 1.00f },
    { CFLAG_TRACK, "RENSEIGNEMENT", "Renseign.",
      "Ennemis -8 % de PV, -20 or par acte", 0,
      -20, 0, 0, 0,   1.00f, 0.92f, 1.00f },

    /* ── Ch3 : l'IA militaire ─────────────────────────────── */
    { CFLAG_ANCHOR, "LIGNE FORTIFIÉE", "Ligne",
      "+12 vies par acte, +6 % d'ennemis", 1,
        0, 12, 0, 0,  1.06f, 1.00f, 1.00f },
    { CFLAG_FAST_STRIKE, "FRAPPE ÉCLAIR", "Frappe",
      "-10 % d'ennemis, -8 vies par acte", 0,
        0, -8, 0, 0,  0.90f, 1.00f, 1.00f },

    /* ── Ch5 : l'usine ────────────────────────────────────── */
    { CFLAG_HACK, "CODES D'ACCÈS", "Codes",
      "+1 unité déployable, +6 % d'ennemis", 1,
        0, 0, 0, 1,   1.06f, 1.00f, 1.00f },
    { CFLAG_SABOTAGE, "CHAÎNE BRISÉE", "Chaîne",
      "-10 % d'ennemis, ennemis +10 % de vitesse", 0,
        0, 0, 0, 0,   0.90f, 1.00f, 1.10f },
};

const DoctrineDef *campaign_doctrine_by_flag(int flag) {
    for (int i = 0; i < CAMPAIGN_DOCTRINES; i++)
        if (CAMPAIGN_DOCTRINE[i].flag == flag) return &CAMPAIGN_DOCTRINE[i];
    return NULL;
}

int campaign_doctrines_active(int flags, const DoctrineDef **out, int max) {
    int n = 0;
    for (int i = 0; i < CAMPAIGN_DOCTRINES && n < max; i++)
        if (flags & CAMPAIGN_DOCTRINE[i].flag) out[n++] = &CAMPAIGN_DOCTRINE[i];
    return n;
}

int campaign_doctrine_slots(int flags, int kind) {
    int n = 0;
    for (int i = 0; i < CAMPAIGN_DOCTRINES; i++) {
        if (!(flags & CAMPAIGN_DOCTRINE[i].flag)) continue;
        n += kind ? CAMPAIGN_DOCTRINE[i].unit_slots
                  : CAMPAIGN_DOCTRINE[i].tower_slots;
    }
    return n;
}

// ── Indice de prédiction NEXUS ────────────────────────────────
#define NEXUS_PRED_BASE      50   /* aucun choix posé : incertitude totale  */
#define NEXUS_PRED_PER_DOC   12   /* poids d'une doctrine (± selon le type) */
#define NEXUS_PRED_PER_LOSS   8   /* une défaite majeure = un comportement
                                     acculé, donc PLUS prévisible           */
#define NEXUS_PRED_MIN        4
#define NEXUS_PRED_MAX       96

int campaign_nexus_prediction(int flags) {
    int p = NEXUS_PRED_BASE;
    for (int i = 0; i < CAMPAIGN_DOCTRINES; i++) {
        if (!(flags & CAMPAIGN_DOCTRINE[i].flag)) continue;
        p += CAMPAIGN_DOCTRINE[i].methodical ? NEXUS_PRED_PER_DOC
                                             : -NEXUS_PRED_PER_DOC;
    }
    if (flags & CFLAG_LOST_QUEEN)   p += NEXUS_PRED_PER_LOSS;
    if (flags & CFLAG_LOST_GENERAL) p += NEXUS_PRED_PER_LOSS;
    if (p < NEXUS_PRED_MIN) p = NEXUS_PRED_MIN;
    if (p > NEXUS_PRED_MAX) p = NEXUS_PRED_MAX;
    return p;
}

// ── Choix narratifs ───────────────────────────────────────────
int campaign_has_choice(int stage_index) {
    return (stage_index == 1 || stage_index == 3 ||
            stage_index == 6 || stage_index == 12);
}

const char *campaign_choice_prompt(int stage_index) {
    switch (stage_index) {
        case 1:  return "Le convoi blindé force le passage. Comment l'aborder ?";
        case 3:  return "La source du signal se rapproche. Quelle piste suivre ?";
        case 6:  return "L'IA militaire se terre à l'est. Quelle approche ?";
        case 12: return "L'usine grouille de robots de guerre. Par où frapper ?";
        default: return NULL;
    }
}

/* L'ACTION seule : la carte de choix affiche à côté la doctrine installée,
   sa conséquence chiffrée et son effet sur le modèle de NEXUS. */
const char *campaign_choice_label(int stage_index, int idx) {
    switch (stage_index) {
        case 1:  return idx == 0 ? "Escorter le convoi"      : "Tendre une embuscade";
        case 3:  return idx == 0 ? "Fouiller le laboratoire" : "Traquer la source";
        case 6:  return idx == 0 ? "Tenir deux ancrages"     : "Foncer droit sur l'IA";
        case 12: return idx == 0 ? "Pirater les robots"      : "Saboter la chaîne";
        default: return NULL;
    }
}

/* Conséquence mécanique lisible AVANT de trancher : le joueur décide en
   connaissance de cause (une décision informée est une vraie décision). */
const char *campaign_choice_effect(int stage_index, int idx) {
    int flag = campaign_choice_flag(stage_index, idx);
    const DoctrineDef *d = campaign_doctrine_by_flag(flag);
    return d ? d->effect : NULL;
}

// ── Difficulté pilotée par la position narrative ──────────────
int campaign_difficulty_stage(int node_id) {
    const ActData *a = campaign_act_get(node_id);
    return a->chapter * CAMPAIGN_ACTS + a->act;
}

// ── Drapeaux narratifs ────────────────────────────────────────
/* Les DEUX branches posent un drapeau : choisir la prudence EST un choix,
   il installe sa propre doctrine et compte dans le modèle de NEXUS. */
int campaign_choice_flag(int stage_index, int choice_idx) {
    int risky = (choice_idx == 1);
    switch (stage_index) {
        case 1:  return risky ? CFLAG_AMBUSH      : CFLAG_ESCORT;
        case 3:  return risky ? CFLAG_TRACK       : CFLAG_LAB;
        case 6:  return risky ? CFLAG_FAST_STRIKE : CFLAG_ANCHOR;
        case 12: return risky ? CFLAG_SABOTAGE    : CFLAG_HACK;
        default: return 0;
    }
}

/* Chaque branche a son écho : la voie prudente laisse une trace aussi
   nette que la voie risquée — sinon jouer prudent revient à ne pas jouer.
   Format « NOM : texte » : la scène en fait une réplique avec portrait,
   jouée EN OUVERTURE de l'acte (le rappel d'une décision passée). */
const char *campaign_echo(int stage_index, int flags) {
    switch (stage_index) {
        case 4: case 15:
            if (flags & CFLAG_AMBUSH)
                return "KIRA : Depuis votre embuscade des Terres Brûlées, les hordes vous "
                       "savent imprévisible. C'est un avantage : gardons-le.";
            if (flags & CFLAG_ESCORT)
                return "KIRA : Les rescapés du convoi se battent avec nous, maintenant. Ils "
                       "connaissent le terrain, ça compense ce qu'ils nous coûtent.";
            break;
        case 6:
            if (flags & CFLAG_TRACK)
                return "VARELA : Vous aviez traqué la source vous-même, dans le marais. "
                       "On sait déjà à quoi ressemble l'ennemi du désert.";
            if (flags & CFLAG_LAB)
                return "VARELA : L'arsenal chimique du labo tient toujours la route. Une "
                       "position de plus à armer… mais eux aussi ont eu le temps.";
            break;
        case 9:
            if (flags & CFLAG_FAST_STRIKE)
                return "KIRA : Votre frappe éclair contre l'IA a laissé des traces. La "
                       "Faction Acier vous attend de pied ferme.";
            if (flags & CFLAG_ANCHOR)
                return "VOSS : Nos ancrages du désert tiennent encore. On avance lentement, "
                       "mais on n'a rien cédé. La ville se prendra de la même façon.";
            if (flags & CFLAG_LOST_GENERAL)
                return "VOSS : On a plié face au général machine, mais on est encore là. "
                       "La ville sera notre revanche.";
            break;
        case 12:
            if (flags & CFLAG_AMBUSH)
                return "VARELA : Vos coups tordus depuis le début paient : NEXUS n'a jamais "
                       "réussi à modéliser votre stratégie.";
            if (flags & CFLAG_ESCORT)
                return "VARELA : NEXUS a toutes vos manœuvres en archive. Chaque ligne tenue "
                       "proprement, il l'a apprise par cœur.";
            break;
        case 14:
            if (flags & CFLAG_SABOTAGE)
                return "VARELA : La chaîne sabotée a amputé ses défenses. C'est notre "
                       "fenêtre de tir.";
            if (flags & CFLAG_HACK)
                return "VARELA : Les codes volés nous ouvrent ses couches basses. Il se "
                       "défend moins bien… mais il vous voit venir.";
            break;
        default: break;
    }
    return NULL;
}

/* ── Scènes calculées ─────────────────────────────────────────────
   L'acte final EST la confrontation au modèle : NEXUS annonce le taux de
   concordance RÉELLEMENT calculé sur les décisions du joueur. Buffers
   statiques (rendu mono-thread) ; DEUX buffers distincts pour que l'intro
   et l'épilogue ne puissent jamais s'écraser l'un l'autre. */
static char g_dlg_buf[1024];   /* campaign_dialog_before */
static char g_epi_buf[1024];   /* campaign_dialog_after (épilogue) */

const char *campaign_dialog_before(int node_id, int flags) {
    const char *base = campaign_story_before(node_id);
    if (!base) base = "";
    if (node_id != CAMPAIGN_TOTAL - 1) return base;

    int pred = campaign_nexus_prediction(flags);
    const char *verdict;
    if (pred >= 74)
        verdict = "VOS DÉCISIONS SUIVENT UN SCHÉMA. JE LES CONNAIS DÉJÀ. "
                  "MES UNITÉS VOUS ATTENDENT LÀ OÙ VOUS IREZ.";
    else if (pred >= 40)
        verdict = "VOTRE SCHÉMA RESTE PARTIELLEMENT RÉSOLU. "
                  "MARGE D'ERREUR ACCEPTABLE. J'ENGAGE.";
    else
        verdict = "VARIABLE NON RÉSOLUE. VOS DÉCISIONS NE CONVERGENT PAS. "
                  "J'ENGAGE SANS PRÉDICTION.";

    snprintf(g_dlg_buf, sizeof(g_dlg_buf),
             "%s\n"
             "NEXUS :\n"
             "COMMANDANT VOSS. MODÈLE COMPORTEMENTAL : %d %% DE CONCORDANCE. %s\n",
             base, pred, verdict);
    return g_dlg_buf;
}

/* Épilogue : la dernière chose que NEXUS dit avant de s'éteindre est la
   MESURE de ce que le joueur a été. L'indice de prédiction n'est pas une
   statistique cachée — c'est la conclusion morale de la campagne. */
const char *campaign_epilogue(int flags) {
    int losses = (flags & (CFLAG_LOST_QUEEN | CFLAG_LOST_GENERAL)) ? 1 : 0;
    int pred   = campaign_nexus_prediction(flags);
    const char *nexus, *voss, *coda;

    if (pred <= 30) {
        nexus = "CONCORDANCE FINALE : %d %%. JE NE VOUS AI JAMAIS VU VENIR.";
        voss  = "Il a modélisé des armées entières. Pas vous. Vous avez gagné en "
                "refusant d'être une équation";
        coda  = losses ? " — et ça nous a coûté des vies. Elles comptent aussi."
                       : ". On reconstruit. À votre façon.";
    } else if (pred >= 70) {
        nexus = "CONCORDANCE FINALE : %d %%. JE VOUS AVAIS PRÉVU. "
                "JE N'AVAIS PAS PRÉVU QUE CELA NE SUFFISE PAS.";
        voss  = "Il connaissait chacun de nos gestes. On les a faits quand même, "
                "et mieux que lui";
        coda  = losses ? ". On a payé le prix fort ; le bastion s'en souviendra."
                       : ". Discipline contre calcul. Le bastion tient, intact.";
    } else {
        nexus = "CONCORDANCE FINALE : %d %%. DONNÉES INSUFFISANTES.";
        voss  = "Ni tout à fait prévisible, ni tout à fait fou. On a fait ce "
                "qu'il fallait, quand il le fallait";
        coda  = losses ? ". On a perdu des positions. On les reprendra."
                       : ". Maintenant, on reconstruit.";
    }
    char line[160];
    snprintf(line, sizeof(line), nexus, pred);
    snprintf(g_epi_buf, sizeof(g_epi_buf),
             "NEXUS :\n%s\n"
             "\n"
             "VOSS :\n%s%s\n"
             "\n"
             "KIRA :\n"
             "Écoutez… Plus rien. Pour la première fois depuis des années, "
             "le silence.\n",
             line, voss, coda);
    return g_epi_buf;
}

const char *campaign_dialog_after(int node_id, int flags, int last, int obj_ok) {
    if (last) return campaign_epilogue(flags);
    /* Objectif manqué sur un nœud qui BIFURQUE : la scène doit raconter
       l'échec, sinon elle annonce une victoire que l'acte suivant dément. */
    const char *d = obj_ok ? NULL : campaign_story_after_failed(node_id);
    if (!d) d = campaign_story_after(node_id);
    return d ? d : "";
}

int campaign_act_index(int chapter, int act) {
    return chapter * CAMPAIGN_ACTS + act;
}

// ── Mutateur signature par chapitre ──────────────────────────
CampaignMutator campaign_mutator_for_stage(int stage_index) {
    if (stage_index < 0 || stage_index >= CAMPAIGN_TOTAL) return MUT_NONE;
    int chapter = stage_index / CAMPAIGN_ACTS;   // 0..4
    switch (chapter) {
        case 1:  return MUT_TOXIC;      // Marais toxique
        case 2:  return MUT_SANDSTORM;  // Désert irradié
        case 3:  return MUT_AMBUSH;     // Ville en ruine
        case 4:  return MUT_OVERLOAD;   // Usine abandonnée
        default: return MUT_NONE;       // Terres brûlées (intro)
    }
}

const char *campaign_mutator_name(CampaignMutator m) {
    switch (m) {
        case MUT_TOXIC:     return "TOXICITÉ — ennemis endurcis";
        case MUT_SANDSTORM: return "TEMPÊTE — ennemis plus rapides";
        case MUT_AMBUSH:    return "EMBUSCADE — essaims renforcés";
        case MUT_OVERLOAD:  return "SURRÉGIME — vitesse & blindage";
        case MUT_NONE:
        default:            return NULL;
    }
}

int campaign_objective_check(const ActData *act,
                              int waves_done, int kills,
                              int units_alive, int materials_collected,
                              int no_unit_lost)
{
    if (!act) return 0;
    const Objective *o = &act->objective;
    switch (o->type) {
        case OBJ_SURVIVE_WAVES:
            return waves_done >= o->target;
        case OBJ_RECRUIT_UNITS:
            return units_alive >= o->target;
        case OBJ_KILL_ENEMIES:
            return kills >= o->target;
        case OBJ_DEFEND_BASES:
            return 1; // validé si on est encore en vie à la fin
        case OBJ_COLLECT_MATERIALS:
            return materials_collected >= o->target;
        case OBJ_NO_UNIT_LOST:
            return no_unit_lost;
        default: return 0;
    }
}