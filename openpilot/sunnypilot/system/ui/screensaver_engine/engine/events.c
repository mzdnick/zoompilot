#include "events.h"
#include "road.h"
#include "traffic.h"
#include "vehicle.h"
#include "rendering.h"
#include "environment.h"
#include "util.h"
#include <string.h>

typedef struct {
    int   active;
    float s, lat, vLat, vS;
    float phase;      // gallop bob clock
} Deer;

static struct {
    Rng     rng;
    float   starNext, starT;
    Vector3 starDir;
    float   roadNext;
    Deer    deer;
} V;

void events_init(uint64_t seed){
    memset(&V, 0, sizeof(V));
    V.rng.s = mix_seed(seed, 0xBEEF);
    V.starNext = rng_range(&V.rng, 40.0f, 200.0f);
    V.roadNext = rng_range(&V.rng, 180.0f, 420.0f);
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

void events_update(float dt){
    star_update(dt);
    road_event_update(dt);
}

void events_draw(void){
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
