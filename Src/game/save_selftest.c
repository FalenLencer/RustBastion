/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 *
 * Test FUMÉE du format de sauvegarde robuste (HORS Makefile — possède son main).
 * Réplique wsec/rsec/rscalar de save.c et vérifie que :
 *   - un round-trip normal restitue les valeurs ;
 *   - une SECTION dont la taille a changé est défautée MAIS ne désaligne pas
 *     les sections/scalaires suivants (cœur de la robustesse) ;
 *   - un scalaire absent (save tronquée/ancienne) prend la valeur par défaut.
 *
 * Compile : gcc -Wall -Wextra Src/game/save_selftest.c -o /tmp/save_test && /tmp/save_test
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int wsec(FILE *f, const void *p, size_t n) {
    uint32_t len = (uint32_t)n;
    if (fwrite(&len, sizeof(len), 1, f) != 1) return 0;
    if (n > 0 && fwrite(p, n, 1, f) != 1)     return 0;
    return 1;
}
static void rsec(FILE *f, void *p, size_t n) {
    uint32_t len = 0;
    if (fread(&len, sizeof(len), 1, f) != 1) { if (p && n) memset(p, 0, n); return; }
    if (len == (uint32_t)n) {
        if (n > 0 && fread(p, n, 1, f) != 1) { if (p && n) memset(p, 0, n); }
    } else {
        if (p && n) memset(p, 0, n);
        fseek(f, (long)len, SEEK_CUR);
    }
}
static void rscalar(FILE *f, void *p, size_t n) {
    if (fread(p, n, 1, f) != 1) memset(p, 0, n);
}

int main(void) {
    printf("=== save : test format robuste ===\n");
    const char *path = "/tmp/rb_save_test.bin";
    int fails = 0;

    int    aw[3] = { 11, 22, 33 };
    double bw    = 3.14159;
    char   cw[6] = "TEST";
    int    sw    = 4242;

    FILE *f = fopen(path, "wb");
    if (!f) { printf("  [X] fopen w\n"); return 1; }
    uint32_t magic = 0x52425456u;
    fwrite(&magic, sizeof(magic), 1, f);
    wsec(f, aw, sizeof(aw));
    wsec(f, &bw, sizeof(bw));
    wsec(f, cw, sizeof(cw));
    fwrite(&sw, sizeof(sw), 1, f);
    fclose(f);

    /* 1) Round-trip normal */
    {
        FILE *g = fopen(path, "rb");
        uint32_t m = 0; fread(&m, sizeof(m), 1, g);
        int ar[3]; double br; char cr[6]; int sr = 0;
        rsec(g, ar, sizeof(ar));
        rsec(g, &br, sizeof(br));
        rsec(g, cr, sizeof(cr));
        rscalar(g, &sr, sizeof(sr));
        fclose(g);
        int ok = (m == magic) && !memcmp(ar, aw, sizeof(aw)) &&
                 br == bw && !strcmp(cr, cw) && sr == sw;
        if (ok) printf("  [OK] round-trip normal\n");
        else { printf("  [X] round-trip normal\n"); fails++; }
    }

    /* 2) Section du MILIEU (b) « grossie » → défaut + alignement preserve */
    {
        FILE *g = fopen(path, "rb");
        uint32_t m = 0; fread(&m, sizeof(m), 1, g);
        int ar[3];
        struct { double x, y; } bbig;   /* 16 octets au lieu de 8 → mismatch */
        char cr[6]; int sr = 0;
        rsec(g, ar, sizeof(ar));
        rsec(g, &bbig, sizeof(bbig));   /* doit etre defaute (zero) + saute */
        rsec(g, cr, sizeof(cr));        /* DOIT rester aligne */
        rscalar(g, &sr, sizeof(sr));    /* DOIT rester aligne */
        fclose(g);
        int ok = !memcmp(ar, aw, sizeof(aw)) &&
                 bbig.x == 0.0 && bbig.y == 0.0 &&
                 !strcmp(cr, cw) && sr == sw;
        if (ok) printf("  [OK] section changee sautee SANS desaligner la suite\n");
        else { printf("  [X] desalignement apres section changee (cr='%s' sr=%d)\n", cr, sr); fails++; }
    }

    /* 3) Scalaire absent (fichier tronque) → defaut */
    {
        FILE *g = fopen(path, "rb");
        uint32_t m = 0; fread(&m, sizeof(m), 1, g);
        int ar[3]; double br; char cr[6];
        rsec(g, ar, sizeof(ar));
        rsec(g, &br, sizeof(br));
        rsec(g, cr, sizeof(cr));
        /* lit 2 scalaires alors qu'un seul existe → le 2e doit defauter a 0 */
        int s1 = -1, s2 = -1;
        rscalar(g, &s1, sizeof(s1));
        rscalar(g, &s2, sizeof(s2));
        fclose(g);
        if (s1 == sw && s2 == 0) printf("  [OK] scalaire absent → defaut (0)\n");
        else { printf("  [X] scalaire absent (s1=%d s2=%d)\n", s1, s2); fails++; }
    }

    remove(path);
    printf("=== %s ===\n", fails == 0 ? "TOUT OK (0 echec)" : "ECHECS");
    return fails ? 1 : 0;
}
