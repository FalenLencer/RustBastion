/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "ui_utils.h"
#include "raylib.h"
#include <string.h>

void ui_clip_text(const char *src, int max_w, int fs,
                  char *buf, int buf_sz)
{
    if (!src || !buf || buf_sz <= 0) return;

    int i = 0;
    while (i < buf_sz - 1 && src[i]) {
        buf[i] = src[i];
        i++;
    }
    buf[i] = '\0';

    if (max_w <= 0) return;

    while (i > 0 && mtxt(buf, fs) > max_w) {
        i--;
        buf[i] = '\0';
    }

    if (i < (int)strlen(src) && i > 3) {
        buf[i-1] = '.';
        buf[i-2] = '.';
        buf[i-3] = '.';
    }
}

int adaptive_fs(int base_fs) {
    (void)base_fs;
    return base_fs;
}