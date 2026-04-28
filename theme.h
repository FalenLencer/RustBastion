#pragma once
#include "raylib.h"

typedef enum {
    THEME_WASTELAND = 0,  // terres dévastées (défaut)
    THEME_SWAMP,          // marais toxique
    THEME_DESERT,         // désert irradié
    THEME_CITY,           // ville en ruine
    THEME_FACTORY,        // usine abandonnée
    THEME_COUNT
} ThemeID;

// Palette de couleurs d'un thème
typedef struct {
    Color ground_fill,  ground_stroke;
    Color ruin_fill,    ruin_stroke;
    Color water_fill,   water_stroke;
    Color path_fill,    path_stroke;
    Color spawn_fill,   spawn_stroke;
    Color base_fill,    base_stroke;
    Color bg;           // couleur de fond fenêtre
} ThemePalette;

// Seuils de bruit de Perlin (contrôle la proportion eau/ruine/terrain)
typedef struct {
    float water_thresh;  // en dessous → eau/obstacle
    float ruin_thresh;   // entre water et ruin → ruine
    // au dessus → terrain normal
} ThemeNoise;

// Données gameplay liées au thème
typedef struct {
    ThemeID      id;          // ← AJOUT : identifiant enum (correctif bug wave.c)
    const char  *name;
    const char  *description;
    const char  *water_name;   // nom affiché pour l'obstacle
    const char  *ruin_name;    // nom affiché pour les ruines
    const char  *ground_name;

    ThemePalette palette;
    ThemeNoise   noise;

    // Modificateurs gameplay
    float enemy_speed_mult;   // ex: marais = 0.7 (ennemis plus lents)
    float tower_range_mult;   // ex: désert = 1.3 (meilleure visibilité)
    float gold_mult;          // ex: usine = 1.2 (plus de ressources)
    int   max_paths;          // ex: ville = 3 (rues multiples)
} Theme;

// Accès global
const Theme *theme_get(ThemeID id);
ThemeID      theme_random(int seed);  // choisit un thème selon le seed