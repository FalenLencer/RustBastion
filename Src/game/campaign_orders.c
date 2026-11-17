/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  campaign_orders.c ─ L'ORDRE DU JOUR : une décision avant chaque acte.
 *
 *  Au briefing, deux consignes sont proposées (facultatives). Chacune est
 *  un ÉCHANGE, jamais un cadeau : on gagne sur un axe (or, vies, Renfort)
 *  et on paie sur un autre. Barème de conception, pour que les deux
 *  options se valent : 10 or ≈ 1 vie ≈ 6 Renfort.
 *
 *  Le personnage concerné RÉPOND à la décision (réplique jouée par la
 *  scène) : le joueur voit son ordre exécuté, pas seulement un chiffre.
 *  Les effets sont appliqués au LANCEMENT de l'acte (game_apply_order),
 *  jamais au clic : quitter pendant le briefing puis recharger ne peut
 *  donc pas les cumuler.
 */
#include "campaign_data.h"
#include <stdio.h>

/* Une paire de consignes par position narrative (chapitre×3 + acte) :
   les nœuds de branche héritent de la paire de leur position. */
static const CampaignOrder ORDERS[CAMPAIGN_TOTAL][CAMPAIGN_ORDER_CHOICES] = {
    // ── Ch.1 — Les Terres Brûlées ───────────────────────────────────
    {   { "Démonter les baraquements vides",  40, -3,   0, "KIRA",
          "On démonte les vieux baraquements. Ça fera des munitions… et des courants d'air." },
        { "Murer le portail",                -30,  4,   0, "VOSS",
          "Sacs de sable et ferraille sur le portail. Ça coûte, mais ça tient." } },
    {   { "Siphonner le carburant du poste",  50, -4,   0, "KIRA",
          "On vide leurs réservoirs. Le poste sera plus fragile, mais nos tourelles tourneront." },
        { "Renforcer le poste avancé",       -40,  5,   0, "VOSS",
          "Deux escouades partent au poste avancé. Il ne tombera pas sans se battre." } },
    {   { "Revendre les pièces du convoi",    45,  0, -25, "VARELA",
          "Je revends ce que je ne peux pas monter. Le reste attendra des jours meilleurs." },
        { "Armer les rescapés",              -40,  0,  30, "VOSS",
          "Les rescapés veulent se battre. Donnons-leur des fusils." } },
    // ── Ch.2 — Le Marais Toxique ────────────────────────────────────
    {   { "Brûler les roseaux du camp",      -35,  4,   0, "KIRA",
          "On brûle les roseaux. Plus rien ne s'approche sans qu'on le voie venir." },
        { "Vendre les filtres en surplus",    45, -3,   0, "VARELA",
          "On garde le strict minimum de filtres. Le reste paie nos munitions." } },
    {   { "Éclaireurs en pointe",            -35,  0,  30, "KIRA",
          "Mes éclaireurs passent devant. Ils rapporteront tout ce qu'ils trouvent." },
        { "Pomper les cuves du labo",         50, -4,   0, "VARELA",
          "Les cuves sont pleines de carburant. On pompe, mais la vase ralentit tout." } },
    {   { "Poser des pièges incendiaires",   -45,  5,   0, "VARELA",
          "Du gaz sur toutes les approches. Quand elle viendra, ça brûlera." },
        { "Tout miser sur l'armement",        60, -5,   0, "VOSS",
          "Pas de murs neufs. Chaque pièce d'or va aux tourelles." } },
    // ── Ch.3 — Le Désert Irradié ────────────────────────────────────
    {   { "Bâcher les tourelles",            -30,  4,   0, "VARELA",
          "Des bâches sur les mécanismes. Le sable n'entrera pas." },
        { "Désosser une épave d'hélicoptère", 45,  0, -25, "KIRA",
          "Une épave à un kilomètre. On la désosse avant que le sable l'avale." } },
    {   { "Fouiller le casino",               60, -5,   0, "KIRA",
          "Le casino, commandant. Pour une fois, c'est nous qui allons gagner là-dedans." },
        { "Des guetteurs sur les toits",     -40,  5,   0, "VOSS",
          "Un guetteur sur chaque toit. L'artillerie ne nous surprendra pas." } },
    {   { "Brouiller ses capteurs",          -45,  0,  35, "VARELA",
          "Je l'inonde de bruit radio. Il calcule moins vite, et j'apprends ses fréquences." },
        { "Vendre les batteries de secours",  55, -4,   0, "VOSS",
          "On vend les batteries. Si les murs tombent, on se battra dans le noir." } },
    // ── Ch.4 — La Ville en Ruine ────────────────────────────────────
    {   { "Barricader les rues",             -40,  5,   0, "KIRA",
          "Les rues latérales sont bouchées. Leurs chasseurs passeront par l'avenue." },
        { "Commercer avec les pillards",      50,  0, -25, "VOSS",
          "Les pillards vendent tout, même leurs voisins. Achetons." } },
    {   { "Payer des informateurs",          -40,  0,  35, "KIRA",
          "Deux déserteurs de l'Acier. Ils parlent, contre un repas chaud." },
        { "Désosser une armure capturée",     55, -4,   0, "VARELA",
          "Une armure de l'Acier, entière. Je la démonte pour son alliage." } },
    {   { "Murer l'aile ouest",              -50,  6,   0, "VOSS",
          "On mure l'aile ouest. Moins de couloirs à tenir." },
        { "Fouiller les archives",           -45,  0,  40, "VARELA",
          "Les archives du parlement : des plans, des codes. De quoi armer la suite." } },
    // ── Ch.5 — L'Usine Abandonnée ───────────────────────────────────
    {   { "Démonter un robot inachevé",       55, -5,   0, "VARELA",
          "Un robot à moitié assemblé. Ses pièces valent de l'or." },
        { "Blinder la base",                 -45,  6,   0, "VOSS",
          "Des plaques d'acier sur la base. NEXUS va frapper fort." } },
    {   { "Ravitailler les ouvriers",        -45,  0,  40, "VOSS",
          "Les ouvriers mangent avant de partir. Ils reviendront plus vite." },
        { "Couper notre éclairage",           50, -4,   0, "VARELA",
          "On coupe nos lumières et on revend l'énergie en munitions. On se battra dans le noir." } },
    {   { "Tenir la dernière ligne",         -60,  8,   0, "VOSS",
          "Tout ce qu'il nous reste va dans les murs. On ne reculera plus." },
        { "Tout pour l'artillerie",           70, -6,   0, "KIRA",
          "Pas de murs. Juste du feu. Beaucoup de feu." } },
};

