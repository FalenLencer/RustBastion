/* ════════════════════════════════════════════════════════════════
   audio.c — RUST BASTION
   - Throttle SFX haute fréquence (tours) → pas de saturation audio
   - Tableau g_sfx_valid[] / g_music_valid[] → guard avant tout call
   - audio_apply_volumes() n'itère que les slots valides
   - audio_update() : UpdateMusicStream d'abord, relance si arrêté
   - Volumes par défaut cohérents avec le menu
   - Wasteland : cycling automatique entre _01 et _02 (sans loop)
   ════════════════════════════════════════════════════════════════ */
#include "audio.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>

/* ── Chemins des assets ──────────────────────────────────────── */
static const char *const AUDIO_MUSIC_FILES[AUDIO_MUSIC_COUNT] = {
    NULL,                                              /* WASTELAND — géré par WASTELAND_FILES */
    "assets/sounds/music/Swamp/ambience_swamp.wav",
    "assets/sounds/music/Desert/ambience_desert.wav",
    "assets/sounds/music/City/ambience_city.wav",
    "assets/sounds/music/Factory/ambience_factory.wav",
};

#define WASTELAND_TRACK_COUNT 2
static const char *const WASTELAND_FILES[WASTELAND_TRACK_COUNT] = {
    "assets/sounds/music/Wasteland/ambience_wasteland_01.wav",
    "assets/sounds/music/Wasteland/ambience_wasteland_02.wav",
};

static const char *const AUDIO_SFX_FILES[AUDIO_SFX_COUNT] = {
    "assets/sounds/sfx/short_metallic_click.wav",   /* MENU_CLICK          */
    "assets/sounds/sfx/Confirmation_sound.wav",      /* MENU_CONFIRM        */
    "assets/sounds/sfx/Tower_placement_gun.wav",     /* TOWER_PLACE_GUN     */
    "assets/sounds/sfx/Tower_placement_sniper.wav",  /* TOWER_PLACE_SNIPER  */
    "assets/sounds/sfx/Tower_placement_flame.wav",   /* TOWER_PLACE_FLAME   */
    "assets/sounds/sfx/Tower_placement_tesla.wav",   /* TOWER_PLACE_TESLA   */
    "assets/sounds/sfx/Automatic_turret_gun.wav",    /* TOWER_FIRE_GUN      */
    "assets/sounds/sfx/Precision_sniper_rif.wav",    /* TOWER_FIRE_SNIPER   */
    "assets/sounds/sfx/Flamethrower_burst.wav",      /* TOWER_FIRE_FLAME    */
    "assets/sounds/sfx/Tesla_coil_electrica.wav",    /* TOWER_FIRE_TESLA    */
    "assets/sounds/sfx/Unit_deployment.wav",         /* UNIT_SPAWN          */
    "assets/sounds/sfx/Enemy_wave_spawn.wav",        /* ENEMY_SPAWN         */
    "assets/sounds/sfx/Enemy_hit_impact.wav",        /* ENEMY_HIT           */
    "assets/sounds/sfx/Enemy_death_sound.wav",       /* ENEMY_DEATH         */
    "assets/sounds/sfx/Wave_incoming_alert.wav",     /* WAVE_START          */
    "assets/sounds/sfx/Game_over.wav",               /* GAME_OVER           */
    "assets/sounds/sfx/Victory.wav",                 /* VICTORY             */
    "assets/sounds/sfx/Material_collected.wav",      /* MATERIAL_COLLECT    */
    "assets/sounds/sfx/Power-up_application.wav",    /* MATERIAL_APPLY      */
};

/* ── État interne ────────────────────────────────────────────── */
static Music  g_music[AUDIO_MUSIC_COUNT];
static Sound  g_sfx[AUDIO_SFX_COUNT];
static int    g_sfx_valid[AUDIO_SFX_COUNT];
static int    g_music_valid[AUDIO_MUSIC_COUNT];
static int    g_audio_ready   = 0;
static int    g_current_music = AUDIO_MUSIC_NONE;
static float  g_master_volume = 0.50f;
static float  g_music_volume  = 0.50f;
static float  g_sfx_volume    = 0.50f;

