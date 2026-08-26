#include "weather.h"
#include "util.h"
#include "environment.h"
#include <string.h>

static const float PAR[4][4] = {
    // fogMul, rain, snow, dim
    { 1.0f, 0.0f, 0.0f, 0.00f },   // clear
    { 2.3f, 1.0f, 0.0f, 0.35f },   // rain
    { 6.5f, 0.08f, 0.0f, 0.55f },  // fog
    { 2.9f, 0.0f, 1.0f, 0.50f },   // snow
};

#define RAIN_N 460
#define SNOW_N 380

typedef struct { float x, y, z, sp; } Drop;

static struct {
    Rng   rng;
    int   cur, nxt;
    int   forced;
    float blend;       // 0 = cur, 1 = nxt
    int   blending;
    float timer;
    float fogMul, rain, snow, dim;
    float wet, ground;
    Drop  rainD[RAIN_N];
    Drop  snowD[SNOW_N];
} W;

const char *weather_name(void){
    static const char *N[4] = { "clear", "rain", "fog", "snow" };
    return W.blending ? N[W.nxt] : N[W.cur];
}

void weather_init(uint64_t seed, int forceKind){
    memset(&W, 0, sizeof(W));
    W.rng.s = mix_seed(seed, 0xAAAA);
    W.cur = forceKind >= 0 ? forceKind : W_CLEAR;
    W.forced = forceKind >= 0;
    W.nxt = W.cur;
    W.timer = 80.0f;
    for (int i = 0; i < RAIN_N; i++){
        W.rainD[i].x = -1; W.rainD[i].y = -1; W.rainD[i].z = -1; W.rainD[i].sp = 1;
    }
    for (int i = 0; i < SNOW_N; i++){
        W.snowD[i].x = -1; W.snowD[i].y = -1; W.snowD[i].z = -1; W.snowD[i].sp = 1;
    }
}

static void pick_next(void){
    int t = W.cur;
    for (int tries = 0; tries < 8; tries++){
        int x = rng_int(&W.rng, 100);
        t = x < 48 ? W_CLEAR : x < 68 ? W_RAIN : x < 85 ? W_FOG : W_SNOW;
        if (t != W.cur) break;
    }
    W.nxt = t;
    W.blending = 1;
    W.blend = 0.0f;
}

static float par(int idx){
    return lerpf(PAR[W.cur][idx], PAR[W.nxt][idx], W.blend);
}

void weather_update(float dt){
    if (W.forced){
        // hold the forced state exactly
        for (int i = 0; i < 4; i++){
            float tgt = PAR[W.cur][i];
            switch (i){
                case 0: W.fogMul = tgt; break;
                case 1: W.rain = tgt; break;
                case 2: W.snow = tgt; break;
                case 3: W.dim = tgt; break;
            }
        }
        float wetT = W.rain > 0.15f ? 1.0f : 0.0f;
        W.wet = wetT;
        float gT = W.snow > 0.2f ? 1.0f : 0.0f;
        W.ground = gT;
        return;
    }
    W.timer -= dt;
    if (W.timer <= 0.0f){
        if (W.blending){
            W.cur = W.nxt;
            W.blending = 0;
            W.blend = 0.0f;
            W.timer = rng_range(&W.rng, 70.0f, 190.0f);
        } else {
            pick_next();
            W.timer = 24.0f;   // transition length
        }
    }
    if (W.blending) W.blend = clampf(W.blend + dt/24.0f, 0.0f, 1.0f);

    W.fogMul = par(0);
    W.rain   = par(1);
    W.snow   = par(2);
    W.dim    = par(3);

    float wetT = W.rain > 0.15f ? 1.0f : 0.0f;
    W.wet = approachf(W.wet, wetT, dt/(wetT > W.wet ? 20.0f : 90.0f));
    float gT = W.snow > 0.2f ? 1.0f : 0.0f;
    W.ground = approachf(W.ground, gT, dt/(gT > W.ground ? 70.0f : 150.0f));
}

