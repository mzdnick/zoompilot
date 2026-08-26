#include "events.h"
#include "road.h"
#include "traffic.h"
#include "vehicle.h"
#include "rendering.h"
#include "environment.h"
#include "weather.h"
#include "util.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    int   active;
    float s, lat, vLat, vS;
    float phase;      // gallop bob clock
} Deer;

typedef struct {
    float   t, dur;       // t < 0 means the slot is free
    Vector3 c;            // burst center, world space
    Color   col;
    uint32_t seed;
    int     n;            // particle count
} Burst;

static struct {
    Rng     rng;
    float   starNext, starT;
    Vector3 starDir;
    float   roadNext;
    Deer    deer;
    // ---- later additions keep their own rng streams, so the original
    // seeded sequences (star, convoy, classic, deer) stay bit-identical ----
    Rng     boltRng;      // lightning storms, only while rain is heavy
    float   boltNext;     // time to the next storm
    int     boltLeft;     // strikes remaining in the active storm
    float   boltGapT;     // time to the next strike inside a storm
    float   boltT;        // active flash, decays to 0
    Vector2 boltA, boltB; // bolt endpoints as screen fractions
    int     boltSegs, boltRe;
    uint32_t boltSeed;
    Rng     stopRng;      // police traffic stop
    float   stopNext;
    int     stopActive;
    float   stopS, stopClock;
    Rng     fwRng;        // fireworks, only over city at night
    float   fwNext;
    int     fwLeft;       // bursts remaining in the active show
    float   fwGapT;
    Burst   fw[3];
} V;

void events_init(uint64_t seed){
    memset(&V, 0, sizeof(V));
    V.rng.s = mix_seed(seed, 0xBEEF);
    V.starNext = rng_range(&V.rng, 40.0f, 200.0f);
    V.roadNext = rng_range(&V.rng, 180.0f, 420.0f);
    V.boltRng.s = mix_seed(seed, 0x570B);
    V.boltNext = rng_range(&V.boltRng, 25.0f, 80.0f);
    V.stopRng.s = mix_seed(seed, 0xC0FF);
    V.stopNext = rng_range(&V.stopRng, 900.0f, 2400.0f);
    V.fwRng.s = mix_seed(seed, 0xF1EA);
    V.fwNext = rng_range(&V.fwRng, 60.0f, 160.0f);
    for (int i = 0; i < 3; i++) V.fw[i].t = -1.0f;
}

// -------- shooting star --------

int events_star(Vector3 *dir, float *phase){
    if (V.starT <= 0.0f) return 0;
    *dir = V.starDir;
    *phase = V.starT;
    return 1;
}

static void star_update(float dt){
    if (V.starT > 0.0f){
        V.starT += dt/0.9f;
        if (V.starT >= 1.0f) V.starT = 0.0f;
        return;
    }
    // hold the countdown in daylight: a daytime trigger could not fire and
    // its re-roll would waste a draw
    if (envl.stars <= 0.5f) return;
    V.starNext -= dt;
    if (V.starNext <= 0.0f){
        float a = rng_range(&V.rng, 0.0f, 6.2832f);
        float y = rng_range(&V.rng, 0.35f, 0.85f);
        float rr = sqrtf(1.0f - y*y);
        V.starDir = (Vector3){ rr*cosf(a), y, rr*sinf(a) };
        V.starT = 0.001f;
        V.starNext = rng_range(&V.rng, 90.0f, 300.0f);
    }
}

// -------- road events --------

