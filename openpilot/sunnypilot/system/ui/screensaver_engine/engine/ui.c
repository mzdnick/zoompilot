#include "ui.h"
#include "traffic.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>

static Texture2D vign;
static float intro = 0.0f;
static int   showFps = 0;

// trip odometer: meters driven, kept across runs in a plain text file.
// UI state only - the seeded world is untouched.
static double odoM = 0.0;
static double odoSaved = 0.0;
static float  odoLastS = -1.0f;
static int    odoPersist = 0;

static void odo_path(char *buf, int n){
    const char *home = getenv("HOME");
    snprintf(buf, n, "%s/.infinite-drive.odometer", home ? home : ".");
}

static void odo_load(void){
    char path[512];
    odo_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    double v = -1.0;
    if (fscanf(f, "%lf", &v) == 1 && v >= 0.0 && v < 4.0e10) odoM = v;
    fclose(f);
}

static void odo_save(void){
    char path[512];
    odo_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%.1f\n", odoM);
    fclose(f);
    odoSaved = odoM;
}

void ui_init(int persist){
    odoPersist = persist;
    if (persist) odo_load();
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

void ui_shutdown(void){
    if (odoPersist) odo_save();
}

void ui_update(float dt){
    intro += dt;
    float s = traffic_ego_s();
    if (odoLastS < 0.0f) odoLastS = s;
    float ds = s - odoLastS;
    odoLastS = s;
    if (ds > 0.0f && ds < 100.0f){
        odoM += ds;
        // autosave covers sessions that never reach a clean quit
        if (odoPersist && odoM - odoSaved > 2000.0) odo_save();
    }
}

void ui_fps_toggle(void){ showFps = !showFps; }

void ui_draw(int w, int h){
    float sc = (float)h/720.0f;
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
        float tsz = 24.0f*sc;
        Vector2 ts = MeasureTextEx(f, title, tsz, 2);
        DrawTextEx(f, title, (Vector2){ (float)w*0.5f - ts.x*0.5f, (float)h - 78.0f*sc }, tsz, 2,
                   col_a((Color){ 205, 215, 228, 255 }, ta*0.9f));
        const char *sub = "autonomous night drive";
        float ssz = 10.0f*sc;
        Vector2 ss = MeasureTextEx(f, sub, ssz, 2);
        DrawTextEx(f, sub, (Vector2){ (float)w*0.5f - ss.x*0.5f, (float)h - 46.0f*sc }, ssz, 2,
                   col_a((Color){ 150, 160, 175, 255 }, ta*0.7f));
    }

    if (intro > 3.0f){
        // the rounded km value changes rarely; cache text and measured width
        static char txt[20];
        static Vector2 ts = { 0, 0 };
        static int lastKm = -1;
        int km = (int)(odoM/1000.0 + 0.5);
        if (km != lastKm){
            lastKm = km;
            snprintf(txt, sizeof(txt), "ODO %d km", km);
            ts = MeasureTextEx(GetFontDefault(), txt, 12, 1);
        }
        DrawTextEx(GetFontDefault(), txt, (Vector2){ (float)w - ts.x - 20, (float)h - 30 }, 12, 1,
                   col_a((Color){ 190, 215, 235, 255 }, 0.35f));
    }

    if (showFps) DrawFPS(8, 8);
}
