/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  render3d_tower_cfg.h ─ RÉGLAGES et SHADERS des tours 3D.
 *
 *  INTERNE à render3d.c (seul includeur) : données statiques (cadrage des
 *  caméras, pivots, bouches, durées d'animation, sources GLSL). Séparé du
 *  code pour garder render3d.c lisible ; toutes les molettes de réglage
 *  visuel des tours sont ici, au même endroit.
 */
#include "raylib.h"

/* ── Ancrage au sol + échelle COMMUNE des 4 tours ────────────────────
   Chaque modèle est cadré par sa caméra ortho dans sa RT ; au blit, on
   place la projection de l'ORIGINE du modèle (centre du pied, au sol) sur
   le centre de la case, et on met l'emprise au sol de son socle (boîte
   englobante) à R3D_FOOT_PX de large. Avant : 4 cadrages réglés à l'œil
   (la mitrailleuse paraissait 2× plus petite, les socles flottaient). */
#define R3D_FOOT_PX      34.0f   /* emprise au sol affichée (px monde ≈ 0,85 case) */
#define R3D_GROUND_DY    1.0f    /* pied un poil sous le centre de case (vue 3/4)  */
#define R3D_KINDS        4

/* ── Rendu « pixel art » : même grain que le décor ───────────────────
   La RT est rendue à R3D_RT_PER_PX pixel par px monde (0,5 = 1 pixel
   d'art pour 2 px, le grain du terrain et des ruines), agrandie SANS
   filtrage, puis cernée d'un contour sombre d'1 pixel d'art (comme
   pp_outline) au blit : la tour 3D parle la même langue que le décor. */
#define R3D_RT_PER_PX    0.5f
#define R3D_OUTLINE_COL  ((Color){16, 12, 10, 255})   /* = contour des ruines */
static const char *FS_OUTLINE =
"#version 330\n"
"in vec2 fragTexCoord;\n"
"in vec4 fragColor;\n"
"uniform sampler2D texture0;\n"
"uniform vec4 colDiffuse;\n"
"uniform vec2 texel;\n"
"uniform vec4 outlineCol;\n"
"uniform float flash;\n"          /* éclair de dégât : silhouette → blanc (0..1) */
"out vec4 finalColor;\n"
"void main(){\n"
"    vec4 c = texture(texture0, fragTexCoord);\n"
"    if (c.a >= 0.5) { finalColor = vec4(mix(c.rgb, vec3(1.0), flash), 1.0)\n"
"                                 * colDiffuse * fragColor; return; }\n"
"    float n = max(max(texture(texture0, fragTexCoord + vec2(texel.x, 0.0)).a,\n"
"                      texture(texture0, fragTexCoord - vec2(texel.x, 0.0)).a),\n"
"                  max(texture(texture0, fragTexCoord + vec2(0.0, texel.y)).a,\n"
"                      texture(texture0, fragTexCoord - vec2(0.0, texel.y)).a));\n"
"    if (n < 0.5) discard;\n"
"    finalColor = vec4(outlineCol.rgb, outlineCol.a * colDiffuse.a * fragColor.a);\n"
"}\n";

/* ── Veille : sans ennemi à portée, la tourelle balaie lentement ──── */
#define R3D_IDLE_SWEEP   0.55f   /* amplitude du balayage (rad)                    */
#define R3D_IDLE_FREQ    0.70f   /* vitesse du balayage (rad/s)                    */
#define R3D_ALERT_RATE   3.0f    /* bascule veille ↔ alerte (1/s)                  */

/* ── Réglages TOWER_GUN ──────────────────────────────────────────── */
#define R3D_RT_SIZE     168
#define R3D_ORTHO       4.7f
#define R3D_YAW_OFF     0.0f
#define R3D_YAW_PIVOT_Y 0.92f
#define R3D_GUN_HEIGHT  24.0f
#define R3D_PITCH_MAX   26.0f
#define R3D_RECOIL      0.18f       /* recul d'un canon (un peu plus visible)  */
#define R3D_RECOIL_DUR  0.14f       /* fenêtre recul+flash par balle émise      */
#define R3D_FLASH_DUR   0.05f
static const Vector3 CAM_POS    = { -3.6f, 3.0f, -4.0f };
static const Vector3 CAM_TGT    = {  0.0f, 1.00f, 0.0f };
static const Vector3 GUN_PIVOT  = {  0.0f, 0.557f, -0.58f };
static const Vector3 MUZZLE     = {  0.0f, 0.305f, -1.544f };
static const Vector3 RECOIL_DIR = {  0.0f, -0.105f, 0.995f };
static const float   BARREL_X[3] = { -0.40f, 0.0f, 0.40f };

/* ── Réglages TOWER_SNIPER ───────────────────────────────────────── */
#define SN_RT_W         224
#define SN_RT_H         304
#define SN_ORTHO        8.7f
#define SN_YAW_OFF      0.0f
#define SN_GUN_HEIGHT   40.0f          /* hauteur tube (px) pour l'élévation   */
#define SN_PITCH_MAX    22.0f
#define SN_PITCH_SIGN   (-1.0f)        /* bouche-haut (signe rot Z, à régler)  */
#define SN_RECOIL       0.24f
#define SN_RECOIL_DUR   0.14f
#define SN_FLASH_DUR    0.05f
static const Vector3 SN_CAM_POS     = { -6.0f, 8.2f, -7.0f };
static const Vector3 SN_CAM_TGT     = {  0.0f, 3.25f, 0.0f };
static const Vector3 SN_YAW_PIVOT   = {  0.32f, 5.06f, 0.0f };  /* Y-up           */
static const Vector3 SN_PITCH_PIVOT = {  0.51f, 0.46f, 0.0f };  /* rel. yaw, Y-up */
static const Vector3 SN_MUZZLE      = {  1.83f, 0.02f, 0.0f };  /* rel. pitch     */
static const Vector3 SN_RECOIL_DIR  = { -1.0f, 0.0f, 0.0f };

/* ── Réglages TOWER_TESLA ────────────────────────────────────────── */
#define TE_RT_W         200
#define TE_RT_H         248
#define TE_ORTHO        4.9f
#define TE_ORB_PULSE    0.05f          /* amplitude de pulsation de l'orbe      */
static const Vector3 TE_CAM_POS = { -4.2f, 4.7f, -5.2f };
static const Vector3 TE_CAM_TGT = {  0.0f, 2.40f, 0.0f };  /* base ~ bas du RT  */
static const Vector3 TE_ORB_POS = {  0.0f, 3.00f, 0.0f };  /* centre orbe (Y-up) */

/* ── Réglages TOWER_FLAME ────────────────────────────────────────── */
#define FL_RT_W         236
#define FL_RT_H         236
#define FL_ORTHO        6.2f            /* large : doit contenir le balayage du  */
                                        /* canon + le jet dans toute direction   */
#define FL_YAW_OFF      0.0f            /* canon +X comme le sniper (offset 0)   */
/* Fenêtre du jet : DOIT dépasser la période de tir (fire_rate=3.0 → 0.333 s)
   sinon le jet clignote entre deux tirs. Réarmée à chaque tir → jet CONTINU
   tant que la tour tire, qui ne s'éteint que ~FL_FIRE_DUR après le dernier. */
#define FL_FIRE_DUR     0.42f
static const Vector3 FL_CAM_POS  = { -4.6f, 4.5f, -5.5f };
static const Vector3 FL_CAM_TGT  = {  0.0f, 1.05f, 0.0f };  /* axe → balayage centré */
/* Bouche du canon + axe de tir (Y-up, repère tourelle local, yaw=0).
   Bouche monde Z-up=(1.692,0.169,1.47) → Y-up=(x,z,-y). Axe canon Z-up
   d=(cos6,0,sin6) → Y-up=(cos6,sin6,0). */
static const Vector3 FL_MUZZLE   = {  1.692f, 1.470f, -0.169f };
static const Vector3 FL_FIRE_DIR = {  0.9945f, 0.1045f, 0.0f };

/* (Lumière directionnelle : R3D_LIGHT_DIR, partagée — cf. render3d.h) */

/* Orientation au repos (angle sol raylib) de l'AVANT de chaque modèle, pour le
   helper de visée render3d_yaw_for_aim : la mitrailleuse pointe -Z (π), le
   sniper et la flamme pointent +X (π/2). Si la visée est décalée d'un angle
   CONSTANT en jeu, ajuster le *_YAW_OFF correspondant (offset en degrés). */
#define R3D_REST_PHI  3.14159265f
#define SN_REST_PHI   1.57079633f
#define FL_REST_PHI   1.57079633f

/* ── Shader éclairage directionnel — teinte plate (TOWER_GUN) ────── */
static const char *VS =
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec2 vertexTexCoord;\n"
"in vec3 vertexNormal;\n"
"uniform mat4 mvp;\n"
"uniform mat4 matNormal;\n"
"out vec2 fragTexCoord;\n"
"out vec3 fragNormal;\n"
"out float fragDist;\n"
"void main(){\n"
"    fragTexCoord = vertexTexCoord;\n"
"    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal,1.0)));\n"
"    gl_Position = mvp*vec4(vertexPosition,1.0);\n"
"    fragDist = gl_Position.w;\n"   /* profondeur vue (persp.) ; 1 en ortho */
"}\n";
static const char *FS =
"#version 330\n"
"in vec2 fragTexCoord;\n"
"in vec3 fragNormal;\n"
"in float fragDist;\n"
"uniform sampler2D texture0;\n"
"uniform vec4 colDiffuse;\n"
"uniform vec3 lightDir;\n"
"uniform vec3 fogColor;\n"
"uniform float fogDensity;\n"
"out vec4 finalColor;\n"
"void main(){\n"
"    vec4 base = texture(texture0, fragTexCoord)*colDiffuse;\n"
"    float d = max(dot(normalize(fragNormal), normalize(-lightDir)), 0.0);\n"
"    float l = 0.38 + 0.62*d;\n"
"    float fog = clamp(1.0 - exp(-fogDensity*fragDist), 0.0, 1.0);\n"
"    finalColor = vec4(mix(base.rgb*l, fogColor, fog), 1.0);\n"
"}\n";

/* ── Shader VERTEX-COLOR (TOWER_SNIPER) ──────────────────────────── */
static const char *VS_VC =
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec3 vertexNormal;\n"
"in vec4 vertexColor;\n"
"uniform mat4 mvp;\n"
"uniform mat4 matNormal;\n"
"out vec3 fragNormal;\n"
"out vec4 fragColor;\n"
"out float fragDist;\n"
"void main(){\n"
"    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal,1.0)));\n"
"    fragColor = vertexColor;\n"
"    gl_Position = mvp*vec4(vertexPosition,1.0);\n"
"    fragDist = gl_Position.w;\n"
"}\n";
static const char *FS_VC =
"#version 330\n"
"in vec3 fragNormal;\n"
"in vec4 fragColor;\n"
"in float fragDist;\n"
"uniform vec3 lightDir;\n"
"uniform vec3 fogColor;\n"
"uniform float fogDensity;\n"
"out vec4 finalColor;\n"
"void main(){\n"
"    float d = max(dot(normalize(fragNormal), normalize(-lightDir)), 0.0);\n"
"    float l = 0.42 + 0.58*d;\n"
"    float fog = clamp(1.0 - exp(-fogDensity*fragDist), 0.0, 1.0);\n"
"    finalColor = vec4(mix(fragColor.rgb*l, fogColor, fog), 1.0);\n"
"}\n";

/* ── Shader ÉMISSIF (orbe Tesla : plasma violet brillant, pulsé) ─── */
static const char *VS_EMIT =
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec4 vertexColor;\n"
"uniform mat4 mvp;\n"
"out vec4 fragColor;\n"
"void main(){\n"
"    fragColor = vertexColor;\n"
"    gl_Position = mvp*vec4(vertexPosition,1.0);\n"
"}\n";
static const char *FS_EMIT =
"#version 330\n"
"in vec4 fragColor;\n"
"uniform float glow;\n"
"out vec4 finalColor;\n"
"void main(){\n"
"    finalColor = vec4(fragColor.rgb*glow + vec3(0.10,0.03,0.16), 1.0);\n"
"}\n";
