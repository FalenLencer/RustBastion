#include "meta.h"
#include "../map/theme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// ════════════════════════════════════════════════════
// COÛTS
// ════════════════════════════════════════════════════
const int COST_TOWER_DMG  [MAX_LVL_TOWER_DMG]    = {30, 50, 80, 120, 180};
const int COST_TOWER_RANGE[MAX_LVL_TOWER_RANGE]   = {40, 70, 110};
const int COST_TOWER_RATE [MAX_LVL_TOWER_RATE]    = {35, 60,  90, 140};
const int COST_UNIT_HP    [MAX_LVL_UNIT_HP]       = {25, 40,  65, 100, 150};
const int COST_UNIT_DMG   [MAX_LVL_UNIT_DMG]      = {30, 50,  80, 120};
const int COST_START_GOLD [MAX_LVL_START_GOLD]    = {50, 80, 120, 180};
const int COST_LIVES      [MAX_LVL_LIVES]         = {60, 100, 160};
const int COST_SCRAP_BONUS[MAX_LVL_SCRAP_BONUS]   = {45,  75, 120};

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
};

// ════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════
void meta_init(MetaProgress *meta) {
    memset(meta, 0, sizeof(MetaProgress));
    meta->magic   = META_MAGIC;
    meta->version = META_VERSION;
    meta->scrap   = 0;
}

// ════════════════════════════════════════════════════
// SAUVEGARDE / CHARGEMENT
// ════════════════════════════════════════════════════
void meta_save(const MetaProgress *meta) {
    FILE *f = fopen(META_SAVE_FILE, "wb");
    if (!f) return;
    fwrite(meta, sizeof(MetaProgress), 1, f);
    fclose(f);
}

int meta_load(MetaProgress *meta) {
    FILE *f = fopen(META_SAVE_FILE, "rb");
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
void meta_compute(const MetaProgress *meta, MetaBonuses *out) {
    out->tower_dmg_mult   = 1.0f + meta->lvl_tower_dmg   * 0.15f;
    out->tower_range_mult = 1.0f + meta->lvl_tower_range  * 0.10f;
    out->tower_rate_mult  = 1.0f + meta->lvl_tower_rate   * 0.10f;
    out->unit_hp_mult     = 1.0f + meta->lvl_unit_hp      * 0.20f;
    out->unit_dmg_mult    = 1.0f + meta->lvl_unit_dmg     * 0.15f;
    out->start_gold       = 150  + meta->lvl_start_gold   * 25;
    out->start_lives      = 20   + meta->lvl_lives        * 5;
    out->scrap_mult       = 1.0f + meta->lvl_scrap_bonus  * 0.20f;
}

// ════════════════════════════════════════════════════
// ORDRE DES THÈMES EN CAMPAGNE
// Chaque campagne a un ordre aléatoire différent.
// Fisher-Yates shuffle avec seed aléatoire.
// ════════════════════════════════════════════════════
void meta_campaign_theme_order(int campaign_num, int out_themes[CAMPAIGN_STAGES]) {
    (void)campaign_num;  // non utilisé maintenant

    // Initialise le RNG avec le temps actuel pour aléatoire réel
    srand((unsigned int)time(NULL));

    // Remplit 0..4
    for (int i = 0; i < CAMPAIGN_STAGES; i++)
        out_themes[i] = i;

    // Fisher-Yates shuffle
    for (int i = CAMPAIGN_STAGES - 1; i > 0; i--) {
        int j = rand() % (i + 1);
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
    // Ferraille = vague * 12 + kills * 3 + or/4
    // Bonus si stage avancé dans la campagne
    float stage_mult = 1.0f + stage_index * 0.25f;   // +25% par stage
    float base_scrap = (wave_reached * 12.0f
                      + kills        *  3.0f
                      + gold_remaining / 4.0f) * stage_mult;

    MetaBonuses bonuses;
    meta_compute(meta, &bonuses);
    int earned = (int)(base_scrap * bonuses.scrap_mult);
    if (earned < 8) earned = 8;   // minimum garanti

    meta->scrap              += earned;
    meta->total_scrap_earned += earned;
    meta->total_kills        += kills;
    if (wave_reached > meta->best_wave)
        meta->best_wave = wave_reached;

    // Si c'est le dernier stage, on compte une campagne complète
    if (stage_index == CAMPAIGN_STAGES - 1)
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
        default: return 0;
    }
    meta_save(meta);
    return 1;
}