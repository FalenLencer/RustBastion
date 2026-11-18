/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_anim_poses.c ─ Bibliothèque de poses de l'ouvrier de l'écran titre
 *  (cf. menu_anim_worker.h). Extraite de menu_anim_worker.c (règle < 500 l.) :
 *  ici le « jeu d'acteur » (positions clés), là-bas la peinture.
 *
 *  Repère : px d'art, y vers le HAUT, origine au sol sous le personnage,
 *  tourné vers +x. Les mains et les pieds sont des CIBLES : l'IK du peintre
 *  place coudes et genoux.
 */
#include "menu_anim_worker.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define WK_ANKLE      2.2f      /* hauteur de cheville, pied au sol        */
#define WK_STRIDE     5.5f      /* demi-foulée                             */
#define WK_STEP_H     3.0f      /* levée du pied en marche                 */
#define WK_HIP_WALK  21.6f      /* hauteur du bassin en marche             */
#define WK_STEP_REF   5.0f      /* écart de pied (px d'art) = pas complet  */

static Vector2 V(float x, float y) { return (Vector2){x, y}; }

WorkerPose worker_pose_lerp(const WorkerPose *a, const WorkerPose *b, float k) {
    WorkerPose o;
    o.hip    = Vector2Lerp(a->hip, b->hip, k);
    o.torso  = a->torso + (b->torso - a->torso) * k;
    o.head   = a->head  + (b->head  - a->head)  * k;
    o.foot_n = Vector2Lerp(a->foot_n, b->foot_n, k);
    o.foot_f = Vector2Lerp(a->foot_f, b->foot_f, k);
    o.hand_n = Vector2Lerp(a->hand_n, b->hand_n, k);
    o.hand_f = Vector2Lerp(a->hand_f, b->hand_f, k);
    o.crate  = Vector2Lerp(a->crate, b->crate, k);
    o.crate_rot = a->crate_rot + (b->crate_rot - a->crate_rot) * k;
    o.carry  = (k < 0.5f) ? a->carry : b->carry;
    /* Un pied qui change de place fait un PAS (il se lève), il ne glisse pas. */
    float lift = sinf(PI * k) * WK_STEP_H;
    o.foot_n.y += lift * fminf(1.0f, fabsf(b->foot_n.x - a->foot_n.x) / WK_STEP_REF);
    o.foot_f.y += lift * fminf(1.0f, fabsf(b->foot_f.x - a->foot_f.x) / WK_STEP_REF);
    return o;
}

/* Pied au cycle u : appui (recule linéairement, collé au sol) puis
   balancier (revient vers l'avant en se levant). */
static Vector2 foot_at(float u) {
    if (u < 0.5f) return V(WK_STRIDE - 4.0f * WK_STRIDE * u, WK_ANKLE);
    float k = (u - 0.5f) * 2.0f, s = k * k * (3.0f - 2.0f * k);
    return V(-WK_STRIDE + 2.0f * WK_STRIDE * s, WK_ANKLE + WK_STEP_H * sinf(PI * k));
}

float worker_walk_stride(void) { return 4.0f * WK_STRIDE; }

WorkerPose worker_pose_walk(float cycle) {
    WorkerPose p;
    memset(&p, 0, sizeof p);
    float u = cycle - floorf(cycle);
    float b = cosf(cycle * 4.0f * PI);                     // 1 au contact
    p.foot_n = foot_at(u);
    p.foot_f = foot_at(fmodf(u + 0.5f, 1.0f));
    p.hip    = V(1.0f, WK_HIP_WALK - 0.7f * b);
    p.torso  = 0.10f + 0.03f * b;                          // penché sous la charge
    p.head   = -0.04f;
    Vector2 S = worker_shoulder(&p);
    p.crate     = Vector2Add(S, V(-3.0f, 5.6f + 0.4f * b));   // caisse sur l'épaule
    p.crate_rot = 0.10f + 0.05f * b;
    p.carry     = 1.0f;
    p.hand_n    = Vector2Add(S, V(2.6f, 1.6f + 0.3f * b));
    p.hand_f    = Vector2Add(S, V(-0.5f + 0.55f * p.foot_n.x, -14.0f));   // bras qui balance
    return p;
}

