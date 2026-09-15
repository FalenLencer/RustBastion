/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include <math.h>

/* Distance euclidienne au carré (évite le sqrtf quand seule la comparaison importe). */
static inline float gdist2(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return dx*dx + dy*dy;
}

/* Distance euclidienne réelle. */
static inline float gdist(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return sqrtf(dx*dx + dy*dy);
}

/* Fait tourner `cur` vers `tgt` d'au plus `max_d` rad, par le chemin le
   plus court. SOURCE UNIQUE (était dupliqué dans tower.c et les trois
   modules render3d) : visée des tours + lissage des caps 2D/3D. */
static inline float angle_approach(float cur, float tgt, float max_d) {
    float d = tgt - cur;
    while (d >  3.14159265f) d -= 6.28318531f;
    while (d < -3.14159265f) d += 6.28318531f;
    if (d >  max_d) d =  max_d;
    if (d < -max_d) d = -max_d;
    return cur + d;
}
