#include "canvas.h"
#include "../ui/ui.h"
#include <math.h>

void canvas_compute(int sw, int sh, float *s, float *ox, float *oy) {
    float sx = (float)sw / VIRT_W, sy = (float)sh / VIRT_H;
    *s = sx < sy ? sx : sy;
    if (*s < 0.25f) *s = 0.25f;
    if (*s > 4.00f) *s = 4.00f;
    *ox = floorf((sw - VIRT_W * *s) * 0.5f);
    *oy = floorf((sh - VIRT_H * *s) * 0.5f);
}

void canvas_set_mouse_offset(float ox, float oy, float scale) {
    ui_set_mouse_offset(ox, oy, scale);
}