/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
/* ════════════════════════════════════════════════════════════════
   ui/render3d.c — rendu 3D des tours (voir render3d.h).

   Quatre types 3D (interne « kind » : 0 gun, 1 sniper, 2 tesla, 3 flamme) :
   • TOWER_GUN (Mitrailleuse) — base + tourelle + 3 canons (tir séquentiel).
       GLB : tower_base.glb / tower_turret.glb / tower_gun.glb (teinte plate).
   • TOWER_SNIPER — tour de guet HAUTE : statique + mont (azimut) + tube
       (élévation). GLB : tower_sniper_base/mount/barrel.glb, AVEC vertex-colors
       (camo/bois/pierre/métal conservés). Bien plus haut → RT/cam dédiées,
       blit ancré par le bas (le sprite dépasse au-dessus de la case).
   • TOWER_TESLA — base statique vertex-colors + orbe ÉMISSIVE pulsante
       (shader dédié). GLB : tower_tesla_base/orb.glb. Arcs électriques en 2D
       (renderer.c, draw_tesla_arcs), seulement au tir.
   • TOWER_FLAME — lance-flammes : base statique + tourelle ROTATIVE (yaw autour
       de l'axe Y à l'origine = plateau pivotant). GLB : tower_flame_base/turret.glb,
       vertex-colors. Jet de feu dessiné EN 3D dans le RT (draw_flame_jet),
       jaillit du canon UNIQUEMENT au tir.

   La tourelle vise : azimut = tw->angle, élévation = dépression vers l'ennemi
   le plus proche. À chaque tir : recul + flash de bouche (jet pour la flamme).

   Réglages visuels en #define ci-dessous (aucun rendu visible côté outil).
   ════════════════════════════════════════════════════════════════ */
#include "render3d.h"
#include "render3d_skin.h"   /* skin_set_fog / skin_apply_vc */
#include "rlgl.h"
#include "raymath.h"          /* Vector3RotateByAxisAngle (position de la bouche) */
#include <math.h>
#include <stddef.h>

#include "render3d_tower_cfg.h"   /* réglages + shaders (interne) */

/* ── État ───────────────────────────────────────────────────────── */
static int             g_loaded     = 0;
static int             g_have_gun   = 0;
static int             g_have_snipr = 0;
static int             g_have_tesla = 0;
static int             g_have_flame = 0;
static Model           g_base, g_turret, g_gun;
static Model           g_sn_base, g_sn_mount, g_sn_barrel;
static Model           g_te_base, g_te_orb;
static Model           g_fl_base, g_fl_turret;
static Shader          g_shader, g_shader_vc, g_shader_emit;
static Shader          g_outline;               /* blit : contour d'1 pixel d'art */
static int             g_loc_texel = -1;
static int             g_loc_flash = -1;
static Camera3D        g_cam, g_cam_sn, g_cam_te, g_cam_fl;
static RenderTexture2D g_rt[MAX_TOWERS];
static int             g_rt_kind[MAX_TOWERS];   /* -1 aucun, 0 gun, 1 sniper, 2 tesla, 3 flamme */
static int             g_used[MAX_TOWERS];
static float           g_prev_ft[MAX_TOWERS];
static float           g_fire_t [MAX_TOWERS];
static int             g_barrel [MAX_TOWERS];   /* canon courant (Mitrailleuse) */
static float           g_alert  [MAX_TOWERS];   /* 0 veille (balayage) → 1 cible à portée */
static Vector2         g_muz_fire[MAX_TOWERS];  /* bouche du dernier tir (px RT)   */
static Vector2         g_muz_next[MAX_TOWERS];  /* bouche du PROCHAIN tir (px RT)  */
static double          g_clock_last = -1.0;     /* pré-passe : r3d_anim_step       */

/* Cadrage par type : taille de RT, échelle RT→monde, pied projeté (px RT). */
typedef struct { int rt_w, rt_h; float k; Vector2 anchor; } KindFrame;
static KindFrame g_kf[R3D_KINDS];

static void tint_model(Model *m, Color c) {
    for (int i = 0; i < m->materialCount; i++) {
        m->materials[i].shader = g_shader;
        m->materials[i].maps[MATERIAL_MAP_DIFFUSE].color = c;
    }
}
/* (Application du shader VC : skin_apply_vc, render3d_skin.h.) */
static void shade_emit(Model *m) {
    for (int i = 0; i < m->materialCount; i++)
        m->materials[i].shader = g_shader_emit;
}
static void set_light(Shader sh) {
    int loc = GetShaderLocation(sh, "lightDir");
    Vector3 ld = R3D_LIGHT_DIR;
    if (loc >= 0) SetShaderValue(sh, loc, &ld, SHADER_UNIFORM_VEC3);
}

