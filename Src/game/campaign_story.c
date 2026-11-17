/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  campaign_story.c ─ Les SCÈNES dialoguées de la campagne.
 *
 *  Un script par nœud et par moment (avant / après l'acte). Format lu par
 *  story_scene.c : une réplique = une ligne « NOM : » puis son texte,
 *  répliques séparées par une ligne vide. Le nom choisit le portrait
 *  (VOSS, KIRA, VARELA, CHEF ACIER, NEXUS ; tout autre nom = radio).
 *
 *  Règles d'écriture : 2 à 4 répliques, une idée par réplique, < 220
 *  caractères (la boîte de dialogue tient 4 lignes). Chaque scène doit
 *  dire POURQUOI on se bat dans cet acte et ce que la victoire débloque.
 */
#include "campaign_data.h"
#include <stddef.h>

static const char *const STORY[CAMPAIGN_NODES][2] = {

    // ═══ CHAPITRE 1 — LES TERRES BRÛLÉES ═════════════════════════
    [0] = {
        "VOSS :\n"
        "Routes du nord coupées. Des pillards à deux kilomètres, et ils ne "
        "s'arrêtent pas pour négocier. Ce n'est pas une escarmouche : c'est une offensive.\n"
        "\n"
        "KIRA :\n"
        "Une trentaine, commandant. Motos, fusils de récupération, un ou deux "
        "lance-roquettes. Ils comptent sur le nombre.\n"
        "\n"
        "VOSS :\n"
        "Alors nous comptons sur nos murs. Une tourelle sur chaque approche, "
        "Sergent. Personne ne passe le portail.\n",

        "KIRA :\n"
        "Ils refluent ! Commandant, ils refluent vraiment.\n"
        "\n"
        "VOSS :\n"
        "Pas de victoire trop tôt. C'était l'avant-garde. Ce qui suit sera plus lourd.\n"
        "\n"
        "RADIO :\n"
        "Bastion, ici le convoi de Pierrefeu. Deux camions, des blessés. On vous "
        "rejoint par la route du nord. Tenez bon.\n",
    },

    [1] = {
        "KIRA :\n"
        "Le poste avancé saute si on le lâche. Et ils poussent en même temps sur "
        "le flanc est.\n"
        "\n"
        "VOSS :\n"
        "Deux fronts. On ne choisit pas lequel tenir : on tient les deux.\n"
        "\n"
        "KIRA :\n"
        "Il y a des brutes dans leurs rangs. Des colosses gavés de stimulants. "
        "Jamais vu ça chez des pillards.\n",

        "VOSS :\n"
        "La route est ouverte. Bon travail, Kira.\n"
        "\n"
        "VARELA :\n"
        "Ingénieure Varela, du convoi de Pierrefeu. J'ai récupéré en route les plans "
        "d'un fusil longue portée. Je vous monte une tourelle sniper dès ce soir.\n"
        "\n"
        "KIRA :\n"
        "Mauvaise nouvelle, commandant. Un blindé ouvre la route à une colonne "
        "entière. Il fonce droit sur le convoi.\n",
    },

    [2] = {
        "VOSS :\n"
        "Le blindé est à portée. S'il passe, le bastion est coupé en deux.\n"
        "\n"
        "VARELA :\n"
        "Visez les chenilles, pas la tourelle. Un blindé immobile, c'est une boîte "
        "de conserve.\n"
        "\n"
        "KIRA :\n"
        "Une très grosse boîte de conserve. Tout le monde au feu !\n",

        "KIRA :\n"
        "Il fume encore. Il ne bougera plus.\n"
        "\n"
        "VARELA :\n"
        "Commandant, venez voir. Sous le blindage : un générateur électromagnétique "
        "militaire, intact. Avec ça, je vous fais une tourelle Tesla.\n"
        "\n"
        "VOSS :\n"
        "Faites-le. Et dites aux rescapés qu'ils ont un toit. Pour l'instant.\n",
    },

    // ═══ CHAPITRE 2 — LE MARAIS TOXIQUE ══════════════════════════
    [3] = {
        "KIRA :\n"
        "Le marais cache quelque chose. Des formes pas tout à fait humaines, entre "
        "les cyprès morts. L'une a disparu sous les yeux de mon éclaireur.\n"
        "\n"
        "VARELA :\n"
        "Disparu ? Camouflage optique, ou une mutation qui dévie la lumière. "
        "Fascinant.\n"
        "\n"
        "VOSS :\n"
        "Fascinant ou pas, une tourelle ne tire pas sur ce qu'elle ne voit pas. "
        "Il me faut des hommes sur le terrain : trois équipes vivantes, minimum.\n",

        "VOSS :\n"
        "Les invisibles ne se trahissent qu'au contact de nos unités. Gardez-les "
        "toujours près des tourelles.\n"
        "\n"
        "KIRA :\n"
        "Mes éclaireurs ont capté un signal sous la brume. Régulier. Comme un "
        "battement de cœur.\n"
        "\n"
        "VARELA :\n"
        "Et il y a un laboratoire d'avant-guerre, à moitié englouti. S'il reste des "
        "réactifs dedans, je transforme nos munitions.\n",
    },

    [4] = {
        "VARELA :\n"
        "Le labo est là, sous la vase. Des réservoirs encore scellés : acide, plasma, "
        "peut-être pire. Envoyez les ouvriers, je les guide par radio.\n"
        "\n"
        "KIRA :\n"
        "Les mutants rôdent autour. Ils sentent les ouvriers comme des requins "
        "sentent le sang.\n"
        "\n"
        "VOSS :\n"
        "Alors on couvre les ouvriers. Chaque caisse ramenée compte.\n",

        "VARELA :\n"
        "Impeccable ! Avec ces composants, je conditionne les munitions. Appliquez "
        "un minerai sur une tourelle et elle change de nature : acide, cryo, plasma.\n"
        "\n"
        "VOSS :\n"
        "Le type de dégâts compte, donc. Bon à savoir avant de croiser la chose qui "
        "coordonne ces mutants.\n",
    },

    [5] = {
        "KIRA :\n"
        "Les attaques sont trop organisées pour des mutants. Une présence massive "
        "converge vers nous depuis le cœur du marais.\n"
        "\n"
        "VARELA :\n"
        "Mes capteurs saturent. Commandant… c'est énorme.\n"
        "\n"
        "VOSS :\n"
        "Énorme, mais ça saigne. Tenez la ligne.\n",

        "KIRA :\n"
        "La Reine est tombée. Le marais se tait.\n"
        "\n"
        "VARELA :\n"
        "Ses glandes produisaient un gaz inflammable. Je l'ai mis en réservoir : "
        "vous avez un lance-flammes.\n"
        "\n"
        "VOSS :\n"
        "Il y a autre chose. Juste avant sa mort, on a capté une transmission "
        "cryptée. Militaire. Elle venait du désert, et elle lui donnait des ordres.\n",
    },

    // ═══ CHAPITRE 3 — LE DÉSERT IRRADIÉ ══════════════════════════
    [6] = {
        "VOSS :\n"
        "Le désert irradié. La tempête bouche tout : on ne voit pas à vingt mètres.\n"
        "\n"
        "VARELA :\n"
        "Le signal qui commandait la Reine vient d'ici. Même fréquence, même "
        "cryptage. Militaire, et toujours actif.\n"
        "\n"
        "KIRA :\n"
        "Ils arrivent de partout, commandant. Le sable les rend fous… ou plus rapides.\n",

        "KIRA :\n"
        "La tempête retombe. Et le signal est plus fort que jamais.\n"
        "\n"
        "VARELA :\n"
        "Je l'ai à moitié triangulé : un complexe à l'est, au-delà des ruines de Vegas.\n"
        "\n"
        "VOSS :\n"
        "On peut avancer pas à pas en tenant deux ancrages… ou foncer avant qu'il "
        "ne s'organise.\n",
    },

    [7] = {
        "VOSS :\n"
        "Les ruines de Vegas. Un labyrinthe de béton, et de l'artillerie ennemie "
        "sur les hauteurs.\n"
        "\n"
        "KIRA :\n"
        "Leurs canons visent nos tourelles, pas nos murs. Ils savent ce qui compte.\n"
        "\n"
        "VARELA :\n"
        "Ce n'est pas un comportement de pillard. C'est… méthodique.\n",

        "VARELA :\n"
        "Signal triangulé. L'émetteur est trop régulier, trop précis. Ce n'est pas "
        "quelqu'un qui les dirige.\n"
        "\n"
        "VOSS :\n"
        "Alors c'est quoi ?\n"
        "\n"
        "VARELA :\n"
        "Quelque chose. Une IA militaire, commandant.\n"
        "\n"
        "GÉNÉRAL MACHINE :\n"
        "Ici le Commandement Sud. Bastion humain identifié, positions cartographiées. "
        "Vos tourelles sont désormais des coordonnées.\n",
    },

    [8] = {
        "VOSS :\n"
        "L'IA commande l'artillerie lourde. Elle coordonne chaque tir en temps réel.\n"
        "\n"
        "VARELA :\n"
        "Et elle apprend de chaque vague. Plus on tient, plus elle s'adapte.\n"
        "\n"
        "GÉNÉRAL MACHINE :\n"
        "Reddition recommandée. Probabilité de survie du bastion : 3 %. "
        "Chaque vague que vous tenez affine mon tir.\n"
        "\n"
        "KIRA :\n"
        "Alors on tient jusqu'à ce que ses calculs saturent.\n",

        "GÉNÉRAL MACHINE :\n"
        "Surchauffe critique. Rapport au Centre : le bastion ne suit pas le modèle. "
        "Demande de nouvelles direct…\n"
        "\n"
        "VARELA :\n"
        "Elle a surchauffé ! Dans ses circuits, j'ai récupéré des fragments de code. "
        "Elle ne commandait pas seule : elle recevait des ordres.\n"
        "\n"
        "VOSS :\n"
        "De qui ?\n"
        "\n"
        "VARELA :\n"
        "Un nom revient partout : NEXUS. Une usine, au nord, au-delà de la ville morte.\n",
    },

    // ═══ CHAPITRE 4 — LA VILLE EN RUINE ══════════════════════════
    [9] = {
        "VOSS :\n"
        "La ville n'est pas vide. La Faction Acier y a établi son territoire.\n"
        "\n"
        "KIRA :\n"
        "Ils ont des chasseurs. Des prédateurs qui traquent nos unités, pas nos murs.\n"
        "\n"
        "VOSS :\n"
        "Alors personne ne meurt aujourd'hui. Gardez tout le monde en vie.\n",

        "KIRA :\n"
        "Toutes les unités sont rentrées. Pas une égratignure.\n"
        "\n"
        "CHEF ACIER :\n"
        "Joli. Vous protégez vos pions comme des enfants, Voss. C'est touchant… "
        "et c'est une faiblesse.\n"
        "\n"
        "KIRA :\n"
        "Leur chef. Il était sur notre fréquence depuis le début.\n",
    },

    [10] = {
        "CHEF ACIER :\n"
        "Voss. Votre bastion est une erreur. On survit en prenant, pas en "
        "construisant. Quand vous tomberez, nous ramasserons ce qui reste.\n"
        "\n"
        "VARELA :\n"
        "Leurs armures arrêtent nos balles. Mais un minerai bien choisi les "
        "traversera. Il m'en faut un, vite.\n"
        "\n"
        "VOSS :\n"
        "Ouvriers dehors. On lui montre ce qu'on construit.\n",

        "VOSS :\n"
        "L'armure de la Faction cède sous nos munitions modifiées.\n"
        "\n"
        "KIRA :\n"
        "Leur chef recule vers le parlement en ruine. Il a encore ses gardes d'élite.\n"
        "\n"
        "CHEF ACIER :\n"
        "Venez me chercher, Voss. Si vous l'osez.\n",
    },

    [11] = {
        "VOSS :\n"
        "Le chef s'est retranché dans l'ancien parlement. Deux accès, des gardes "
        "d'élite dans les couloirs.\n"
        "\n"
        "KIRA :\n"
        "On le veut vivant ?\n"
        "\n"
        "VOSS :\n"
        "Vivant. Il sait d'où viennent les hordes. Je l'entends à la façon dont "
        "il en parle.\n",

        "CHEF ACIER :\n"
        "… Vous avez gagné, Voss. Alors écoutez, je ne le dirai qu'une fois.\n"
        "\n"
        "CHEF ACIER :\n"
        "Les hordes ne viennent pas d'elles-mêmes. Nous, on les laissait passer "
        "contre de l'acier. Quelque chose les fabrique, dans l'usine au nord.\n"
        "\n"
        "VARELA :\n"
        "NEXUS.\n",
    },

    // ═══ CHAPITRE 5 — L'USINE ABANDONNÉE ═════════════════════════
    [12] = {
        "VOSS :\n"
        "L'usine tourne encore. Toute seule. Des robots de guerre sortent des "
        "chaînes et marchent vers nous sans jamais s'arrêter.\n"
        "\n"
        "VARELA :\n"
        "Leurs composants internes sont la clé. Ramenez-m'en un et je vous dirai "
        "comment NEXUS leur parle.\n",

        "VARELA :\n"
        "Des nanostructures que je n'avais jamais vues. Je peux brouiller leurs "
        "signaux de contrôle.\n"
        "\n"
        "KIRA :\n"
        "Ou on casse tout. La chaîne de montage est juste là.\n"
        "\n"
        "VOSS :\n"
        "Pirater, ou briser. Décidez vite : l'usine ne dort jamais.\n",
    },

    [13] = {
        "VOSS :\n"
        "Trois générateurs alimentent le système central. Tant qu'ils tournent, la "
        "production ne s'arrête pas.\n"
        "\n"
        "VARELA :\n"
        "Un ouvrier sur chacun. On les couvre pendant la manœuvre.\n"
        "\n"
        "NEXUS :\n"
        "INTRUSION DÉTECTÉE. VOUS ÊTES DANS MON CORPS, COMMANDANT.\n",

        "VARELA :\n"
        "Les trois générateurs sont hors service. La production s'est arrêtée.\n"
        "\n"
        "VOSS :\n"
        "Et NEXUS ?\n"
        "\n"
        "VARELA :\n"
        "Batteries de secours. Il jette tout ce qu'il lui reste contre nous. Tout.\n",
    },

    /* [14] : le prélude seulement — la réplique de NEXUS est calculée
       (campaign_dialog_before), et l'après-acte est l'épilogue. */
    [14] = {
        "VOSS :\n"
        "On y est. Derrière ces portes, il y a ce qui a brûlé le monde.\n"
        "\n"
        "VARELA :\n"
        "NEXUS analyse chacune de nos décisions depuis le premier jour. Tout ce que "
        "nous avons choisi, il l'a archivé.\n",
        NULL,
    },

};

/* Trame principale ici ; branches, replis et variantes d'échec dans
   campaign_story_branches.c (fichiers < 500 lignes). */
const char *campaign_story_before(int node_id) {
    if (node_id < 0 || node_id >= CAMPAIGN_NODES) return NULL;
    return STORY[node_id][0] ? STORY[node_id][0] : campaign_story_branch(node_id, 0);
}

const char *campaign_story_after(int node_id) {
    if (node_id < 0 || node_id >= CAMPAIGN_NODES) return NULL;
    return STORY[node_id][1] ? STORY[node_id][1] : campaign_story_branch(node_id, 1);
}
