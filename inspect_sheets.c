#include <stdio.h>
#include "raylib.h"

/* Détecte des clusters larges (> min_w) dans une image,
   cherche des blocs de colonnes contiguës ayant des pixels visibles */
static void find_wide_clusters(const Color *px, int W, int H,
                                int ylo, int yhi, int min_w)
{
    int xs = -1;
    for (int x = 0; x <= W; x++) {
        int has = 0;
        if (x < W)
            for (int y = ylo; y <= yhi; y++)
                if (px[y*W+x].a > 10) { has = 1; break; }
        if (has && xs < 0) xs = x;
        if (!has && xs >= 0) {
            int w = x - xs;
            if (w >= min_w) {
                /* Find row bounds within this column range */
                int ymin=yhi, ymax=ylo;
                for (int cy = ylo; cy <= yhi; cy++)
                    for (int cx = xs; cx < x; cx++)
                        if (px[cy*W+cx].a > 10) {
                            if (cy < ymin) ymin = cy;
                            if (cy > ymax) ymax = cy;
                        }
                printf("  cluster x=%d..%d w=%d  y=%d..%d h=%d\n",
                       xs, x-1, w, ymin, ymax, ymax-ymin+1);
            }
            xs = -1;
        }
    }
}

int main(void) {
    SetTraceLogLevel(LOG_NONE);
    InitWindow(1, 1, "inspect");

    printf("=== welding_anime.png ===\n");
    Image img = LoadImage("assets/textures/Animation/welding_anime.png");
    Color *px = LoadImageColors(img);
    int W = img.width, H = img.height;
    /* cherche les clusters larges de >40px de largeur */
    find_wide_clusters(px, W, H, 0, H-1, 40);
    UnloadImageColors(px);
    UnloadImage(img);

    printf("\n=== dust_anim.png (rappel frames) ===\n");
    img = LoadImage("assets/textures/Animation/dust_anim.png");
    px = LoadImageColors(img);
    W = img.width; H = img.height;
    find_wide_clusters(px, W, H, 0, H-1, 50);
    UnloadImageColors(px);
    UnloadImage(img);

    CloseWindow();
    return 0;
}
