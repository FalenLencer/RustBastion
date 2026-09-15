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
        .min_waves  = 12,   /* lissé : 15 était un pic isolé (tous les autres 8-12) */
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
            .description = "Survivre 9 vagues en terrain decouvert",
        },
        .dialog_before =
            "SERGENT KIRA :\n"
            "\"Vous avez choisi la voie directe. Pas de couvert, pas de\n"
            "labo ou se retrancher — juste la brume et ce qui s'y cache.\n"
            "On suit le signal jusqu'a la source. Restez groupes.\"\n",
        .dialog_after  =
            "KIRA :\n"
            "\"La piste mene droit au coeur du marais. La creature qui\n"
            "coordonne les mutants est juste devant. Pas de detour cette\n"
            "fois — on l'affronte de face.\"\n",
        .min_waves  = 9,
        .unlock_msg = NULL,
    },

    // [16] Détour "réacteur" du Chapitre 5 (échec collecte au nœud 13) → NEXUS (14)
    [16] = {
        .chapter  = 4, .act = 1,      // difficulté ≈ milieu Ch.5
        .title    = "Le reacteur s'emballe",
        .subtitle = "Chapitre 5 — L'Usine Abandonnee",
        .theme    = THEME_FACTORY,
        .objective = {
            .type        = OBJ_SURVIVE_WAVES,
            .target      = 9,
            .before_wave = 0,
            .description = "Tenir 9 vagues malgre la surchauffe",
        },
        .dialog_before =
            "INGENIEURE VARELA :\n"
            "\"Les generateurs n'ont pas tous ete neutralises — le coeur\n"
            "entre en surchauffe. C'est dangereux, mais l'instabilite\n"
            "perturbe aussi les robots. Servez-vous-en. Tenez la ligne.\"\n",
        .dialog_after  =
            "VARELA :\n"
            "\"La surchauffe a force NEXUS a rediriger son energie.\n"
            "Une breche s'ouvre vers le coeur central. C'est notre\n"
            "chance — on fonce sur NEXUS maintenant.\"\n",
        .min_waves  = 9,
        .unlock_msg = NULL,
    },

    // [17] Ch1 — branche "embuscade" (choix au nœud 1) → rejoint le convoi (2)
    [17] = {
        .chapter  = 0, .act = 1,
        .title    = "L'embuscade des cendres",
        .subtitle = "Chapitre 1 — Les Terres Brulees",
        .theme    = THEME_WASTELAND,
        .objective = {
            .type = OBJ_KILL_ENEMIES, .target = 25, .before_wave = 0,
            .description = "Aneantir 25 pillards dans l'embuscade",
        },
        .dialog_before =
            "SERGENT KIRA :\n"
            "\"On ne les escorte pas — on les saigne. Position en tenaille\n"
            "dans les cendres, on les laisse entrer puis on referme.\n"
            "Risque : s'ils percent, ils filent droit sur la base.\"\n",
        .dialog_after  =
            "KIRA :\n"
            "\"Embuscade nette. Le convoi est en miettes avant meme\n"
            "d'avoir atteint nos murs. Ca va faire jaser chez les pillards.\"\n",
        .min_waves  = 9,
        .unlock_msg = NULL,
    },

    // [18] Ch3 — branche "frappe rapide" (choix au nœud 6) → rejoint le Général (8)
    [18] = {
        .chapter  = 2, .act = 1,
        .title    = "Course contre la machine",
        .subtitle = "Chapitre 3 — Le Desert Irradie",
        .theme    = THEME_DESERT,
        .objective = {
            .type = OBJ_SURVIVE_WAVES, .target = 8, .before_wave = 0,
            .description = "Percer vers l'IA en 8 vagues, a decouvert",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Pas le temps de fortifier deux points : on fonce sur le\n"
            "signal. Une seule ligne, expose au vent de sable. Si on tient,\n"
            "on gagne un temps precieux sur l'IA. Sinon... on improvise.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"Le pari paie : on a coupe court a ses calculs.\n"
            "L'IA n'a pas eu le temps d'organiser sa defense. En avant.\"\n",
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
        .dialog_before =
            "CHEF DE LA FACTION ACIER :\n"
            "\"Vos jouets n'ont pas perce notre blindage, Voss. Maintenant\n"
            "c'est nous qui dictons le tempo. Vous allez saigner pour\n"
            "chaque metre — et nous, nous avons tout notre temps.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"On a encaisse sans plasma efficace — a la sueur et au sang.\n"
            "La Faction nous croit a bout. Qu'elle continue de le croire.\n"
            "Le parlement est a portee.\"\n",
        .min_waves  = 11,
        .unlock_msg = NULL,
    },

    // [20] Ch5 — branche "sabotage" (choix au nœud 12) → rejoint le cœur (13)
    [20] = {
        .chapter  = 4, .act = 0,
        .title    = "Sabotage de la chaine",
        .subtitle = "Chapitre 5 — L'Usine Abandonnee",
        .theme    = THEME_FACTORY,
        .objective = {
            .type = OBJ_KILL_ENEMIES, .target = 35, .before_wave = 0,
            .description = "Detruire 35 robots sortis des chaines",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Oubliez le piratage fin : on casse la chaine de montage.\n"
            "Plus brutal, plus de robots d'un coup — mais si on en vient\n"
            "a bout, la production s'effondre net. Tenez le choc.\"\n",
        .dialog_after  =
            "TECHNICIEN :\n"
            "\"Chaine pulverisee. NEXUS a perdu son bras arme principal.\n"
            "Le coeur de l'usine est expose. On peut viser plus profond.\"\n",
        .min_waves  = 10,
        .unlock_msg = NULL,
    },

    // [21] Repli après la Reine (défaite nœud 5) → entre dans le desert (6)
    [21] = {
        .chapter  = 1, .act = 1,
        .title    = "La derniere ligne",
        .subtitle = "Chapitre 2 — Le Marais Toxique",
        .theme    = THEME_SWAMP,
        .objective = {
            .type = OBJ_SURVIVE_WAVES, .target = 10, .before_wave = 0,
            .description = "Couvrir la retraite : tenir 10 vagues",
        },
        .dialog_before =
            "KIRA :\n"
            "\"La Reine nous a submerges. On decroche — mais pas en deroute.\n"
            "On tient cette derniere ligne assez longtemps pour evacuer.\n"
            "Chaque seconde gagnee, c'est des vies sauvees.\"\n",
        .dialog_after  =
            "VOSS :\n"
            "\"Repli reussi. On a perdu le marais, pas la guerre.\n"
            "Le signal venait du desert — c'est la qu'on rebondit.\"\n",
        .min_waves  = 10,
        .unlock_msg = NULL,
    },

    // [22] Repli après le Général (défaite nœud 8) → entre dans la ville (9)
    [22] = {
        .chapter  = 2, .act = 1,
        .title    = "Repli dans les ruines",
        .subtitle = "Chapitre 3 — Le Desert Irradie",
        .theme    = THEME_DESERT,
        .objective = {
            .type = OBJ_SURVIVE_WAVES, .target = 9, .before_wave = 0,
            .description = "Survivre 9 vagues en retraite ordonnee",
        },
        .dialog_before =
            "VOSS :\n"
            "\"Le general machine nous a deborde. On se replie vers la ville\n"
            "en ruine — terrain hostile, mais qui ralentit ses blindes.\n"
            "On transforme la defaite en piege.\"\n",
        .dialog_after  =
            "KIRA :\n"
            "\"On a sème l'IA dans les decombres. Elle nous suit en ville,\n"
            "mais sur NOTRE terrain maintenant. A nous de jouer.\"\n",
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
        case 21: return 6;   // "La derniere ligne" (repli Reine)   → désert (6)
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
    if (stage_index == 5) return 21;   // défaite contre la Reine → "La derniere ligne"
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
    { CFLAG_ESCORT, "CONVOI SAUVE", "Convoi",
      "+1 unite deployable, +5% d'ennemis", 1,
        0, 0, 0, 1,   1.05f, 1.00f, 1.00f },
    { CFLAG_AMBUSH, "BUTIN DES CENDRES", "Butin",
      "+30 or par acte, -8 vies par acte", 0,
       30, -8, 0, 0,  1.00f, 1.00f, 1.00f },

    /* ── Ch2 : la source du signal ────────────────────────── */
    { CFLAG_LAB, "ARSENAL CHIMIQUE", "Arsenal",
      "+1 tour deployable, ennemis +6% de PV", 1,
        0, 0, 1, 0,   1.00f, 1.06f, 1.00f },
    { CFLAG_TRACK, "RENSEIGNEMENT", "Renseign.",
      "Ennemis -8% de PV, -20 or par acte", 0,
      -20, 0, 0, 0,   1.00f, 0.92f, 1.00f },

    /* ── Ch3 : l'IA militaire ─────────────────────────────── */
    { CFLAG_ANCHOR, "LIGNE FORTIFIEE", "Ligne",
      "+12 vies par acte, +6% d'ennemis", 1,
        0, 12, 0, 0,  1.06f, 1.00f, 1.00f },
    { CFLAG_FAST_STRIKE, "FRAPPE ECLAIR", "Frappe",
      "-10% d'ennemis, -1 tour deployable", 0,
        0, 0, -1, 0,  0.90f, 1.00f, 1.00f },

    /* ── Ch5 : l'usine ────────────────────────────────────── */
    { CFLAG_HACK, "CODES D'ACCES", "Codes",
      "+1 unite deployable, +6% d'ennemis", 1,
        0, 0, 0, 1,   1.06f, 1.00f, 1.00f },
    { CFLAG_SABOTAGE, "CHAINE BRISEE", "Chaine",
      "-10% d'ennemis, ennemis +10% de vitesse", 0,
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
        case 1:  return "Le convoi blinde force le passage. Comment l'aborder ?";
        case 3:  return "La source du signal se rapproche. Quelle piste suivez-vous ?";
        case 6:  return "L'IA militaire se terre a l'est. Quelle approche ?";
        case 12: return "L'usine grouille de robots de guerre. Par ou frapper ?";
        default: return NULL;
    }
}

/* Libellés : ils annoncent la DOCTRINE installée, plus une vague promesse
   de « risque » que rien ne tenait. La conséquence chiffrée est donnée
   juste en dessous par campaign_choice_effect. */
const char *campaign_choice_label(int stage_index, int idx) {
    switch (stage_index) {
        case 1:  return (idx == 0)
            ? "Escorter le convoi           -> CONVOI SAUVE"
            : "Tendre une embuscade         -> BUTIN DES CENDRES";
        case 3:  return (idx == 0)
            ? "Fouiller le laboratoire      -> ARSENAL CHIMIQUE"
            : "Traquer la source            -> RENSEIGNEMENT";
        case 6:  return (idx == 0)
            ? "Tenir deux ancrages          -> LIGNE FORTIFIEE"
            : "Course rapide vers l'IA      -> FRAPPE ECLAIR";
        case 12: return (idx == 0)
            ? "Pirater les robots           -> CODES D'ACCES"
            : "Saboter la chaine            -> CHAINE BRISEE";
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
   nette que la voie risquée — sinon jouer prudent revient à ne pas jouer. */
const char *campaign_echo(int stage_index, int flags) {
    switch (stage_index) {
        case 4: case 15:
            if (flags & CFLAG_AMBUSH)
                return "KIRA : Apres votre embuscade des Terres Brulees, les hordes\n"
                       "vous savent imprevisible. C'est un avantage — gardons-le.";
            if (flags & CFLAG_ESCORT)
                return "KIRA : Les rescapes du convoi se battent avec nous, maintenant.\n"
                       "Ils connaissent le terrain. Ca compense ce qu'ils nous coutent.";
            break;
        case 6:
            if (flags & CFLAG_TRACK)
                return "VARELA : Vous aviez traque la source vous-meme dans le marais.\n"
                       "On sait deja a quoi ressemble l'ennemi du desert.";
            if (flags & CFLAG_LAB)
                return "VARELA : L'arsenal chimique du labo tient toujours la route.\n"
                       "Une position de plus a armer — mais ils ont eu le temps aussi.";
            break;
        case 9:
            if (flags & CFLAG_FAST_STRIKE)
                return "KIRA : Votre frappe eclair contre l'IA a laisse des traces.\n"
                       "La Faction Acier vous attend de pied ferme.";
            if (flags & CFLAG_ANCHOR)
                return "VOSS : Nos ancrages du desert tiennent encore. On avance lentement,\n"
                       "mais on n'a rien cede. La ville se prendra de la meme facon.";
            if (flags & CFLAG_LOST_GENERAL)
                return "VOSS : On a plie face au general, mais on est encore la.\n"
                       "La ville sera notre revanche.";
            break;
        case 12:
            if (flags & CFLAG_AMBUSH)
                return "VARELA : Vos coups tordus depuis le debut paient :\n"
                       "NEXUS n'a jamais pu modeliser votre strategie.";
            if (flags & CFLAG_ESCORT)
                return "VARELA : NEXUS a toutes vos manoeuvres en archive. Chaque ligne\n"
                       "que vous avez tenue proprement, il l'a apprise par coeur.";
            break;
        case 14:
            if (flags & CFLAG_SABOTAGE)
                return "VARELA : La chaine sabotee a ampute ses defenses.\n"
                       "C'est notre fenetre de tir. Maintenant.";
            if (flags & CFLAG_HACK)
                return "VARELA : Les codes voles nous ouvrent ses couches basses.\n"
                       "Il se defend moins bien — mais il vous voit venir.";
            break;
        default: break;
    }
    return NULL;
}

/* ── Dialogue d'introduction (dynamique pour NEXUS) ────────────
   L'acte final EST la confrontation au modèle : NEXUS annonce le taux de
   concordance RÉELLEMENT calculé sur les décisions du joueur. Buffers
   statiques (rendu mono-thread) ; DEUX buffers distincts pour que l'intro
   et l'épilogue ne puissent jamais s'écraser l'un l'autre. */
static char g_dlg_buf[512];   /* campaign_dialog_before */
static char g_epi_buf[512];   /* campaign_epilogue      */

const char *campaign_dialog_before(int node_id, int flags) {
    const ActData *a = campaign_act_get(node_id);
    if (node_id != CAMPAIGN_TOTAL - 1) return a->dialog_before;

    int pred = campaign_nexus_prediction(flags);
    const char *verdict;
    if (pred >= 74)
        verdict = "VOS DECISIONS SUIVENT UN SCHEMA. JE LES CONNAIS DEJA.\n"
                  "TOUTES LES UNITES SONT POSITIONNEES OU VOUS IREZ.";
    else if (pred >= 40)
        verdict = "VOTRE SCHEMA RESTE PARTIELLEMENT RESOLU.\n"
                  "MARGE D'ERREUR ACCEPTABLE. J'ENGAGE.";
    else
        verdict = "VARIABLE NON RESOLUE. VOS DECISIONS NE CONVERGENT PAS.\n"
                  "AUCUN MODELE FIABLE. J'ENGAGE SANS PREDICTION.";

    snprintf(g_dlg_buf, sizeof(g_dlg_buf),
             "NEXUS — IA CENTRALE :\n"
             "\"COMMANDANT VOSS. MODELE COMPORTEMENTAL : %d POURCENT\n"
             "DE CONCORDANCE.\n"
             "%s\"\n", pred, verdict);
    return g_dlg_buf;
}

/* Épilogue : la dernière chose que NEXUS dit avant de s'éteindre est la
   MESURE de ce que le joueur a été. L'indice de prédiction n'est pas une
   statistique cachée — c'est la conclusion morale de la campagne. */
const char *campaign_epilogue(int flags) {
    int losses = (flags & (CFLAG_LOST_QUEEN | CFLAG_LOST_GENERAL)) ? 1 : 0;
    int pred   = campaign_nexus_prediction(flags);

    if (pred <= 30)
        snprintf(g_epi_buf, sizeof(g_epi_buf),
            "NEXUS — DERNIERE TRANSMISSION :\n"
            "\"CONCORDANCE FINALE : %d POURCENT. JE NE VOUS AI JAMAIS VU VENIR.\"\n"
            "\n"
            "VOSS :\n"
            "\"Il a modelise des armees entieres. Pas vous. Vous avez gagne\n"
            "en refusant d'etre une equation%s\"\n",
            pred,
            losses ? " — et ca nous a coute des vies. Elles comptent aussi."
                   : ". On reconstruit. A votre facon.");
    else if (pred >= 70)
        snprintf(g_epi_buf, sizeof(g_epi_buf),
            "NEXUS — DERNIERE TRANSMISSION :\n"
            "\"CONCORDANCE FINALE : %d POURCENT. JE VOUS AVAIS PREVU.\n"
            "JE N'AVAIS PAS PREVU QUE CELA NE SUFFISE PAS.\"\n"
            "\n"
            "VOSS :\n"
            "\"Il savait chacun de nos gestes. On les a faits quand meme,\n"
            "mieux que lui%s\"\n",
            pred,
            losses ? ", et on a paye le prix fort. Le bastion s'en souviendra."
                   : ". Discipline contre calcul. Le bastion tient, intact.");
    else
        snprintf(g_epi_buf, sizeof(g_epi_buf),
            "NEXUS — DERNIERE TRANSMISSION :\n"
            "\"CONCORDANCE FINALE : %d POURCENT. DONNEES INSUFFISANTES.\"\n"
            "\n"
            "VOSS :\n"
            "\"Ni tout a fait previsible, ni tout a fait fou. On a fait ce\n"
            "qu'il fallait, quand il le fallait%s\"\n",
            pred,
            losses ? ". On a perdu des positions. On les reprendra."
                   : ". Maintenant, on reconstruit.");
    return g_epi_buf;
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
        case MUT_TOXIC:     return "TOXICITE — ennemis endurcis";
        case MUT_SANDSTORM: return "TEMPETE — ennemis plus rapides";
        case MUT_AMBUSH:    return "EMBUSCADE — essaims renforces";
        case MUT_OVERLOAD:  return "SURREGIME — vitesse & blindage";
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