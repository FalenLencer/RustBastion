#include "theme.h"
#include <stdlib.h>

static const Theme THEMES[THEME_COUNT] = {

    // ── WASTELAND ─────────────────────────────────────────────
    [THEME_WASTELAND] = {
        .id          = THEME_WASTELAND,          // ← AJOUT
        .name        = "Terres devastees",
        .description = "Cendres et beton. Le monde d'avant n'est plus.",
        .water_name  = "Flaques toxiques",
        .ruin_name   = "Ruines beton",
        .ground_name = "Terre brulee",
        .palette = {
            .ground_fill   = {45,  30,  10,  255},
            .ground_stroke = {61,  42,  16,  255},
            .ruin_fill     = {74,  53,  32,  255},
            .ruin_stroke   = {107, 76,  40,  255},
            .water_fill    = {13,  43,  26,  255},
            .water_stroke  = {26,  74,  46,  255},
            .path_fill     = {92,  61,  0,   255},
            .path_stroke   = {139, 94,  0,   255},
            .spawn_fill    = {107, 16,  16,  255},
            .spawn_stroke  = {192, 57,  43,  255},
            .base_fill     = {13,  61,  26,  255},
            .base_stroke   = {39,  174, 96,  255},
            .bg            = {15,  10,  5,   255},
        },
        .noise = { .water_thresh = 0.25f, .ruin_thresh = 0.48f },
        .enemy_speed_mult = 1.0f,
        .tower_range_mult = 1.0f,
        .gold_mult        = 1.0f,
        .max_paths        = 2,
    },

    // ── SWAMP ─────────────────────────────────────────────────
    [THEME_SWAMP] = {
        .id          = THEME_SWAMP,              // ← AJOUT
        .name        = "Marais toxique",
        .description = "Vegetation mutante, brume verte, eau stagnante.",
        .water_name  = "Marecages",
        .ruin_name   = "Vegetation dense",
        .ground_name = "Boue compacte",
        .palette = {
            .ground_fill   = {26,  46,  26,  255},
            .ground_stroke = {42,  74,  42,  255},
            .ruin_fill     = {42,  61,  32,  255},
            .ruin_stroke   = {61,  92,  48,  255},
            .water_fill    = {13,  61,  13,  255},
            .water_stroke  = {26,  107, 26,  255},
            .path_fill     = {26,  61,  26,  255},
            .path_stroke   = {46,  204, 113, 255},
            .spawn_fill    = {107, 16,  16,  255},
            .spawn_stroke  = {192, 57,  43,  255},
            .base_fill     = {13,  61,  26,  255},
            .base_stroke   = {39,  174, 96,  255},
            .bg            = {8,   18,  8,   255},
        },
        .noise = { .water_thresh = 0.35f, .ruin_thresh = 0.55f },
        .enemy_speed_mult = 0.70f,
        .tower_range_mult = 0.85f,
        .gold_mult        = 1.10f,
        .max_paths        = 3,
    },

    // ── DESERT ────────────────────────────────────────────────
    [THEME_DESERT] = {
        .id          = THEME_DESERT,             // ← AJOUT
        .name        = "Desert irradie",
        .description = "Sable orange, epaves calcinees, dunes de cendres.",
        .water_name  = "Sables mouvants",
        .ruin_name   = "Epaves calcinees",
        .ground_name = "Sable irradie",
        .palette = {
            .ground_fill   = {74,  48,  16,  255},
            .ground_stroke = {107, 74,  32,  255},
            .ruin_fill     = {92,  61,  26,  255},
            .ruin_stroke   = {139, 94,  42,  255},
            .water_fill    = {61,  40,  0,   255},
            .water_stroke  = {107, 69,  0,   255},
            .path_fill     = {122, 92,  32,  255},
            .path_stroke   = {196, 150, 42,  255},
            .spawn_fill    = {107, 16,  16,  255},
            .spawn_stroke  = {192, 57,  43,  255},
            .base_fill     = {13,  61,  26,  255},
            .base_stroke   = {39,  174, 96,  255},
            .bg            = {30,  18,  5,   255},
        },
        .noise = { .water_thresh = 0.20f, .ruin_thresh = 0.42f },
        .enemy_speed_mult = 1.30f,
        .tower_range_mult = 1.30f,
        .gold_mult        = 0.90f,
        .max_paths        = 2,
    },

    // ── CITY ──────────────────────────────────────────────────
    [THEME_CITY] = {
        .id          = THEME_CITY,               // ← AJOUT
        .name        = "Ville en ruine",
        .description = "Blocs de beton, rues effondrees, gratte-ciels eventres.",
        .water_name  = "Inondations",
        .ruin_name   = "Decombres urbains",
        .ground_name = "Bitume fissure",
        .palette = {
            .ground_fill   = {42,  42,  42,  255},
            .ground_stroke = {61,  61,  61,  255},
            .ruin_fill     = {61,  61,  61,  255},
            .ruin_stroke   = {92,  92,  92,  255},
            .water_fill    = {26,  26,  46,  255},
            .water_stroke  = {42,  42,  74,  255},
            .path_fill     = {26,  26,  26,  255},
            .path_stroke   = {74,  74,  74,  255},
            .spawn_fill    = {107, 16,  16,  255},
            .spawn_stroke  = {192, 57,  43,  255},
            .base_fill     = {13,  61,  26,  255},
            .base_stroke   = {39,  174, 96,  255},
            .bg            = {10,  10,  10,  255},
        },
        .noise = { .water_thresh = 0.22f, .ruin_thresh = 0.50f },
        .enemy_speed_mult = 1.10f,
        .tower_range_mult = 0.90f,
        .gold_mult        = 1.20f,
        .max_paths        = 3,
    },

    // ── FACTORY ───────────────────────────────────────────────
    [THEME_FACTORY] = {
        .id          = THEME_FACTORY,            // ← AJOUT
        .name        = "Usine abandonnee",
        .description = "Convoyeurs rouilles, cuves chimiques, robots defectueux.",
        .water_name  = "Dechets chimiques",
        .ruin_name   = "Machines rouillees",
        .ground_name = "Sol industriel",
        .palette = {
            .ground_fill   = {26,  18,  16,  255},
            .ground_stroke = {45,  30,  26,  255},
            .ruin_fill     = {45,  26,  16,  255},
            .ruin_stroke   = {74,  42,  26,  255},
            .water_fill    = {13,  26,  13,  255},
            .water_stroke  = {26,  61,  26,  255},
            .path_fill     = {42,  26,  10,  255},
            .path_stroke   = {92,  58,  26,  255},
            .spawn_fill    = {107, 16,  16,  255},
            .spawn_stroke  = {192, 57,  43,  255},
            .base_fill     = {13,  61,  26,  255},
            .base_stroke   = {39,  174, 96,  255},
            .bg            = {8,   5,   3,   255},
        },
        .noise = { .water_thresh = 0.23f, .ruin_thresh = 0.46f },
        .enemy_speed_mult = 1.20f,
        .tower_range_mult = 1.10f,
        .gold_mult        = 1.30f,
        .max_paths        = 2,
    },
};

const Theme *theme_get(ThemeID id) {
    if (id < 0 || id >= THEME_COUNT) return &THEMES[THEME_WASTELAND];
    return &THEMES[id];
}

ThemeID theme_random(int seed) {
    srand((unsigned)seed);
    return (ThemeID)(rand() % THEME_COUNT);
}