#include "raylib.h"
#include "raymath.h"
#include "util.h"
#include "road.h"
#include "world.h"
#include "traffic.h"
#include "environment.h"
#include "weather.h"
#include "rendering.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    uint64_t    seed;
    int         fullscreen, vsync, msaa;
    int         width, height;
    float       dayLength, startTime;
    int         weatherForce;
    float       seek;
    const char *shot;
    const char *scan;
    const char *burst;
    const char *showcase;
    int         burstN;
    int         bench;
    int         zones;
} Args;

static void usage(void){
    printf(
        "Infinite Drive - procedural 3D screensaver\n"
        "  --seed N          world seed (default: random)\n"
        "  --res WxH         window size (default 1280x720)\n"
        "  --fullscreen      start fullscreen\n"
        "  --day-length S    seconds for a full day cycle (default 480)\n"
        "  --start-time T    start time of day, 0..1 (default 0.93, late night)\n"
        "  --weather NAME    start weather: clear, rain, fog, snow\n"
        "Testing flags:\n"
        "  --seek S          fast-forward S seconds of simulation before drawing\n"
        "  --shot PATH       save one screenshot after ~1 s, then exit\n"
        "  --scan PREFIX     save screenshots at t=35,120,300,600,960,1440 s, then exit\n"
        "  --bench N         run N frames without vsync, print average frame time\n"
        "  --burst N PREFIX  save N consecutive frame screenshots, then exit\n"
        "  --showcase PATH  render a parked row of one vehicle per type, save and exit\n"
        "  --zones          print the zone layout (type, range, features) and exit\n"
        "Keys: ESC quit, F11 fullscreen, F3 fps\n");
}

static void parse_args(int argc, char **argv, Args *a){
    memset(a, 0, sizeof(*a));
    a->seed = (uint64_t)time(NULL);
    a->width = 1280; a->height = 720;
    a->dayLength = 480.0f; a->startTime = 0.93f;
    a->weatherForce = -1; a->vsync = 1; a->msaa = 1; a->seek = 0.0f;
    for (int i = 1; i < argc; i++){
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) a->seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--res") && i + 1 < argc){
            sscanf(argv[++i], "%dx%d", &a->width, &a->height);
            if (a->width < 320) a->width = 320;
            if (a->height < 200) a->height = 200;
        }
        else if (!strcmp(argv[i], "--fullscreen")) a->fullscreen = 1;
        else if (!strcmp(argv[i], "--day-length") && i + 1 < argc) a->dayLength = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--start-time") && i + 1 < argc) a->startTime = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--weather") && i + 1 < argc){
            const char *wn = argv[++i];
            a->weatherForce = !strcmp(wn, "clear") ? W_CLEAR : !strcmp(wn, "rain") ? W_RAIN :
                              !strcmp(wn, "fog") ? W_FOG : !strcmp(wn, "snow") ? W_SNOW : -1;
        }
        else if (!strcmp(argv[i], "--seek") && i + 1 < argc) a->seek = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) a->shot = argv[++i];
        else if (!strcmp(argv[i], "--scan") && i + 1 < argc) a->scan = argv[++i];
        else if (!strcmp(argv[i], "--bench") && i + 1 < argc) a->bench = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--burst") && i + 2 < argc){
            a->burstN = atoi(argv[++i]);
            a->burst = argv[++i];
        }
        else if (!strcmp(argv[i], "--zones")) a->zones = 1;
        else if (!strcmp(argv[i], "--showcase") && i + 1 < argc) a->showcase = argv[++i];
        else if (!strcmp(argv[i], "--help")){ usage(); exit(0); }
        else if (!strcmp(argv[i], "--novsync")) a->vsync = 0;
        else if (!strcmp(argv[i], "--nomsaa")) a->msaa = 0;
    }
}

static float simT = 0.0f;

static void simulate(float dt, const Args *a){
    simT += dt;
    render_adas_time(simT);
    weather_update(dt);
    road_update(traffic_ego_s());
    traffic_update(dt);
    world_update(traffic_ego_s());
    env_update(dt);
    ui_update(dt);
    (void)a;
}

