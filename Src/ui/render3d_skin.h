/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  render3d_skin.h ─ SOURCE UNIQUE des helpers de modèles GLB skinnés,
 *  partagés par les modules JUMEAUX render3d_units.c / render3d_enemies.c
 *  (et le setter de fog par render3d.c). Étaient dupliqués à l'identique :
 *  shaders vertex-color, matcher d'anims par mots-clés, gain de luminance
 *  par modèle, application du shader, uniforms de brouillard.
 *
 *  Interne au rendu 3D — ne pas inclure hors de render3d*.c.
 *  (Sources de shader via des FONCTIONS inline : une chaîne statique non
 *  utilisée déclencherait -Wunused-const-variable dans les TU qui
 *  n'incluent ce header que pour skin_set_fog.)
 */
#include "raylib.h"
#include <string.h>
#include <ctype.h>

/* ── Shader vertex-color : éclairage directionnel + gain + gamma + fog ── */
static inline const char *skin_vs_vc(void) {
    return
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
    "    fragDist = gl_Position.w;\n"   /* profondeur vue (persp.) ; 1 en ortho */
    "}\n";
}
static inline const char *skin_fs_vc(void) {
    return
    "#version 330\n"
    "in vec3 fragNormal;\n"
    "in vec4 fragColor;\n"
    "in float fragDist;\n"
    "uniform vec3 lightDir;\n"
    "uniform float gain;\n"          /* normalisation luminosité PAR MODÈLE  */
    "uniform vec3 fogColor;\n"
    "uniform float fogDensity;\n"    /* 0 = coupé (pré-passe 2D)             */
    "out vec4 finalColor;\n"
    "void main(){\n"
    "    float d = max(dot(normalize(fragNormal), normalize(-lightDir)), 0.0);\n"
    "    float l = 0.58 + 0.48*d;\n"                 /* ombres relevées       */
    "    vec3 c = clamp(fragColor.rgb*gain*l, 0.0, 1.0);\n"
    "    c = pow(c, vec3(1.0/1.8));\n"               /* gamma linéaire→écran  */
    "    float fog = clamp(1.0 - exp(-fogDensity*fragDist), 0.0, 1.0);\n"
    "    finalColor = vec4(mix(c, fogColor, fog), 1.0);\n"
    "}\n";
}

/* ── Matcher d'anims par mots-clés (insensible à la casse) ─────────
   Tolère les conventions variées des GLB (Idle/idle, Attack/Fight/mine…).
   Renvoie l'index de la 1re anim dont le nom contient un mot-clé, ou -1. */
static inline int skin_anim_match(const ModelAnimation *anims, int count,
                                  const char *const *kw, int nkw) {
    for (int i = 0; i < count; i++) {
        char low[64]; int n = 0;
        const char *s = anims[i].name;
        for (; s[n] && n < 63; n++) low[n] = (char)tolower((unsigned char)s[n]);
        low[n] = '\0';
        for (int k = 0; k < nkw; k++)
            if (strstr(low, kw[k])) return i;
    }
    return -1;
}

/* ── Gain de luminance par modèle ──────────────────────────────────
   Certains GLB sont exportés très sombres : on ramène la luminance
   moyenne des vertex-colors vers une cible commune, borné pour ne pas
   blanchir les clairs ni sur-booster les quasi-noirs. */
#define SKIN_GAIN_TARGET 0.34f
#define SKIN_GAIN_MIN    0.75f
#define SKIN_GAIN_MAX    4.5f
static inline float skin_model_gain(const Model *m) {
    double sum = 0.0; long n = 0;
    for (int j = 0; j < m->meshCount; j++) {
        const unsigned char *c = m->meshes[j].colors;
        if (!c) continue;
        int vc = m->meshes[j].vertexCount;
        for (int v = 0; v < vc; v++) {
            double r = c[v*4]/255.0, g = c[v*4+1]/255.0, b = c[v*4+2]/255.0;
            sum += 0.299*r + 0.587*g + 0.114*b; n++;
        }
    }
    if (n == 0) return 1.0f;
    double mean = sum / (double)n;
    if (mean < 1e-4) return 1.0f;
    double gain = SKIN_GAIN_TARGET / mean;
    if (gain < SKIN_GAIN_MIN) gain = SKIN_GAIN_MIN;
    if (gain > SKIN_GAIN_MAX) gain = SKIN_GAIN_MAX;
    return (float)gain;
}

/* Applique le shader vertex-color à tous les matériaux du modèle. */
static inline void skin_apply_vc(Model *m, Shader sh) {
    for (int i = 0; i < m->materialCount; i++) {
        m->materials[i].shader = sh;
        m->materials[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }
}

/* Pousse les uniforms de brouillard (density 0 = coupé). */
static inline void skin_set_fog(Shader sh, Color col, float density) {
    int lc = GetShaderLocation(sh, "fogColor");
    int ld = GetShaderLocation(sh, "fogDensity");
    Vector3 fc = { col.r / 255.0f, col.g / 255.0f, col.b / 255.0f };
    if (lc >= 0) SetShaderValue(sh, lc, &fc, SHADER_UNIFORM_VEC3);
    if (ld >= 0) SetShaderValue(sh, ld, &density, SHADER_UNIFORM_FLOAT);
}