const CampaignOrder *campaign_order(int node_id, int idx) {
    if (idx < 0 || idx >= CAMPAIGN_ORDER_CHOICES) return NULL;
    int st = campaign_difficulty_stage(node_id);
    if (st < 0 || st >= CAMPAIGN_TOTAL) return NULL;
    return &ORDERS[st][idx];
}

int campaign_order_affordable(const CampaignOrder *o,
                              int gold, int lives, int renfort) {
    if (!o) return 0;
    if (o->gold    < 0 && gold    + o->gold    < CAMPAIGN_MIN_GOLD)  return 0;
    if (o->lives   < 0 && lives   + o->lives   < CAMPAIGN_MIN_LIVES) return 0;
    if (o->renfort < 0 && renfort + o->renfort < 0)                  return 0;
    return 1;
}

/* Une seule source pour le texte d'effet : il est TIRÉ des chiffres,
   jamais écrit à côté (impossible qu'il mente). */
const char *campaign_order_effect(const CampaignOrder *o, char *buf, int n) {
    if (!o || !buf || n <= 0) return "";
    int  k = 0;
    buf[0] = '\0';
    struct { int v; const char *unit; } part[3] = {
        { o->gold, "or" }, { o->lives, "vies" }, { o->renfort, "Renfort" },
    };
    for (int i = 0; i < 3 && k < n; i++) {
        if (!part[i].v) continue;
        k += snprintf(buf + k, (size_t)(n - k), "%s%+d %s",
                      k ? "   " : "", part[i].v, part[i].unit);
    }
    return buf;
}