int main(int argc, char **argv){
    Args a;
    parse_args(argc, argv, &a);
    SetTraceLogLevel(LOG_WARNING);

    unsigned int flags = 0;
    if (a.msaa) flags |= FLAG_MSAA_4X_HINT;
    if (a.vsync && a.bench == 0) flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);
    InitWindow(a.width, a.height, "Infinite Drive");
    if (a.fullscreen) ToggleFullscreen();
    HideCursor();
    SetTargetFPS(0);

    render_init();
    env_init(a.seed, a.dayLength, a.startTime);
    road_init(a.seed);
    world_init(a.seed);
    traffic_init(a.seed);
    weather_init(a.seed, a.weatherForce);
    ui_init();

    if (a.zones){
        road_update(30000.0f);
        road_debug_zones();
        CloseWindow();
        return 0;
    }

    if (a.showcase){
        traffic_set_showcase(1);
        float base = traffic_ego_s();
        int fr = 0;
        while (!WindowShouldClose()){
            float dt = GetFrameTime();
            if (dt > 0.05f) dt = 0.05f;
            simulate(dt, &a);
            Camera3D cam = { 0 };
            cam.position = road_point(base + 2.0f, 14.6f, 2.6f);
            cam.target = road_point(base + 32.0f, 10.4f, 1.1f);
            cam.up = (Vector3){ 0, 1, 0 };
            cam.fovy = 55.0f;
            cam.projection = CAMERA_PERSPECTIVE;
            BeginDrawing();
            render_frame(cam, base, dt);
            ui_draw(GetScreenWidth(), GetScreenHeight());
            fr++;
            if (fr > 40) TakeScreenshot(a.showcase);
            EndDrawing();
            if (fr > 40){
                printf("saved %s\n", a.showcase);
                break;
            }
        }
        CloseWindow();
        return 0;
    }

    // camera initial state
    float camBob = 0.0f;

    // fast-forward modes
    if (a.seek > 0.0f){
        for (float t = 0.0f; t < a.seek; t += 1.0f/30.0f) simulate(1.0f/30.0f, &a);
    }

    if (a.scan){
        static const float times[6] = { 35, 120, 300, 600, 960, 1440 };
        char path[512];
        for (int k = 0; k < 6; k++){
            while (simT < times[k]) simulate(1.0f/30.0f, &a);
            for (int j = 0; j < 10; j++){
                float dt = GetFrameTime();
                BeginDrawing();
                Vector3 pos, fwd, right, up;
                road_frame(traffic_ego_s(), &pos, &fwd, &right, &up);
                (void)fwd; (void)right; (void)up;
                Vector3 cp = road_point(traffic_ego_s(), traffic_ego_lat(), 1.35f);
                Vector3 lt = road_point(traffic_ego_s() + 30.0f + traffic_ego_v()*0.35f,
                                        traffic_ego_lat()*0.8f, 1.15f);
                Camera3D cam = { 0 };
                cam.position = cp; cam.target = lt;
                cam.up = (Vector3){ 0, 1, 0 };
                cam.fovy = 64.0f; cam.projection = CAMERA_PERSPECTIVE;
                render_frame(cam, traffic_ego_s(), dt);
                ui_draw(GetScreenWidth(), GetScreenHeight());
                EndDrawing();
                if (WindowShouldClose()) return 0;
            }
            snprintf(path, sizeof(path), "%s_%03d.png", a.scan, (int)times[k]);
            TakeScreenshot(path);
            printf("saved %s\n", path);
        }
        CloseWindow();
        return 0;
    }

    int frames = 0, burstSaved = 0;
    double benchStart = 0.0;

    while (!WindowShouldClose()){
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        if (IsKeyPressed(KEY_ESCAPE)) break;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_F3)) ui_fps_toggle();

        simulate(dt, &a);

        // camera: driver's eye in the ego vehicle
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

        BeginDrawing();
        render_frame(cam, egoS, dt);
        ui_draw(GetScreenWidth(), GetScreenHeight());
        frames++;

        // capture inside the drawing block: after EndDrawing the back buffer
        // is undefined on macOS and readback shows stale/black rows
        if (a.burst && simT > 0.3f){
            char path[512];
            snprintf(path, sizeof(path), "%s_%03d.png", a.burst, burstSaved);
            TakeScreenshot(path);
            burstSaved++;
        }
        EndDrawing();

        if (a.bench > 0){
            if (frames == 1) benchStart = GetTime();
            if (frames >= a.bench){
                double el = GetTime() - benchStart;
                printf("[bench] frames=%d avg=%.2f ms (%.0f fps)\n",
                       frames, el*1000.0/frames, frames/el);
                break;
            }
        }
        if (a.shot && frames > 30){
            TakeScreenshot(a.shot);
            printf("saved %s\n", a.shot);
            break;
        }
        if (burstSaved >= a.burstN && a.burst){
            printf("saved %d burst frames\n", burstSaved);
            break;
        }
    }

    CloseWindow();
    return 0;
}
