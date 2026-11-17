/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "runperks.h"
#include "raylib.h"      // GetRandomValue
#include <string.h>

RunMods g_run_mods;

// ── Clés de voûte : réglages ─────────────────────────────────────
#define KEY_FORT_DMG        0.50f   // Forteresse : + dégâts toutes tours
#define KEY_FORT_RANGE      0.25f   //              + portée toutes tours
#define KEY_FORT_SLOTS      (-3)    //              tours déployables
#define KEY_SPOILS_GOLD     0.50f   // Butin de guerre : × or de début d'acte
#define KEY_SPOILS_KILL     6       //                   + or par ennemi tué
#define KEY_GEO_HAZARD      3.0f    // Géomancien : × dégâts du terrain piégé
#define KEY_GEO_ENEMY_HP    1.12f   //              × PV ennemis (le revers)

// ── Poids de tirage par rareté (butin, boutique, rachat) ─────────
#define PERK_W_COMMON       60
#define PERK_W_RARE         34
#define PERK_W_RARE_BIAS    18      // + par cran de biais (voie risquée)
#define PERK_W_EPIC         14
#define PERK_W_EPIC_BIAS    14
#define PERK_W_KEYSTONE     12      // boutique seulement, une par run

// ════════════════════════════════════════════════════
// CATALOGUE — conçu par clusters de synergie
//   Mono-type burst : PERK_GUN/SNIPER/FLAME/TESLA + PUISSANCE + CADENCE
//                      + GLASS (offset par PORTEE) + ARSENAL (épique)
//   Éco → puissance  : BUTIN + SUBV + CAPITAL + FORTUNE (épique)
//   Armée            : BLINDAGE + FUSIL + MEDIC + LEGION (épique)
//   Survie           : GARNISON + REVENTE + éco
// ════════════════════════════════════════════════════
const PerkDef RUN_PERKS[PERK_COUNT] = {
    [PERK_GUN]      = {"Canons renforcés",   "TOUR",  "Tourelles : +25% dégâts / niv.",      RAR_COMMON, 3, 40},
    [PERK_SNIPER]   = {"Optique de précision","SNIP", "Snipers : +25% dégâts / niv.",        RAR_COMMON, 3, 40},
    [PERK_FLAME]    = {"Carburant amélioré", "FLAM", "Lance-flammes : +25% dégâts / niv.",   RAR_COMMON, 3, 40},
    [PERK_TESLA]    = {"Bobines surchargées","TESL", "Tesla : +25% dégâts / niv.",           RAR_COMMON, 3, 40},

    [PERK_PUISSANCE]= {"Surcharge",          "PUIS",  "Toutes les tours : +20% dégâts / niv.",RAR_RARE,   3, 75},
    [PERK_PORTEE]   = {"Optiques longues",   "PORT",  "Toutes les tours : +12% portée / niv.",RAR_COMMON, 3, 45},
    [PERK_CADENCE]  = {"Servomoteurs",       "CAD", "Toutes les tours : +20% cadence / niv.",RAR_RARE,  3, 75},
    [PERK_GLASS]    = {"Canon de verre",     "VERR", "+60% dégâts mais -25% portée.",         RAR_RARE,   2, 70},

    [PERK_BUTIN]    = {"Pillage",            "PILL", "+3 or par ennemi tué / niv.",           RAR_COMMON, 3, 45},
    [PERK_SUBV]     = {"Subvention",         "SUBV", "+60 or au début de chaque acte / niv.", RAR_COMMON, 3, 45},
    [PERK_REVENTE]  = {"Recyclage total",    "RECY", "Revente des tours : remboursement +40%.",RAR_COMMON,1, 50},
    [PERK_CAPITAL]  = {"Capitaliste",        "CAPI", "Tours : +5% dégâts par 100 or possédé.",RAR_RARE,   2, 80},

    [PERK_BLINDAGE] = {"Blindage",           "BLIN", "Unités : +25% PV / niv.",               RAR_COMMON, 3, 45},
    [PERK_FUSIL]    = {"Armement lourd",     "ARME",  "Unités : +25% dégâts / niv.",           RAR_COMMON, 3, 45},
    [PERK_MEDIC]    = {"Trousse avancée",    "SOIN", "Soin des medics x2 / niv.",             RAR_RARE,   2, 70},

    [PERK_GARNISON] = {"Garnison",           "GARN", "+15 vies au début de chaque acte / niv.",RAR_COMMON,3, 50},

    [PERK_ARSENAL]  = {"Arsenal complet",    "ARSE", "Tours : +30% dégâts, +15% portée & cadence.",RAR_EPIC,1,120},
    [PERK_FORTUNE]  = {"Fortune de guerre",  "FORT", "+8 or/kill ET +120 or au début d'acte.",RAR_EPIC,  1,120},
    [PERK_LEGION]   = {"Legion",             "LEGI", "Unités : +40% PV ET +40% dégâts.",      RAR_EPIC,  1,120},

    [PERK_K_FORTRESS] = {"Forteresse",       "BAST", "Tours +50% dégâts, +25% portée. 3 tours de moins.", RAR_KEYSTONE, 1, 150},
    [PERK_K_SPOILS]   = {"Butin de guerre",  "SAC",  "Moitié d'or en début d'acte, +6 or par kill.",      RAR_KEYSTONE, 1, 150},
    [PERK_K_GEOMANCER]= {"Géomancien",       "GÉO",  "Terrain piégé x3, unités immunisées. Ennemis +12% PV.", RAR_KEYSTONE, 1, 150},
};

