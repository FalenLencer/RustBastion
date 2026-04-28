#include "save.h"
#include "theme.h"
#include "pathfinding.h"
#include "map_gen.h"
#include <stdio.h>
#include <string.h>

// ── Entête de fichier ─────────────────────────────────────────
typedef struct {
    unsigned int magic;
    int          version;
    int          slot;
    // Résumé rapide pour save_info()
    int          seed;
    int          theme;
    int          wave;
    int          gold;
    int          lives;
} SaveHeader;

// ── Chemin de fichier ─────────────────────────────────────────
static void slot_path(int slot, char *buf, int bufsz) {
    snprintf(buf, bufsz, "%s%d.sav", SAVE_FILE_PREFIX, slot);
}

// ════════════════════════════════════════════════════
// ÉCRITURE
// ════════════════════════════════════════════════════
int save_write(const GameState *gs, int slot) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return 0;

    char path[128];
    slot_path(slot, path, sizeof(path));

    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    // Entête
    SaveHeader hdr = {
        .magic   = SAVE_MAGIC,
        .version = SAVE_VERSION,
        .slot    = slot,
        .seed    = gs->map.seed,
        .theme   = (int)gs->map.theme,
        .wave    = gs->wave_manager.number,
        .gold    = gs->gold,
        .lives   = gs->lives,
    };
    fwrite(&hdr, sizeof(hdr), 1, f);

    // État complet
    fwrite(&gs->map,          sizeof(gs->map),          1, f);
    fwrite(&gs->enemy_paths,  sizeof(gs->enemy_paths),  1, f);
    fwrite(&gs->enemies,      sizeof(gs->enemies),       1, f);
    fwrite(&gs->wave_manager, sizeof(gs->wave_manager),  1, f);
    fwrite(&gs->towers,       sizeof(gs->towers),        1, f);
    fwrite(&gs->units,        sizeof(gs->units),         1, f);
    fwrite(&gs->bonuses,      sizeof(gs->bonuses),       1, f);
    fwrite(&gs->phase,        sizeof(gs->phase),         1, f);
    fwrite(&gs->gold,         sizeof(gs->gold),          1, f);
    fwrite(&gs->lives,        sizeof(gs->lives),         1, f);
    fwrite(&gs->kills,        sizeof(gs->kills),         1, f);
    // Note : meta est sauvegardé séparément par meta_save()
    // Note : UIState n'est pas sauvegardé (réinitialisée au chargement)

    fclose(f);
    return 1;
}

// ════════════════════════════════════════════════════
// LECTURE
// ════════════════════════════════════════════════════
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

    // Charge les meta d'abord (déjà chargées normalement, mais on recharge)
    if (!meta_load(&gs->meta)) meta_init(&gs->meta);
    meta_compute(&gs->meta, &gs->bonuses);

    int ok = 1;
    ok &= (fread(&gs->map,          sizeof(gs->map),          1, f) == 1);
    ok &= (fread(&gs->enemy_paths,  sizeof(gs->enemy_paths),  1, f) == 1);
    ok &= (fread(&gs->enemies,      sizeof(gs->enemies),       1, f) == 1);
    ok &= (fread(&gs->wave_manager, sizeof(gs->wave_manager),  1, f) == 1);
    ok &= (fread(&gs->towers,       sizeof(gs->towers),        1, f) == 1);
    ok &= (fread(&gs->units,        sizeof(gs->units),         1, f) == 1);
    ok &= (fread(&gs->bonuses,      sizeof(gs->bonuses),       1, f) == 1);
    ok &= (fread(&gs->phase,        sizeof(gs->phase),         1, f) == 1);
    ok &= (fread(&gs->gold,         sizeof(gs->gold),          1, f) == 1);
    ok &= (fread(&gs->lives,        sizeof(gs->lives),         1, f) == 1);
    ok &= (fread(&gs->kills,        sizeof(gs->kills),         1, f) == 1);

    fclose(f);

    if (!ok) return 0;

    // Réinitialise l'UI (position des boutons dépend de la fenêtre)
    ui_init(&gs->ui);

    return 1;
}

// ════════════════════════════════════════════════════
// RÉSUMÉ
// ════════════════════════════════════════════════════
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

    out->exists = 1;
    out->slot   = slot;
    out->seed   = hdr.seed;
    out->theme  = hdr.theme;
    out->wave   = hdr.wave;
    out->gold   = hdr.gold;
    out->lives  = hdr.lives;

    const Theme *th = theme_get((ThemeID)hdr.theme);
    strncpy(out->theme_name, th->name, sizeof(out->theme_name) - 1);
    out->theme_name[sizeof(out->theme_name) - 1] = '\0';

    return 1;
}

// ════════════════════════════════════════════════════
// SUPPRESSION
// ════════════════════════════════════════════════════
void save_delete(int slot) {
    char path[128];
    slot_path(slot, path, sizeof(path));
    remove(path);
}

// ════════════════════════════════════════════════════
// SCAN DE TOUS LES SLOTS
// ════════════════════════════════════════════════════
void save_scan(SaveInfo infos[SAVE_SLOT_COUNT]) {
    for (int i = 0; i < SAVE_SLOT_COUNT; i++)
        save_info(i, &infos[i]);
}