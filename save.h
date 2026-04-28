#pragma once
#include "game_state.h"

// ── Constantes ───────────────────────────────────────────────
#define SAVE_SLOT_COUNT   3
#define SAVE_MAGIC        0x52425356u   // "RBSV"
#define SAVE_VERSION      1
#define SAVE_FILE_PREFIX  "rustbastion_slot"   // → rustbastion_slot0.sav ...

// ── Résumé d'un slot (affiché dans le menu sans charger tout) ─
typedef struct {
    int   exists;           // 1 = fichier valide présent
    int   slot;             // index 0-2
    int   seed;             // seed de la carte
    int   theme;            // ThemeID
    int   wave;             // vague atteinte
    int   gold;
    int   lives;
    char  theme_name[32];   // copie du nom pour affichage
} SaveInfo;

// ── API ──────────────────────────────────────────────────────

// Sauvegarde l'état complet dans le slot donné (0-2)
// Retourne 1 si succès, 0 si erreur
int  save_write(const GameState *gs, int slot);

// Charge un slot dans gs. Retourne 1 si succès
int  save_read (GameState *gs, int slot);

// Lit juste le résumé (pour affichage menu), sans charger tout
int  save_info (int slot, SaveInfo *out);

// Efface un slot
void save_delete(int slot);

// Remplit un tableau de SAVE_SLOT_COUNT infos (appelle save_info pour chaque slot)
void save_scan(SaveInfo infos[SAVE_SLOT_COUNT]);