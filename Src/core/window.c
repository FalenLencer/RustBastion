#include "window.h"
#include "canvas.h"

void window_apply_size(int w, int h) {
    SetWindowSize(w, h);
    int mw = GetMonitorWidth(0), mh = GetMonitorHeight(0);
    Vector2 mp = GetMonitorPosition(0);
    SetWindowPosition((int)(mp.x+(mw-w)/2), (int)(mp.y+(mh-h)/2));
}

void window_center(void) {
    int mw = GetMonitorWidth(0), mh = GetMonitorHeight(0);
    Vector2 mp = GetMonitorPosition(0);
    SetWindowPosition((int)(mp.x+(mw-VIRT_W)/2),
                      (int)(mp.y+(mh-VIRT_H)/2));
}