float weather_fog_mul(void){ return W.fogMul; }
float weather_rain(void){ return W.rain; }
float weather_snow(void){ return W.snow; }
float weather_wet(void){ return W.wet; }
float weather_ground_snow(void){ return W.ground; }
float weather_dim(void){ return W.dim; }

void weather_draw(Camera3D cam, float dt){
    int w = GetScreenWidth(), h = GetScreenHeight();

    if (W.dim > 0.03f){
        Color veil = col_lerp(envl.fogColor, (Color){ 230, 232, 236, 255 }, 0.25f);
        DrawRectangle(0, 0, w, h, col_a(veil, 0.16f*W.dim + 0.10f*(W.fogMul > 3.0f ? 1.0f : 0.0f)));
    }

    Vector3 cr, cu, cf;
    cam_basis(cam, &cr, &cu, &cf);

    if (W.rain > 0.05f){
        float a = 0.16f*W.rain;
        Color rc = { 178, 196, 216, cuc((int)(a*255.0f)) };
        for (int i = 0; i < RAIN_N; i++){
            Drop *d = &W.rainD[i];
            if (d->y < 0.0f){
                d->x = rng_range(&W.rng, -26.0f, 26.0f);
                d->y = rng_range(&W.rng, 8.0f, 16.0f);
                d->z = rng_range(&W.rng, 3.0f, 55.0f);
                d->sp = rng_range(&W.rng, 0.8f, 1.3f);
            }
            d->y -= 36.0f*d->sp*dt;
            d->x += 1.2f*dt;
            if (d->x > 26.0f) d->x -= 52.0f;
            Vector3 p = Vector3Add(cam.position,
                Vector3Add(Vector3Scale(cr, d->x), Vector3Add(Vector3Scale(cu, d->y), Vector3Scale(cf, d->z))));
            Vector3 p2 = Vector3Add(p, Vector3Add(Vector3Scale(cu, -0.55f), Vector3Scale(cr, 0.03f)));
            Vector2 sp1 = GetWorldToScreen(p, cam);
            Vector2 sp2 = GetWorldToScreen(p2, cam);
            if (sp1.x < -30 || sp1.x > (float)(w + 30) || sp1.y < -30 || sp1.y > (float)(h + 30)){ d->y = -1.0f; continue; }
            DrawLineEx(sp1, sp2, 1.4f, rc);
        }
    }

    if (W.snow > 0.05f){
        Color sc = { 235, 238, 246, cuc((int)(0.75f*255.0f*W.snow)) };
        double time = GetTime();
        for (int i = 0; i < SNOW_N; i++){
            Drop *d = &W.snowD[i];
            if (d->y < 0.0f || d->y > 17.0f){
                d->x = rng_range(&W.rng, -22.0f, 22.0f);
                d->y = rng_range(&W.rng, 2.0f, 16.0f);
                d->z = rng_range(&W.rng, 2.0f, 45.0f);
                d->sp = rng_range(&W.rng, 0.6f, 1.4f);
            }
            d->y -= 1.9f*d->sp*dt;
            d->x += sinf((float)time*0.9f + (float)i*1.3f)*0.02f;
            if (d->x > 22.0f) d->x -= 44.0f;
            if (d->x < -22.0f) d->x += 44.0f;
            Vector3 p = Vector3Add(cam.position,
                Vector3Add(Vector3Scale(cr, d->x), Vector3Add(Vector3Scale(cu, d->y), Vector3Scale(cf, d->z))));
            Vector2 sp1 = GetWorldToScreen(p, cam);
            if (sp1.x < -10 || sp1.x > (float)(w + 10) || sp1.y < -10 || sp1.y > (float)(h + 10)){ d->y = -1.0f; continue; }
            float r = d->z < 10.0f ? 2.4f : (d->z < 22.0f ? 1.6f : 1.0f);
            DrawCircle((int)sp1.x, (int)sp1.y, r, sc);
        }
    }
}
