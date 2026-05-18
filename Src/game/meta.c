/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "meta.h"
#include "../map/theme.h"
#include "../engine/paths.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ════════════════════════════════════════════════════
// COÛTS
// ════════════════════════════════════════════════════
const int COST_TOWER_DMG  [MAX_LVL_TOWER_DMG]    = {45,  75, 120, 180, 270};
const int COST_TOWER_RANGE[MAX_LVL_TOWER_RANGE]   = {60, 105, 165};
const int COST_TOWER_RATE [MAX_LVL_TOWER_RATE]    = {52,  90, 135, 210};
const int COST_UNIT_HP    [MAX_LVL_UNIT_HP]       = {37,  60,  97, 150, 225};
const int COST_UNIT_DMG   [MAX_LVL_UNIT_DMG]      = {45,  75, 120, 180};
const int COST_START_GOLD [MAX_LVL_START_GOLD]    = {75, 120, 180, 270};
const int COST_LIVES      [MAX_LVL_LIVES]         = {90, 150, 240};
const int COST_SCRAP_BONUS[MAX_LVL_SCRAP_BONUS]   = {67, 112, 180};
const int COST_TOWER_LIMIT[MAX_LVL_TOWER_LIMIT] = {80, 140, 220};
const int COST_UNIT_LIMIT [MAX_LVL_UNIT_LIMIT]  = {80, 140, 220};
// ════════════════════════════════════════════════════
// NOMS ET DESCRIPTIONS
// ════════════════════════════════════════════════════
const char *UPGRADE_NAMES[UPGRADE_COUNT] = {
    "Degats tours",
    "Portee tours",
    "Cadence tir",
    "Vie unites",
    "Degats unites",
    "Or de depart",
    "Vies max",
    "Ferraille bonus",
    [UPGRADE_TOWER_LIMIT] = "Slots tours",
    [UPGRADE_UNIT_LIMIT]  = "Slots unites",
};
const char *UPGRADE_DESC[UPGRADE_COUNT] = {
    "+15% degats par niv",
    "+10% portee par niv",
    "+10% cadence par niv",
    "+20% HP unites/niv",
    "+15% degats unites/niv",
    "+25 or de depart/niv",
    "+5 vies max/niv",
    "+20% ferraille/niv",
    [UPGRADE_TOWER_LIMIT] = "+2 tours max par niv",
    [UPGRADE_UNIT_LIMIT]  = "+2 unites max par niv",
};

// ════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════
void meta_init(MetaProgress *meta) {
    memset(meta, 0, sizeof(MetaProgress));
    memset(meta->act_stars, 0, sizeof(meta->act_stars));
    meta->magic   = META_MAGIC;
    meta->version = META_VERSION;
    meta->scrap   = 0;
    meta->lvl_tower_limit = 0;
    meta->lvl_unit_limit  = 0;
}

// ════════════════════════════════════════════════════
// SAUVEGARDE / CHARGEMENT
// ════════════════════════════════════════════════════
void meta_save(const MetaProgress *meta) {
    data_mkdir("saves");
    char path[512];
    FILE *f = fopen(data_path(path, sizeof(path), "saves/rustbastion_meta.sav"), "wb");
    if (!f) return;
    fwrite(meta, sizeof(MetaProgress), 1, f);
    fclose(f);
}

int meta_load(MetaProgress *meta) {
    char path[512];
    FILE *f = fopen(data_path(path, sizeof(path), "saves/rustbastion_meta.sav"), "rb");
    if (!f) return 0;
    MetaProgress tmp;
    int ok = (fread(&tmp, sizeof(MetaProgress), 1, f) == 1);
    fclose(f);
    if (!ok || tmp.magic != META_MAGIC || tmp.version != META_VERSION)
        return 0;
    *meta = tmp;
    return 1;
}

// ════════════════════════════════════════════════════
// CALCUL DES BONUS
// ════════════════════════════════════════════════════
/* Utilitaire : clamp int entre 0 et max */
static int clamp_lvl(int v, int max) {
    if (v < 0) return 0;
    if (v > max) return max;
    return v;
}

