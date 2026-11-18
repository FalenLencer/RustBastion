/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  version.h ─ Version du jeu + tampon de compilation (SOURCE UNIQUE).
 *
 *  Le Makefile injecte la date/heure de compilation et la révision git
 *  (RB_BUILD_STAMP, RB_BUILD_REV) : on reconnaît d'un coup d'œil, sur
 *  l'écran titre, en pause ou dans le journal, un exécutable de release
 *  en retard sur les sources. Build hors Makefile (sondes) : « dev ».
 */
#define GAME_VERSION "v0.1"
#ifndef RB_BUILD_STAMP
#define RB_BUILD_STAMP "dev"
#endif
#ifndef RB_BUILD_REV
#define RB_BUILD_REV "?"
#endif
/* « v0.1 · 2026-11-18 15:02 · 5e76ccf » */
#define GAME_VERSION_FULL GAME_VERSION " · " RB_BUILD_STAMP " · " RB_BUILD_REV
