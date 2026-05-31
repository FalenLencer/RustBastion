/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "runperks.h"
#include "raylib.h"      // GetRandomValue
#include <string.h>

RunMods g_run_mods;

// ════════════════════════════════════════════════════
// CATALOGUE — conçu par clusters de synergie
//   Mono-type burst : PERK_GUN/SNIPER/FLAME/TESLA + PUISSANCE + CADENCE
//                      + GLASS (offset par PORTEE) + ARSENAL (épique)
//   Éco → puissance  : BUTIN + SUBV + CAPITAL + FORTUNE (épique)
//   Armée            : BLINDAGE + FUSIL + MEDIC + LEGION (épique)
//   Survie           : GARNISON + REVENTE + éco
// ════════════════════════════════════════════════════
const PerkDef RUN_PERKS[PERK_COUNT] = {
    [PERK_GUN]      = {"Canons renforces",   "GUN",  "Tourelles : +25% degats / niv.",      RAR_COMMON, 3, 40},
    [PERK_SNIPER]   = {"Optique de precision","SNIP", "Snipers : +25% degats / niv.",        RAR_COMMON, 3, 40},
    [PERK_FLAME]    = {"Carburant ameliore", "FLAM", "Lance-flammes : +25% degats / niv.",   RAR_COMMON, 3, 40},
    [PERK_TESLA]    = {"Bobines surchargees","TESL", "Tesla : +25% degats / niv.",           RAR_COMMON, 3, 40},

    [PERK_PUISSANCE]= {"Surcharge",          "PWR",  "Toutes les tours : +20% degats / niv.",RAR_RARE,   3, 75},
    [PERK_PORTEE]   = {"Optiques longues",   "RNG",  "Toutes les tours : +12% portee / niv.",RAR_COMMON, 3, 45},
    [PERK_CADENCE]  = {"Servomoteurs",       "RATE", "Toutes les tours : +20% cadence / niv.",RAR_RARE,  3, 75},
    [PERK_GLASS]    = {"Canon de verre",     "GLAS", "+60% degats mais -25% portee.",         RAR_RARE,   2, 70},

    [PERK_BUTIN]    = {"Pillage",            "LOOT", "+3 or par ennemi tue / niv.",           RAR_COMMON, 3, 45},
    [PERK_SUBV]     = {"Subvention",         "SUBV", "+60 or au debut de chaque acte / niv.", RAR_COMMON, 3, 45},
    [PERK_REVENTE]  = {"Recyclage total",    "SELL", "Revente des tours : remboursement +40%.",RAR_COMMON,1, 50},
    [PERK_CAPITAL]  = {"Capitaliste",        "CAP$", "Tours : +5% degats par 100 or possede.",RAR_RARE,   2, 80},

    [PERK_BLINDAGE] = {"Blindage",           "ARMR", "Unites : +25% PV / niv.",               RAR_COMMON, 3, 45},
    [PERK_FUSIL]    = {"Armement lourd",     "ATK",  "Unites : +25% degats / niv.",           RAR_COMMON, 3, 45},
    [PERK_MEDIC]    = {"Trousse avancee",    "HEAL", "Soin des medics x2 / niv.",             RAR_RARE,   2, 70},

    [PERK_GARNISON] = {"Garnison",           "LIFE", "+15 vies au debut de chaque acte / niv.",RAR_COMMON,3, 50},

    [PERK_ARSENAL]  = {"Arsenal complet",    "ARSE", "Tours : +30% degats, +15% portee & cadence.",RAR_EPIC,1,120},
    [PERK_FORTUNE]  = {"Fortune de guerre",  "FORT", "+8 or/kill ET +120 or au debut d'acte.",RAR_EPIC,  1,120},
    [PERK_LEGION]   = {"Legion",             "LEGI", "Unites : +40% PV ET +40% degats.",      RAR_EPIC,  1,120},
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

    // Garde-fous
    if (m->trange_all < 0.30f) m->trange_all = 0.30f;
    if (m->tdmg_all   < 0.10f) m->tdmg_all   = 0.10f;
}

int runbuild_roll_offers(const RunBuild *rb, int *out, int n,
                         int bias, int shop_only) {
    (void)shop_only;   // même pool pour butin et boutique
    if (bias < 0) bias = 0;
    int n_out = 0;
    int picked[PERK_COUNT];
    for (int i = 0; i < PERK_COUNT; i++) picked[i] = 0;

    for (int k = 0; k < n && k < PERK_COUNT; k++) {
        int  weights[PERK_COUNT];
        long total = 0;
        for (int i = 0; i < PERK_COUNT; i++) {
            weights[i] = 0;
            if (picked[i]) continue;
            if (rb->count[i] >= RUN_PERKS[i].max_stack) continue;
            int w;
            switch (RUN_PERKS[i].rarity) {
                case RAR_RARE: w = 34 + 18 * bias; break;   // un peu plus présents
                case RAR_EPIC: w = 14 + 14 * bias; break;   // rares mais atteignables (× voie risquée)
                default:       w = 60;             break;
            }
            weights[i] = w;
            total += w;
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

int runperk_category(int perk_id) {
    switch (perk_id) {
        case PERK_BUTIN: case PERK_SUBV: case PERK_REVENTE: case PERK_FORTUNE:
            return 1;   // économie
        case PERK_BLINDAGE: case PERK_FUSIL: case PERK_MEDIC: case PERK_LEGION:
            return 2;   // unité
        case PERK_GARNISON:
            return 3;   // survie
        default:
            return 0;   // tour
    }
}

RunColor runperk_rarity_color(PerkRarity rar) {
    switch (rar) {
        case RAR_RARE: return (RunColor){ 90, 150, 230, 255};
        case RAR_EPIC: return (RunColor){200, 120, 255, 255};
        case RAR_COMMON:
        default:       return (RunColor){150, 175, 150, 255};
    }
}
