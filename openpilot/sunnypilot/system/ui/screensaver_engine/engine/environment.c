#include "environment.h"
#include "rlgl.h"
#include "util.h"
#include "weather.h"
#include "road.h"
#include "events.h"

EnvLight envl;
float envIndoor = 0.0f;

static struct {
    float dayLen;
    float t;            // 0..1, 0 = midnight
    Rng  rng;
    Vector3 starDir[260];
    float  starTw[260];
    float  aurora;      // smoothed aurora amount, 0..1
} E;

typedef struct { float t; int zen[3], hor[3], amb[3]; float sunI, fog, stars; } KF;

static const KF KFS[] = {
    { 0.00f, {  5,  8, 16}, { 14, 20, 34}, { 23, 27, 40}, 0.00f, 0.00200f, 1.00f },
    { 0.20f, {  6,  9, 19}, { 22, 26, 44}, { 25, 29, 43}, 0.00f, 0.00195f, 0.95f },
    { 0.25f, { 24, 32, 64}, {210,110, 80}, { 46, 48, 66}, 0.25f, 0.00210f, 0.25f },
    { 0.30f, { 56, 84,140}, {255,170,110}, { 92, 96,110}, 0.65f, 0.00180f, 0.00f },
    { 0.38f, { 88,132,190}, {190,208,224}, {128,134,148}, 0.95f, 0.00140f, 0.00f },
    { 0.50f, { 98,146,205}, {205,218,232}, {150,155,166}, 1.05f, 0.00120f, 0.00f },
    { 0.62f, { 92,134,192}, {198,206,220}, {140,142,152}, 0.95f, 0.00130f, 0.00f },
    { 0.70f, { 70, 92,150}, {250,185,120}, {120,116,120}, 0.75f, 0.00150f, 0.00f },
    { 0.75f, { 44, 52,100}, {250,120, 80}, { 86, 80, 92}, 0.40f, 0.00165f, 0.05f },
    { 0.80f, { 18, 24, 48}, {110, 70, 80}, { 40, 40, 56}, 0.08f, 0.00190f, 0.35f },
    { 0.87f, {  6,  9, 18}, { 18, 24, 40}, { 24, 28, 42}, 0.00f, 0.00200f, 0.90f },
    { 1.00f, {  5,  8, 16}, { 14, 20, 34}, { 23, 27, 40}, 0.00f, 0.00200f, 1.00f },
};
#define NKF ((int)(sizeof(KFS)/sizeof(KFS[0])))

static Color icol(const int *c){ return (Color){ cuc(c[0]), cuc(c[1]), cuc(c[2]), 255 }; }

void env_init(uint64_t seed, float dayLength, float startT){
    E.dayLen = dayLength > 0.0f ? dayLength : 480.0f;
    E.t = startT;
    if (E.t < 0.0f) E.t = 0.0f;
    if (E.t >= 1.0f) E.t -= 1.0f;
    E.rng.s = mix_seed(seed, 0xE401);
    for (int i = 0; i < 260; i++){
        // spread stars over the upper hemisphere, denser near poles
        float a = rng_range(&E.rng, 0.0f, 6.2832f);
        float y = rng_range(&E.rng, 0.02f, 1.0f);
        float rr = sqrtf(1.0f - y*y);
        E.starDir[i] = (Vector3){ rr*cosf(a), y, rr*sinf(a) };
        E.starTw[i] = rng_range(&E.rng, 0.5f, 3.0f);
    }
    env_update(0.0f);
}

