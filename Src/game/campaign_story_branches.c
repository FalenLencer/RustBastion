/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  campaign_story_branches.c ─ Scènes des BRANCHES de la campagne :
 *  voies alternatives choisies par le joueur, replis après une défaite de
 *  finale, et bilans d'échec des nœuds dont l'objectif oriente la route.
 *  Même format que campaign_story.c (trame principale).
 */
#include "campaign_data.h"
#include <stddef.h>

static const char *const BRANCH[CAMPAIGN_NODES][2] = {
    // ═══ BRANCHES ═════════════════════════════════════════════════
    [15] = {   /* Ch.2 — la traque */
        "KIRA :\n"
        "La voie directe. Pas de labo où se retrancher : la brume, le signal, et ce "
        "qui s'y cache.\n"
        "\n"
        "VOSS :\n"
        "On suit le battement jusqu'à la source. Restez groupés.\n",

        "KIRA :\n"
        "La piste mène droit au cœur du marais. La créature qui coordonne les "
        "mutants est juste devant.\n"
        "\n"
        "VARELA :\n"
        "Et on sait comment elle les commande : une émission biochimique. La couper, "
        "c'est couper la horde.\n",
    },

    [16] = {   /* Ch.5 — le réacteur s'emballe */
        "VARELA :\n"
        "Tous les générateurs ne sont pas tombés. Le cœur entre en surchauffe.\n"
        "\n"
        "VARELA :\n"
        "C'est dangereux, mais l'instabilité perturbe aussi les robots. "
        "Servez-vous-en.\n",

        "VARELA :\n"
        "La surchauffe a forcé NEXUS à rediriger son énergie. Une brèche s'ouvre "
        "vers le cœur central.\n"
        "\n"
        "VOSS :\n"
        "C'est maintenant ou jamais.\n",
    },

    [17] = {   /* Ch.1 — l'embuscade */
        "KIRA :\n"
        "On ne les escorte pas, on les saigne. Tenaille dans les cendres : on les "
        "laisse entrer, puis on referme.\n"
        "\n"
        "VOSS :\n"
        "Si la tenaille lâche, ils filent droit sur la base.\n"
        "\n"
        "KIRA :\n"
        "Ils ne s'y attendent pas plus que vous, commandant.\n",

        "KIRA :\n"
        "Embuscade nette. La colonne est en miettes avant d'avoir vu nos murs.\n"
        "\n"
        "VOSS :\n"
        "Le butin couvrira nos pertes. Et les pillards sauront que nous ne sommes "
        "plus une cible facile.\n",
    },

    [18] = {   /* Ch.3 — la frappe éclair */
        "VOSS :\n"
        "Pas le temps de fortifier deux points. On fonce sur le signal : une seule "
        "ligne, exposée au vent.\n"
        "\n"
        "KIRA :\n"
        "Si on tient, on lui vole des heures de calcul. Sinon…\n"
        "\n"
        "VOSS :\n"
        "Sinon, on improvise. Comme toujours.\n",

        "GÉNÉRAL MACHINE :\n"
        "Trajectoire non modélisée. Recalcul en cours… recalcul en cours…\n"
        "\n"
        "VOSS :\n"
        "Le pari paie : l'IA n'a pas eu le temps d'organiser sa défense.\n"
        "\n"
        "VARELA :\n"
        "Je vois ses émissions changer en direct. Elle recalcule. Elle a peur ? "
        "Non… elle apprend.\n",
    },

    [19] = {   /* Ch.4 — sous le joug de l'Acier */
        "CHEF ACIER :\n"
        "Vos jouets n'ont pas percé notre blindage. C'est nous qui dictons le tempo, "
        "maintenant.\n"
        "\n"
        "VOSS :\n"
        "Sans munitions adaptées, on encaisse à la force des bras. Serrez les dents.\n",

        "VOSS :\n"
        "On a tenu, à la sueur et au sang. La Faction nous croit à bout.\n"
        "\n"
        "KIRA :\n"
        "Qu'elle continue de le croire. Le parlement est à portée.\n",
    },

    [20] = {   /* Ch.5 — le sabotage */
        "VOSS :\n"
        "Oubliez le piratage fin. On casse la chaîne de montage.\n"
        "\n"
        "KIRA :\n"
        "Plus brutal, plus de robots d'un coup. Mais si on en vient à bout, la "
        "production s'effondre net.\n",

        "TECHNICIEN :\n"
        "Chaîne pulvérisée ! NEXUS a perdu son bras armé principal.\n"
        "\n"
        "VARELA :\n"
        "Le cœur de l'usine est exposé. On peut viser plus profond.\n",
    },

    [21] = {   /* repli après la Reine */
        "KIRA :\n"
        "La Reine nous a submergés. On décroche, mais pas en déroute.\n"
        "\n"
        "VOSS :\n"
        "Chaque minute tenue ici, c'est un camion de réfugiés qui passe. Tenez cette "
        "dernière ligne.\n",

        "VOSS :\n"
        "Repli réussi. On a perdu le marais, pas la guerre.\n"
        "\n"
        "VARELA :\n"
        "Et la transmission qui commandait la Reine venait du désert. C'est là qu'on "
        "rebondit.\n",
    },

    [22] = {   /* repli après le général machine */
        "GÉNÉRAL MACHINE :\n"
        "Secteur repris. Unités humaines en retraite vers la ville morte. Poursuite "
        "jugée inutile : elles y seront broyées.\n"
        "\n"
        "VOSS :\n"
        "Le général machine nous a débordés. On se replie vers la ville en ruine.\n"
        "\n"
        "KIRA :\n"
        "Terrain hostile, mais ses blindés y passent mal. On transforme la défaite "
        "en piège.\n",

        "KIRA :\n"
        "On a semé l'IA dans les décombres. Elle nous suit en ville, mais sur NOTRE "
        "terrain.\n"
        "\n"
        "VOSS :\n"
        "Et la ville n'est pas vide. Nous ne sommes pas les seuls à la vouloir.\n",
    },
};

const char *campaign_story_branch(int node_id, int after) {
    if (node_id < 0 || node_id >= CAMPAIGN_NODES) return NULL;
    return BRANCH[node_id][after ? 1 : 0];
}

/* Bilans d'ÉCHEC des nœuds où l'objectif oriente la route
   (cf. campaign_next_node : 10 → 19, 13 → 16). */
static const char *const STORY_FAILED[CAMPAIGN_NODES] = {
    [10] =
        "VOSS :\n"
        "Leurs armures ont tenu. Nos munitions ricochent sur l'acier de la Faction.\n"
        "\n"
        "CHEF ACIER :\n"
        "Vous voyez, Voss ? On ne bâtit rien sur du sable. Désormais, c'est moi "
        "qui donne le tempo.\n"
        "\n"
        "KIRA :\n"
        "On encaisse, commandant. Mais on encaisse debout.\n",
    [13] =
        "VARELA :\n"
        "Un générateur tourne encore ! Le cœur reçoit trop d'énergie : "
        "il entre en surchauffe.\n"
        "\n"
        "VOSS :\n"
        "Combien de temps ?\n"
        "\n"
        "VARELA :\n"
        "Assez pour que tout saute… ou pour que ses robots déraillent. Ça peut "
        "jouer pour nous.\n",
};

const char *campaign_story_after_failed(int node_id) {
    if (node_id < 0 || node_id >= CAMPAIGN_NODES) return NULL;
    return STORY_FAILED[node_id];
}