static void road_event_update(float dt){
    if (V.deer.active){
        V.deer.s += V.deer.vS*dt;
        // stops steering left once past the far verge, then just runs on;
        // deactivation happens behind the camera, so it never pops out
        if (V.deer.lat > -22.0f) V.deer.lat += V.deer.vLat*dt;
        V.deer.phase += dt;
        if (V.deer.s < traffic_ego_s() - 60.0f)
            V.deer.active = 0;
        return;
    }
    V.roadNext -= dt;
    if (V.roadNext > 0.0f) return;
    V.roadNext = rng_range(&V.rng, 240.0f, 600.0f);

    // deer needs forest or mountain ahead; without it the pick range stops
    // before the deer branch so no trigger is wasted
    int zoneAhead = road_zone_at(traffic_ego_s() + 220.0f);
    int deerOk = (zoneAhead == ZN_FOREST || zoneAhead == ZN_MOUNTAIN);
    int pick = rng_int(&V.rng, deerOk ? 100 : 65);
    if (pick < 40){
        // truck convoy: matched grey semi tractors running the slow lane
        static const Color convoy[4] = {
            { 226, 226, 230, 255 }, { 218, 218, 224, 255 },
            { 226, 226, 230, 255 }, { 214, 214, 220, 255 }
        };
        traffic_spawn_group(VT_TRUCK, 4, 22.0f, 1, 1, convoy);
    } else if (pick < 65){
        // classic-car run: vintage two-tone sedans in the fast lane
        static const Color rally[3] = {
            { 236, 227, 196, 255 }, { 146, 58, 40, 255 }, { 46, 96, 94, 255 }
        };
        traffic_spawn_group(VT_SEDAN, 3, 24.0f, 0, 1, rally);
    } else {
        // deer breaks cover: spawns far ahead and clears both carriageways
        // long before the ego closes the distance (the ego never brakes)
        V.deer.active = 1;
        V.deer.s = traffic_ego_s() + 220.0f;
        V.deer.lat = 15.0f;
        V.deer.vLat = -8.5f;
        V.deer.vS = 2.0f;
        V.deer.phase = 0.0f;
    }
}

// -------- lightning --------

static void bolt_fire(void){
    V.boltGapT = rng_range(&V.boltRng, 1.5f, 4.0f);
    V.boltLeft--;
    V.boltT = 0.35f;
    V.boltRe = rng_chance(&V.boltRng, 30);
    V.boltA = (Vector2){ rng_range(&V.boltRng, 0.15f, 0.85f), 0.04f };
    V.boltB = (Vector2){ clampf(V.boltA.x + rng_range(&V.boltRng, -0.12f, 0.12f), 0.05f, 0.95f), 0.45f };
    V.boltSegs = 5 + rng_int(&V.boltRng, 5);
    V.boltSeed = (uint32_t)rng_u64(&V.boltRng);
}

static void bolt_update(float dt){
    if (V.boltT > 0.0f){
        V.boltT -= dt;
        // one re-strike partway through the decay keeps the flash alive
        if (V.boltRe && V.boltT < 0.15f){
            V.boltRe = 0;
            V.boltT = 0.30f;
        }
        return;
    }
    if (V.boltLeft > 0){
        V.boltGapT -= dt;
        if (V.boltGapT <= 0.0f) bolt_fire();
        return;
    }
    // storms only roll while the rain is solidly down; the countdown holds
    // in drizzle or clear sky, so an episode usually carries one cluster
    if (weather_rain() > 0.5f){
        V.boltNext -= dt;
        if (V.boltNext <= 0.0f){
            V.boltLeft = 2 + rng_int(&V.boltRng, 3);
            V.boltGapT = 0.0f;
            V.boltNext = rng_range(&V.boltRng, 100.0f, 200.0f);
        }
    }
}

// -------- police traffic stop --------

static void stop_update(float dt){
    if (V.stopActive){
        V.stopClock += dt;
        if (V.stopS < traffic_ego_s() - 60.0f){
            V.stopActive = 0;
            V.stopNext = rng_range(&V.stopRng, 900.0f, 2400.0f);
        }
        return;
    }
    V.stopNext -= dt;
    if (V.stopNext > 0.0f) return;
    V.stopNext = rng_range(&V.stopRng, 900.0f, 2400.0f);
    if (envIndoor > 0.3f) return;   // never stage a stop inside a tunnel
    V.stopActive = 1;
    V.stopS = traffic_ego_s() + 260.0f;
    V.stopClock = 0.0f;
}