/* ── Wasteland cycling ───────────────────────────────────────── */
static Music g_wasteland[WASTELAND_TRACK_COUNT];
static int   g_wasteland_valid[WASTELAND_TRACK_COUNT];
static int   g_wasteland_idx = 0;   /* index du track en cours (0 ou 1) */

/* ── Volume individuel par SFX ───────────────────────────────────
 * Multiplicateur appliqué par-dessus le volume SFX global.
 * Permet de réduire certains sons trop forts sans toucher au mix global.
 * ─────────────────────────────────────────────────────────────── */
static const float SFX_VOLUME_SCALE[AUDIO_SFX_COUNT] = {
    1.0f,  /* MENU_CLICK          */
    1.0f,  /* MENU_CONFIRM        */
    1.0f,  /* TOWER_PLACE_GUN     */
    1.0f,  /* TOWER_PLACE_SNIPER  */
    1.0f,  /* TOWER_PLACE_FLAME   */
    1.0f,  /* TOWER_PLACE_TESLA   */
    1.0f,  /* TOWER_FIRE_GUN      */
    1.0f,  /* TOWER_FIRE_SNIPER   */
    1.0f,  /* TOWER_FIRE_FLAME    */
    1.0f,  /* TOWER_FIRE_TESLA    */
    1.0f,  /* UNIT_SPAWN          */
    1.0f,  /* ENEMY_SPAWN         */
    1.0f,  /* ENEMY_HIT           */
    1.0f,  /* ENEMY_DEATH         */
    0.10f, /* WAVE_START          */
    1.0f,  /* GAME_OVER           */
    1.0f,  /* VICTORY             */
    1.0f,  /* MATERIAL_COLLECT    */
    1.0f,  /* MATERIAL_APPLY      */
};

/* ── Pool d'alias SFX — permet de jouer le même son en overlap ──
 * LoadSoundAlias() partage le buffer audio du son source sans en
 * être propriétaire. Chaque slot a son propre état de lecture.
 * Quand audio_play_sfx() est appelé, on choisit le premier slot
 * libre en round-robin → jamais de StopSound → zéro artefact.
 * ─────────────────────────────────────────────────────────────── */
#define ALIAS_POOL_SIZE     4
#define ALIASED_SFX_COUNT   3
static const AudioSfxID ALIASED_IDS[ALIASED_SFX_COUNT] = {
    AUDIO_SFX_MENU_CLICK,
    AUDIO_SFX_MENU_CONFIRM,
    AUDIO_SFX_MATERIAL_APPLY,
};
static Sound g_sfx_pool[ALIASED_SFX_COUNT][ALIAS_POOL_SIZE];
static int   g_sfx_pool_valid[ALIASED_SFX_COUNT];
static int   g_sfx_pool_next[ALIASED_SFX_COUNT];

/* ── Throttle SFX — empêche les redémarrages multiples par frame ─
 * Chaque SFX listé ici ne peut se déclencher qu'une fois par fenêtre
 * de THROTTLE_DELAY secondes.
 * ─────────────────────────────────────────────────────────────── */
#define THROTTLE_COUNT 8
static const AudioSfxID THROTTLED_SFX[THROTTLE_COUNT] = {
    AUDIO_SFX_TOWER_FIRE_GUN,
    AUDIO_SFX_TOWER_FIRE_SNIPER,
    AUDIO_SFX_TOWER_FIRE_FLAME,
    AUDIO_SFX_TOWER_FIRE_TESLA,
    AUDIO_SFX_ENEMY_DEATH,
    AUDIO_SFX_ENEMY_SPAWN,
    AUDIO_SFX_UNIT_SPAWN,
    AUDIO_SFX_ENEMY_HIT,
};
static const float THROTTLE_DELAY[THROTTLE_COUNT] = {
    0.08f,   /* gun          */
    0.12f,   /* sniper       */
    0.06f,   /* flame        */
    0.10f,   /* tesla        */
    0.10f,   /* enemy_death  */
    0.15f,   /* enemy_spawn  */
    0.20f,   /* unit_spawn   */
    0.07f,   /* enemy_hit    — splash peut toucher N ennemis/frame */
};
static float g_throttle_timer[AUDIO_SFX_COUNT];

