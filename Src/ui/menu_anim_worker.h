/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  menu_anim_worker.h ─ L'ouvrier de l'écran titre, en PIXEL ART articulé.
 *
 *  Un squelette 2D (bassin, buste, tête, bras et jambes en IK à deux os)
 *  est REPEINT à chaque frame dans une petite image (1 pixel d'art =
 *  WK_PX px virtuels, filtre « nearest ») : aplats ombrés, liseré de
 *  contre-jour chaud, contour sombre d'1 px — le grain des sprites du jeu,
 *  sans pixels tournés ni flous quelle que soit la pose.
 *
 *  Repère d'une pose : PIXELS D'ART, y vers le HAUT, origine = point au sol
 *  sous le personnage, personnage tourné vers +x.
 */
#include "raylib.h"

#define WK_PX  3.0f   /* px virtuels par pixel d'art (grain du personnage) */

typedef struct {
    Vector2 hip;              /* bassin                                        */
    float   torso;            /* inclinaison du buste (rad, + = vers l'avant)  */
    float   head;             /* inclinaison de la tête (rad, + = vers le bas) */
    Vector2 foot_n, foot_f;   /* chevilles : jambe proche / jambe lointaine    */
    Vector2 hand_n, hand_f;   /* mains (cibles de l'IK des bras)               */
    Vector2 crate;            /* centre de la caisse-tour tenue                */
    float   crate_rot;        /* rotation de la caisse (rad, sens trigo)       */
    float   carry;            /* > 0,5 : la caisse est en main (dessinée)      */
} WorkerPose;

void worker_art_init  (void);
void worker_art_unload(void);

/* Repeint la pose et la dessine, point au sol en (feet_x, feet_y) (px
   virtuels). `wind` anime l'écharpe (temps en s). */
void worker_art_draw(const WorkerPose *p, float feet_x, float feet_y, float wind);
/* Caisse-tour seule (en vol / à l'impact), centre en (cx, cy) px virtuels.
   squash 0..1 : écrasement à l'atterrissage (base posée). */
void worker_art_draw_crate(float cx, float cy, float rot, float squash);
/* Demi-hauteur de la caisse (px d'art) : pose au sol exacte. */
float worker_crate_half_h(void);

/* Épaule (px d'art, repère pose) — point d'attache de la caisse portée. */
Vector2 worker_shoulder(const WorkerPose *p);

/* Interpolation de poses (k 0..1) ; un pied qui se déplace est soulevé. */
WorkerPose worker_pose_lerp(const WorkerPose *a, const WorkerPose *b, float k);

/* Bibliothèque de poses (ouvrier tourné vers la droite). */
WorkerPose worker_pose_walk   (float cycle);  /* marche, caisse sur l'épaule  */
float      worker_walk_stride (void);         /* px d'art parcourus / cycle   */
WorkerPose worker_pose_windup (void);         /* élan : caisse derrière la tête */
WorkerPose worker_pose_release(void);         /* lâcher : bras tendus en avant */
WorkerPose worker_pose_follow (void);         /* accompagnement du geste       */
WorkerPose worker_pose_flinch (void);         /* se protège de l'impact        */
/* Debout, mains sur les hanches (respiration t) ; clap 0..1 = s'époussette
   les mains (sep = écart des mains, 0 = jointes). */
WorkerPose worker_pose_idle   (float t, float clap, float sep);
