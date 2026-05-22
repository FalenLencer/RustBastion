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

    [0] = {
        .chapter  = 0, .act = 0,
        .title    = "Premier contact",
        .subtitle = "Chapitre 1 — Les Terres Brulees",
        .theme    = THEME_WASTELAND,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 5,
            .before_wave = 0,
            .description = "Survivre 5 vagues",
        },
        .dialog_before =
            "COMMANDANT VOSS :\n"
            "\"Rapport de situation : les routes du nord sont coupees.\n"
            "Des pillards ont ete signales a moins de deux kilometres.\n"
            "Fortifiez les defenses. Maintenant.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"On tient encore. Mais ce n'etait que l'avant-garde.\n"
            "Les survivants du convoi nord rejoignent le bastion.\n"
            "Nous allons avoir besoin de chaque paire de mains.\"\n",
        .min_waves  = 5,
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
            .description = "Defendre 2 bases simultanement",
        },
        .dialog_before =
            "RADIO :\n"
            "\"Commandant, le poste avance est sous pression.\n"
            "Nous avons besoin d'une diversion sur le flanc est.\n"
            "Les brutes commencent a apparaitre dans les rangs ennemis.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"La route est ouverte. Le convoi peut passer.\n"
            "Des ingenieurs rejoignent nos rangs — ils ont trouve\n"
            "des plans de construction avances dans les ruines.\"\n",
        .min_waves  = 6,
        .unlock_msg = "DEBLOQUE : Tourelle Sniper",
        .forced_base_count = 2,  // texte impose 2 bases
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
            "\"Un vehicule blinde escort des pillards vers nos positions.\n"
            "S'il passe, le bastion est coupe en deux.\n"
            "Concentrez le feu. Tout le monde sur le pont.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"Le vehicule est neutralise. Dans les decombres,\n"
            "nous avons trouve des plans de tourelle electrique.\n"
            "L'ingenieur dit qu'il peut la construire. La Tesla est en ligne.\"\n",
        .min_waves  = 7,
        .unlock_msg = "DEBLOQUE : Tourelle Tesla",
    },

    // ── CHAPITRE 2 — LE MARAIS TOXIQUE ───────────────────────

    [3] = {
        .chapter  = 1, .act = 0,
        .title    = "Brumes vertes",
        .subtitle = "Chapitre 2 — Le Marais Toxique",
        .theme    = THEME_SWAMP,
        .objective = {
            .type        = OBJ_RECRUIT_UNITS,
            .target      = 3,
            .before_wave = 3,
            .description = "Recruter 3 unites avant la vague 3",
        },
        .dialog_before =
            "ECLAIREUR :\n"
            "\"Commandant, les brumes du marais cachent quelque chose.\n"
            "Des formes — pas tout a fait humaines — se deplacent\n"
            "entre les cypres. Et certaines... disparaissent.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"Les mutants du marais sont plus nombreux que prevu.\n"
            "Mais nos eclaireurs ont appris a les pister.\n"
            "Certains ennemis sont invisibles aux tours — vos hommes\n"
            "devront faire le travail.\"\n",
        .min_waves  = 5,
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
            "VOSS :\n"
            "\"Un labo pharmaceutique pre-guerre est partiellement emerge.\n"
            "Des reservoirs de composants acides y sont encore intacts.\n"
            "Si on peut les recuperer... nos armes seront bien plus efficaces.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"Excellent travail. Les ouvriers ont ramene les composants.\n"
            "L'alchimiste du bastion peut maintenant modifier nos munitions.\n"
            "Les tours peuvent desormais appliquer des types de degats specifiques.\"\n",
        .min_waves  = 5,
        .unlock_msg = "DEBLOQUE : Systeme de materiaux",
    },

    [5] = {
        .chapter  = 1, .act = 2,
        .title    = "La reine des marais",
        .subtitle = "Chapitre 2 — Le Marais Toxique",
        .theme    = THEME_SWAMP,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 8,
            .before_wave = 0,
            .description = "Survivre 8 vagues et eliminer le boss",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Quelqu'un controle les mutants. Un signal coordonne.\n"
            "Une creature massive approche du centre du marais.\n"
            "C'est la source. Detruisez-la.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"La creature est tombee. Mais avant de mourir...\n"
            "elle a emis un signal. Quelqu'un d'autre les controle.\n"
            "Et ce quelqu'un vient du desert.\"\n",
        .min_waves  = 8,
        .unlock_msg = "DEBLOQUE : Tourelle Flammes",
    },

    // ── CHAPITRE 3 — LE DESERT IRRADIE ───────────────────────

    [6] = {
        .chapter  = 2, .act = 0,
        .title    = "Tempete de sable",
        .subtitle = "Chapitre 3 — Le Desert Irradie",
        .theme    = THEME_DESERT,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 6,
            .before_wave = 0,
            .description = "Survivre 6 vagues avec portee reduite",
        },
        .dialog_before =
            "VOSS :\n"
            "\"La tempete de sable bloque les capteurs.\n"
            "La portee de nos tours est reduite de moitie.\n"
            "Adaptez la disposition. On n'a pas le choix.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"La tempete se leve. Et avec elle, un signal radio.\n"
            "Crypte. Militaire. Il vient d'un ancien complexe\n"
            "quelque part a l'est du desert.\"\n",
        .min_waves  = 6,
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
            .description = "Proteger 2 bases reparties sur la carte",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Les ruines de l'ancienne ville sont un labyrinthe.\n"
            "Des machines d'artillerie ennemies se positionnent\n"
            "a distance. Elles peuvent detruire nos tours.\n"
            "Protegez les deux points d'ancrage a tout prix.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"Le signal vient bien du complexe militaire.\n"
            "Ce n'est pas un humain qui l'emet.\n"
            "C'est une machine.\"\n",
        .min_waves  = 7,
        .unlock_msg = NULL,
        .forced_base_count = 2,  // texte impose 2 bases (MAX_BASES=2)
    },

    [8] = {
        .chapter  = 2, .act = 2,
        .title    = "Le general machine",
        .subtitle = "Chapitre 3 — Le Desert Irradie",
        .theme    = THEME_DESERT,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 10,
            .before_wave = 0,
            .description = "Survivre 10 vagues et detruire le general",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Une unite d'artillerie lourde commandee par une IA militaire.\n"
            "Elle coordonne les attaques. Elle apprend.\n"
            "Detruisez-la avant qu'elle adapte sa strategie.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"L'IA est defaillante. Mais pas detruite.\n"
            "Dans ses circuits, nos techniciens ont recupere\n"
            "une routine de controle de drones. Defective, mais utilisable.\n"
            "Le destin de ce bastion pourrait changer.\"\n",
        .min_waves  = 10,
        .unlock_msg = "DEBLOQUE : Systeme de drones (prochaine campagne)",
    },

    // ── CHAPITRE 4 — LA VILLE EN RUINE ───────────────────────

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
            "\"La ville n'est pas abandonnee. Une faction humaine\n"
            "organisee y a etabli son territoire. Ils ont des chasseurs —\n"
            "rapides, impitoyables. Protegez vos hommes.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"Nos unites sont intactes. Bien.\n"
            "Mais la Faction Acier surveille nos mouvements.\n"
            "Leur chef a envoye un ultimatum. Il arrive.\"\n",
        .min_waves  = 6,
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
            .description = "Utiliser du materiau PLASMA pour percer les armures",
        },
        .dialog_before =
            "CHEF DE LA FACTION ACIER :\n"
            "\"Commandant Voss. Votre bastion est une aberration.\n"
            "Nous avons survecu en prenant. Vous survivez en construisant.\n"
            "C'est une faiblesse. Nos soldats porteront votre etendard\n"
            "quand vous serez tombe.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"L'armure electrique de la faction est percee.\n"
            "Le plasma fonctionne. Maintenez la pression.\n"
            "Le chef recule vers le parlement en ruine.\"\n",
        .min_waves  = 8,
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
            .description = "Tenir 12 vagues et capturer le chef",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Le chef de la Faction Acier se retranche dans l'ancien parlement.\n"
            "Deux points vitaux a defendre. Des gardes d'elite partout.\n"
            "Capturez-le vivant. Il sait quelque chose sur les hordes.\"\n",
        .dialog_after  =
            "CHEF CAPTIF :\n"
            "\"...Vous avez gagne, Voss. Alors sachez ceci :\n"
            "les hordes ne viennent pas d'elles-memes.\n"
            "Quelque chose les dirige. Une intelligence. Dans l'usine.\"\n",
        .min_waves  = 12,
        .unlock_msg = NULL,
        .forced_base_count = 2,  // texte impose 2 points vitaux
    },

    // ── CHAPITRE 5 — L'USINE ABANDONNEE ──────────────────────

    [12] = {
        .chapter  = 4, .act = 0,
        .title    = "La source",
        .subtitle = "Chapitre 5 — L'Usine Abandonnee",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_COLLECT_MATERIALS,
            .target      = 1,
            .before_wave = 0,
            .description = "Collecter du materiau NANO pour pirater les robots",
        },
        .dialog_before =
            "VOSS :\n"
            "\"L'usine tourne encore. Toute seule.\n"
            "Des robots de guerre sortent des chaines de montage\n"
            "et marchent directement vers nous.\n"
            "Trouvez les composants NANO. C'est notre seule chance.\"\n",
        .dialog_after  =
            "TECHNICIEN :\n"
            "\"Commandant, avec ces composants nano, on peut\n"
            "perturber les signaux de controle des robots.\n"
            "Mais la source est plus profonde. Il y a une IA centrale.\"\n",
        .min_waves  = 6,
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
            .description = "Envoyer des ouvriers desactiver 3 generateurs",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Trois generateurs alimentent le systeme central.\n"
            "Tant qu'ils tournent, les robots ne s'arretent pas.\n"
            "Envoyez les ouvriers. On les protege.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"Deux generateurs hors service. Le troisieme resiste.\n"
            "L'IA a detecte notre strategie. Elle envoie ses meilleures unites.\n"
            "Les soigneurs, les chasseurs, l'artillerie... tout.\"\n",
        .min_waves  = 0,  // objectif de collecte, pas de vagues min
        .unlock_msg = NULL,
    },

    [14] = {
        .chapter  = 4, .act = 2,
        .title    = "NEXUS",
        .subtitle = "Chapitre 5 — L'Usine Abandonnee",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 15,
            .before_wave = 0,
            .description = "Survivre 15 vagues et detruire NEXUS",
        },
        .dialog_before =
            "NEXUS — IA CENTRALE :\n"
            "\"COMMANDANT VOSS. IDENTIFICATION COMPLETE.\n"
            "VOUS ETES UNE VARIABLE NON PREVUE.\n"
            "PROTOCOLE DE NEUTRALISATION : ACTIF.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"NEXUS est hors ligne.\n"
            "Les hordes... se sont arretees. D'un coup.\n"
            "Pour la premiere fois depuis des annees,\n"
            "le silence. Un vrai silence.\n"
            "On a gagne. Maintenant on reconstruit.\"\n",
        .min_waves  = 15,
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