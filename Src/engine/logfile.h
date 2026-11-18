/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  logfile.h ─ Journal de session : config/rustbastion.log.
 *
 *  Tout ce que raylib et le jeu tracent (TraceLog) est aussi écrit dans ce
 *  fichier, réécrit à chaque lancement : un joueur qui signale un bug peut
 *  l'envoyer (version, carte graphique, assets manquants, sauvegardes…).
 *  Chaque ligne est vidée sur disque aussitôt (rien de perdu si le jeu
 *  plante) ; le fichier est plafonné pour qu'une erreur répétée à chaque
 *  frame ne remplisse pas le disque.
 */

/* À appeler après setup_working_dir() et AVANT InitWindow (capture les
   messages d'initialisation : pilote OpenGL, résolution, audio). */
void logfile_init(void);
/* Ferme le journal (après CloseWindow). */
void logfile_close(void);
