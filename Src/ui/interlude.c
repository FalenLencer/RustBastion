#include "interlude.h"
#include "renderer.h"
#include "../meta/meta.h"
#include "../map/theme.h"
#include <math.h>

void interlude_render(const GameState *gs, int scrap_earned, int last_stage) {
    const int CX = VIRT_W/2, CY = VIRT_H/2;
    DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){0,0,0,200});

    int pw = 460, ph = last_stage ? 260 : 220;
    DrawRectangle(CX-pw/2, CY-ph/2, pw, ph, (Color){10,6,2,250});
    DrawRectangleLinesEx(
        (Rectangle){(float)(CX-pw/2),(float)(CY-ph/2),(float)pw,(float)ph},
        2.5f, (Color){46,204,113,255});

    const char *title = last_stage ? "CAMPAGNE TERMINEE !" : "STAGE TERMINE !";
    int tw = MeasureText(title, 22);
    DrawText(title, CX-tw/2, CY-ph/2+18, 22,
             last_stage ? (Color){239,159,39,255} : (Color){46,204,113,255});
    DrawLine(CX-pw/2+20, CY-ph/2+48, CX+pw/2-20, CY-ph/2+48,
             (Color){50,35,15,200});

    DrawText(TextFormat("Vague atteinte  : %d", gs->wave_manager.number),
             CX-pw/2+24, CY-ph/2+60, 14, (Color){180,160,130,255});
    DrawText(TextFormat("Ferraille gagnee : +%d", scrap_earned),
             CX-pw/2+24, CY-ph/2+82, 14, (Color){127,200,50,255});
    DrawText(TextFormat("Ferraille totale : %d", gs->meta.scrap),
             CX-pw/2+24, CY-ph/2+104, 12, (Color){80,120,60,255});

    if (!last_stage) {
        int themes[CAMPAIGN_STAGES];
        meta_campaign_theme_order(gs->campaign_num, themes);
        const Theme *nth = theme_get((ThemeID)themes[gs->campaign_stage+1]);
        DrawText(TextFormat("Prochain : %s", nth->name),
                 CX-pw/2+24, CY-ph/2+130, 13, (Color){100,160,200,255});
    } else {
        DrawText(TextFormat("Campagnes terminees : %d",
                     gs->meta.campaigns_completed),
                 CX-pw/2+24, CY-ph/2+130, 13, (Color){239,159,39,255});
    }

    const char *hint = last_stage ? "[ESPACE] Retour au menu"
                                  : "[ESPACE] Continuer";
    tw = MeasureText(hint, 14);
    DrawText(hint, CX-tw/2, CY+ph/2-36, 14, (Color){160,140,100,255});
}