void meta_compute(const MetaProgress *meta, MetaBonuses *out) {
    /* Clampés pour résister à une éventuelle corruption du fichier */
    int tdmg  = clamp_lvl(meta->lvl_tower_dmg,   MAX_LVL_TOWER_DMG);
    int trng  = clamp_lvl(meta->lvl_tower_range,  MAX_LVL_TOWER_RANGE);
    int trate = clamp_lvl(meta->lvl_tower_rate,   MAX_LVL_TOWER_RATE);
    int uhp   = clamp_lvl(meta->lvl_unit_hp,      MAX_LVL_UNIT_HP);
    int udmg  = clamp_lvl(meta->lvl_unit_dmg,     MAX_LVL_UNIT_DMG);
    int sgold = clamp_lvl(meta->lvl_start_gold,   MAX_LVL_START_GOLD);
    int lives = clamp_lvl(meta->lvl_lives,         MAX_LVL_LIVES);
    int scrap = clamp_lvl(meta->lvl_scrap_bonus,  MAX_LVL_SCRAP_BONUS);
    int tlim  = clamp_lvl(meta->lvl_tower_limit,  MAX_LVL_TOWER_LIMIT);
    int ulim  = clamp_lvl(meta->lvl_unit_limit,   MAX_LVL_UNIT_LIMIT);

    out->tower_dmg_mult    = 1.0f + tdmg  * 0.15f;
    out->tower_range_mult  = 1.0f + trng  * 0.10f;
    out->tower_rate_mult   = 1.0f + trate * 0.10f;
    out->unit_hp_mult      = 1.0f + uhp   * 0.20f;
    out->unit_dmg_mult     = 1.0f + udmg  * 0.15f;
    out->start_gold        = 100  + sgold * 25;
    out->start_lives       = 20   + lives * 5;
    out->scrap_mult        = 1.0f + scrap * 0.20f;
    out->tower_limit_bonus = tlim;
    out->unit_limit_bonus  = ulim;
}

// ════════════════════════════════════════════════════
// ORDRE DES THÈMES EN CAMPAGNE
// Chaque campagne a un ordre aléatoire différent.
// Fisher-Yates shuffle avec seed aléatoire.
// ════════════════════════════════════════════════════
void meta_campaign_theme_order(int seed, int out_themes[CAMPAIGN_STAGES]) {
    for (int i = 0; i < CAMPAIGN_STAGES; i++)
        out_themes[i] = i;

    // xorshift déterministe — même seed = même ordre, toujours
    unsigned int rng = (unsigned int)(seed ^ 0xDEADBEEF);
    if (rng == 0) rng = 1;
    for (int i = CAMPAIGN_STAGES - 1; i > 0; i--) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        int j = (int)(rng % (unsigned int)(i + 1));
        int tmp = out_themes[i];
        out_themes[i] = out_themes[j];
        out_themes[j] = tmp;
    }
}

// ════════════════════════════════════════════════════
// FIN DE PARTIE ARCADE (pas de ferraille)
// ════════════════════════════════════════════════════
void meta_end_of_run(MetaProgress *meta, int wave_reached,
                     int kills, int gold_remaining)
{
    // En arcade, on enregistre les stats mais pas de ferraille
    meta->runs_completed++;
    meta->total_kills += kills;
    if (wave_reached > meta->best_wave)
        meta->best_wave = wave_reached;
    (void)gold_remaining;
    meta_save(meta);
}

// ════════════════════════════════════════════════════
// FIN D'UN STAGE DE CAMPAGNE — ferraille gagnée ici
// ════════════════════════════════════════════════════
int meta_end_of_campaign_stage(MetaProgress *meta, int wave_reached,
                                int kills, int gold_remaining,
                                int stage_index)
{
    // Nouvelle formule : vague*5 + kills/3 + or/10
    float stage_mult = 1.0f + stage_index * 0.25f;
    float base_scrap = (wave_reached  *  5.0f
                      + kills         /  3.0f
                      + gold_remaining / 10.0f) * stage_mult;

    MetaBonuses bonuses;
    meta_compute(meta, &bonuses);
    int earned = (int)(base_scrap * bonuses.scrap_mult);
    if (earned < 5)   earned = 5;    // minimum garanti
    if (earned > 200) earned = 200;  // plafond par partie

    meta->scrap              += earned;
    meta->total_scrap_earned += earned;
    meta->total_kills        += kills;
    if (wave_reached > meta->best_wave)
        meta->best_wave = wave_reached;

    if (stage_index == CAMPAIGN_TOTAL - 1)
        meta->campaigns_completed++;

    meta_save(meta);
    return earned;
}