/* Brouillard : uniforms des 2 shaders éclairés (l'émissif Tesla est
   volontairement épargné : le plasma perce la brume). density 0 = coupé.
   (Implémentation partagée : skin_set_fog, render3d_skin.h.) */
void render3d_set_fog(Color col, float density) {
    if (!g_loaded) return;
    skin_set_fog(g_shader, col, density);
    skin_set_fog(g_shader_vc, col, density);
}

/* Cadrage d'un type : échelle telle que l'emprise au sol du socle (boîte
   englobante X/Z) fasse R3D_FOOT_PX, RT dimensionnée à R3D_RT_PER_PX ×
   la taille affichée (même rapport L/H qu'avant → même cadrage caméra),
   pied = projection de l'origine du modèle dans la RT. */
static void frame_kind(int kind, Camera3D cam, const Model *base, int asp_w, int asp_h) {
    BoundingBox b = GetModelBoundingBox(*base);
    float foot = fmaxf(b.max.x - b.min.x, b.max.z - b.min.z);
    if (foot < 0.1f) foot = 1.0f;
    float px_per_unit = R3D_FOOT_PX / foot;                    /* écran (px monde) */
    KindFrame *f = &g_kf[kind];
    f->rt_h   = (int)ceilf(cam.fovy * px_per_unit * R3D_RT_PER_PX);
    f->rt_w   = (int)ceilf((float)f->rt_h * (float)asp_w / (float)asp_h);
    f->k      = 1.0f / R3D_RT_PER_PX;                          /* px RT → px monde */
    f->anchor = GetWorldToScreenEx((Vector3){0, 0, 0}, cam, f->rt_w, f->rt_h);
}

