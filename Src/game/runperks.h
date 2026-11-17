/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  runperks.h ─ Système rogue-lite « build de run » (intra-campagne).
 *
 *  Un RunBuild accumule des PERKS pendant toute une campagne (persiste
 *  entre les actes, remis à zéro au départ d'une run, sauvegardé dans le
 *  slot campagne). Les perks dérivent un jeu de modificateurs (RunMods)
 *  appliqués par-dessus le meta permanent aux tours, unités et à l'économie.
 *
 *  Catalogue conçu par CLUSTERS DE SYNERGIE : assembler un combo complet
 *  (ex. mono-type + cadence + arsenal) demande de la chance au tirage.
 */
#pragma once
#include "../combat/tower.h"   // TOWER_TYPE_COUNT

// ── Identifiants de perks ─────────────────────────────────────
typedef enum {
    // Dégâts par type de tour (cluster « mono-type »)
    PERK_GUN = 0,     // Canons renforcés
    PERK_SNIPER,      // Optique de précision
    PERK_FLAME,       // Carburant amélioré
    PERK_TESLA,       // Bobines surchargées
    // Tours — globaux
    PERK_PUISSANCE,   // Surcharge : dégâts toutes tours
    PERK_PORTEE,      // Optiques longues : portée
    PERK_CADENCE,     // Servomoteurs : cadence
    PERK_GLASS,       // Canon de verre : +dégâts / -portée (combo avec PORTEE)
    // Économie (cluster « éco → puissance »)
    PERK_BUTIN,       // Pillage : or par kill
    PERK_SUBV,        // Subvention : or au début de chaque acte
    PERK_REVENTE,     // Recyclage total : revente 100 %
    PERK_CAPITAL,     // Capitaliste : dégâts tours selon l'or possédé
    // Unités (cluster « armée »)
    PERK_BLINDAGE,    // Blindage : PV unités
    PERK_FUSIL,       // Armement lourd : dégâts unités
    PERK_MEDIC,       // Trousse avancée : soin médic
    // Survie
    PERK_GARNISON,    // Garnison : +vies au début de chaque acte
    // Épiques (couronnent un combo)
    PERK_ARSENAL,     // Arsenal complet : dégâts + portée + cadence
    PERK_FORTUNE,     // Fortune de guerre : or/kill + or/acte
    PERK_LEGION,      // Légion : PV + dégâts unités
    // Clés de voûte : CHANGENT une règle, avec un revers. Boutique seulement,
    // UNE par run. Toujours ajoutées EN FIN d'énumération : la lecture du
    // build (save.c) migre un ancien count[] plus court sans rien perdre.
    PERK_K_FORTRESS,  // Forteresse : tours bien plus fortes, 3 tours de moins
    PERK_K_SPOILS,    // Butin de guerre : moitié d'or en début d'acte, +or/kill
    PERK_K_GEOMANCER, // Géomancien : terrain piégé ×3, unités immunisées
    PERK_COUNT
} PerkId;

typedef enum {
    RAR_COMMON = 0,
    RAR_RARE,
    RAR_EPIC,
    RAR_KEYSTONE,     // clé de voûte (cf. ci-dessus)
} PerkRarity;

typedef struct {
    const char *name;
    const char *tag;         // étiquette courte (HUD build actif)
    const char *desc;
    PerkRarity  rarity;
    int         max_stack;   // nb max d'exemplaires
    int         shop_cost;   // coût en Renfort (boutique)
} PerkDef;

extern const PerkDef RUN_PERKS[PERK_COUNT];

// ── État du build de run ──────────────────────────────────────
#define MAX_DRAFT_OFFER 3
#define MAX_SHOP_OFFER  4

typedef struct RunBuild {
    int count[PERK_COUNT];   // exemplaires acquis par perk
    int renfort;             // monnaie de run

    // Offres en cours (sérialisées pour survivre à un rechargement)
    int draft_offer[MAX_DRAFT_OFFER]; int draft_n;
    int shop_offer [MAX_SHOP_OFFER];  int shop_n;
    int shop_pending;        // 1 = ouvrir la boutique (transition de chapitre)
} RunBuild;

// ── Modificateurs dérivés (appliqués en jeu) ──────────────────
typedef struct {
    float tdmg_all;                      // mult dégâts toutes tours
    float tdmg_type[TOWER_TYPE_COUNT];   // mult dégâts par type
    float trange_all;                    // mult portée toutes tours
    float trate_all;                     // mult cadence toutes tours
    float unit_hp;                       // mult PV unités
    float unit_dmg;                      // mult dégâts unités
    float medic_heal;                    // mult soin médic
    float sell_refund_add;               // + fraction de remboursement à la vente
    int   gold_per_kill;                 // + or par ennemi tué
    int   act_gold;                      // + or au début de chaque acte
    int   act_lives;                     // + vies au début de chaque acte
    // Clés de voûte
    int   tower_slots;                   // + tours déployables (négatif = moins)
    float act_gold_mult;                 // × or de début d'acte (avant les bonus)
    float enemy_hp;                      // × PV des ennemis à l'apparition
    float hazard_mult;                   // × dégâts des hasards de terrain
    int   hazard_units_immune;           // 1 = vos unités ignorent le terrain
} RunMods;

// Modificateurs actifs courants (lus par tower.c / unit.c / enemy.c).
extern RunMods g_run_mods;

// ── API ───────────────────────────────────────────────────────
void runbuild_reset  (RunBuild *rb);
// Recalcule les modificateurs. `gold` sert au perk Capitaliste (dégâts ∝ or).
void runbuild_compute(const RunBuild *rb, RunMods *out, int gold);
// Ajoute un exemplaire d'un perk (borné par max_stack). 1 = ajouté.
int  runbuild_add    (RunBuild *rb, int perk_id);

// Tire `n` offres aléatoires distinctes (raretés biaisées par `bias`,
// 0 = neutre, >0 = vers le rare/épique). Écrit dans `out`, retourne le nb.
int  runbuild_roll_offers(const RunBuild *rb, int *out, int n,
                          int bias, int shop_only);

// Remplace l'offre `slot` par un NOUVEAU perk éligible (count<max, non déjà
// présent dans les autres slots). Pour la boutique : rachat infini, l'article
// acheté est remplacé. Retourne le nouvel id, ou -1 si rien (slot inchangé).
int  runbuild_reroll_slot(const RunBuild *rb, int *offers, int n, int slot);

// Couleur et libellé d'affichage d'une rareté (sources uniques).
typedef struct { unsigned char r, g, b, a; } RunColor;
RunColor    runperk_rarity_color(PerkRarity rar);
const char *runperk_rarity_name (PerkRarity rar);   // « ÉPIQUE », « CLÉ DE VOÛTE »…
// 1 si le build possède déjà une clé de voûte (une seule par run).
int         runbuild_has_keystone(const RunBuild *rb);

// Catégorie d'un perk (pour glyphe d'affichage) :
//   0 = tour, 1 = économie, 2 = unité, 3 = survie.
int runperk_category(int perk_id);