/* ════════════════════════════════════════════════════════════════
   INTERNES
   ════════════════════════════════════════════════════════════════ */
static float clamp01(float v) {
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

static void audio_apply_volumes(void) {
    if (!g_audio_ready) return;
    float mv = g_music_volume * g_master_volume;
    float sv = g_sfx_volume   * g_master_volume;
    for (int i = 0; i < WASTELAND_TRACK_COUNT; i++)
        if (g_wasteland_valid[i]) SetMusicVolume(g_wasteland[i], mv);
    for (int i = 0; i < AUDIO_MUSIC_COUNT; i++)
        if (g_music_valid[i]) SetMusicVolume(g_music[i], mv);
    for (int i = 0; i < AUDIO_SFX_COUNT; i++)
        if (g_sfx_valid[i])   SetSoundVolume(g_sfx[i], sv * SFX_VOLUME_SCALE[i]);
    for (int a = 0; a < ALIASED_SFX_COUNT; a++) {
        if (!g_sfx_pool_valid[a]) continue;
        float vol = sv * SFX_VOLUME_SCALE[ALIASED_IDS[a]];
        for (int p = 1; p < ALIAS_POOL_SIZE; p++)
            SetSoundVolume(g_sfx_pool[a][p], vol);
    }
}

/* ════════════════════════════════════════════════════════════════
   INIT / SHUTDOWN
   ════════════════════════════════════════════════════════════════ */
int audio_init(void) {
    if (g_audio_ready) return 1;

    memset(g_sfx_valid,       0, sizeof(g_sfx_valid));
    memset(g_music_valid,     0, sizeof(g_music_valid));
    memset(g_wasteland_valid, 0, sizeof(g_wasteland_valid));
    memset(g_throttle_timer,  0, sizeof(g_throttle_timer));
    memset(g_sfx_pool_valid,  0, sizeof(g_sfx_pool_valid));
    memset(g_sfx_pool_next,   0, sizeof(g_sfx_pool_next));
    g_wasteland_idx = 0;

    SetAudioStreamBufferSizeDefault(32768);

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        fprintf(stderr, "[AUDIO] Échec init device\n");
        return 0;
    }
    g_audio_ready = 1;

    for (int i = 0; i < AUDIO_SFX_COUNT; i++) {
        g_sfx[i]       = LoadSound(AUDIO_SFX_FILES[i]);
        g_sfx_valid[i] = IsSoundValid(g_sfx[i]) ? 1 : 0;
        if (!g_sfx_valid[i])
            fprintf(stderr, "[AUDIO] SFX manquant : %s\n", AUDIO_SFX_FILES[i]);
    }

    /* Wasteland : deux tracks sans loop, cycling géré dans audio_update */
    for (int i = 0; i < WASTELAND_TRACK_COUNT; i++) {
        g_wasteland[i]       = LoadMusicStream(WASTELAND_FILES[i]);
        g_wasteland_valid[i] = IsMusicValid(g_wasteland[i]) ? 1 : 0;
        if (g_wasteland_valid[i])
            g_wasteland[i].looping = false;
        else
            fprintf(stderr, "[AUDIO] Wasteland track manquante : %s\n", WASTELAND_FILES[i]);
    }

    /* Autres thèmes musicaux (loop) */
    for (int i = 0; i < AUDIO_MUSIC_COUNT; i++) {
        if (i == AUDIO_MUSIC_WASTELAND) { g_music_valid[i] = 0; continue; }
        g_music[i]       = LoadMusicStream(AUDIO_MUSIC_FILES[i]);
        g_music_valid[i] = IsMusicValid(g_music[i]) ? 1 : 0;
        if (g_music_valid[i])
            g_music[i].looping = true;
        else
            fprintf(stderr, "[AUDIO] Musique manquante : %s\n", AUDIO_MUSIC_FILES[i]);
    }

    /* Pools d'alias pour les sons qui peuvent se chevaucher */
    for (int a = 0; a < ALIASED_SFX_COUNT; a++) {
        AudioSfxID id = ALIASED_IDS[a];
        if (!g_sfx_valid[id]) continue;
        g_sfx_pool[a][0]    = g_sfx[id];
        g_sfx_pool_valid[a] = 1;
        for (int p = 1; p < ALIAS_POOL_SIZE; p++)
            g_sfx_pool[a][p] = LoadSoundAlias(g_sfx[id]);
    }

    audio_apply_volumes();
    fprintf(stderr, "[AUDIO] Prêt\n");
    return 1;
}