// ════════════════════════════════════════════════════
// API
// ════════════════════════════════════════════════════
void runbuild_reset(RunBuild *rb) {
    memset(rb, 0, sizeof(*rb));
}

int runbuild_add(RunBuild *rb, int perk_id) {
    if (perk_id < 0 || perk_id >= PERK_COUNT) return 0;
    if (rb->count[perk_id] >= RUN_PERKS[perk_id].max_stack) return 0;
    rb->count[perk_id]++;
    return 1;
}

void runbuild_compute(const RunBuild *rb, RunMods *m, int gold) {
    // Valeurs neutres
    m->tdmg_all = 1.0f; m->trange_all = 1.0f; m->trate_all = 1.0f;
    for (int i = 0; i < TOWER_TYPE_COUNT; i++) m->tdmg_type[i] = 1.0f;
    m->unit_hp = 1.0f; m->unit_dmg = 1.0f; m->medic_heal = 1.0f;
    m->sell_refund_add = 0.0f;
    m->gold_per_kill = 0; m->act_gold = 0; m->act_lives = 0;
    m->tower_slots = 0; m->act_gold_mult = 1.0f; m->enemy_hp = 1.0f;
    m->hazard_mult = 1.0f; m->hazard_units_immune = 0;

    const int *c = rb->count;

    // Dégâts par type
    if (TOWER_GUN    < TOWER_TYPE_COUNT) m->tdmg_type[TOWER_GUN]    += 0.25f * c[PERK_GUN];
    if (TOWER_SNIPER < TOWER_TYPE_COUNT) m->tdmg_type[TOWER_SNIPER] += 0.25f * c[PERK_SNIPER];
    if (TOWER_FLAME  < TOWER_TYPE_COUNT) m->tdmg_type[TOWER_FLAME]  += 0.25f * c[PERK_FLAME];
    if (TOWER_TESLA  < TOWER_TYPE_COUNT) m->tdmg_type[TOWER_TESLA]  += 0.25f * c[PERK_TESLA];

    // Globaux tours
    m->tdmg_all   += 0.20f * c[PERK_PUISSANCE];
    m->trange_all += 0.12f * c[PERK_PORTEE];
    m->trate_all  += 0.20f * c[PERK_CADENCE];
    m->tdmg_all   += 0.60f * c[PERK_GLASS];
    m->trange_all -= 0.25f * c[PERK_GLASS];

    // Capitaliste : +5% dégâts par 100 or possédé, par exemplaire.
    // Plafonné pour éviter une montée en puissance infinie en thésaurisant l'or.
    if (c[PERK_CAPITAL] > 0) {
        float cap = ((float)gold / 100.0f) * 0.05f * c[PERK_CAPITAL];
        float cap_max = 1.50f * c[PERK_CAPITAL];   // +150% max par exemplaire
        if (cap > cap_max) cap = cap_max;
        m->tdmg_all += cap;
    }

    // Arsenal (épique)
    m->tdmg_all   += 0.30f * c[PERK_ARSENAL];
    m->trange_all += 0.15f * c[PERK_ARSENAL];
    m->trate_all  += 0.15f * c[PERK_ARSENAL];

    // Économie
    m->gold_per_kill += 3   * c[PERK_BUTIN];
    m->act_gold      += 60  * c[PERK_SUBV];
    m->sell_refund_add += 0.40f * c[PERK_REVENTE];
    m->gold_per_kill += 8   * c[PERK_FORTUNE];
    m->act_gold      += 120 * c[PERK_FORTUNE];

    // Unités
    m->unit_hp    += 0.25f * c[PERK_BLINDAGE];
    m->unit_dmg   += 0.25f * c[PERK_FUSIL];
    m->medic_heal += 1.00f * c[PERK_MEDIC];
    m->unit_hp    += 0.40f * c[PERK_LEGION];
    m->unit_dmg   += 0.40f * c[PERK_LEGION];

    // Survie
    m->act_lives  += 15 * c[PERK_GARNISON];

    // Clés de voûte (max 1 exemplaire chacune)
    if (c[PERK_K_FORTRESS] > 0) {
        m->tdmg_all    += KEY_FORT_DMG;
        m->trange_all  += KEY_FORT_RANGE;
        m->tower_slots += KEY_FORT_SLOTS;
    }
    if (c[PERK_K_SPOILS] > 0) {
        m->act_gold_mult  *= KEY_SPOILS_GOLD;
        m->gold_per_kill  += KEY_SPOILS_KILL;
    }
    if (c[PERK_K_GEOMANCER] > 0) {
        m->hazard_mult        *= KEY_GEO_HAZARD;
        m->hazard_units_immune = 1;
        m->enemy_hp           *= KEY_GEO_ENEMY_HP;
    }

    // Garde-fous
    if (m->trange_all < 0.30f) m->trange_all = 0.30f;
    if (m->tdmg_all   < 0.10f) m->tdmg_all   = 0.10f;
}

