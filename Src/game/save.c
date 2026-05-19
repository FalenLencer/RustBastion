/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "save.h"
#include "../map/theme.h"
#include "../map/pathfinding.h"
#include "../map/map_gen.h"
#include "../engine/paths.h"
#include <stdio.h>
#include <string.h>

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

    /* Champs endless */
    int          is_endless;
    int          endless_series;
    float        endless_multiplier;
    int          endless_pending_extract;
} SaveHeader;

static void slot_path(int slot, char *buf, int bufsz) {
    data_mkdir("saves");
    char rel[64];
    snprintf(rel, sizeof(rel), "saves/rustbastion_slot%d.sav", slot);
    data_path(buf, bufsz, rel);
}

/* ════════════════════════════════════════════════════
   INITIALISATION
   ════════════════════════════════════════════════════ */
void save_init(void) {
    data_mkdir("saves");
}
int save_write(const GameState *gs, int slot) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return 0;

    char path[512];
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
        .campaign_order_seed = gs->campaign_order_seed,
        .is_endless              = gs->is_endless,
        .endless_series          = gs->endless_series,
        .endless_multiplier      = gs->endless_multiplier,
        .endless_pending_extract = gs->endless_pending_extract,
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
    ok &= (fwrite(&gs->campaign_order_seed,      sizeof(gs->campaign_order_seed),      1, f) == 1);
    ok &= (fwrite(&gs->is_endless,               sizeof(gs->is_endless),               1, f) == 1);
    ok &= (fwrite(&gs->endless_series,           sizeof(gs->endless_series),           1, f) == 1);
    ok &= (fwrite(&gs->endless_multiplier,       sizeof(gs->endless_multiplier),       1, f) == 1);
    ok &= (fwrite(&gs->endless_pending_extract,  sizeof(gs->endless_pending_extract),  1, f) == 1);
    /* Champs suivi objectifs campagne (act_* réinitialisés si save manquant) */
    ok &= (fwrite(&gs->act_no_unit_lost,         sizeof(gs->act_no_unit_lost),         1, f) == 1);
    ok &= (fwrite(&gs->act_materials_collected,  sizeof(gs->act_materials_collected),  1, f) == 1);
    ok &= (fwrite(&gs->act_objective_done,       sizeof(gs->act_objective_done),       1, f) == 1);

    fclose(f);
    return ok;
}

/* ════════════════════════════════════════════════════
   LECTURE
   ════════════════════════════════════════════════════ */
int save_read(GameState *gs, int slot) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return 0;

    char path[512];
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
    ok &= (fread(&gs->campaign_order_seed,      sizeof(gs->campaign_order_seed),      1, f) == 1);
    ok &= (fread(&gs->is_endless,               sizeof(gs->is_endless),               1, f) == 1);
    ok &= (fread(&gs->endless_series,           sizeof(gs->endless_series),           1, f) == 1);
    ok &= (fread(&gs->endless_multiplier,       sizeof(gs->endless_multiplier),       1, f) == 1);
    ok &= (fread(&gs->endless_pending_extract,  sizeof(gs->endless_pending_extract),  1, f) == 1);
    /* Champs suivi objectifs campagne */
    ok &= (fread(&gs->act_no_unit_lost,         sizeof(gs->act_no_unit_lost),         1, f) == 1);
    ok &= (fread(&gs->act_materials_collected,  sizeof(gs->act_materials_collected),  1, f) == 1);
    ok &= (fread(&gs->act_objective_done,       sizeof(gs->act_objective_done),       1, f) == 1);

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

    char path[512];
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
   SUPPRESSION / SCAN (Arcade)
   ════════════════════════════════════════════════════ */
void save_delete(int slot) {
    char path[512];
    slot_path(slot, path, sizeof(path));
    remove(path);
}

void save_scan(SaveInfo infos[SAVE_SLOT_COUNT]) {
    for (int i = 0; i < SAVE_SLOT_COUNT; i++)
        save_info(i, &infos[i]);
}

/* ════════════════════════════════════════════════════
   CAMPAGNE — système de sauvegarde indépendant
   Fichiers : saves/rustbastion_camp%d.sav
   Magic différent → impossible de confondre avec arcade
   ════════════════════════════════════════════════════ */

/* Header propre à la campagne (inclut l'état interlude) */
typedef struct {
    unsigned int magic;
    int          version;
    int          slot;
    int          seed;
    int          theme;
    int          wave;
    int          gold;
    int          lives;
    int          campaign_num;
    int          campaign_stage;
    int          campaign_order_seed;
    /* État interlude — permet de restaurer le dialogue de fin d'acte */
    int          interlude;
    int          interlude_scrap;
    int          interlude_stars;
    int          interlude_last;
    /* Tailles pour détecter les incompatibilités de structures */
    int          sz_map;
    int          sz_enemy_pool;
    int          sz_wave_manager;
    int          sz_tower_pool;
    int          sz_unit_pool;
    int          sz_inventory;
} CampaignSaveHeader;