void render3d_init(void) {
    g_loaded = 0; g_have_gun = 0; g_have_snipr = 0; g_have_tesla = 0; g_have_flame = 0;

    g_shader      = LoadShaderFromMemory(VS, FS);
    g_shader_vc   = LoadShaderFromMemory(VS_VC, FS_VC);
    g_shader_emit = LoadShaderFromMemory(VS_EMIT, FS_EMIT);
    set_light(g_shader); set_light(g_shader_vc);
    g_outline     = LoadShaderFromMemory(NULL, FS_OUTLINE);   /* VS par défaut */
    g_loc_texel   = GetShaderLocation(g_outline, "texel");
    g_loc_flash   = GetShaderLocation(g_outline, "flash");
    {
        Color   oc = R3D_OUTLINE_COL;
        float   ov[4] = { oc.r / 255.0f, oc.g / 255.0f, oc.b / 255.0f, oc.a / 255.0f };
        int     lo = GetShaderLocation(g_outline, "outlineCol");
        if (lo >= 0) SetShaderValue(g_outline, lo, ov, SHADER_UNIFORM_VEC4);
    }

    /* TOWER_GUN */
    g_base   = LoadModel("assets/3d/3D_Tours/tower_base.glb");
    g_turret = LoadModel("assets/3d/3D_Tours/tower_turret.glb");
    g_gun    = LoadModel("assets/3d/3D_Tours/tower_gun.glb");
    if (g_base.meshCount && g_turret.meshCount && g_gun.meshCount) {
        /* Tourelle et canons éclaircis (acier) : en grain pixel art, le gris
           presque noir se confondait avec le contour et on ne voyait plus
           la tourelle tourner. */
        tint_model(&g_base,   (Color){150,  72,  42, 255});
        tint_model(&g_turret, (Color){132, 130, 140, 255});
        tint_model(&g_gun,    (Color){ 98,  96, 106, 255});
        g_have_gun = 1;
    }

    /* TOWER_SNIPER (vertex-colors) */
    g_sn_base   = LoadModel("assets/3d/3D_Tours/tower_sniper_base.glb");
    g_sn_mount  = LoadModel("assets/3d/3D_Tours/tower_sniper_mount.glb");
    g_sn_barrel = LoadModel("assets/3d/3D_Tours/tower_sniper_barrel.glb");
    if (g_sn_base.meshCount && g_sn_mount.meshCount && g_sn_barrel.meshCount) {
        skin_apply_vc(&g_sn_base, g_shader_vc); skin_apply_vc(&g_sn_mount, g_shader_vc); skin_apply_vc(&g_sn_barrel, g_shader_vc);
        g_have_snipr = 1;
    }

    /* TOWER_TESLA (base vertex-colors + orbe émissive) */
    g_te_base = LoadModel("assets/3d/3D_Tours/tower_tesla_base.glb");
    g_te_orb  = LoadModel("assets/3d/3D_Tours/tower_tesla_orb.glb");
    if (g_te_base.meshCount && g_te_orb.meshCount) {
        skin_apply_vc(&g_te_base, g_shader_vc); shade_emit(&g_te_orb);
        g_have_tesla = 1;
    }

    /* TOWER_FLAME (base statique + tourelle rotative, vertex-colors).
       Le jet de feu est dessiné en 3D dans le RT (cf. draw_flame_jet). */
    g_fl_base   = LoadModel("assets/3d/3D_Tours/tower_flame_base.glb");
    g_fl_turret = LoadModel("assets/3d/3D_Tours/tower_flame_turret.glb");
    if (g_fl_base.meshCount && g_fl_turret.meshCount) {
        skin_apply_vc(&g_fl_base, g_shader_vc); skin_apply_vc(&g_fl_turret, g_shader_vc);
        g_have_flame = 1;
    }

    g_cam.position   = CAM_POS;   g_cam.target     = CAM_TGT;
    g_cam.up         = (Vector3){0,1,0};
    g_cam.fovy       = R3D_ORTHO; g_cam.projection = CAMERA_ORTHOGRAPHIC;

    g_cam_sn.position   = SN_CAM_POS; g_cam_sn.target     = SN_CAM_TGT;
    g_cam_sn.up         = (Vector3){0,1,0};
    g_cam_sn.fovy       = SN_ORTHO;   g_cam_sn.projection = CAMERA_ORTHOGRAPHIC;

    g_cam_te.position   = TE_CAM_POS; g_cam_te.target     = TE_CAM_TGT;
    g_cam_te.up         = (Vector3){0,1,0};
    g_cam_te.fovy       = TE_ORTHO;   g_cam_te.projection = CAMERA_ORTHOGRAPHIC;

    g_cam_fl.position   = FL_CAM_POS; g_cam_fl.target     = FL_CAM_TGT;
    g_cam_fl.up         = (Vector3){0,1,0};
    g_cam_fl.fovy       = FL_ORTHO;   g_cam_fl.projection = CAMERA_ORTHOGRAPHIC;

    if (g_have_gun)   frame_kind(0, g_cam,    &g_base,    R3D_RT_SIZE, R3D_RT_SIZE);
    if (g_have_snipr) frame_kind(1, g_cam_sn, &g_sn_base, SN_RT_W, SN_RT_H);
    if (g_have_tesla) frame_kind(2, g_cam_te, &g_te_base, TE_RT_W, TE_RT_H);
    if (g_have_flame) frame_kind(3, g_cam_fl, &g_fl_base, FL_RT_W, FL_RT_H);

    for (int i = 0; i < MAX_TOWERS; i++) {
        g_rt[i].id   = 0;  g_rt_kind[i] = -1;  g_used[i] = 0;
        g_prev_ft[i] = 0;  g_fire_t[i]  = 0;   g_barrel[i] = 0;   g_alert[i] = 0;
    }
    g_loaded = (g_have_gun || g_have_snipr || g_have_tesla || g_have_flame);
}

void render3d_shutdown(void) {
    if (g_outline.id != 0) { UnloadShader(g_outline); g_outline.id = 0; }   /* aussi sans tours */
    if (!g_loaded) return;
    for (int i = 0; i < MAX_TOWERS; i++)
        if (g_rt[i].id != 0) UnloadRenderTexture(g_rt[i]);
    UnloadShader(g_shader); UnloadShader(g_shader_vc); UnloadShader(g_shader_emit);
    if (g_have_gun)   { UnloadModel(g_base);    UnloadModel(g_turret);  UnloadModel(g_gun); }
    if (g_have_snipr) { UnloadModel(g_sn_base); UnloadModel(g_sn_mount); UnloadModel(g_sn_barrel); }
    if (g_have_tesla) { UnloadModel(g_te_base); UnloadModel(g_te_orb); }
    if (g_have_flame) { UnloadModel(g_fl_base); UnloadModel(g_fl_turret); }
    g_loaded = 0;
}

int render3d_available(void) { return g_loaded; }

/* (Ré)alloue le RT du slot i aux dimensions du type (cadrage g_kf). Grain
   pixel art (agrandie) → filtre « nearest » ; sur-échantillonnée → bilinéaire. */
