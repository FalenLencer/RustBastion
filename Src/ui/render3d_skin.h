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
#include "render3d.h"   /* R3D_ENT_FOOT_DY */
#include "../combat/fx.h"   /* fx_clock (horloge de simulation) */
#include <string.h>
#include <ctype.h>
#include <math.h>

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

/* 0 si un maillage INDEXÉ dépasse 65 535 sommets : raylib stocke les indices
   en 16 bits et TRONQUE ceux du GLB (avertissement « u32 to u16 ») → maillage
   corrompu à l'écran (cas : PathBreaker.glb, 66 888 sommets par primitive).
   Mieux vaut le repli 2D. Correctif réel : réexporter le modèle allégé. */
#define SKIN_MAX_INDEXED_VERTS 65535
static inline int skin_model_indices_ok(const Model *m, const char *path) {
    for (int j = 0; j < m->meshCount; j++) {
        if (m->meshes[j].indices && m->meshes[j].vertexCount > SKIN_MAX_INDEXED_VERTS) {
            TraceLog(LOG_WARNING,
                     "3D: %s maillage %d = %d sommets (> %d, indices 16 bits tronques)"
                     " -> modele ignore, repli 2D. Reexporter le modele allege.",
                     path, j, m->meshes[j].vertexCount, SKIN_MAX_INDEXED_VERTS);
            return 0;
        }
    }
    return 1;
}

