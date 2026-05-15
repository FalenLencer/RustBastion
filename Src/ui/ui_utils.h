#pragma once
#include "raylib.h"
#include <string.h>

void ui_clip_text(const char *src, int max_w, int fs,
                  char *buf, int buf_sz);

static inline void clip_text(const char *src, int max_w, int fs,
                             char *buf, int buf_sz)
{
    ui_clip_text(src, max_w, fs, buf, buf_sz);
}

static inline void safe_clip(const char *src, int max_w, int fs,
                             char *buf, int buf_sz)
{
    ui_clip_text(src, max_w, fs, buf, buf_sz);
}

int adaptive_fs(int base_fs);