static void ensure_rt(int i, int kind) {
    if (g_rt_kind[i] == kind && g_rt[i].id != 0) return;
    if (g_rt[i].id != 0) UnloadRenderTexture(g_rt[i]);
    g_rt[i] = LoadRenderTexture(g_kf[kind].rt_w, g_kf[kind].rt_h);
    SetTextureFilter(g_rt[i].texture,
                     R3D_RT_PER_PX < 1.0f ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
    g_rt_kind[i] = kind;
}

/* Un ennemi visible est-il à portée de tir ? (veille ↔ alerte) */
static int enemy_in_range(const Tower *tw, const EnemyPool *ep) {
    if (!ep) return 0;
    float rng = tw->range * TILE_SIZE, rng2 = rng * rng;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f || e->invisible) continue;
        float dx = e->x - tw->cx, dy = e->y - tw->cy;
        if (dx*dx + dy*dy <= rng2) return 1;
    }
    return 0;
}

/* Rotation d'un point autour d'un axe (degrés, même sens que rlRotatef). */
static Vector3 rot3(Vector3 v, Vector3 axis, float deg) {
    return Vector3RotateByAxisAngle(v, axis, deg * DEG2RAD);
}

/* Bouche du canon en repère MODÈLE (Y-up) : mêmes enchaînements que les
   draw_*_model (translations puis rotations, de l'intérieur vers
   l'extérieur). barrel : canon de la mitrailleuse (0..2). Tesla = orbe. */
static Vector3 muzzle_model(int kind, float yaw, float pitch, int barrel) {
    const Vector3 AY = {0, 1, 0}, AX = {1, 0, 0}, AZ = {0, 0, 1};
    Vector3 p;
    switch (kind) {
        case 0:
            p = (Vector3){BARREL_X[barrel] + MUZZLE.x, MUZZLE.y, MUZZLE.z};
            p = rot3(p, AX, pitch);
            p = Vector3Add(p, GUN_PIVOT);
            p = rot3(p, AY, yaw);
            return Vector3Add(p, (Vector3){0, R3D_YAW_PIVOT_Y, 0});
        case 1:
            p = rot3(SN_MUZZLE, AZ, pitch);
            p = Vector3Add(p, SN_PITCH_PIVOT);
            p = rot3(p, AY, yaw);
            return Vector3Add(p, SN_YAW_PIVOT);
        case 3:
            return rot3(FL_MUZZLE, AY, yaw);
        default:
            return TE_ORB_POS;
    }
}

/* Dépression vers l'ennemi le plus proche (canon au-dessus du sol). */
static float pitch_to_enemy(const Tower *tw, const EnemyPool *ep,
                            float gun_h, float pmax) {
    if (!ep) return 0.0f;
    float bd2 = 1e18f, best = -1.0f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active) continue;
        float dx = e->x - tw->cx, dy = e->y - tw->cy;
        float d2 = dx*dx + dy*dy;
        if (d2 < bd2) { bd2 = d2; best = sqrtf(d2); }
    }
    if (best < 0.0f) return 0.0f;
    if (best < 1.0f) best = 1.0f;
    float p = atan2f(gun_h, best) * RAD2DEG;
    return (p > pmax) ? pmax : p;
}

/* ── TOWER_GUN : base + tourelle (azimut) + 3 canons (un par balle) ────
   Le canon `fire_barrel` (avancé à CHAQUE balle émise, cf. prepass) recule +
   flash de bouche pendant la fenêtre de tir. fire_t décroît de R3D_RECOIL_DUR
   à 0 → recul et flash s'estompent. Chaque canon tire donc à son tour, en
   synchro avec l'émission des projectiles. */
static void draw_tower_model(float yaw_deg, float pitch_deg, int fire_barrel, float fire_t) {
    DrawModel(g_base, (Vector3){0,0,0}, 1.0f, WHITE);
    rlPushMatrix();
        rlTranslatef(0.0f, R3D_YAW_PIVOT_Y, 0.0f);
        rlRotatef(yaw_deg, 0, 1, 0);
        DrawModel(g_turret, (Vector3){0,0,0}, 1.0f, WHITE);
        rlTranslatef(GUN_PIVOT.x, GUN_PIVOT.y, GUN_PIVOT.z);
        rlRotatef(pitch_deg, 1, 0, 0);

        float fa = (fire_t > 0.0f) ? fire_t / R3D_RECOIL_DUR : 0.0f;   /* 1 → 0 */
        if (fa > 1.0f) fa = 1.0f;
        for (int b = 0; b < 3; b++) {
            float r = (b == fire_barrel) ? R3D_RECOIL * fa : 0.0f;
            rlPushMatrix();
                rlTranslatef(BARREL_X[b] + RECOIL_DIR.x*r, RECOIL_DIR.y*r, RECOIL_DIR.z*r);
                DrawModel(g_gun, (Vector3){0,0,0}, 1.0f, WHITE);
            rlPopMatrix();
        }
        if (fa > 0.0f) {     /* flash sur TOUTE la fenêtre, taille décroissante */
            Vector3 m = { BARREL_X[fire_barrel] + MUZZLE.x, MUZZLE.y, MUZZLE.z };
            DrawSphere(m, 0.15f + 0.13f*fa, (Color){255, 224, 130, 255});
            DrawSphere(m, 0.08f + 0.07f*fa, (Color){255, 255, 220, 255});
        }
    rlPopMatrix();
}

