/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "campaign_data.h"
#include <stddef.h>

// ════════════════════════════════════════════════════
// DONNÉES DES 15 ACTES (5 chapitres × 3 actes)
// ════════════════════════════════════════════════════
static const ActData ACT_DATA[CAMPAIGN_TOTAL] = {

    // ── CHAPITRE 1 — LES TERRES BRULEES ──────────────────────
    // Introduction : raiders humains, Voss etablit la situation.
    // Personnages : Voss (commandant), Kira (sergent terrain).

    [0] = {
        .chapter  = 0, .act = 0,
        .title    = "Premier contact",
        .subtitle = "Chapitre 1 — Les Terres Brulees",
        .theme    = THEME_WASTELAND,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 8,
            .before_wave = 0,
            .description = "Survivre 8 vagues",
        },
        .dialog_before =
            "COMMANDANT VOSS :\n"
            "\"Routes du nord coupees. Des pillards signales a deux\n"
            "kilometres. Ce n'est pas une escarmouche — c'est une offensive.\n"
            "Fortifiez les defenses. Chaque position compte.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"On tient. Mais ce n'etait que l'avant-garde.\n"
            "Des rescapes du convoi arrivent au bastion. Renforcez\n"
            "les lignes — la prochaine vague sera plus lourde.\"\n",
        .min_waves  = 8,
        .unlock_msg = NULL,
    },

    [1] = {
        .chapter  = 0, .act = 1,
        .title    = "La route du nord",
        .subtitle = "Chapitre 1 — Les Terres Brulees",
        .theme    = THEME_WASTELAND,
        .objective = {
            .type        = OBJ_DEFEND_BASES,
            .target      = 2,
            .before_wave = 0,
            .description = "Defendre 2 positions simultanement",
        },
        .dialog_before =
            "SERGENT KIRA :\n"
            "\"Commandant, le poste avance est sous pression.\n"
            "Ils poussent sur le flanc est en meme temps. Des brutes\n"
            "dans les rangs — du jamais vu. On tient les deux points.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"La route est ouverte. Bon travail, Kira.\n"
            "Des ingenieurs du convoi rejoignent nos rangs — ils ont\n"
            "recupere des plans dans les ruines. Un sniper est assemblable.\"\n",
        .min_waves  = 9,
        .unlock_msg = "DEBLOQUE : Tourelle Sniper",
        .forced_base_count = 2,
    },

    [2] = {
        .chapter  = 0, .act = 2,
        .title    = "Le convoi",
        .subtitle = "Chapitre 1 — Les Terres Brulees",
        .theme    = THEME_WASTELAND,
        .objective = {
            .type        = OBJ_KILL_ENEMIES,
            .target      = 30,
            .before_wave = 0,
            .description = "Eliminer 30 ennemis",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Un vehicule blinde ouvre la route a une colonne de pillards.\n"
            "S'il passe, le bastion est coupe en deux. Pas d'hesitation.\n"
            "Tout le monde sur le pont — concentrez le feu.\"\n",
        .dialog_after  =
            "INGENIEURE VARELA :\n"
            "\"Commandant, j'ai fouille les decombres du blindee.\n"
            "Un generateur electromagnetique militaire — encore intact.\n"
            "Je peux l'adapter en tourelle. La Tesla est en ligne.\"\n",
        .min_waves  = 10,
        .unlock_msg = "DEBLOQUE : Tourelle Tesla",
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
            .description = "Avoir 3 unites combattantes en vie",
        },
        .dialog_before =
            "SERGENT KIRA :\n"
            "\"Le marais cache quelque chose. Des formes — pas tout\n"
            "a fait humaines — se deplacent entre les cypres morts.\n"
            "L'une d'elles a disparu sous les yeux de mon eclaireur.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"Les mutants sont plus nombreux que prevu. Et certains\n"
            "sont invisibles aux tourelles — seules vos unites de terrain\n"
            "peuvent les detecter. Adaptez la composition de vos equipes.\"\n",
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
            .description = "Collecter 2 materiaux avec des ouvriers",
        },
        .dialog_before =
            "INGENIEURE VARELA :\n"
            "\"Un laboratoire pre-guerre partiellement emerge du marais.\n"
            "Des reservoirs de composants chimiques encore intacts a l'interieur.\n"
            "Envoyez les ouvriers. Ces materiaux peuvent modifier nos armes.\"\n",
        .dialog_after  =
            "VARELA :\n"
            "\"Impeccable. Avec ces composants, on peut conditionner\n"
            "les munitions de nos tourelles — electrique, cryogenique, nano.\n"
            "Le type de degats change tout face a certains ennemis.\"\n",
        .min_waves  = 9,
        .unlock_msg = "DEBLOQUE : Systeme de materiaux pour les tourelles",
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
        .dialog_before =
            "SERGENT KIRA :\n"
            "\"Les attaques sont trop organisees pour des mutants.\n"
            "Quelqu'un les coordonne. Une presence massive converge\n"
            "vers notre position depuis le coeur du marais. Tenez.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"La horde s'est dispersee. Mais avant la fin du signal,\n"
            "on a capte une transmission cryptee. Militaire.\n"
            "Elle vient du desert, a l'est. Quelqu'un d'autre les dirigeait.\"\n",
        .min_waves  = 10,
        .unlock_msg = "DEBLOQUE : Tourelle Lance-flammes",
    },

    // ── CHAPITRE 3 — LE DESERT IRRADIE ───────────────────────
    // Revelation : une IA militaire coordonne les hordes.
    // Personnages : Voss, Kira, Varela.

    [6] = {
        .chapter  = 2, .act = 0,
        .title    = "Tempete de sable",
        .subtitle = "Chapitre 3 — Le Desert Irradie",
        .theme    = THEME_DESERT,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 9,
            .before_wave = 0,
            .description = "Survivre 9 vagues dans le desert irradie",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Le desert irradie. La tempete de sable bloque toute\n"
            "visibilite. Les ennemis arrivent de toutes les directions.\n"
            "Positionnez les defenses en profondeur. Pas d'autre choix.\"\n",
        .dialog_after  =
            "SERGENT KIRA :\n"
            "\"La tempete se leve. Et avec elle... un signal radio.\n"
            "Crypte. Militaire. Pas des pillards, pas des mutants.\n"
            "Ca vient d'un complexe actif, quelque part a l'est.\"\n",
        .min_waves  = 9,
        .unlock_msg = NULL,
    },

    [7] = {
        .chapter  = 2, .act = 1,
        .title    = "Les ruines de Vegas",
        .subtitle = "Chapitre 3 — Le Desert Irradie",
        .theme    = THEME_DESERT,
        .objective = {
            .type        = OBJ_DEFEND_BASES,
            .target      = 2,
            .before_wave = 0,
            .description = "Proteger 2 positions reparties sur la carte",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Les ruines de l'ancienne ville forment un labyrinthe.\n"
            "Des unites d'artillerie ennemies se positionnent a distance —\n"
            "elles peuvent detruire nos tourelles. Tenez les deux points.\"\n",
        .dialog_after  =
            "INGENIEURE VARELA :\n"
            "\"J'ai triangule le signal. Il vient du complexe militaire a l'est.\n"
            "L'emetteur est trop regulier, trop precis pour etre humain.\n"
            "Ce n'est pas quelqu'un qui les dirige. C'est quelque chose.\"\n",
        .min_waves  = 10,
        .unlock_msg = NULL,
        .forced_base_count = 2,
    },

    [8] = {
        .chapter  = 2, .act = 2,
        .title    = "Le general machine",
        .subtitle = "Chapitre 3 — Le Desert Irradie",
        .theme    = THEME_DESERT,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 11,
            .before_wave = 0,
            .description = "Survivre 11 vagues face a l'IA militaire",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Une IA militaire commande les unites d'artillerie lourde.\n"
            "Elle coordonne chaque attaque en temps reel. Elle apprend.\n"
            "Tenez les positions jusqu'a ce que ses calculs echouent.\"\n",
        .dialog_after  =
            "VARELA :\n"
            "\"L'IA a surchauffe. Dans ses circuits, j'ai recupere des fragments.\n"
            "Elle ne commandait pas seule — elle recevait des ordres\n"
            "d'une intelligence superieure. Quelque part dans une usine, au nord.\"\n",
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
            .description = "Ne perdre aucune unite alliee",
        },
        .dialog_before =
            "VOSS :\n"
            "\"La ville n'est pas vide. La Faction Acier y a etabli\n"
            "son territoire. Ils ont des chasseurs — des predateurs\n"
            "qui traquent nos unites. Gardez tout le monde en vie.\"\n",
        .dialog_after  =
            "SERGENT KIRA :\n"
            "\"Toutes les unites sont rentrees. Pas une egratignure.\n"
            "Mais la Faction surveille nos mouvements. Leur chef\n"
            "a envoye un ultimatum. Il considere notre bastion comme une menace.\"\n",
        .min_waves  = 10,
        .unlock_msg = NULL,
    },

    [10] = {
        .chapter  = 3, .act = 1,
        .title    = "La faction Acier",
        .subtitle = "Chapitre 4 — La Ville en Ruine",
        .theme    = THEME_CITY,
        .objective = {
            .type        = OBJ_COLLECT_MATERIALS,
            .target      = 1,
            .before_wave = 0,
            .description = "Collecter et appliquer au moins 1 materiau",
        },
        .dialog_before =
            "CHEF DE LA FACTION ACIER :\n"
            "\"Voss. Votre bastion est une erreur. On survit en prenant,\n"
            "pas en construisant. Vous allez tomber. Et quand vous tomberez,\n"
            "la Faction Acier sera la pour ramasser ce qui reste.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"L'armure de la Faction cede sous nos materiaux modifies.\n"
            "Il recule — vers le parlement en ruine. Il a encore\n"
            "ses gardes d'elite. Ce n'est pas fini.\"\n",
        .min_waves  = 11,
        .unlock_msg = NULL,
    },

    [11] = {
        .chapter  = 3, .act = 2,
        .title    = "Siege du parlement",
        .subtitle = "Chapitre 4 — La Ville en Ruine",
        .theme    = THEME_CITY,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 12,
            .before_wave = 0,
            .description = "Tenir 12 vagues et prendre le parlement",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Le chef de la Faction se retranche dans l'ancien parlement.\n"
            "Deux points d'acces a defendre. Ses gardes d'elite tiennent\n"
            "les couloirs. Capturez-le vivant — il sait quelque chose.\"\n",
        .dialog_after  =
            "CHEF CAPTIF :\n"
            "\"...Vous avez gagne, Voss. Alors je vous le dis :\n"
            "les hordes ne viennent pas d'elles-memes. Quelque chose\n"
            "les dirige. Une intelligence. Dans l'usine au nord.\"\n",
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
        .subtitle = "Chapitre 5 — L'Usine Abandonnee",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_COLLECT_MATERIALS,
            .target      = 1,
            .before_wave = 0,
            .description = "Collecter 1 materiau dans l'usine",
        },
        .dialog_before =
            "VOSS :\n"
            "\"L'usine tourne encore. Toute seule. Des robots de guerre\n"
            "sortent des chaines de montage et marchent vers nous sans\n"
            "s'arreter. Recuperez les composants internes. C'est notre ouverture.\"\n",
        .dialog_after  =
            "INGENIEURE VARELA :\n"
            "\"Ces composants... des nano-structures que je n'avais jamais vues.\n"
            "Avec ca, on peut perturber les signaux de controle robotique.\n"
            "Mais la source est plus profonde. Il faut aller au coeur.\"\n",
        .min_waves  = 15,
        .unlock_msg = NULL,
    },

    [13] = {
        .chapter  = 4, .act = 1,
        .title    = "Coeur de l'usine",
        .subtitle = "Chapitre 5 — L'Usine Abandonnee",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_COLLECT_MATERIALS,
            .target      = 3,
            .before_wave = 0,
            .description = "Desactiver 3 generateurs avec des ouvriers",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Trois generateurs alimentent le systeme central.\n"
            "Tant qu'ils tournent, la production ne s'arrete pas.\n"
            "Envoyez les ouvriers — on les couvre pendant la manoeuvre.\"\n",
        .dialog_after  =
            "VARELA :\n"
            "\"Tous les generateurs sont hors service. La production s'est arretee.\n"
            "Mais l'IA centrale — NEXUS — tourne toujours sur batteries de secours.\n"
            "Elle envoie tout ce qu'il lui reste. Chasseurs, artillerie... tout.\"\n",
        .min_waves  = 10,
        .unlock_msg = NULL,
    },

    [14] = {
        .chapter  = 4, .act = 2,
        .title    = "NEXUS",
        .subtitle = "Chapitre 5 — L'Usine Abandonnee",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 20,
            .before_wave = 0,
            .description = "Survivre 20 vagues et resister a NEXUS",
        },
        .dialog_before =
            "NEXUS — IA CENTRALE :\n"
            "\"COMMANDANT VOSS. ANALYSE TERMINEE.\n"
            "PROBABILITE DE VOTRE SURVIE : 0,4 POURCENT.\n"
            "TOUTES LES UNITES DISPONIBLES SONT EN ROUTE.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"NEXUS est hors ligne. Les hordes... se sont arretees.\n"
            "D'un coup. Partout en meme temps. Pour la premiere fois\n"
            "depuis des annees — le silence. Un vrai silence.\n"
            "On a gagne. Maintenant, on reconstruit.\"\n",
        .min_waves  = 20,
        .unlock_msg = "FIN DE CAMPAGNE — Le monde peut recommencer.",
    },
};

// ════════════════════════════════════════════════════
// API
// ════════════════════════════════════════════════════
const ActData *campaign_act_get(int stage_index) {
    if (stage_index < 0 || stage_index >= CAMPAIGN_TOTAL) return &ACT_DATA[0];
    return &ACT_DATA[stage_index];
}

int campaign_act_index(int chapter, int act) {
    return chapter * CAMPAIGN_ACTS + act;
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