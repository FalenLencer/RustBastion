/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "canvas.h"
#include "../ui/hud.h"
#include <math.h>

void canvas_compute(int sw, int sh, float *sx, float *sy, float *ox, float *oy) {
    (void)sw;
    float scale = (float)sh / VIRT_H;
    if (scale < 0.05f) scale = 0.05f;
    *sx = scale; *sy = scale;
    *ox = 0.0f;  *oy = 0.0f;
}

void canvas_set_mouse_offset(float ox, float oy, float sx, float sy) {
    ui_set_mouse_offset(ox, oy, sx, sy);
}