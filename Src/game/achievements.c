/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/* ════════════════════════════════════════════════════════════════
   achievements.c — Succès + méta-déblocages (P1.3)
   Voir achievements.h pour le contrat. Fichier de sauvegarde SÉPARÉ
   du méta, format tolérant aux ajouts futurs de succès.
   ════════════════════════════════════════════════════════════════ */
#include "achievements.h"
#include "game_state.h"
#include "meta.h"
#include "../engine/paths.h"
#include "../ui/ui_palette.h"
#include <stdio.h>
#include <string.h>

/* ── Catalogue ──────────────────────────────────────────────────── */
const AchDef ACH_DEFS[ACH_COUNT] = {
    [ACH_FIRST_BLOOD]   = { "Premier sang",     "Detruire votre premier ennemi",          10 },
    [ACH_KILLS_100]     = { "Exterminateur",    "Detruire 100 ennemis en une partie",     20 },
    [ACH_WAVE_10]       = { "Tenace",           "Atteindre la vague 10",                  15 },
    [ACH_WAVE_20]       = { "Inebranlable",     "Atteindre la vague 20",                  30 },
    [ACH_TOWERS_8]      = { "Architecte",       "Avoir 8 tours en meme temps",            15 },
    [ACH_ALL_TOWERS]    = { "Arsenal complet",  "Les 4 types de tours dans une partie",   20 },
    [ACH_RICH]          = { "Fortune de guerre","Amasser 1000 or en une partie",          20 },
    [ACH_FIRST_ACT]     = { "Premiers pas",     "Campagne : completer un acte",           15 },
    [ACH_PERFECT_ACT]   = { "Sans faute",       "Campagne : reussir un objectif bonus",   25 },
    [ACH_CAMPAIGN_DONE] = { "Conquerant",       "Terminer une campagne complete",         60 },
    [ACH_BOSS_DOWN]     = { "Tueur de titans",  "Vaincre un boss de chapitre",            30 },
    [ACH_HERO_MODE]     = { "Sur le terrain",   "Jouer une partie en mode Heros",         15 },
    [ACH_MULTI]         = { "Camaraderie",      "Jouer une partie multijoueur",           15 },
    [ACH_ENDLESS_15]    = { "Marathon",         "Endless : atteindre la vague 15",        25 },
};

/* ── Seuils des conditions pollables ────────────────────────────── */
#define ACH_KILLS_TARGET    100    /* ACH_KILLS_100                   */
#define ACH_WAVE_A           10    /* ACH_WAVE_10                     */
#define ACH_WAVE_B           20    /* ACH_WAVE_20                     */
#define ACH_TOWERS_TARGET     8    /* ACH_TOWERS_8                    */
#define ACH_GOLD_TARGET    1000    /* ACH_RICH                        */
#define ACH_ENDLESS_WAVE     15    /* ACH_ENDLESS_15                  */

/* ── Persistance ────────────────────────────────────────────────── */
#define ACH_MAGIC   0x52424143u    /* "RBAC" */
#define ACH_VERSION 1
#define ACH_FILE    "saves/rustbastion_achievements.sav"

static int g_ach[ACH_COUNT];
static int g_ach_loaded = 0;

static void ach_load(void) {
    if (g_ach_loaded) return;
    g_ach_loaded = 1;
    memset(g_ach, 0, sizeof(g_ach));

    char path[512];
    FILE *f = fopen(data_path(path, sizeof(path), ACH_FILE), "rb");
    if (!f) return;
    unsigned int magic = 0;
    int version = 0, count = 0;
    if (fread(&magic,   sizeof(magic),   1, f) == 1 &&
        fread(&version, sizeof(version), 1, f) == 1 &&
        fread(&count,   sizeof(count),   1, f) == 1 &&
        magic == ACH_MAGIC && version == ACH_VERSION &&
        count > 0 && count <= 256) {
        /* Tolerant : lit min(count, ACH_COUNT) — un vieux fichier avec
           moins de succes garde ses deblocages, le reste part a zero. */
        int n = count < ACH_COUNT ? count : ACH_COUNT;
        for (int i = 0; i < n; i++) {
            int v = 0;
            if (fread(&v, sizeof(v), 1, f) != 1) break;
            g_ach[i] = v ? 1 : 0;
        }
    }
    fclose(f);
}