void env_update(float dt){
    E.t += dt / E.dayLen;
    E.t -= floorf(E.t);

    const KF *a = &KFS[0], *b = &KFS[1];
    for (int i = 0; i < NKF - 1; i++){
        if (E.t >= KFS[i].t && E.t < KFS[i+1].t){ a = &KFS[i]; b = &KFS[i+1]; break; }
    }
    float t = (E.t - a->t) / (b->t - a->t);

    envl.zenith   = col_lerp(icol(a->zen), icol(b->zen), t);
    envl.horizon  = col_lerp(icol(a->hor), icol(b->hor), t);
    envl.ambient  = col_lerp(icol(a->amb), icol(b->amb), t);
    envl.sunI     = lerpf(a->sunI, b->sunI, t);
    float fogK    = lerpf(a->fog, b->fog, t);
    envl.stars    = lerpf(a->stars, b->stars, t);

    float elev = sinf(6.2832f*(E.t - 0.25f))*1.15f;
    float az = 6.2832f*(E.t - 0.20f);
    envl.sunElev = elev;
    envl.sunDir = Vector3Normalize((Vector3){ cosf(elev)*sinf(az), sinf(elev), cosf(elev)*cosf(az) });

    float warm = clampf(elev/0.5f, 0.0f, 1.0f);
    envl.sunCol = col_lerp((Color){ 255, 140, 80, 255 }, (Color){ 255, 246, 230, 255 }, warm);
    float dim = weather_dim();
    envl.sunI *= 1.0f - 0.75f*dim;

    float lights = 1.0f - smooth01((elev - 0.03f)/0.11f);
    envl.lightsOn = lights;

    envl.stars *= (1.0f - dim);

    envl.fogColor = col_lerp(envl.horizon, (Color){ 6, 7, 10, 255 }, 0.85f*envIndoor);
    envl.fogDensity = fogK*weather_fog_mul()*(1.0f + 0.9f*envIndoor);

    envl.ambient = col_scale(envl.ambient, 1.0f - 0.72f*envIndoor);
    envl.zenith = col_lerp(envl.zenith, (Color){ 8, 8, 12, 255 }, envIndoor);
}