static void stop_draw(void){
    if (!V.stopActive) return;
    float d = V.stopS - traffic_ego_s();
    if (d > 420.0f || d < -30.0f) return;

    float lights = envl.lightsOn;
    float wet = weather_wet();

    // stopped sedan on the right verge, brake lights and hazards on
    Vector3 pos, fwd, right, up;
    road_frame_at(V.stopS, 17.4f, 1, &pos, &fwd, &right, &up);
    (void)up;
    vehicle_draw(pos, fwd, right, VT_SEDAN, (Color){ 36, 40, 46, 255 },
                 (Color){ 200, 202, 206, 255 }, lights, 1.0f, wet, 0, 0);
    if (fmodf(V.stopClock*1.4f, 1.0f) < 0.55f){
        Color amber = { 255, 178, 48, 255 };
        Vector3 a = Vector3Add(pos, Vector3Scale(right, 0.62f));
        Vector3 b = Vector3Add(pos, Vector3Scale(right, -0.62f));
        glow_add((Vector3){ a.x, a.y + 0.80f, a.z }, amber, 0.24f, 1.0f, 0.38f);
        glow_add((Vector3){ b.x, b.y + 0.80f, b.z }, amber, 0.24f, 1.0f, 0.38f);
    }

    // police sedan angled in behind, roof bar strobing red/blue
    Vector3 pp, pf, pr, pu;
    road_frame_at(V.stopS - 12.0f, 18.0f, 1, &pp, &pf, &pr, &pu);
    (void)pu;
    vehicle_draw(pp, pf, pr, VT_SEDAN, (Color){ 238, 240, 244, 255 },
                 (Color){ 30, 34, 44, 255 }, 1.0f, 0.0f, wet, 0, 0);
    Vector3 barC = Vector3Add(pp, (Vector3){ 0, 1.44f, 0 });
    Vector3 barR = Vector3Add(barC, Vector3Scale(pr, 0.30f));
    Vector3 barL = Vector3Add(barC, Vector3Scale(pr, -0.30f));
    Vector3 upW = { 0, 1, 0 };
    Color rc = { 255, 60, 50, 255 }, bc = { 70, 120, 255, 255 };
    geo_box(barR, pr, upW, pf, 0.26f, 0.07f, 0.10f, rc, 1.0f);
    geo_box(barL, pr, upW, pf, 0.26f, 0.07f, 0.10f, bc, 1.0f);
    int redOn = fmodf(V.stopClock*4.0f, 1.0f) < 0.5f;
    glow_add(redOn ? barR : barL, redOn ? rc : bc, 0.42f, 1.0f, 0.55f);
}

// -------- fireworks over the city --------

static void fw_launch(void){
    int slot = -1;
    for (int i = 0; i < 3; i++)
        if (V.fw[i].t < 0.0f){ slot = i; break; }
    V.fwLeft--;
    if (slot < 0) return;
    static const Color pal[4] = {
        { 255, 196, 90, 255 }, { 255, 96, 70, 255 },
        { 120, 200, 255, 255 }, { 200, 160, 255, 255 }
    };
    float lat = (rng_chance(&V.fwRng, 50) ? 1.0f : -1.0f)*rng_range(&V.fwRng, 40.0f, 120.0f);
    float s = traffic_ego_s() + rng_range(&V.fwRng, 300.0f, 500.0f);
    V.fw[slot].t = 0.0f;
    V.fw[slot].dur = rng_range(&V.fwRng, 1.0f, 1.4f);
    V.fw[slot].c = road_point(s, lat, rng_range(&V.fwRng, 60.0f, 120.0f));
    V.fw[slot].col = pal[rng_int(&V.fwRng, 4)];
    V.fw[slot].seed = (uint32_t)rng_u64(&V.fwRng);
    V.fw[slot].n = 8 + rng_int(&V.fwRng, 5);
}

static void fw_update(float dt){
    for (int i = 0; i < 3; i++){
        if (V.fw[i].t < 0.0f) continue;
        V.fw[i].t += dt;
        if (V.fw[i].t >= V.fw[i].dur) V.fw[i].t = -1.0f;
    }
    if (V.fwLeft > 0){
        V.fwGapT -= dt;
        if (V.fwGapT <= 0.0f){
            fw_launch();
            V.fwGapT = rng_range(&V.fwRng, 0.8f, 1.6f);
        }
        return;
    }
    // shows only roll while a city stretch is ahead and the sky is dark
    if (road_zone_at(traffic_ego_s() + 400.0f) == ZN_CITY &&
        envl.stars > 0.6f && envIndoor < 0.3f){
        V.fwNext -= dt;
        if (V.fwNext <= 0.0f){
            V.fwLeft = 4 + rng_int(&V.fwRng, 4);
            V.fwGapT = 0.0f;
            V.fwNext = rng_range(&V.fwRng, 60.0f, 160.0f);
        }
    }
}

static void fw_draw(void){
    for (int b = 0; b < 3; b++){
        if (V.fw[b].t < 0.0f) continue;
        float u = V.fw[b].t/V.fw[b].dur;
        float fade = 1.0f - u;
        float rad = (14.0f + 12.0f*lat_f(V.fw[b].seed ^ 1u))*u;
        float droop = 1.5f*V.fw[b].t*V.fw[b].t;
        for (int i = 0; i < V.fw[b].n; i++){
            float ang = 6.2832f*(float)i/(float)V.fw[b].n;
            float el = (lat_f(V.fw[b].seed + (uint32_t)i*7u) - 0.35f)*1.6f;
            Vector3 p = Vector3Add(V.fw[b].c, (Vector3){
                cosf(ang)*cosf(el)*rad,
                sinf(el)*rad - droop,
                sinf(ang)*cosf(el)*rad });
            glow_add(p, V.fw[b].col, 0.55f, 1.0f, 0.85f*fade);
        }
        if (V.fw[b].t < 0.15f)
            glow_add(V.fw[b].c, (Color){ 255, 240, 210, 255 }, 1.6f, 1.0f,
                     0.5f*(1.0f - V.fw[b].t/0.15f));
    }
}