/* ── TOWER_SNIPER : statique + mont (azimut) + tube (élévation+recul) ─ */
static void draw_sniper_model(float yaw_deg, float pitch_deg, float fire_t) {
    DrawModel(g_sn_base, (Vector3){0,0,0}, 1.0f, WHITE);
    rlPushMatrix();
        rlTranslatef(SN_YAW_PIVOT.x, SN_YAW_PIVOT.y, SN_YAW_PIVOT.z);
        rlRotatef(yaw_deg, 0, 1, 0);
        DrawModel(g_sn_mount, (Vector3){0,0,0}, 1.0f, WHITE);
        rlTranslatef(SN_PITCH_PIVOT.x, SN_PITCH_PIVOT.y, SN_PITCH_PIVOT.z);
        rlRotatef(pitch_deg, 0, 0, 1);
        float r = (fire_t > 0.0f) ? SN_RECOIL * (fire_t / SN_RECOIL_DUR) : 0.0f;
        rlPushMatrix();
            rlTranslatef(SN_RECOIL_DIR.x*r, SN_RECOIL_DIR.y*r, SN_RECOIL_DIR.z*r);
            DrawModel(g_sn_barrel, (Vector3){0,0,0}, 1.0f, WHITE);
        rlPopMatrix();
        if (fire_t > (SN_RECOIL_DUR - SN_FLASH_DUR)) {
            Vector3 m = { SN_MUZZLE.x + SN_RECOIL_DIR.x*r, SN_MUZZLE.y, SN_MUZZLE.z };
            DrawSphere(m, 0.34f, (Color){255, 224, 130, 255});
            DrawSphere(m, 0.18f, (Color){255, 255, 220, 255});
        }
    rlPopMatrix();
}

/* ── TOWER_TESLA : modèle statique + orbe émissive pulsante ─────── */
static void draw_tesla_model(float t) {
    DrawModel(g_te_base, (Vector3){0,0,0}, 1.0f, WHITE);
    float pulse = 1.0f + TE_ORB_PULSE * sinf(t * 4.5f);
    float glow  = 1.45f + 0.25f * sinf(t * 4.5f);
    int loc = GetShaderLocation(g_shader_emit, "glow");
    if (loc >= 0) SetShaderValue(g_shader_emit, loc, &glow, SHADER_UNIFORM_FLOAT);
    rlPushMatrix();
        rlTranslatef(TE_ORB_POS.x, TE_ORB_POS.y, TE_ORB_POS.z);
        DrawModel(g_te_orb, (Vector3){0,0,0}, pulse, WHITE);
    rlPopMatrix();
}

/* ── TOWER_FLAME : jet de feu jaillissant du canon (UNIQUEMENT au tir) ─
   Dessiné dans le repère tourelle (après le yaw) → le jet suit la bouche.
   Cônes emboîtés du plus long/sombre (langue rouge) au plus court/clair
   (cœur), + éclat de bouche. Longueur et éclat scintillent. */
static void draw_flame_jet(float fire_t, float t) {
    float inten = fire_t / FL_FIRE_DUR;            /* 1 au tir → 0          */
    if (inten < 0.0f) inten = 0.0f;
    if (inten > 1.0f) inten = 1.0f;
    float fl  = 0.78f + 0.25f * sinf(t * 27.0f) + 0.12f * sinf(t * 61.0f);  /* flicker */
    if (fl < 0.50f) fl = 0.50f;
    if (fl > 1.15f) fl = 1.15f;
    float len = (0.80f + 0.45f * inten) * fl;      /* portée du jet (bornée) */
    Vector3 m = FL_MUZZLE, d = FL_FIRE_DIR;
    float   wob = 0.10f * sinf(t * 18.0f);         /* fouettement latéral   */
    Vector3 tip = { m.x + d.x*len,         m.y + d.y*len + wob*0.5f, m.z + d.z*len + wob };
    Vector3 pm  = { m.x + d.x*len*0.60f,   m.y + d.y*len*0.60f,      m.z + d.z*len*0.60f };
    Vector3 pc  = { m.x + d.x*len*0.28f,   m.y + d.y*len*0.28f,      m.z + d.z*len*0.28f };
    unsigned char a = (unsigned char)(185 + 70*inten);
    DrawCylinderEx(m, tip, 0.20f*fl, 0.05f, 9, (Color){208,  55, 18, a});  /* langue rouge */
    DrawCylinderEx(m, pm,  0.27f*fl, 0.10f, 9, (Color){255, 120, 28, a});  /* corps orange */
    DrawCylinderEx(m, pc,  0.22f*fl, 0.07f, 9, (Color){255, 238,150, a});  /* cœur clair   */
    DrawSphere(m, 0.16f*fl, (Color){255, 244, 190, a});                    /* éclat bouche */
}