void audio_shutdown(void) {
    if (!g_audio_ready) return;
    audio_stop_music();
    for (int a = 0; a < ALIASED_SFX_COUNT; a++) {
        if (!g_sfx_pool_valid[a]) continue;
        for (int p = 1; p < ALIAS_POOL_SIZE; p++)
            UnloadSoundAlias(g_sfx_pool[a][p]);
    }
    for (int i = 0; i < WASTELAND_TRACK_COUNT; i++)
        if (g_wasteland_valid[i]) UnloadMusicStream(g_wasteland[i]);
    for (int i = 0; i < AUDIO_SFX_COUNT; i++)
        if (g_sfx_valid[i])   UnloadSound(g_sfx[i]);
    for (int i = 0; i < AUDIO_MUSIC_COUNT; i++)
        if (g_music_valid[i]) UnloadMusicStream(g_music[i]);
    CloseAudioDevice();
    g_audio_ready   = 0;
    g_current_music = AUDIO_MUSIC_NONE;
}

/* ════════════════════════════════════════════════════════════════
   UPDATE — appelé chaque frame depuis main.c
   ════════════════════════════════════════════════════════════════ */
void audio_update(void) {
    if (!g_audio_ready) return;

    float dt = GetFrameTime();
    for (int i = 0; i < AUDIO_SFX_COUNT; i++) {
        if (g_throttle_timer[i] > 0.0f) {
            g_throttle_timer[i] -= dt;
            if (g_throttle_timer[i] < 0.0f) g_throttle_timer[i] = 0.0f;
        }
    }

    if (g_current_music == AUDIO_MUSIC_NONE) return;

    /* Wasteland : cycling _01 → _02 → _01 … */
    if (g_current_music == AUDIO_MUSIC_WASTELAND) {
        int cur = g_wasteland_idx;
        if (!g_wasteland_valid[cur]) return;
        UpdateMusicStream(g_wasteland[cur]);
        if (!IsMusicStreamPlaying(g_wasteland[cur])) {
            int nxt = 1 - cur;
            if (g_wasteland_valid[nxt]) {
                g_wasteland_idx = nxt;
                SetMusicVolume(g_wasteland[nxt], g_music_volume * g_master_volume);
                PlayMusicStream(g_wasteland[nxt]);
            } else {
                /* Seul un track valide → on le relance */
                PlayMusicStream(g_wasteland[cur]);
            }
        }
        return;
    }

    if (!g_music_valid[g_current_music]) return;
    UpdateMusicStream(g_music[g_current_music]);
}

/* ════════════════════════════════════════════════════════════════
   SFX
   ════════════════════════════════════════════════════════════════ */
