/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "assets.h"
#include <string.h>

// ════════════════════════════════════════════════════
// TABLEAUX GLOBAUX
// ════════════════════════════════════════════════════
Texture2D g_tower_splash[TOWER_TYPE_COUNT];
Texture2D g_unit_splash [UNIT_TYPE_COUNT];
Font      g_font;          // police étendue — initialisée dans assets_load()

// ════════════════════════════════════════════════════
// HELPER — charge une texture ; retourne {0} si absent
// Les textures sont toujours relatives au CWD (racine du projet),
// pas au g_data_prefix (qui est réservé aux saves/config).
// ════════════════════════════════════════════════════
static Texture2D load_safe(const char *path) {
    if (!FileExists(path)) return (Texture2D){0};
    Texture2D t = LoadTexture(path);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    return t;
}

// ════════════════════════════════════════════════════
// CHARGEMENT
// ════════════════════════════════════════════════════
void assets_load(void) {
    memset(g_tower_splash, 0, sizeof(g_tower_splash));
    memset(g_unit_splash,  0, sizeof(g_unit_splash));

    // ── Police étendue — codepoints ASCII + Latin-1 complet + symboles ─────
    // Priorité : Rajdhani-Bold (couverture complète) → GROBOLD → défaut Raylib
    {
        int cps[512];
        int n = 0;
        for (int c = 32;    c <= 126;  c++) cps[n++] = c;  // ASCII imprimable (32-126)
        for (int c = 0xA0;  c <= 0xFF; c++) cps[n++] = c;  // Latin-1 Supplément COMPLET
                                                             //  (accents FR, ×, °, ±…)
        cps[n++] = 0x2013;  // –  tiret demi-cadratin
        cps[n++] = 0x2014;  // —  tiret cadratin
        cps[n++] = 0x2019;  // '  apostrophe typographique
        cps[n++] = 0x2026;  // …  points de suspension
        cps[n++] = 0x2713;  // ✓  coche
        cps[n++] = 0x25B2;  // ▲  triangle haut
        cps[n++] = 0x25BC;  // ▼  triangle bas
        cps[n++] = 0x25C4;  // ◄  flèche gauche
        cps[n++] = 0x25BA;  // ►  flèche droite
        cps[n++] = 0x2665;  // ♥  cœur (vies)
        cps[n++] = 0x2605;  // ★  étoile pleine
        cps[n++] = 0x2606;  // ☆  étoile vide

        // Essaie Rajdhani-Bold en premier (couverture complète, thème militaire)
        const char *font_main    = "assets/fonts/Rajdhani/Rajdhani-Bold.ttf";
        const char *font_fallback = "assets/fonts/GROBOLD.ttf";
        const char *font_path = FileExists(font_main) ? font_main : font_fallback;

        if (FileExists(font_path))
            g_font = LoadFontEx(font_path, 72, cps, n);  // 72 px → qualité optimale pour FONT_SCALE=1.4 (fs max ≈ 28*1.4≈39)
        else
            g_font = GetFontDefault();
        SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
    }

    // ── Tours ────────────────────────────────────────────────────
    // TOWER_GUN n'a pas de splash dédié → on réutilise tower_base
    g_tower_splash[TOWER_GUN]    = load_safe("assets/textures/splash_art/tower_base.png");
    g_tower_splash[TOWER_SNIPER] = load_safe("assets/textures/splash_art/tower_sniper.png");
    g_tower_splash[TOWER_FLAME]  = load_safe("assets/textures/splash_art/tower_flame.png");
    g_tower_splash[TOWER_TESLA]  = load_safe("assets/textures/splash_art/tower_tesla.png");

    // ── Unités ───────────────────────────────────────────────────
    g_unit_splash[UNIT_SOLDIER] = load_safe("assets/textures/splash_art/soldat_splash_art.png");
    g_unit_splash[UNIT_HEAVY]   = load_safe("assets/textures/splash_art/lourd_splash_art.png");
    g_unit_splash[UNIT_MEDIC]   = load_safe("assets/textures/splash_art/medic_splash_art.png");
    g_unit_splash[UNIT_DOG]     = load_safe("assets/textures/splash_art/dog_splash_art.png");
    // UNIT_WORKER — pas de splash art pour l'instant → reste {0}
}

// ════════════════════════════════════════════════════
// LIBÉRATION
// ════════════════════════════════════════════════════
void assets_unload(void) {
    for (int i = 0; i < TOWER_TYPE_COUNT; i++)
        if (g_tower_splash[i].id != 0) UnloadTexture(g_tower_splash[i]);
    for (int i = 0; i < UNIT_TYPE_COUNT; i++)
        if (g_unit_splash[i].id != 0)  UnloadTexture(g_unit_splash[i]);
    if (g_font.texture.id != 0) UnloadFont(g_font);
}
