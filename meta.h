#pragma once

#define META_SAVE_FILE "rustbastion.sav"
#define META_MAGIC     0x52425354u  // "RBST"
#define META_VERSION   1

// ── Niveaux maximum par amélioration ─────────────────────────
#define MAX_LVL_TOWER_DMG    5
#define MAX_LVL_TOWER_RANGE  3
#define MAX_LVL_TOWER_RATE   4
#define MAX_LVL_UNIT_HP      5
#define MAX_LVL_UNIT_DMG     4
#define MAX_LVL_START_GOLD   4
#define MAX_LVL_LIVES        3
#define MAX_LVL_SCRAP_BONUS  3

// ── Coûts par niveau ─────────────────────────────────────────
// ← CORRECTION bug #6 : extern au lieu de static const dans le header.
//   Les tableaux sont définis une seule fois dans meta.c.
//   (static dans un header = copie silencieuse par unité de compilation)
extern const int COST_TOWER_DMG  [MAX_LVL_TOWER_DMG];
extern const int COST_TOWER_RANGE[MAX_LVL_TOWER_RANGE];
extern const int COST_TOWER_RATE [MAX_LVL_TOWER_RATE];
extern const int COST_UNIT_HP    [MAX_LVL_UNIT_HP];
extern const int COST_UNIT_DMG   [MAX_LVL_UNIT_DMG];
extern const int COST_START_GOLD [MAX_LVL_START_GOLD];
extern const int COST_LIVES      [MAX_LVL_LIVES];
extern const int COST_SCRAP_BONUS[MAX_LVL_SCRAP_BONUS];

// ── Structure de méta-progression ────────────────────────────
typedef struct {
    unsigned int magic;    // validation du fichier
    int          version;

    // Monnaie persistante
    int scrap;
    int total_scrap_earned;

    // Statistiques
    int runs_completed;
    int best_wave;
    int total_kills;

    // Niveaux d'amélioration
    int lvl_tower_dmg;
    int lvl_tower_range;
    int lvl_tower_rate;
    int lvl_unit_hp;
    int lvl_unit_dmg;
    int lvl_start_gold;
    int lvl_lives;
    int lvl_scrap_bonus;
} MetaProgress;

// ── Multiplicateurs calculés depuis les niveaux ───────────────
typedef struct {
    float tower_dmg_mult;    // ex: 1.0 + lvl * 0.15
    float tower_range_mult;
    float tower_rate_mult;
    float unit_hp_mult;
    float unit_dmg_mult;
    int   start_gold;        // or de départ
    int   start_lives;       // vies de départ
    float scrap_mult;        // multiplicateur de ferraille gagnée
} MetaBonuses;

// ── API ──────────────────────────────────────────────────────
void meta_init        (MetaProgress *meta);
void meta_save        (const MetaProgress *meta);
int  meta_load        (MetaProgress *meta);   // 0=échec, 1=succès
void meta_compute     (const MetaProgress *meta, MetaBonuses *out);

// Appelé en fin de partie
void meta_end_of_run  (MetaProgress *meta, int wave_reached,
                       int kills, int gold_remaining);

// Coût de la prochaine amélioration (-1 si déjà au max)
int  meta_upgrade_cost(const MetaProgress *meta, int upgrade_id);

// Effectue une amélioration (retourne 1 si succès)
int  meta_upgrade     (MetaProgress *meta, int upgrade_id);

// IDs des améliorations (pour meta_upgrade / meta_upgrade_cost)
typedef enum {
    UPGRADE_TOWER_DMG   = 0,
    UPGRADE_TOWER_RANGE,
    UPGRADE_TOWER_RATE,
    UPGRADE_UNIT_HP,
    UPGRADE_UNIT_DMG,
    UPGRADE_START_GOLD,
    UPGRADE_LIVES,
    UPGRADE_SCRAP_BONUS,
    UPGRADE_COUNT
} UpgradeID;

extern const char *UPGRADE_NAMES[UPGRADE_COUNT];
extern const char *UPGRADE_DESC [UPGRADE_COUNT];