void audio_play_sfx(AudioSfxID id) {
    if (!g_audio_ready) return;
    if (id < 0 || id >= AUDIO_SFX_COUNT) return;
    if (!g_sfx_valid[id]) return;
    if (g_throttle_timer[id] > 0.0f) return;

    /* Sons avec pool d'alias : on choisit le premier slot libre
     * en round-robin → chevauchement propre, aucun StopSound. */
    for (int a = 0; a < ALIASED_SFX_COUNT; a++) {
        if (ALIASED_IDS[a] != id) continue;
        if (!g_sfx_pool_valid[a]) break;
        for (int p = 0; p < ALIAS_POOL_SIZE; p++) {
            int slot = (g_sfx_pool_next[a] + p) % ALIAS_POOL_SIZE;
            if (!IsSoundPlaying(g_sfx_pool[a][slot])) {
                PlaySound(g_sfx_pool[a][slot]);
                g_sfx_pool_next[a] = (slot + 1) % ALIAS_POOL_SIZE;
                return;
            }
        }
        return; /* tous les slots occupés (extrêmement rare) */
    }

    /* Sons sans pool : on joue uniquement si le slot est libre.
     * On ne coupe jamais un son en cours → zéro artefact. */
    if (IsSoundPlaying(g_sfx[id])) return;
    PlaySound(g_sfx[id]);

    for (int k = 0; k < THROTTLE_COUNT; k++) {
        if (THROTTLED_SFX[k] == id) {
            g_throttle_timer[id] = THROTTLE_DELAY[k];
            break;
        }
    }
}

/* ════════════════════════════════════════════════════════════════
   MUSIQUE
   ════════════════════════════════════════════════════════════════ */
void audio_play_theme_music(ThemeID theme) {
    if (!g_audio_ready) return;
    int next = (int)theme;
    if (next < 0 || next >= AUDIO_MUSIC_COUNT) next = AUDIO_MUSIC_WASTELAND;

    /* Déjà en cours ? */
    if (g_current_music == next) {
        if (next == AUDIO_MUSIC_WASTELAND) {
            int cur = g_wasteland_idx;
            if (g_wasteland_valid[cur] && IsMusicStreamPlaying(g_wasteland[cur])) return;
        } else {
            if (g_music_valid[next] && IsMusicStreamPlaying(g_music[next])) return;
        }
    }

    /* Arrête la musique actuelle */
    if (g_current_music == AUDIO_MUSIC_WASTELAND) {
        int cur = g_wasteland_idx;
        if (g_wasteland_valid[cur] && IsMusicStreamPlaying(g_wasteland[cur]))
            StopMusicStream(g_wasteland[cur]);
    } else if (g_current_music != AUDIO_MUSIC_NONE &&
               g_music_valid[g_current_music] &&
               IsMusicStreamPlaying(g_music[g_current_music])) {
        StopMusicStream(g_music[g_current_music]);
    }

    g_current_music = next;

    /* Lance la nouvelle musique */
    if (next == AUDIO_MUSIC_WASTELAND) {
        g_wasteland_idx = 0;
        if (!g_wasteland_valid[0]) return;
        SetMusicVolume(g_wasteland[0], g_music_volume * g_master_volume);
        PlayMusicStream(g_wasteland[0]);
    } else {
        if (!g_music_valid[next]) return;
        SetMusicVolume(g_music[next], g_music_volume * g_master_volume);
        PlayMusicStream(g_music[next]);
    }
}

void audio_stop_music(void) {
    if (!g_audio_ready) return;
    if (g_current_music == AUDIO_MUSIC_NONE) return;
    if (g_current_music == AUDIO_MUSIC_WASTELAND) {
        int cur = g_wasteland_idx;
        if (g_wasteland_valid[cur] && IsMusicStreamPlaying(g_wasteland[cur]))
            StopMusicStream(g_wasteland[cur]);
    } else if (g_music_valid[g_current_music] &&
               IsMusicStreamPlaying(g_music[g_current_music])) {
        StopMusicStream(g_music[g_current_music]);
    }
    g_current_music = AUDIO_MUSIC_NONE;
}

/* ════════════════════════════════════════════════════════════════
   VOLUMES
   ════════════════════════════════════════════════════════════════ */
void audio_set_master_volume(float v) { g_master_volume = clamp01(v); audio_apply_volumes(); }
void audio_set_music_volume(float v)  { g_music_volume  = clamp01(v); audio_apply_volumes(); }
void audio_set_sfx_volume(float v)    { g_sfx_volume    = clamp01(v); audio_apply_volumes(); }
float audio_get_master_volume(void)   { return g_master_volume; }
float audio_get_music_volume(void)    { return g_music_volume;  }
float audio_get_sfx_volume(void)      { return g_sfx_volume;    }
