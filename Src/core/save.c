#include "save.h"
#include "../map/theme.h"
#include "../map/pathfinding.h"
#include "../map/map_gen.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

/* ── Entête de fichier ──────────────────────────────────────── */
typedef struct {
    unsigned int magic;
    int          version;
    int          slot;
    int          seed;
    int          theme;
    int          wave;
    int          gold;
    int          lives;
    int          mode;
    int          campaign_num;
    int          campaign_stage;
    int          campaign_order_seed;   /* CORRECTIF #1 : champ manquant */

    /* CORRECTIF #13 : tailles des structs sérialisées pour détecter
       les incompatibilités entre compilations sans bumper la version */
    int          sz_map;
    int          sz_enemy_pool;
    int          sz_wave_manager;
    int          sz_tower_pool;
    int          sz_unit_pool;
    int          sz_inventory;
    int          sz_meta_bonuses;
} SaveHeader;

static void slot_path(int slot, char *buf, int bufsz) {
    /* Créer le dossier saves/ si nécessaire */
    mkdir("saves", 0755);

    snprintf(buf, bufsz, "%s%d.sav", SAVE_FILE_PREFIX, slot);
}

/* ════════════════════════════════════════════════════
   INITIALISATION
   ════════════════════════════════════════════════════ */
void save_init(void) {
    /* Créer le dossier saves/ si nécessaire */
    mkdir("saves", 0755);
}
int save_write(const GameState *gs, int slot) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return 0;

    char path[128];
    slot_path(slot, path, sizeof(path));

    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    SaveHeader hdr = {
        .magic               = SAVE_MAGIC,
        .version             = SAVE_VERSION,
        .slot                = slot,
        .seed                = gs->map.seed,
        .theme               = (int)gs->map.theme,
        .wave                = gs->wave_manager.number,
        .gold                = gs->gold,
        .lives               = gs->lives,
        .mode                = gs->is_campaign ? SAVE_MODE_CAMPAIGN : SAVE_MODE_ARCADE,
        .campaign_num        = gs->campaign_num,
        .campaign_stage      = gs->campaign_stage,
        .campaign_order_seed = gs->campaign_order_seed,   /* CORRECTIF #1 */
        .sz_map              = (int)sizeof(gs->map),
        .sz_enemy_pool       = (int)sizeof(gs->enemies),
        .sz_wave_manager     = (int)sizeof(gs->wave_manager),
        .sz_tower_pool       = (int)sizeof(gs->towers),
        .sz_unit_pool        = (int)sizeof(gs->units),
        .sz_inventory        = (int)sizeof(gs->inventory),
        .sz_meta_bonuses     = (int)sizeof(gs->bonuses),
    };

    int ok = 1;
    ok &= (fwrite(&hdr,              sizeof(hdr),              1, f) == 1);
    ok &= (fwrite(&gs->map,          sizeof(gs->map),          1, f) == 1);
    ok &= (fwrite(&gs->enemy_paths,  sizeof(gs->enemy_paths),  1, f) == 1);
    ok &= (fwrite(&gs->enemies,      sizeof(gs->enemies),      1, f) == 1);
    ok &= (fwrite(&gs->wave_manager, sizeof(gs->wave_manager), 1, f) == 1);
    ok &= (fwrite(&gs->towers,       sizeof(gs->towers),       1, f) == 1);
    ok &= (fwrite(&gs->units,        sizeof(gs->units),        1, f) == 1);
    ok &= (fwrite(&gs->inventory,    sizeof(gs->inventory),    1, f) == 1);
    ok &= (fwrite(&gs->inventory_count, sizeof(gs->inventory_count), 1, f) == 1);
    ok &= (fwrite(&gs->bonuses,      sizeof(gs->bonuses),      1, f) == 1);
    ok &= (fwrite(&gs->phase,        sizeof(gs->phase),        1, f) == 1);
    ok &= (fwrite(&gs->gold,         sizeof(gs->gold),         1, f) == 1);
    ok &= (fwrite(&gs->lives,        sizeof(gs->lives),        1, f) == 1);
    ok &= (fwrite(&gs->kills,        sizeof(gs->kills),        1, f) == 1);
    ok &= (fwrite(&gs->is_campaign,  sizeof(gs->is_campaign),  1, f) == 1);
    ok &= (fwrite(&gs->campaign_num, sizeof(gs->campaign_num), 1, f) == 1);
    ok &= (fwrite(&gs->campaign_stage,      sizeof(gs->campaign_stage),      1, f) == 1);
    ok &= (fwrite(&gs->campaign_order_seed, sizeof(gs->campaign_order_seed), 1, f) == 1); /* CORRECTIF #1 */

    fclose(f);
    return ok; /* CORRECTIF #9 : retourne le résultat de l'écriture */
}

/* ════════════════════════════════════════════════════
   LECTURE
   ════════════════════════════════════════════════════ */
