/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "game_state.h"

#define SAVE_SLOT_COUNT   3
#define SAVE_MAGIC        0x52425356u
#define SAVE_VERSION      7            /* bump : Unit +home_base_px/py, multi-base spawn */
#define SAVE_FILE_PREFIX  "saves/rustbastion_slot"

/* ── Mode de jeu d'un slot ──────────────────────────────────── */
typedef enum {
    SAVE_MODE_ARCADE    = 0,
    SAVE_MODE_CAMPAIGN  = 1,
} SaveMode;

/* ── Résumé d'un slot ───────────────────────────────────────── */
typedef struct {
    int      exists;
    int      slot;
    int      seed;
    int      theme;
    int      wave;
    int      gold;
    int      lives;
    char     theme_name[32];

    /* Informations campagne */
    SaveMode mode;
    int      campaign_num;
    int      campaign_stage;
    int      campaign_order_seed;
} SaveInfo;

/* ── API ─────────────────────────────────────────────────────── */
void save_init  (void);
int  save_write (const GameState *gs, int slot);
int  save_read  (GameState *gs, int slot);
int  save_info  (int slot, SaveInfo *out);
void save_delete(int slot);
void save_scan  (SaveInfo infos[SAVE_SLOT_COUNT]);