/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once

#define MAX_MATERIAL_DEPOSITS 20   // dépôts max sur une carte (valeurs >20 clampées)
#define MAX_INVENTORY          8   // matériaux max en inventaire

// ── Types de matériaux collectables ─────────────────────────
typedef enum {
    MAT_NONE  = -1,
    MAT_IRON  = 0,   // +10% dégâts physiques
    MAT_ACID,        // dégâts poison (DoT)
    MAT_PLASMA,      // électrique, chain sur 2 cibles proches
    MAT_CRYO,        // ralentissement renforcé -70%
    MAT_NANO,        // régénération des unités alliées proches
    MAT_COUNT
} MaterialType;

// ── Types de dégâts ──────────────────────────────────────────
typedef enum {
    DMG_PHYSICAL = 0,
    DMG_POISON,
    DMG_ELECTRIC,
    DMG_CRYO,
    DMG_NANO,
    DMG_FIRE,         // flammes (lance-flammes)
} DamageType;

// ── Dépôt de matériau sur la carte ──────────────────────────
typedef struct {
    int          tile_x, tile_y;  // position sur la carte
    MaterialType type;
    int          active;          // 1 = disponible, 0 = collecté
    int          spawn_wave;      // 0 = dispo dès le début, >0 = s'active à la vague N
} MaterialDeposit;

// ── Matériau lâché sur le sol (ouvrier tué en portant) ───────
#define MAX_DROPPED_MATS 8
typedef struct {
    float        x, y;
    MaterialType type;
    int          active;
    float        lifetime;   // disparaît après N secondes
} DroppedMat;

// ── Noms et couleurs ─────────────────────────────────────────
extern const char  *MATERIAL_NAMES [MAT_COUNT];
extern const char  *MATERIAL_DESC  [MAT_COUNT];   // courte (HUD)
extern const char  *MATERIAL_LORE  [MAT_COUNT];   // longue (bestiaire)
extern const char  *DAMAGE_NAMES   [6];