/* ── TOWER_FLAME : base statique + tourelle rotative (yaw) + jet ─────── */
static void draw_flame_model(float yaw_deg, float fire_t, float t) {
    DrawModel(g_fl_base, (Vector3){0,0,0}, 1.0f, WHITE);
    rlPushMatrix();
        rlRotatef(yaw_deg, 0, 1, 0);          /* plateau rotatif (axe Y, origine) */
        DrawModel(g_fl_turret, (Vector3){0,0,0}, 1.0f, WHITE);
        if (fire_t > 0.0f) draw_flame_jet(fire_t, t);
    rlPopMatrix();
}

void render3d_prepass(const TowerPool *tp, const EnemyPool *ep) {
    if (!g_loaded || tp == NULL) return;
    render3d_set_fog(BLANK, 0.0f);   /* mode 2D : jamais de brouillard */
    float dt   = r3d_anim_step(&g_clock_last);   /* temps de SIMULATION (×vitesse, pause) */
    float tnow = (float)fx_clock();              /* balayage, arcs Tesla, jet de flamme  */
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        int kind = -1;
        if (tw->active) {
            if (tw->type == TOWER_GUN    && g_have_gun)   kind = 0;
            if (tw->type == TOWER_SNIPER && g_have_snipr) kind = 1;
            if (tw->type == TOWER_TESLA  && g_have_tesla) kind = 2;
            if (tw->type == TOWER_FLAME  && g_have_flame) kind = 3;
        }
        if (kind < 0) { g_used[i] = 0; g_fire_t[i] = 0.0f; continue; }

        /* Détection du tir : fire_timer remonte = UNE balle émise. À chaque
           balle, la Mitrailleuse passe au canon SUIVANT (tir en cascade). */
        float dur = (kind == 1) ? SN_RECOIL_DUR
                  : (kind == 3) ? FL_FIRE_DUR : R3D_RECOIL_DUR;
        if (tw->fire_timer > g_prev_ft[i] + 0.02f) {
            g_barrel[i] = (g_barrel[i] + 1) % 3;   /* canon suivant par balle */
            g_fire_t[i] = dur;
        }
        g_prev_ft[i] = tw->fire_timer;
        g_fire_t[i] -= dt;
        if (g_fire_t[i] < 0.0f) g_fire_t[i] = 0.0f;

        /* Veille : sans ennemi à portée, la tourelle balaie lentement autour
           de sa dernière visée (purement visuel : tw->angle n'est pas touché). */
        float alert = enemy_in_range(tw, ep) ? 1.0f : 0.0f;
        g_alert[i] += (alert - g_alert[i]) * fminf(1.0f, dt * R3D_ALERT_RATE);
        float aim = tw->angle + (1.0f - g_alert[i]) * R3D_IDLE_SWEEP
                              * sinf(tnow * R3D_IDLE_FREQ + (float)i * 1.3f);

        /* Yaw CORRECT pour la caméra oblique (cf. render3d_yaw_for_aim) : on
           projette la visée via la caméra du type. Chaque type a sa caméra et
           son orientation au repos. */
        float yaw, pitch = 0.0f;
        Camera3D cam;
        if (kind == 1) {
            cam   = g_cam_sn;
            yaw   = render3d_yaw_for_aim(cam, aim, SN_REST_PHI) + SN_YAW_OFF;
            pitch = SN_PITCH_SIGN * pitch_to_enemy(tw, ep, SN_GUN_HEIGHT, SN_PITCH_MAX);
        } else if (kind == 3) {
            cam = g_cam_fl;
            yaw = render3d_yaw_for_aim(cam, aim, FL_REST_PHI) + FL_YAW_OFF;
        } else if (kind == 2) {
            cam = g_cam_te;
            yaw = 0.0f;
        } else {
            cam   = g_cam;
            yaw   = render3d_yaw_for_aim(cam, aim, R3D_REST_PHI) + R3D_YAW_OFF;
            pitch = pitch_to_enemy(tw, ep, R3D_GUN_HEIGHT, R3D_PITCH_MAX);
        }

        ensure_rt(i, kind);
        BeginTextureMode(g_rt[i]);
            ClearBackground(BLANK);
            BeginMode3D(cam);
                if      (kind == 3) draw_flame_model(yaw, g_fire_t[i], tnow);
                else if (kind == 2) draw_tesla_model(tnow);
                else if (kind == 1) draw_sniper_model(yaw, pitch, g_fire_t[i]);
                else                draw_tower_model(yaw, pitch, g_barrel[i], g_fire_t[i]);
            EndMode3D();
        EndTextureMode();

        /* Bouches projetées dans la RT : celle du dernier tir (éclair) et
           celle du PROCHAIN (les balles émises cette frame partent de là :
           la pré-passe tourne AVANT la simulation). */
        const KindFrame *f = &g_kf[kind];
        g_muz_fire[i] = GetWorldToScreenEx(muzzle_model(kind, yaw, pitch, g_barrel[i]),
                                           cam, f->rt_w, f->rt_h);
        g_muz_next[i] = GetWorldToScreenEx(muzzle_model(kind, yaw, pitch, (g_barrel[i] + 1) % 3),
                                           cam, f->rt_w, f->rt_h);
        g_used[i] = 1;
    }
}