static void campaign_slot_path(int slot, char *buf, int bufsz) {
    data_mkdir("saves");
    char rel[64];
    snprintf(rel, sizeof(rel), "saves/rustbastion_camp%d.sav", slot);
    data_path(buf, bufsz, rel);
}

int campaign_save_write(const GameState *gs, int slot,
                        int interlude,       int interlude_scrap,
                        int interlude_stars, int interlude_last)
{
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return 0;
    char path[512];
    campaign_slot_path(slot, path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    CampaignSaveHeader hdr = {
        .magic               = SAVE_CAMPAIGN_MAGIC,
        .version             = SAVE_CAMPAIGN_VERSION,
        .slot                = slot,
        .seed                = gs->map.seed,
        .theme               = (int)gs->map.theme,
        .wave                = gs->wave_manager.number,
        .gold                = gs->gold,
        .lives               = gs->lives,
        .campaign_num        = gs->campaign_num,
        .campaign_stage      = gs->campaign_stage,
        .campaign_order_seed = gs->campaign_order_seed,
        .interlude           = interlude,
        .interlude_scrap     = interlude_scrap,
        .interlude_stars     = interlude_stars,
        .interlude_last      = interlude_last,
        .sz_map              = (int)sizeof(gs->map),
        .sz_enemy_pool       = (int)sizeof(gs->enemies),
        .sz_wave_manager     = (int)sizeof(gs->wave_manager),
        .sz_tower_pool       = (int)sizeof(gs->towers),
        .sz_unit_pool        = (int)sizeof(gs->units),
        .sz_inventory        = (int)sizeof(gs->inventory),
    };

    int ok = 1;
    ok &= (fwrite(&hdr,                   sizeof(hdr),                   1, f) == 1);
    ok &= (fwrite(&gs->map,               sizeof(gs->map),               1, f) == 1);
    ok &= (fwrite(&gs->enemy_paths,       sizeof(gs->enemy_paths),       1, f) == 1);
    ok &= (fwrite(&gs->enemies,           sizeof(gs->enemies),           1, f) == 1);
    ok &= (fwrite(&gs->wave_manager,      sizeof(gs->wave_manager),      1, f) == 1);
    ok &= (fwrite(&gs->towers,            sizeof(gs->towers),            1, f) == 1);
    ok &= (fwrite(&gs->units,             sizeof(gs->units),             1, f) == 1);
    ok &= (fwrite(&gs->inventory,         sizeof(gs->inventory),         1, f) == 1);
    ok &= (fwrite(&gs->inventory_count,   sizeof(gs->inventory_count),   1, f) == 1);
    ok &= (fwrite(&gs->phase,             sizeof(gs->phase),             1, f) == 1);
    ok &= (fwrite(&gs->gold,              sizeof(gs->gold),              1, f) == 1);
    ok &= (fwrite(&gs->lives,             sizeof(gs->lives),             1, f) == 1);
    ok &= (fwrite(&gs->kills,             sizeof(gs->kills),             1, f) == 1);
    ok &= (fwrite(&gs->campaign_num,      sizeof(gs->campaign_num),      1, f) == 1);
    ok &= (fwrite(&gs->campaign_stage,    sizeof(gs->campaign_stage),    1, f) == 1);
    ok &= (fwrite(&gs->campaign_order_seed,
                  sizeof(gs->campaign_order_seed), 1, f) == 1);
    ok &= (fwrite(&gs->act_no_unit_lost,
                  sizeof(gs->act_no_unit_lost), 1, f) == 1);
    ok &= (fwrite(&gs->act_materials_collected,
                  sizeof(gs->act_materials_collected), 1, f) == 1);
    ok &= (fwrite(&gs->act_objective_done,
                  sizeof(gs->act_objective_done), 1, f) == 1);
    fclose(f);
    return ok;
}

int campaign_save_read(GameState *gs, int slot,
                       int *out_interlude, int *out_scrap,
                       int *out_stars,     int *out_last)
{
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return 0;
    char path[512];
    campaign_slot_path(slot, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    CampaignSaveHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return 0; }
    if (hdr.magic   != SAVE_CAMPAIGN_MAGIC ||
        hdr.version != SAVE_CAMPAIGN_VERSION) { fclose(f); return 0; }

    /* Vérification de compatibilité des structs */
    if (hdr.sz_map          != (int)sizeof(gs->map)          ||
        hdr.sz_enemy_pool   != (int)sizeof(gs->enemies)      ||
        hdr.sz_wave_manager != (int)sizeof(gs->wave_manager) ||
        hdr.sz_tower_pool   != (int)sizeof(gs->towers)       ||
        hdr.sz_unit_pool    != (int)sizeof(gs->units)        ||
        hdr.sz_inventory    != (int)sizeof(gs->inventory)) {
        fclose(f); return 0;
    }

    /* Méta rechargée depuis le fichier séparé, bonus recalculés.
       On n'écrase JAMAIS les bonus avec des valeurs en cache. */
    if (!meta_load(&gs->meta)) meta_init(&gs->meta);
    meta_compute(&gs->meta, &gs->bonuses);

    int ok = 1;
    ok &= (fread(&gs->map,          sizeof(gs->map),          1, f) == 1);
    ok &= (fread(&gs->enemy_paths,  sizeof(gs->enemy_paths),  1, f) == 1);
    ok &= (fread(&gs->enemies,      sizeof(gs->enemies),      1, f) == 1);
    ok &= (fread(&gs->wave_manager, sizeof(gs->wave_manager), 1, f) == 1);
    ok &= (fread(&gs->towers,       sizeof(gs->towers),       1, f) == 1);
    ok &= (fread(&gs->units,        sizeof(gs->units),        1, f) == 1);
    ok &= (fread(&gs->inventory,    sizeof(gs->inventory),    1, f) == 1);
    ok &= (fread(&gs->inventory_count,   sizeof(gs->inventory_count),   1, f) == 1);
    ok &= (fread(&gs->phase,        sizeof(gs->phase),        1, f) == 1);
    ok &= (fread(&gs->gold,         sizeof(gs->gold),         1, f) == 1);
    ok &= (fread(&gs->lives,        sizeof(gs->lives),        1, f) == 1);
    ok &= (fread(&gs->kills,        sizeof(gs->kills),        1, f) == 1);
    ok &= (fread(&gs->campaign_num, sizeof(gs->campaign_num), 1, f) == 1);
    ok &= (fread(&gs->campaign_stage,
                 sizeof(gs->campaign_stage), 1, f) == 1);
    ok &= (fread(&gs->campaign_order_seed,
                 sizeof(gs->campaign_order_seed), 1, f) == 1);
    ok &= (fread(&gs->act_no_unit_lost,
                 sizeof(gs->act_no_unit_lost), 1, f) == 1);
    ok &= (fread(&gs->act_materials_collected,
                 sizeof(gs->act_materials_collected), 1, f) == 1);
    ok &= (fread(&gs->act_objective_done,
                 sizeof(gs->act_objective_done), 1, f) == 1);
    fclose(f);
    if (!ok) return 0;

    gs->is_campaign = 1;
    /* Restitution de l'état interlude pour éviter le bug double-ferraille */
    if (out_interlude) *out_interlude = hdr.interlude;
    if (out_scrap)     *out_scrap     = hdr.interlude_scrap;
    if (out_stars)     *out_stars     = hdr.interlude_stars;
    if (out_last)      *out_last      = hdr.interlude_last;

    ui_init(&gs->ui);
    return 1;
}

int campaign_save_info(int slot, SaveInfo *out) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return 0;
    char path[512];
    campaign_slot_path(slot, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) { out->exists = 0; out->slot = slot; return 0; }

    CampaignSaveHeader hdr;
    int ok = (fread(&hdr, sizeof(hdr), 1, f) == 1);
    fclose(f);

    if (!ok || hdr.magic   != SAVE_CAMPAIGN_MAGIC ||
               hdr.version != SAVE_CAMPAIGN_VERSION) {
        out->exists = 0; out->slot = slot; return 0;
    }

    out->exists              = 1;
    out->slot                = slot;
    out->seed                = hdr.seed;
    out->theme               = hdr.theme;
    out->wave                = hdr.wave;
    out->gold                = hdr.gold;
    out->lives               = hdr.lives;
    out->mode                = SAVE_MODE_CAMPAIGN;
    out->campaign_num        = hdr.campaign_num;
    out->campaign_stage      = hdr.campaign_stage;
    out->campaign_order_seed = hdr.campaign_order_seed;

    const Theme *th = theme_get((ThemeID)hdr.theme);
    strncpy(out->theme_name, th->name, sizeof(out->theme_name) - 1);
    out->theme_name[sizeof(out->theme_name) - 1] = '\0';
    return 1;
}

void campaign_save_delete(int slot) {
    char path[512];
    campaign_slot_path(slot, path, sizeof(path));
    remove(path);
}

void campaign_save_scan(SaveInfo infos[SAVE_SLOT_COUNT]) {
    for (int i = 0; i < SAVE_SLOT_COUNT; i++)
        campaign_save_info(i, &infos[i]);
}