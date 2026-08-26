// Device/embedder entry points for the Infinite Drive screensaver engine.
// The host owns the window and the raylib context; these functions only
// advance the simulation and draw into the current context.
// Build with raylib symbols undefined so they bind to the loader's instance.
#include "raylib.h"
#include "road.h"
#include "world.h"
#include "traffic.h"
#include "environment.h"
#include "events.h"
#include "weather.h"
#include "rendering.h"
#include "ui.h"
#include "util.h"
#include <stdint.h>

static float simT = 0.0f;
static float camBob = 0.0f;
static int   inited = 0;

void id_reset(uint64_t seed);

void id_init(uint64_t seed){
    if (!inited){
        render_init();   // loads textures; must run once with a GL context
        ui_init(1);      // device runs are real trips: persist the odometer
        inited = 1;
    }
    id_reset(seed);
}

void id_reset(uint64_t seed){
    simT = 0.0f;
    camBob = 0.0f;
    env_init(seed, 480.0f, 0.93f);
    road_init(seed);
    world_init();
    traffic_init(seed);
    weather_init(seed, -1);
    events_init(seed);
}

void id_render(float dt){
    if (dt > 0.05f) dt = 0.05f;
    simT += dt;
    render_adas_time(simT);
    weather_update(dt);
    road_update(traffic_ego_s());
    events_update(dt);   // reads zones ahead; spawns settle before traffic runs
    traffic_update(dt);
    world_update(traffic_ego_s(), dt);
    env_update(dt);
    ui_update(dt);

    camBob += dt;
    float bob = sinf(camBob*1.3f)*0.025f + sinf(camBob*2.1f)*0.012f;
    float egoS = traffic_ego_s();
    float curv = road_curv_at(egoS + 40.0f);
    Vector3 pos, fwd, right, up;
    road_frame(egoS, &pos, &fwd, &right, &up);
    Vector3 cp = Vector3Add(pos, Vector3Add(Vector3Scale(right, traffic_ego_lat()),
                                            Vector3Scale(up, 1.35f + bob)));
    float lookLead = 26.0f + traffic_ego_v()*0.35f;
    float latLead = traffic_ego_lat() + clampf(curv*260.0f, -2.2f, 2.2f);
    Vector3 lt = road_point(egoS + lookLead, latLead, 1.15f + bob*0.5f);
    Vector3 viewDir = Vector3Normalize(Vector3Subtract(lt, cp));
    float roll = clampf(curv*30.0f, -0.05f, 0.05f) + sinf(camBob*0.9f)*0.004f;
    Vector3 cup = Vector3RotateByAxisAngle((Vector3){ 0, 1, 0 }, viewDir, -roll);

    Camera3D cam = { 0 };
    cam.position = cp;
    cam.target = lt;
    cam.up = cup;
    cam.fovy = 62.0f + (traffic_ego_v() - 26.0f)*0.22f;
    cam.projection = CAMERA_PERSPECTIVE;

    render_frame(cam, egoS, dt);
    ui_draw(GetScreenWidth(), GetScreenHeight());
}