void env_draw_sky(Camera3D cam, float sCam, float dt){
    int w = GetScreenWidth(), h = GetScreenHeight();

    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 flat = (Vector3){ fwd.x, 0.0f, fwd.z };
    float fl = Vector3Length(flat);
    if (fl < 0.001f) flat = (Vector3){ 0, 0, 1 }; else flat = Vector3Scale(flat, 1.0f/fl);
    Vector3 hp = Vector3Add(cam.position, Vector3Scale(flat, 2400.0f));
    Vector2 hy2 = GetWorldToScreen(hp, cam);
    float horizonY = hy2.y;
    if (horizonY < 0) horizonY = 0;
    if (horizonY > (float)h) horizonY = (float)h;

    // sky gradient: stacked gradient strips sample the fixed u^1.6 curve.
    // Neighbour strips share their edge u, so they share their edge color;
    // each strip also overlaps the one below by 1 px, so the clear color
    // can never show through between strips (solid bands of fixed height
    // left such gaps wherever the curve outpaces the band spacing).
    const int bands = 10;
    static float skyU[11];
    static int skyUInit = 0;
    if (!skyUInit){
        for (int k = 0; k <= bands; k++)
            skyU[k] = powf((float)k/(float)bands, 1.6f);
        skyUInit = 1;
    }
    for (int k = 0; k < bands; k++){
        float uA = skyU[k], uB = skyU[k + 1];
        int yLow  = (int)(horizonY*(1.0f - uA));
        int yHigh = (int)(horizonY*(1.0f - uB));
        DrawRectangleGradientV(0, yHigh, w, yLow - yHigh + 1,
                               col_lerp(envl.zenith, envl.horizon, uB),
                               col_lerp(envl.zenith, envl.horizon, uA));
    }
    if (horizonY < (float)h)
        DrawRectangle(0, (int)horizonY, w, h - (int)horizonY, envl.horizon);

    // stars
    if (envl.stars > 0.02f){
        double time = GetTime();
        for (int i = 0; i < 260; i++){
            Vector3 d = E.starDir[i];
            // cull before projecting: 0.55 is below cos(half-diagonal-FOV)
            // for every camera fovy in use, so no on-screen star is lost
            float dot = Vector3DotProduct(d, fwd);
            if (dot < 0.55f) continue;
            Vector3 p = Vector3Add(cam.position, Vector3Scale(d, 3000.0f));
            Vector2 sp = GetWorldToScreen(p, cam);
            if (sp.x < 0 || sp.y < 0 || sp.x > (float)w || sp.y > horizonY) continue;
            float tw = 0.55f + 0.45f*sinf((float)time*E.starTw[i] + (float)i*1.7f);
            unsigned char a = cuc((int)(210.0f*tw*envl.stars));
            Color c = { 224, 230, 244, a };
            if ((i & 7) == 0) DrawRectangle((int)sp.x, (int)sp.y, 2, 2, c);
            else DrawPixel((int)sp.x, (int)sp.y, c);
        }
    }

    rlSetBlendMode(RL_BLEND_ADDITIVE);

    // aurora: night ribbons over mountain zones only; the amount ramps so it
    // never pops at a zone boundary. E.t drives the wave, so --seek stays
    // deterministic.
    {
        float tgt = (road_zone_at(sCam) == ZN_MOUNTAIN && envl.stars > 0.5f && envIndoor < 0.3f)
                  ? envl.stars : 0.0f;
        E.aurora += (tgt - E.aurora)*clampf(dt/8.0f, 0.0f, 1.0f);
        if (E.aurora > 0.02f){
            const int NSTR = 26;
            Color green = { 70, 220, 150, 255 }, purple = { 130, 90, 220, 255 };
            for (int rb = 0; rb < 3; rb++){
                for (int k = 0; k < NSTR; k++){
                    float u = (float)k/(float)NSTR;
                    float n  = vnoise1(u*6.0f  + (float)rb*7.31f + E.t*14.0f);
                    float n2 = vnoise1(u*13.0f + (float)rb*3.70f - E.t*23.0f);
                    float hgt = horizonY*(0.14f + 0.11f*(float)rb)*(0.55f + 0.45f*(0.5f*n + 0.5f));
                    float baseY = horizonY*(1.0f - (0.30f + 0.09f*(float)rb) - 0.05f*n2);
                    int x0 = (int)(u*(float)w) - 2;
                    int x1 = (int)((u + 1.0f/(float)NSTR)*(float)w) + 2;
                    float A = 26.0f*E.aurora*(0.55f + 0.45f*(0.5f*n2 + 0.5f));
                    DrawRectangleGradientV(x0, (int)(baseY - hgt), x1 - x0, (int)hgt + 1,
                                           col_a(purple, 0.0f), col_a(green, A));
                }
            }
        }
    }

    // shooting star: head slides along a short arc, tail trails behind
    {
        Vector3 sd; float sph;
        if (events_star(&sd, &sph) && Vector3DotProduct(sd, flat) > 0.25f){
            float fade = sph < 0.25f ? sph/0.25f : 1.0f - (sph - 0.25f)/0.75f;
            Vector3 side = Vector3Normalize(Vector3CrossProduct(sd, (Vector3){ 0, 1, 0 }));
            Vector3 head = Vector3Add(Vector3Add(cam.position, Vector3Scale(sd, 3000.0f)),
                                      Vector3Scale(side, 160.0f*sph - 80.0f));
            Vector3 tail = Vector3Add(head, Vector3Scale(side, -55.0f));
            Vector2 h2 = GetWorldToScreen(head, cam), t2 = GetWorldToScreen(tail, cam);
            if (h2.x > -80 && h2.x < (float)(w + 80) && h2.y > -80 && h2.y < horizonY + 80){
                DrawLineEx(h2, t2, 2.0f, col_a((Color){ 225, 235, 255, 255 }, 200.0f*fade));
                DrawCircleV(h2, 1.6f, col_a((Color){ 255, 255, 255, 255 }, 230.0f*fade));
            }
        }
    }

    // sun glow and disc
    float sd = Vector3DotProduct(envl.sunDir, fwd);
    if (sd > -0.15f && envl.sunI > 0.02f){
        Vector3 sp3 = Vector3Add(cam.position, Vector3Scale(envl.sunDir, 3000.0f));
        Vector2 sp = GetWorldToScreen(sp3, cam);
        if (sp.x > -200 && sp.x < (float)(w + 200) && sp.y > -200 && sp.y < (float)(h + 200)){
            float low = 1.0f - clampf(envl.sunElev/0.5f, 0.0f, 1.0f);
            Color inner = col_a((Color){ 255, 210 - (int)(90*low), 140, 255 }, 130.0f*envl.sunI);
            DrawCircleGradient(sp, 150.0f + 130.0f*low, inner, (Color){ 0, 0, 0, 0 });
            if (envl.sunElev > -0.02f)
                DrawCircleGradient(sp, 16.0f,
                                   col_a((Color){ 255, 244, 214, 255 }, 235.0f), (Color){ 0, 0, 0, 0 });
        }
    }

    // moon
    Vector3 md = Vector3Scale(envl.sunDir, -1.0f);
    if (md.y > 0.0f && Vector3DotProduct(md, fwd) > 0.05f){
        Vector3 mp3 = Vector3Add(cam.position, Vector3Scale(md, 3000.0f));
        Vector2 mp = GetWorldToScreen(mp3, cam);
        if (mp.x > -100 && mp.x < (float)(w + 100) && mp.y > -100 && mp.y < (float)(h + 100)){
            DrawCircleGradient(mp, 70.0f,
                               col_a((Color){ 160, 175, 205, 255 }, 60.0f), (Color){ 0, 0, 0, 0 });
            DrawCircleV(mp, 11.0f, col_a((Color){ 214, 222, 235, 255 }, 225.0f));
        }
    }

    rlSetBlendMode(RL_BLEND_ALPHA);
}