static void ach_save(void) {
    data_mkdir("saves");
    char path[512];
    FILE *f = fopen(data_path(path, sizeof(path), ACH_FILE), "wb");
    if (!f) return;
    unsigned int magic = ACH_MAGIC;
    int version = ACH_VERSION, count = ACH_COUNT;
    fwrite(&magic,   sizeof(magic),   1, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&count,   sizeof(count),   1, f);
    fwrite(g_ach,    sizeof(int), ACH_COUNT, f);
    fclose(f);
}

/* ── API ────────────────────────────────────────────────────────── */
int ach_unlocked(int id) {
    ach_load();
    if (id < 0 || id >= ACH_COUNT) return 0;
    return g_ach[id];
}

int ach_unlocked_count(void) {
    ach_load();
    int n = 0;
    for (int i = 0; i < ACH_COUNT; i++)
        if (g_ach[i]) n++;
    return n;
}

void ach_unlock(int id, struct GameState *gs) {
    ach_load();
    if (id < 0 || id >= ACH_COUNT) return;
    if (g_ach[id]) return;
    g_ach[id] = 1;
    ach_save();

    /* Meta-deblocage : la recompense alimente la boutique existante. */
    if (gs) {
        gs->meta.scrap              += ACH_DEFS[id].scrap;
        gs->meta.total_scrap_earned += ACH_DEFS[id].scrap;
        meta_save(&gs->meta);
        char buf[52];
        snprintf(buf, sizeof(buf), "SUCCES : %s (+%d ferraille)",
                 ACH_DEFS[id].name, ACH_DEFS[id].scrap);
        ui_push_notif(&gs->ui, buf, UI_ACCENT);
    }
}

/* ── Conditions pollables ───────────────────────────────────────── */
void ach_poll_game(struct GameState *gs) {
    ach_load();

    if (gs->kills >= 1)                 ach_unlock(ACH_FIRST_BLOOD, gs);
    if (gs->kills >= ACH_KILLS_TARGET)  ach_unlock(ACH_KILLS_100,   gs);
    if (gs->wave_manager.number >= ACH_WAVE_A) ach_unlock(ACH_WAVE_10, gs);
    if (gs->wave_manager.number >= ACH_WAVE_B) ach_unlock(ACH_WAVE_20, gs);
    if (gs->towers.tower_count >= ACH_TOWERS_TARGET)
        ach_unlock(ACH_TOWERS_8, gs);
    if (gs->gold >= ACH_GOLD_TARGET)    ach_unlock(ACH_RICH, gs);
    if (gs->is_endless && gs->wave_manager.number >= ACH_ENDLESS_WAVE)
        ach_unlock(ACH_ENDLESS_15, gs);

    /* Les 4 types de tours poses simultanement (evite le scan si deja eu) */
    if (!g_ach[ACH_ALL_TOWERS]) {
        int seen[TOWER_TYPE_COUNT] = {0};
        for (int i = 0; i < MAX_TOWERS; i++) {
            const Tower *t = &gs->towers.towers[i];
            if (t->active && (int)t->type >= 0 && (int)t->type < TOWER_TYPE_COUNT)
                seen[(int)t->type] = 1;
        }
        int all = 1;
        for (int k = 0; k < TOWER_TYPE_COUNT; k++)
            if (!seen[k]) { all = 0; break; }
        if (all) ach_unlock(ACH_ALL_TOWERS, gs);
    }

    /* Campagne : lues depuis le meta (mis a jour par meta_record_act) */
    if (!g_ach[ACH_FIRST_ACT] || !g_ach[ACH_PERFECT_ACT]) {
        for (int s = 0; s < CAMPAIGN_TOTAL; s++) {
            if (gs->meta.act_stars[s] >= 1) ach_unlock(ACH_FIRST_ACT,   gs);
            if (gs->meta.act_stars[s] >= 2) ach_unlock(ACH_PERFECT_ACT, gs);
        }
    }
    if (gs->meta.campaigns_completed >= 1) ach_unlock(ACH_CAMPAIGN_DONE, gs);

    /* Boss vaincu : le flag dead reste visible en fin de frame quand le
       boss tombe sous les tours (le nettoyage a lieu au enemy_pool_update
       SUIVANT). Les kills directs du heros passent par le hook explicite
       de hero_actions.c (dead traite dans la meme frame). */
    if (!g_ach[ACH_BOSS_DOWN]) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            const Enemy *e = &gs->enemies.enemies[i];
            if (e->active && e->is_boss && e->dead) {
                ach_unlock(ACH_BOSS_DOWN, gs);
                break;
            }
        }
    }
}
