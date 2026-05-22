/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_internal.h ─ Constantes, types et helpers internes du HUD.
 *
 *  Ce fichier n'est PAS destiné à être inclus depuis l'extérieur.
 *  À inclure uniquement dans hud.c, hud_input.c et hud_render.c.
 *
 *  Contient :
 *    • Les dimensions des overlays (#define OVERLAY_*)
 *    • Les constantes de coût (#define BASE_REPAIR_RESTORE, SLOT_MAX_BUYS)
 *    • Le type ToolInfo et les déclarations extern des tableaux partagés
 *    • Les déclarations des fonctions partagées (définies dans hud.c)
 */
#pragma once

#include "hud.h"
#include "renderer.h"
#include "ui_utils.h"
#include "../engine/audio.h"
#include "../engine/assets.h"
#include "../game/meta.h"
#include "../map/pathfinding.h"
#include "../combat/material.h"
#include "../game/game_state.h"
#include "menu.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// ════════════════════════════════════════════════════
// DIMENSIONS DES OVERLAYS DÉPLAÇABLES
// ════════════════════════════════════════════════════
#define OVERLAY_W      164
#define OVERLAY_OV_P     6
#define OVERLAY_TL_H   (OVERLAY_OV_P + 17+3 + 14+3 + 14 + OVERLAY_OV_P)
#define OVERLAY_TR_H   (OVERLAY_OV_P + 17+3 + 14+3 + 14+3 + 9+5 + 14 + OVERLAY_OV_P)

// ════════════════════════════════════════════════════
// COÛTS DES ACHATS EN JEU
// ════════════════════════════════════════════════════
#define BASE_REPAIR_RESTORE   20   /* HP restaurés par réparation de base */
#define SLOT_MAX_BUYS          6   /* achats de slots max par partie       */

extern const int SLOT_TOWER_COSTS[SLOT_MAX_BUYS];
extern const int SLOT_UNIT_COSTS [SLOT_MAX_BUYS];

// ════════════════════════════════════════════════════
// TYPE ET DONNÉES DES OUTILS
// ════════════════════════════════════════════════════
typedef struct {
    const char *name;
    const char *shortname;
    const char *icon;
    const char *desc;
    int         cost;
    float       dmg;
    float       range;
    float       rate;
    int         is_unit;
} ToolInfo;

extern const ToolInfo  TOOL_INFO[TOOL_COUNT];
extern       Color     TOOL_COLORS[TOOL_COUNT];
extern const int       TOWER_UNLOCK_AT[TOWER_TYPE_COUNT];
extern const char     *TOWER_UNLOCK_ACT_NAME[TOWER_TYPE_COUNT];

// ════════════════════════════════════════════════════
// FONCTIONS PARTAGÉES — définies dans hud.c
// ════════════════════════════════════════════════════
Vector2 virt_mouse(void);
int     base_repair_cost(int n);
int     tool_is_unlocked(ToolID id, const GameState *gs);
void    disc_pop(UIState *ui);