// ════════════════════════════════════════════════════
// COÛT DE LA PROCHAINE AMÉLIORATION
// ════════════════════════════════════════════════════
int meta_upgrade_cost(const MetaProgress *meta, int id) {
    switch ((UpgradeID)id) {
        case UPGRADE_TOWER_DMG:
            if (meta->lvl_tower_dmg   >= MAX_LVL_TOWER_DMG)   return -1;
            return COST_TOWER_DMG  [meta->lvl_tower_dmg];
        case UPGRADE_TOWER_RANGE:
            if (meta->lvl_tower_range >= MAX_LVL_TOWER_RANGE)  return -1;
            return COST_TOWER_RANGE[meta->lvl_tower_range];
        case UPGRADE_TOWER_RATE:
            if (meta->lvl_tower_rate  >= MAX_LVL_TOWER_RATE)   return -1;
            return COST_TOWER_RATE [meta->lvl_tower_rate];
        case UPGRADE_UNIT_HP:
            if (meta->lvl_unit_hp     >= MAX_LVL_UNIT_HP)      return -1;
            return COST_UNIT_HP    [meta->lvl_unit_hp];
        case UPGRADE_UNIT_DMG:
            if (meta->lvl_unit_dmg    >= MAX_LVL_UNIT_DMG)     return -1;
            return COST_UNIT_DMG   [meta->lvl_unit_dmg];
        case UPGRADE_START_GOLD:
            if (meta->lvl_start_gold  >= MAX_LVL_START_GOLD)   return -1;
            return COST_START_GOLD [meta->lvl_start_gold];
        case UPGRADE_LIVES:
            if (meta->lvl_lives       >= MAX_LVL_LIVES)        return -1;
            return COST_LIVES      [meta->lvl_lives];
        case UPGRADE_SCRAP_BONUS:
            if (meta->lvl_scrap_bonus >= MAX_LVL_SCRAP_BONUS)  return -1;
            return COST_SCRAP_BONUS[meta->lvl_scrap_bonus];
        case UPGRADE_TOWER_LIMIT:
            if (meta->lvl_tower_limit >= MAX_LVL_TOWER_LIMIT) return -1;
            return COST_TOWER_LIMIT[meta->lvl_tower_limit];
        case UPGRADE_UNIT_LIMIT:
            if (meta->lvl_unit_limit >= MAX_LVL_UNIT_LIMIT)  return -1;
            return COST_UNIT_LIMIT [meta->lvl_unit_limit];
        default: return -1;
    }
}

// ════════════════════════════════════════════════════
// EFFECTUE UNE AMÉLIORATION
// ════════════════════════════════════════════════════
int meta_upgrade(MetaProgress *meta, int id) {
    int cost = meta_upgrade_cost(meta, id);
    if (cost < 0 || meta->scrap < cost) return 0;
    meta->scrap -= cost;
    switch ((UpgradeID)id) {
        case UPGRADE_TOWER_DMG:   meta->lvl_tower_dmg++;   break;
        case UPGRADE_TOWER_RANGE: meta->lvl_tower_range++;  break;
        case UPGRADE_TOWER_RATE:  meta->lvl_tower_rate++;   break;
        case UPGRADE_UNIT_HP:     meta->lvl_unit_hp++;      break;
        case UPGRADE_UNIT_DMG:    meta->lvl_unit_dmg++;     break;
        case UPGRADE_START_GOLD:  meta->lvl_start_gold++;   break;
        case UPGRADE_LIVES:       meta->lvl_lives++;         break;
        case UPGRADE_SCRAP_BONUS: meta->lvl_scrap_bonus++;  break;
        case UPGRADE_TOWER_LIMIT: meta->lvl_tower_limit++; break;
        case UPGRADE_UNIT_LIMIT:  meta->lvl_unit_limit++;  break;
        default: return 0;
    }
    meta_save(meta);
    return 1;
}

// Retourne le score calculé : vague * mult * 10
int meta_endless_score(int wave, float mult) {
    return (int)((float)wave * mult * 10.0f);
}

// Enregistre si c'est un nouveau record
void meta_endless_end(MetaProgress *meta, int wave, float mult,
                      int extracted)
{
    // Statistiques toujours enregistrées (game over ou extraction)
    meta->runs_completed++;
    if (wave > meta->best_wave)
        meta->best_wave = wave;

    if (!extracted) {
        meta_save(meta);
        return; // 0 ferraille si pas extrait / game over
    }
    int score = meta_endless_score(wave, mult);
    if (score > meta->endless_best_score) {
        meta->endless_best_score = score;
        meta->endless_best_wave  = wave;
    }
    // Ferraille = score / 10 (plafonné à 200)
    int earned = score / 10;
    if (earned > 200) earned = 200;
    meta->scrap              += earned;
    meta->total_scrap_earned += earned;
    meta_save(meta);
}

int meta_record_act(MetaProgress *meta, int stage_index,
                    int objective_done, int bonus_done)
{
    if (stage_index < 0 || stage_index >= CAMPAIGN_TOTAL) return 0;
    int stars = objective_done ? (bonus_done ? 2 : 1) : 0;
    if (stars > meta->act_stars[stage_index])
        meta->act_stars[stage_index] = stars;
    meta_save(meta);
    return stars;
}

int meta_act_unlocked(const MetaProgress *meta, int stage_index) {
    if (stage_index <= 0) return 1;              // premier acte toujours débloqué
    return meta->act_stars[stage_index - 1] > 0; // acte précédent complété
}

int meta_max_stage_completed(const MetaProgress *meta) {
    int max = -1;
    for (int i = 0; i < CAMPAIGN_TOTAL; i++)
        if (meta->act_stars[i] > 0) max = i;
    return max;
}