int runbuild_has_keystone(const RunBuild *rb) {
    for (int i = 0; i < PERK_COUNT; i++)
        if (RUN_PERKS[i].rarity == RAR_KEYSTONE && rb->count[i] > 0) return 1;
    return 0;
}

/* Poids de tirage d'un perk (0 = non proposable) — source unique du butin,
   de la boutique et du rachat. Clé de voûte : boutique seulement, une par run. */
static int perk_weight(const RunBuild *rb, int i, int bias, int shop) {
    if (rb->count[i] >= RUN_PERKS[i].max_stack) return 0;
    switch (RUN_PERKS[i].rarity) {
        case RAR_RARE:     return PERK_W_RARE + PERK_W_RARE_BIAS * bias;
        case RAR_EPIC:     return PERK_W_EPIC + PERK_W_EPIC_BIAS * bias;
        case RAR_KEYSTONE: return (shop && !runbuild_has_keystone(rb)) ? PERK_W_KEYSTONE : 0;
        default:           return PERK_W_COMMON;
    }
}

int runbuild_roll_offers(const RunBuild *rb, int *out, int n,
                         int bias, int shop_only) {
    if (bias < 0) bias = 0;
    int n_out = 0;
    int picked[PERK_COUNT];
    for (int i = 0; i < PERK_COUNT; i++) picked[i] = 0;

    for (int k = 0; k < n && k < PERK_COUNT; k++) {
        int  weights[PERK_COUNT];
        long total = 0;
        for (int i = 0; i < PERK_COUNT; i++) {
            weights[i] = picked[i] ? 0 : perk_weight(rb, i, bias, shop_only);
            total += weights[i];
        }
        if (total <= 0) break;
        long r = GetRandomValue(0, (int)total - 1);
        int chosen = -1;
        for (int i = 0; i < PERK_COUNT; i++) {
            if (!weights[i]) continue;
            if (r < weights[i]) { chosen = i; break; }
            r -= weights[i];
        }
        if (chosen < 0) break;
        picked[chosen] = 1;
        out[n_out++] = chosen;
    }
    return n_out;
}

int runbuild_reroll_slot(const RunBuild *rb, int *offers, int n, int slot) {
    if (!offers || slot < 0 || slot >= n) return -1;
    int picked[PERK_COUNT];
    for (int i = 0; i < PERK_COUNT; i++) picked[i] = 0;
    for (int k = 0; k < n; k++)                 // exclut les autres offres en cours
        if (k != slot && offers[k] >= 0 && offers[k] < PERK_COUNT) picked[offers[k]] = 1;
    // Exclut aussi l'article qui occupait CE slot → le remplaçant est toujours
    // visiblement UN AUTRE article (sinon on croit que rien n'a changé).
    if (offers[slot] >= 0 && offers[slot] < PERK_COUNT) picked[offers[slot]] = 1;

    int  weights[PERK_COUNT];
    long total = 0;
    for (int i = 0; i < PERK_COUNT; i++) {
        weights[i] = picked[i] ? 0 : perk_weight(rb, i, 0, 1);   // rachat = boutique
        total += weights[i];
    }
    if (total <= 0) return -1;                   // rien d'éligible → offre inchangée
    long r = GetRandomValue(0, (int)total - 1);
    int chosen = -1;
    for (int i = 0; i < PERK_COUNT; i++) {
        if (!weights[i]) continue;
        if (r < weights[i]) { chosen = i; break; }
        r -= weights[i];
    }
    if (chosen < 0) return -1;
    offers[slot] = chosen;
    return chosen;
}

int runperk_category(int perk_id) {
    switch (perk_id) {
        case PERK_BUTIN: case PERK_SUBV: case PERK_REVENTE: case PERK_FORTUNE:
        case PERK_K_SPOILS:
            return 1;   // économie
        case PERK_BLINDAGE: case PERK_FUSIL: case PERK_MEDIC: case PERK_LEGION:
            return 2;   // unité
        case PERK_GARNISON: case PERK_K_GEOMANCER:
            return 3;   // survie
        default:
            return 0;   // tour
    }
}

RunColor runperk_rarity_color(PerkRarity rar) {
    switch (rar) {
        case RAR_RARE: return (RunColor){ 90, 150, 230, 255};
        case RAR_EPIC: return (RunColor){200, 120, 255, 255};
        case RAR_KEYSTONE: return (RunColor){240, 196, 70, 255};
        case RAR_COMMON:
        default:       return (RunColor){150, 175, 150, 255};
    }
}

const char *runperk_rarity_name(PerkRarity rar) {
    switch (rar) {
        case RAR_RARE:     return "RARE";
        case RAR_EPIC:     return "ÉPIQUE";
        case RAR_KEYSTONE: return "CLÉ DE VOÛTE";
        case RAR_COMMON:
        default:           return "COMMUN";
    }
}