/* Applique le shader vertex-color à tous les matériaux du modèle. */
static inline void skin_apply_vc(Model *m, Shader sh) {
    for (int i = 0; i < m->materialCount; i++) {
        m->materials[i].shader = sh;
        m->materials[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }
}

/* ── Cadrage AUTOMATIQUE d'une entité (unité / ennemi) ─────────────
   Même principe que les tours (render3d.c) : échelle commune, PIED du
   modèle (origine) posé au sol, grain pixel art (R3D_ENT_RT_PER_PX) et
   contour au blit. La caméra ortho 3/4 (direction R3D_ENT_VIEW, commune à
   tous) est CALÉE sur le cylindre englobant du modèle (rayon XZ autour de
   l'axe de rotation, hauteur) : aucune découpe quel que soit le cap, et
   une RT juste à la taille du modèle (fini les cadrages réglés à l'œil). */
#define R3D_ENT_VIEW        ((Vector3){ 3.5f, -2.5f, 4.3f })   /* regard 3/4      */
#define R3D_ENT_RT_PER_PX   0.5f    /* px de RT par px monde (= grain du décor)  */
#define R3D_ENT_PAD_PX      2       /* marge de RT (contour d'1 px d'art)        */
#define R3D_ENT_ANIM_MARGIN 1.12f   /* les anims débordent la pose de repos      */
#define R3D_ENT_PX_PER_SIZE 4.4f    /* dimension du modèle = size × 4,4 px        */
#define R3D_ENT_LONG_K      0.75f   /* quadrupède / véhicule : 0,75 × la longueur */
/* (R3D_ENT_FOOT_DY : render3d.h, partagé avec le rendu 2D des entités.) */
#define R3D_ENT_SAMPLES     16      /* points du cylindre englobant projetés      */

typedef struct {
    Camera3D cam;
    int      rt_w, rt_h;
    float    k;            /* px RT → px monde (à la taille de base)  */
    Vector2  foot, head;   /* origine / sommet projetés (px RT)        */
} R3dFrame;

/* px monde par unité modèle : la dimension caractéristique (hauteur, ou
   0,75 × longueur pour un modèle allongé) mesure size × R3D_ENT_PX_PER_SIZE. */
static inline float r3d_ent_px_per_unit(BoundingBox bb, float size) {
    float h   = bb.max.y - fminf(bb.min.y, 0.0f);
    float len = fmaxf(bb.max.x - bb.min.x, bb.max.z - bb.min.z) * R3D_ENT_LONG_K;
    float d   = fmaxf(fmaxf(h, len), 0.1f);
    return size * R3D_ENT_PX_PER_SIZE / d;
}

static inline R3dFrame r3d_frame_auto(BoundingBox bb, float px_per_unit) {
    R3dFrame f;
    Vector3 v = R3D_ENT_VIEW;
    float vl = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    Vector3 fw = { v.x / vl, v.y / vl, v.z / vl };
    float rl = sqrtf(fw.z*fw.z + fw.x*fw.x);
    Vector3 rt = { -fw.z / rl, 0.0f, fw.x / rl };                  /* droite      */
    Vector3 up = { rt.y*fw.z - rt.z*fw.y, rt.z*fw.x - rt.x*fw.z,   /* haut caméra */
                   rt.x*fw.y - rt.y*fw.x };
    float ax = fmaxf(fabsf(bb.min.x), fabsf(bb.max.x));
    float az = fmaxf(fabsf(bb.min.z), fabsf(bb.max.z));
    float R  = sqrtf(ax*ax + az*az) * R3D_ENT_ANIM_MARGIN;
    float y0 = fminf(bb.min.y, 0.0f), y1 = bb.max.y * R3D_ENT_ANIM_MARGIN;
    float r0 = 1e9f, r1 = -1e9f, u0 = 1e9f, u1 = -1e9f;
    for (int s = 0; s < R3D_ENT_SAMPLES; s++) {
        float a = 6.2831853f * (float)s / R3D_ENT_SAMPLES;
        for (int e = 0; e < 2; e++) {
            Vector3 p = { R * cosf(a), e ? y1 : y0, R * sinf(a) };
            float pr = p.x*rt.x + p.y*rt.y + p.z*rt.z, pu = p.x*up.x + p.y*up.y + p.z*up.z;
            r0 = fminf(r0, pr); r1 = fmaxf(r1, pr); u0 = fminf(u0, pu); u1 = fmaxf(u1, pu);
        }
    }
    float ppu = px_per_unit * R3D_ENT_RT_PER_PX;                   /* px RT / unité */
    f.rt_w = (int)ceilf((r1 - r0) * ppu) + 2 * R3D_ENT_PAD_PX;
    f.rt_h = (int)ceilf((u1 - u0) * ppu) + 2 * R3D_ENT_PAD_PX;
    float cr = (r0 + r1) * 0.5f, cu = (u0 + u1) * 0.5f;
    Vector3 tg = { rt.x*cr + up.x*cu, rt.y*cr + up.y*cu, rt.z*cr + up.z*cu };
    f.cam.target     = tg;
    f.cam.position   = (Vector3){ tg.x - fw.x * 20.0f, tg.y - fw.y * 20.0f, tg.z - fw.z * 20.0f };
    f.cam.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    f.cam.fovy       = (float)f.rt_h / ppu;                        /* hauteur visée */
    f.cam.projection = CAMERA_ORTHOGRAPHIC;
    f.k    = 1.0f / R3D_ENT_RT_PER_PX;
    f.foot = GetWorldToScreenEx((Vector3){ 0.0f, 0.0f, 0.0f }, f.cam, f.rt_w, f.rt_h);
    f.head = GetWorldToScreenEx((Vector3){ 0.0f, bb.max.y, 0.0f }, f.cam, f.rt_w, f.rt_h);
    return f;
}

/* Rectangle de blit : pied posé en (x, y + size × R3D_ENT_FOOT_DY), à
   l'échelle size / base_size (un ennemi renforcé grossit avec son rayon). */
static inline Rectangle r3d_ent_dst(const R3dFrame *f, float x, float y,
                                    float size, float base_size) {
    float s  = (base_size > 0.0f) ? size / base_size : 1.0f;
    float k  = f->k * s;
    float fy = y + size * R3D_ENT_FOOT_DY;
    return (Rectangle){ x - f->foot.x * k, fy - f->foot.y * k, f->rt_w * k, f->rt_h * k };
}

/* (Ré)alloue une RT d'entité aux dimensions voulues, filtre « nearest »
   (grain pixel art agrandi sans flou). */
static inline void r3d_ent_ensure_rt(RenderTexture2D *rt, int w, int h) {
    if (rt->id != 0 && rt->texture.width == w && rt->texture.height == h) return;
    if (rt->id != 0) UnloadRenderTexture(*rt);
    *rt = LoadRenderTexture(w, h);
    SetTextureFilter(rt->texture, TEXTURE_FILTER_POINT);
}

/* ── Horloge d'animation = temps de SIMULATION ─────────────────────
   Pas (s) écoulé depuis le dernier appel, lu sur fx_clock : ×2/×3 en avance
   rapide, 0 en pause / fiche de découverte / fenêtre sans focus → les modèles
   se figent avec le jeu. Chaque pré-passe garde son `last` (négatif = 1er
   appel → pas nul). Plafonné : retour du mode héros, F4 rallumé, hoquet. */
#define R3D_ANIM_STEP_MAX   0.5f    /* s de simulation max par frame            */
static inline float r3d_anim_step(double *last) {
    double now = fx_clock();
    double d   = (*last < 0.0) ? 0.0 : now - *last;
    *last = now;
    if (d < 0.0) d = 0.0;
    if (d > R3D_ANIM_STEP_MAX) d = R3D_ANIM_STEP_MAX;
    return (float)d;
}

/* ── Marche CALÉE SUR LA DISTANCE ──────────────────────────────────
   À la vitesse de base du type (`ref`, px/s), l'anim de marche joue à sa
   cadence d'origine ; ralenti (gel, marais, patrouille) → pas plus lents,
   accéléré (×2/×3, vague rapide) → plus rapides : les pieds ne glissent plus.
   Renvoie l'avance du temps d'anim (s) ; ref ≤ 0 → horloge de sim. */
#define R3D_STRIDE_MAX      3.0f    /* × cadence d'origine (bond, téléport)     */
static inline float r3d_stride_step(float dist, float ref, float sdt) {
    if (ref <= 0.0f) return sdt;
    return fminf(dist / ref, sdt * R3D_STRIDE_MAX);
}

/* Pousse les uniforms de brouillard (density 0 = coupé). */
static inline void skin_set_fog(Shader sh, Color col, float density) {
    int lc = GetShaderLocation(sh, "fogColor");
    int ld = GetShaderLocation(sh, "fogDensity");
    Vector3 fc = { col.r / 255.0f, col.g / 255.0f, col.b / 255.0f };
    if (lc >= 0) SetShaderValue(sh, lc, &fc, SHADER_UNIFORM_VEC3);
    if (ld >= 0) SetShaderValue(sh, ld, &density, SHADER_UNIFORM_FLOAT);
}