Texture2D render3d_tower_tex(int tower_index) {
    if (!g_loaded || tower_index < 0 || tower_index >= MAX_TOWERS || !g_used[tower_index])
        return (Texture2D){0};
    return g_rt[tower_index].texture;
}

Rectangle render3d_tower_dst(int tower_index, float cx, float cy, float tile) {
    (void)tile;   /* l'échelle vient du cadrage par type (R3D_FOOT_PX) */
    int kind = (tower_index >= 0 && tower_index < MAX_TOWERS) ? g_rt_kind[tower_index] : -1;
    if (kind < 0) return (Rectangle){ cx, cy, 0, 0 };
    /* Pied du modèle (origine projetée) posé sur le centre de la case. */
    const KindFrame *f = &g_kf[kind];
    return (Rectangle){ cx - f->anchor.x * f->k, cy + R3D_GROUND_DY - f->anchor.y * f->k,
                        f->rt_w * f->k, f->rt_h * f->k };
}

void render3d_blit_outlined(Texture2D t, Rectangle dst, Color tint, float flash) {
    if (t.id == 0) return;
    float texel[2] = { 1.0f / (float)t.width, 1.0f / (float)t.height };
    int   shaded   = (g_outline.id != 0);
    if (shaded) {
        if (g_loc_texel >= 0) SetShaderValue(g_outline, g_loc_texel, texel, SHADER_UNIFORM_VEC2);
        if (g_loc_flash >= 0) SetShaderValue(g_outline, g_loc_flash, &flash, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(g_outline);
    }
    /* RenderTexture raylib : verticalement retournée → hauteur source négative */
    DrawTexturePro(t, (Rectangle){0, 0, (float)t.width, -(float)t.height}, dst,
                   (Vector2){0, 0}, 0.0f, tint);
    if (shaded) EndShaderMode();
}

void render3d_tower_blit(int tower_index, Rectangle dst, Color tint) {
    render3d_blit_outlined(render3d_tower_tex(tower_index), dst, tint, 0.0f);
}

int render3d_tower_muzzle(int tower_index, Rectangle dst, int next, Vector2 *out) {
    if (!g_loaded || tower_index < 0 || tower_index >= MAX_TOWERS ||
        !g_used[tower_index] || out == NULL) return 0;
    const KindFrame *f = &g_kf[g_rt_kind[tower_index]];
    Vector2 m = next ? g_muz_next[tower_index] : g_muz_fire[tower_index];
    out->x = dst.x + m.x * dst.width  / (float)f->rt_w;
    out->y = dst.y + m.y * dst.height / (float)f->rt_h;
    return 1;
}

float render3d_tower_fire_k(int tower_index) {
    if (!g_loaded || tower_index < 0 || tower_index >= MAX_TOWERS || !g_used[tower_index])
        return 0.0f;
    int kind = g_rt_kind[tower_index];
    float dur = (kind == 1) ? SN_RECOIL_DUR : (kind == 3) ? FL_FIRE_DUR : R3D_RECOIL_DUR;
    float k = g_fire_t[tower_index] / dur;
    return (k > 1.0f) ? 1.0f : k;
}

/* ════════════════════════════════════════════════════
   MODE HÉROS — dessin direct AVEC visée + animation de tir
   (yaw MONDE : pas de correction caméra-oblique en vraie 3D)
   ════════════════════════════════════════════════════ */
int render3d_tower_draw_world_aimed(const Tower *tw, int tower_index,
                                    const EnemyPool *ep, Vector3 pos,
                                    float scale) {
    if (!g_loaded || tw == NULL ||
        tower_index < 0 || tower_index >= MAX_TOWERS) return 0;

    int kind = -1;
    if (tw->type == TOWER_GUN    && g_have_gun)   kind = 0;
    if (tw->type == TOWER_SNIPER && g_have_snipr) kind = 1;
    if (tw->type == TOWER_TESLA  && g_have_tesla) kind = 2;
    if (tw->type == TOWER_FLAME  && g_have_flame) kind = 3;
    if (kind < 0) return 0;

    /* Même machine d'état de tir que la pré-passe (front de fire_timer). */
    int   i   = tower_index;
    float dt  = GetFrameTime();
    float dur = (kind == 1) ? SN_RECOIL_DUR
              : (kind == 3) ? FL_FIRE_DUR : R3D_RECOIL_DUR;
    if (tw->fire_timer > g_prev_ft[i] + 0.02f) {
        g_barrel[i] = (g_barrel[i] + 1) % 3;
        g_fire_t[i] = dur;
    }
    g_prev_ft[i] = tw->fire_timer;
    g_fire_t[i] -= dt;
    if (g_fire_t[i] < 0.0f) g_fire_t[i] = 0.0f;

    /* Yaw MONDE : dir sim (cos a, sin a) → cap atan2(x, z), corrigé du
       repos PAR TYPE (mêmes REST_PHI/YAW_OFF que la pré-passe). */
    float hd = atan2f(cosf(tw->angle), sinf(tw->angle));
    float yaw;
    if      (kind == 1) yaw = (hd - SN_REST_PHI)  * RAD2DEG + SN_YAW_OFF;
    else if (kind == 3) yaw = (hd - FL_REST_PHI)  * RAD2DEG + FL_YAW_OFF;
    else                yaw = (hd - R3D_REST_PHI) * RAD2DEG + R3D_YAW_OFF;

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlScalef(scale, scale, scale);
    if (kind == 0) {
        float pitch = pitch_to_enemy(tw, ep, R3D_GUN_HEIGHT, R3D_PITCH_MAX);
        draw_tower_model(yaw, pitch, g_barrel[i], g_fire_t[i]);
    } else if (kind == 1) {
        float pitch = SN_PITCH_SIGN *
                      pitch_to_enemy(tw, ep, SN_GUN_HEIGHT, SN_PITCH_MAX);
        draw_sniper_model(yaw, pitch, g_fire_t[i]);
    } else if (kind == 2) {
        draw_tesla_model((float)GetTime());
    } else {
        draw_flame_model(yaw, g_fire_t[i], (float)GetTime());
    }
    rlPopMatrix();
    return 1;
}

/* ════════════════════════════════════════════════════
   MODE HÉROS — dessin direct d'une tour dans la scène 3D
   (socle fixe + tourelle orientée vers map_angle)
   ════════════════════════════════════════════════════ */
/* Décalage constant du cap de tourelle en monde (réglage playtest). */
#define R3D_TURRET_WORLD_YAW_OFF  0.0f

int render3d_tower_draw_world(int type, Vector3 pos, float map_angle,
                              float scale) {
    if (!g_loaded) return 0;
    /* Direction sim (cos a, sin a) → cap monde atan2(x, z). */
    float yaw_deg = atan2f(cosf(map_angle), sinf(map_angle)) * RAD2DEG
                  + R3D_TURRET_WORLD_YAW_OFF;
    float t = (float)GetTime();
    int drawn = 1;

    /* Réutilise les fonctions de la PRÉ-PASSE (pivots/translations
       corrects : R3D_YAW_PIVOT_Y, GUN_PIVOT, SN_*, TE_ORB_POS…) au lieu
       de redessiner les pièces à plat — c'était le bug d'affichage. */
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlScalef(scale, scale, scale);
    switch (type) {
        case TOWER_GUN:
            if (!g_have_gun) { drawn = 0; break; }
            draw_tower_model(yaw_deg, 0.0f, 0, 0.0f);
            break;
        case TOWER_SNIPER:
            if (!g_have_snipr) { drawn = 0; break; }
            draw_sniper_model(yaw_deg, 0.0f, 0.0f);
            break;
        case TOWER_TESLA:
            if (!g_have_tesla) { drawn = 0; break; }
            draw_tesla_model(t);
            break;
        case TOWER_FLAME:
            if (!g_have_flame) { drawn = 0; break; }
            draw_flame_model(yaw_deg, 0.0f, t);
            break;
        default:
            drawn = 0;
            break;
    }
    rlPopMatrix();
    return drawn;
}