void events_update(float dt){
    star_update(dt);
    road_event_update(dt);
    bolt_update(dt);
    stop_update(dt);
    fw_update(dt);
}

void events_draw(void){
    stop_draw();
    fw_draw();
    if (!V.deer.active) return;
    float d = V.deer.s - traffic_ego_s();
    if (d > 420.0f || d < -30.0f) return;

    Vector3 pos, fwd, right, up;
    road_frame_at(V.deer.s, V.deer.lat, 1, &pos, &fwd, &right, &up);
    // travels toward negative lat, so the body axis runs along -right
    Vector3 ax = Vector3Scale(right, -1.0f);
    Vector3 base = pos;
    float bob = fabsf(sinf(V.deer.phase*9.0f))*0.14f;

    Color hide = { 96, 70, 48, 255 };
    Vector3 c = Vector3Add(base, (Vector3){ 0, 0.85f + bob, 0 });
    geo_box(c, ax, up, fwd, 0.62f, 0.32f, 0.26f, hide, 0.0f);                    // body
    geo_box(Vector3Add(c, Vector3Add(Vector3Scale(ax, 0.72f), (Vector3){ 0, 0.45f, 0 })),
            ax, up, fwd, 0.09f, 0.22f, 0.09f, hide, 0.0f);                        // neck
    Vector3 headP = Vector3Add(c, Vector3Add(Vector3Scale(ax, 0.88f), (Vector3){ 0, 0.62f, 0 }));
    geo_box(headP, ax, up, fwd, 0.16f, 0.11f, 0.10f, hide, 0.0f);                 // head
    for (int k = 0; k < 4; k++){
        float fx = (k < 2) ? 0.45f : -0.45f, fz = (k & 1) ? 0.16f : -0.16f;
        Vector3 leg = Vector3Add(base,
            Vector3Add(Vector3Scale(ax, fx), Vector3Add((Vector3){ 0, 0.02f, 0 }, Vector3Scale(fwd, fz))));
        geo_cylinder(leg, Vector3Add(leg, (Vector3){ 0, 0.55f + bob, 0 }), 0.05f, 0.04f, 4,
                     col_scale(hide, 0.85f), 0.0f);
    }
    if (envl.lightsOn > 0.3f)
        glow_add(Vector3Add(headP, (Vector3){ 0, 0.04f, 0 }), (Color){ 255, 214, 120, 255 },
                 0.13f, 1.0f, 0.28f);                                             // eyeshine
}

// lightning flash veil and bolt, drawn over the weather pass; inside a
// tunnel only the dimmed leak of the flash remains, never the bolt
void events_draw2d(void){
    if (V.boltT <= 0.0f) return;
    int w = GetScreenWidth(), h = GetScreenHeight();
    float u = clampf(V.boltT/0.35f, 0.0f, 1.0f);
    int indoor = envIndoor > 0.5f;
    DrawRectangle(0, 0, w, h, col_a((Color){ 210, 224, 255, 255 },
                                    0.30f*u*(indoor ? 0.3f : 1.0f)));
    if (indoor || V.boltT < 0.35f - 0.12f) return;
    Vector2 prev = { V.boltA.x*(float)w, V.boltA.y*(float)h };
    for (int i = 1; i <= V.boltSegs; i++){
        float t = (float)i/(float)V.boltSegs;
        float j = (i < V.boltSegs)
            ? (lat_f(V.boltSeed + (uint32_t)i*13u) - 0.5f)*0.10f*(float)w
              *(1.0f - fabsf(2.0f*t - 1.0f))
            : 0.0f;
        Vector2 p = { lerpf(V.boltA.x, V.boltB.x, t)*(float)w + j,
                      lerpf(V.boltA.y, V.boltB.y, t)*(float)h };
        DrawLineEx(prev, p, 2.5f, col_a((Color){ 235, 240, 255, 255 }, u));
        prev = p;
    }
}
