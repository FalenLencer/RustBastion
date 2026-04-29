#pragma once
#include "game_state.h"

#define SAVE_SLOT_COUNT   3
#define SAVE_MAGIC        0x52425356u
#define SAVE_VERSION      2            // ← bump pour invalider les anciens saves
#define SAVE_FILE_PREFIX  "rustbastion_slot"

// ── Mode de jeu d'un slot ────────────────────────────────────
typedef enum {
    SAVE_MODE_ARCADE    = 0,
    SAVE_MODE_CAMPAIGN  = 1,
} SaveMode;

// ── Résumé d'un slot ─────────────────────────────────────────
typedef struct {
    int      exists;
    int      slot;
    int      seed;
    int      theme;
    int      wave;
    int      gold;
    int      lives;
    char     theme_name[32];

    // Informations campagne
    SaveMode mode;           // arcade ou campagne
    int      campaign_num;   // numéro de campagne (0-based)
    int      campaign_stage; // stage actuel dans le cycle (0..4)
} SaveInfo;

// ── API ──────────────────────────────────────────────────────
int  save_write (const GameState *gs, int slot);
int  save_read  (GameState *gs, int slot);
int  save_info  (int slot, SaveInfo *out);
void save_delete(int slot);
void save_scan  (SaveInfo infos[SAVE_SLOT_COUNT]);