WorkerPose worker_pose_windup(void) {
    WorkerPose p;
    memset(&p, 0, sizeof p);
    p.foot_n = V(7.0f, WK_ANKLE);  p.foot_f = V(-6.5f, WK_ANKLE);
    p.hip    = V(-1.8f, 19.4f);    p.torso  = -0.34f;  p.head = 0.30f;
    Vector2 S = worker_shoulder(&p);
    p.crate  = Vector2Add(S, V(-7.0f, 4.0f));  p.crate_rot = -0.75f;  p.carry = 1.0f;
    p.hand_n = Vector2Add(p.crate, V(2.6f, -3.0f));
    p.hand_f = Vector2Add(p.crate, V(-1.0f, -4.0f));
    return p;
}

WorkerPose worker_pose_release(void) {
    WorkerPose p;
    memset(&p, 0, sizeof p);
    p.foot_n = V(7.5f, WK_ANKLE);  p.foot_f = V(-6.0f, 2.8f);
    p.hip    = V(1.8f, 20.6f);     p.torso  = 0.28f;  p.head = -0.05f;
    Vector2 S = worker_shoulder(&p);
    p.crate  = Vector2Add(S, V(5.0f, 9.5f));  p.crate_rot = 0.55f;  p.carry = 1.0f;
    p.hand_n = Vector2Add(p.crate, V(1.0f, -4.2f));
    p.hand_f = Vector2Add(p.crate, V(-2.6f, -3.6f));
    return p;
}

WorkerPose worker_pose_follow(void) {
    WorkerPose p;
    memset(&p, 0, sizeof p);
    p.foot_n = V(8.0f, WK_ANKLE);  p.foot_f = V(-8.5f, 5.5f);     // pied arrière décollé
    p.hip    = V(3.6f, 19.8f);     p.torso  = 0.58f;  p.head = -0.35f;
    Vector2 S = worker_shoulder(&p);
    p.crate  = S;
    p.hand_n = Vector2Add(S, V(9.5f, -1.5f));
    p.hand_f = Vector2Add(S, V(8.0f, -4.0f));
    return p;
}

WorkerPose worker_pose_flinch(void) {
    WorkerPose p;
    memset(&p, 0, sizeof p);
    p.foot_n = V(5.0f, WK_ANKLE);  p.foot_f = V(-5.0f, WK_ANKLE);
    p.hip    = V(-1.2f, 21.0f);    p.torso  = -0.14f;  p.head = 0.22f;
    Vector2 S = worker_shoulder(&p);
    p.crate  = S;
    p.hand_n = Vector2Add(S, V(4.5f, 4.0f));                      // bras devant le visage
    p.hand_f = Vector2Add(S, V(-3.0f, -12.5f));
    return p;
}

WorkerPose worker_pose_idle(float t, float clap, float sep) {
    WorkerPose p;
    memset(&p, 0, sizeof p);
    float br = sinf(t * 2.6f);                                   // respiration
    p.foot_n = V(4.2f, WK_ANKLE);  p.foot_f = V(-3.8f, WK_ANKLE);
    p.hip    = V(0.2f, 22.0f + 0.35f * br);
    p.torso  = -0.03f + 0.015f * br;
    p.head   = -0.10f;                                           // menton levé, fier
    Vector2 S  = worker_shoulder(&p);
    Vector2 hn = Vector2Add(p.hip, V(3.0f, 2.2f)), hf = Vector2Add(p.hip, V(-3.4f, 2.4f));  // poings aux hanches
    Vector2 cc = Vector2Add(S, V(6.0f, -5.5f));                                            // devant la poitrine
    p.crate  = S;
    p.hand_n = Vector2Lerp(hn, Vector2Add(cc, V(sep * 0.5f, 0.0f)), clap);
    p.hand_f = Vector2Lerp(hf, Vector2Add(cc, V(-sep * 0.5f, 0.4f)), clap);
    return p;
}