int save_read(GameState *gs, int slot) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return 0;

    char path[128];
    slot_path(slot, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    SaveHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return 0; }
    if (hdr.magic != SAVE_MAGIC || hdr.version != SAVE_VERSION) {
        fclose(f); return 0;
    }

    /* CORRECTIF #13 : vérification des tailles de structs avant lecture */
    if (hdr.sz_map          != (int)sizeof(gs->map)          ||
        hdr.sz_enemy_pool   != (int)sizeof(gs->enemies)      ||
        hdr.sz_wave_manager != (int)sizeof(gs->wave_manager) ||
        hdr.sz_tower_pool   != (int)sizeof(gs->towers)       ||
        hdr.sz_unit_pool    != (int)sizeof(gs->units)        ||
        hdr.sz_inventory    != (int)sizeof(gs->inventory)    ||
        hdr.sz_meta_bonuses != (int)sizeof(gs->bonuses)) {
        fclose(f); return 0;
    }

    if (!meta_load(&gs->meta)) meta_init(&gs->meta);
    meta_compute(&gs->meta, &gs->bonuses);

    int ok = 1;
    ok &= (fread(&gs->map,                  sizeof(gs->map),                  1, f) == 1);
    ok &= (fread(&gs->enemy_paths,          sizeof(gs->enemy_paths),          1, f) == 1);
    ok &= (fread(&gs->enemies,              sizeof(gs->enemies),              1, f) == 1);
    ok &= (fread(&gs->wave_manager,         sizeof(gs->wave_manager),         1, f) == 1);
    ok &= (fread(&gs->towers,               sizeof(gs->towers),               1, f) == 1);
    ok &= (fread(&gs->units,                sizeof(gs->units),                1, f) == 1);
    ok &= (fread(&gs->inventory,            sizeof(gs->inventory),            1, f) == 1);
    ok &= (fread(&gs->inventory_count,      sizeof(gs->inventory_count),      1, f) == 1);
    ok &= (fread(&gs->bonuses,              sizeof(gs->bonuses),              1, f) == 1);
    ok &= (fread(&gs->phase,                sizeof(gs->phase),                1, f) == 1);
    ok &= (fread(&gs->gold,                 sizeof(gs->gold),                 1, f) == 1);
    ok &= (fread(&gs->lives,                sizeof(gs->lives),                1, f) == 1);
    ok &= (fread(&gs->kills,                sizeof(gs->kills),                1, f) == 1);
    ok &= (fread(&gs->is_campaign,          sizeof(gs->is_campaign),          1, f) == 1);
    ok &= (fread(&gs->campaign_num,         sizeof(gs->campaign_num),         1, f) == 1);
    ok &= (fread(&gs->campaign_stage,       sizeof(gs->campaign_stage),       1, f) == 1);
    ok &= (fread(&gs->campaign_order_seed,  sizeof(gs->campaign_order_seed),  1, f) == 1); /* CORRECTIF #1 */

    fclose(f);
    if (!ok) return 0;

    ui_init(&gs->ui);
    return 1;
}

/* ════════════════════════════════════════════════════
   RÉSUMÉ
   ════════════════════════════════════════════════════ */
int save_info(int slot, SaveInfo *out) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return 0;

    char path[128];
    slot_path(slot, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) {
        out->exists = 0;
        out->slot   = slot;
        return 0;
    }

    SaveHeader hdr;
    int ok = (fread(&hdr, sizeof(hdr), 1, f) == 1);
    fclose(f);

    if (!ok || hdr.magic != SAVE_MAGIC || hdr.version != SAVE_VERSION) {
        out->exists = 0;
        out->slot   = slot;
        return 0;
    }

    out->exists              = 1;
    out->slot                = slot;
    out->seed                = hdr.seed;
    out->theme               = hdr.theme;
    out->wave                = hdr.wave;
    out->gold                = hdr.gold;
    out->lives               = hdr.lives;
    out->mode                = (SaveMode)hdr.mode;
    out->campaign_num        = hdr.campaign_num;
    out->campaign_stage      = hdr.campaign_stage;
    out->campaign_order_seed = hdr.campaign_order_seed; /* CORRECTIF #1 */

    const Theme *th = theme_get((ThemeID)hdr.theme);
    strncpy(out->theme_name, th->name, sizeof(out->theme_name) - 1);
    out->theme_name[sizeof(out->theme_name) - 1] = '\0';

    return 1;
}

/* ════════════════════════════════════════════════════
   SUPPRESSION / SCAN
   ════════════════════════════════════════════════════ */
void save_delete(int slot) {
    char path[128];
    slot_path(slot, path, sizeof(path));
    remove(path);
}

void save_scan(SaveInfo infos[SAVE_SLOT_COUNT]) {
    for (int i = 0; i < SAVE_SLOT_COUNT; i++)
        save_info(i, &infos[i]);
}