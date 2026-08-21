#include "ui.h"
#include "util.h"

static Texture2D vign;
static float intro = 0.0f;
static int   showFps = 0;

void ui_init(void){
    int w = 256, h = 144;
    Image img = GenImageColor(w, h, (Color){ 0, 0, 0, 0 });
    Color *px = (Color *)img.data;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++){
            float dx = (x - w*0.5f)/(w*0.5f);
            float dy = (y - h*0.5f)/(h*0.5f);
            float r = sqrtf(dx*dx + dy*dy);
            float a = smooth01((r - 0.55f)/0.45f)*0.42f;
            // slight extra darkening at the bottom
            a += 0.08f*smooth01(((float)y/h - 0.72f)/0.28f);
            px[y*w + x] = (Color){ 0, 0, 0, cuc((int)(a*255.0f)) };
        }
    vign = LoadTextureFromImage(img);
    SetTextureFilter(vign, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
}

void ui_update(float dt){ intro += dt; }

void ui_fps_toggle(void){ showFps = !showFps; }

void ui_draw(int w, int h){
    Rectangle src = { 0, 0, 256, 144 };
    Rectangle dst = { 0, 0, (float)w, (float)h };
    DrawTexturePro(vign, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);

    if (intro < 2.2f){
        float fade = 1.0f - smooth01(intro/2.2f);
        DrawRectangle(0, 0, w, h, col_a((Color){ 0, 0, 0, 255 }, fade));
    }

    float ta = smooth01((intro - 0.8f)/1.2f)*(1.0f - smooth01((intro - 5.0f)/1.5f));
    if (ta > 0.01f){
        const char *title = "I N F I N I T E   D R I V E";
        Font f = GetFontDefault();
        Vector2 ts = MeasureTextEx(f, title, 24, 2);
        DrawTextEx(f, title, (Vector2){ (float)w*0.5f - ts.x*0.5f, (float)h - 78 }, 24, 2,
                   col_a((Color){ 205, 215, 228, 255 }, ta*0.9f));
        const char *sub = "autonomous night drive";
        Vector2 ss = MeasureTextEx(f, sub, 10, 2);
        DrawTextEx(f, sub, (Vector2){ (float)w*0.5f - ss.x*0.5f, (float)h - 46 }, 10, 2,
                   col_a((Color){ 150, 160, 175, 255 }, ta*0.7f));
    }

    if (showFps) DrawFPS(8, 8);
}
