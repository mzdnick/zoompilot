#include "rendering.h"
#include "rlgl.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "environment.h"
#include "weather.h"
#include "road.h"
#include "world.h"
#include "traffic.h"
#include "events.h"

// -------- per-frame shading context --------
static Vector3 shPos, shFwd, shRight, shUp;       // camera basis (glow billboards)
static Vector3 shConeFwd, shConeRight;            // road basis (headlight cone)

// -------- glow / flatspot / sign queues --------
typedef struct { Vector3 p; Color c; float size, ys, alpha; } Glow;
typedef struct { Vector3 p, right, fwd; float hw, hl; Color c; float alpha; } Flat;
typedef struct { int atlas; Vector3 b, r, t, l; } SignQ;

#define GLOW_MAX 320
#define FLAT_MAX 160
#define SIGN_MAX 64
#define REFL_MAX 240

static Glow  glows[GLOW_MAX];  static int nGlow;
static Flat  flats[FLAT_MAX];   static int nFlat;
static SignQ signs[SIGN_MAX];   static int nSign;
static Glow  refls[REFL_MAX];  static int nRefl;

static Texture2D whiteTex, glowTex, signAtlas;
// one table drives both the painted rects and the UV layout, so pixels and
// UVs can never drift apart; bg.a == 0 means the panel paints no background
static const struct { int x, y, w, h; Color bg; float emis; } SA_PANEL[SA_COUNT] = {
    [SA_EXIT_BIG]   = {   0,   0, 340, 122, {  16,  96,  52, 255 }, 0.28f },
    [SA_EXIT_SMALL] = { 350,   0, 300, 104, {  16,  96,  52, 255 }, 0.28f },
    [SA_SPEED]      = { 660,   0, 118, 118, {   0,   0,   0,   0 }, 0.28f },
    [SA_FUEL]       = { 790,   0, 118, 118, {  30,  70, 140, 255 }, 0.28f },
    [SA_BILL1]      = {   0, 128, 500, 120, {  24,  26,  34, 255 }, 0.80f },
    [SA_BILL2]      = { 510, 128, 500, 120, {  30,  24,  34, 255 }, 0.80f },
    [SA_BILL3]      = {   0, 256, 500, 120, {  30,  32,  42, 255 }, 0.80f },
};
static float SA_UV[SA_COUNT][4];   // filled from SA_PANEL in render_init

void glow_add(Vector3 p, Color c, float size, float yStretch, float alpha){
    if (nGlow >= GLOW_MAX) return;
    glows[nGlow++] = (Glow){ p, c, size, yStretch, alpha };
}
void refl_add(Vector3 surfaceP, Color c, float size, float yStretch, float alpha){
    if (nRefl >= REFL_MAX) return;
    refls[nRefl++] = (Glow){ surfaceP, c, size, yStretch, alpha };
}
void flatspot_add(Vector3 p, Vector3 right, Vector3 fwd, float hw, float hl, Color c, float alpha){
    if (nFlat >= FLAT_MAX) return;
    flats[nFlat++] = (Flat){ p, right, fwd, hw, hl, c, alpha };
}
void sign_add(int atlas, Vector3 bl, Vector3 br, Vector3 tr, Vector3 tl){
    if (nSign >= SIGN_MAX) return;
    signs[nSign++] = (SignQ){ atlas, bl, br, tr, tl };
}

// -------- lighting and fog --------

static Color shade_vert(Vector3 n, Vector3 wp, Color alb, float emis){
    float ndl = Vector3DotProduct(n, envl.sunDir);
    if (ndl < 0.0f) ndl = 0.0f;
    float sr = (float)envl.ambient.r + (float)envl.sunCol.r*envl.sunI*ndl;
    float sg = (float)envl.ambient.g + (float)envl.sunCol.g*envl.sunI*ndl;
    float sb = (float)envl.ambient.b + (float)envl.sunCol.b*envl.sunI*ndl;

    // ego headlight cone, road-relative so it stays on the lane ahead
    Vector3 d = Vector3Subtract(wp, shPos);
    float dist = Vector3Length(d);
    float ahead = Vector3DotProduct(d, shConeFwd);
    if (envl.lightsOn > 0.02f && ahead < 150.0f){
        float latr = fabsf(Vector3DotProduct(d, shConeRight));
        if (latr < 12.0f){
            // hold full strength at and behind the camera plane: a quad
            // centroid crossing it mid-pass must keep its lit look until
            // the face leaves the view - popping dark and fading both read
            // wrong, so the cutoff only applies to genuinely lit distance
            float cone = 1.0f - ahead/150.0f;
            if (cone > 1.0f) cone = 1.0f;
            cone *= cone;
            float side = 1.0f - latr/12.0f;
            float hl = envl.lightsOn*cone*side*1.6f;
            sr += 255.0f*hl;
            sg += 237.0f*hl;
            sb += 199.0f*hl;
        }
    }

    float lr = (float)alb.r*sr/255.0f;
    float lg = (float)alb.g*sg/255.0f;
    float lb = (float)alb.b*sb/255.0f;

    if (emis > 0.001f){
        lr = lerpf(lr, alb.r, emis);
        lg = lerpf(lg, alb.g, emis);
        lb = lerpf(lb, alb.b, emis);
    }

    // fog, rational approximation of exp(-x^2)
    float x = dist*envl.fogDensity*1.7f;
    float f = (x*x)/(1.0f + x*x);
    f *= (1.0f - 0.62f*emis);
    lr = lerpf(lr, envl.fogColor.r, f);
    lg = lerpf(lg, envl.fogColor.g, f);
    lb = lerpf(lb, envl.fogColor.b, f);

    Color c;
    c.r = cuc((int)lr); c.g = cuc((int)lg); c.b = cuc((int)lb); c.a = 255;
    return c;
}

// -------- immediate geometry --------

static void emit_tri(Vector3 a, Vector3 b, Vector3 c, Color col){
    rlColor4ub(col.r, col.g, col.b, col.a);
    rlVertex3f(a.x, a.y, a.z);
    rlVertex3f(b.x, b.y, b.z);
    rlVertex3f(c.x, c.y, c.z);
}

// two triangles over one texture rect; the shared emitter for every textured
// pass so vertex order and color placement stay identical everywhere
static void emit_tex6(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col,
                      float u0, float v0, float u1, float v1){
    rlColor4ub(col.r, col.g, col.b, col.a);
    rlTexCoord2f(u0, v1); rlVertex3f(a.x, a.y, a.z);
    rlTexCoord2f(u1, v1); rlVertex3f(b.x, b.y, b.z);
    rlTexCoord2f(u1, v0); rlVertex3f(c.x, c.y, c.z);
    rlColor4ub(col.r, col.g, col.b, col.a);
    rlTexCoord2f(u0, v1); rlVertex3f(a.x, a.y, a.z);
    rlTexCoord2f(u1, v0); rlVertex3f(c.x, c.y, c.z);
    rlTexCoord2f(u0, v0); rlVertex3f(d.x, d.y, d.z);
}

// quad normal and centroid, shared by the lit quad and the sign pass
static void quad_frame(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 *n, Vector3 *cen){
    *n = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a));
    float len = Vector3Length(*n);
    if (len > 1e-6f) *n = Vector3Scale(*n, 1.0f/len);
    *cen = Vector3Scale(Vector3Add(Vector3Add(a, b), Vector3Add(c, d)), 0.25f);
}

static void emit_quad_shaded(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col, float emis){
    Vector3 n, cen;
    quad_frame(a, b, c, d, &n, &cen);
    Color sc = shade_vert(n, cen, col, emis);
    emit_tri(a, b, c, sc);
    emit_tri(a, c, d, sc);
}

void geo_quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col, float emis){
    emit_quad_shaded(a, b, c, d, col, emis);
}

void geo_box(Vector3 c, Vector3 rx, Vector3 uy, Vector3 fz,
             float hx, float hy, float hz, Color col, float emis){
    Vector3 cx = Vector3Scale(rx, hx), cy = Vector3Scale(uy, hy), cz = Vector3Scale(fz, hz);
    Vector3 a1 = Vector3Subtract(c, cx), a2 = Vector3Add(c, cx);
    Vector3 mcy = Vector3Negate(cy), mcz = Vector3Negate(cz);
    Vector3 v[8];
    v[0] = Vector3Add(Vector3Add(a1, mcy), cz);
    v[1] = Vector3Add(Vector3Add(a2, mcy), cz);
    v[2] = Vector3Add(Vector3Add(a2, cy), cz);
    v[3] = Vector3Add(Vector3Add(a1, cy), cz);
    v[4] = Vector3Add(Vector3Add(a1, mcy), mcz);
    v[5] = Vector3Add(Vector3Add(a2, mcy), mcz);
    v[6] = Vector3Add(Vector3Add(a2, cy), mcz);
    v[7] = Vector3Add(Vector3Add(a1, cy), mcz);
    emit_quad_shaded(v[0], v[1], v[2], v[3], col, emis);   // +z
    emit_quad_shaded(v[5], v[4], v[7], v[6], col, emis);   // -z
    emit_quad_shaded(v[1], v[5], v[6], v[2], col, emis);   // +x
    emit_quad_shaded(v[4], v[0], v[3], v[7], col, emis);   // -x
    emit_quad_shaded(v[3], v[2], v[6], v[7], col, emis);   // +y
    emit_quad_shaded(v[4], v[5], v[1], v[0], col, emis);   // -y
}

void geo_cylinder(Vector3 a, Vector3 b, float ra, float rb, int sides, Color col, float emis){
    if (sides < 3 || sides > 15) return;
    Vector3 ax = Vector3Subtract(b, a);
    float len = Vector3Length(ax);
    if (len < 1e-6f) return;
    ax = Vector3Scale(ax, 1.0f/len);
    Vector3 ref = fabsf(ax.y) < 0.9f ? (Vector3){ 0, 1, 0 } : (Vector3){ 1, 0, 0 };
    Vector3 u = Vector3Normalize(Vector3CrossProduct(ax, ref));
    Vector3 w = Vector3CrossProduct(ax, u);
    // unit ring computed once; sides, caps, and cones all reuse it
    Vector3 ring[16];
    for (int j = 0; j <= sides; j++){
        float ang = 6.2832f*(float)j/(float)sides;
        ring[j] = Vector3Add(Vector3Scale(u, cosf(ang)), Vector3Scale(w, sinf(ang)));
    }
    for (int i = 1; i <= sides; i++){
        Vector3 qa = Vector3Add(a, Vector3Scale(ring[i - 1], ra));
        Vector3 qb = Vector3Add(a, Vector3Scale(ring[i], ra));
        Vector3 vc = Vector3Add(b, Vector3Scale(ring[i], rb));
        Vector3 vd = Vector3Add(b, Vector3Scale(ring[i - 1], rb));
        emit_quad_shaded(qa, qb, vc, vd, col, emis);
    }
    // cap both ends so cylinders never look hollow; wind outward to survive culling
    for (int end = 0; end < 2; end++){
        Vector3 cen = end ? b : a;
        float cr = end ? rb : ra;
        Vector3 n = end ? ax : Vector3Scale(ax, -1.0f);
        if (cr < 0.001f) continue;
        Color cc = shade_vert(n, cen, col, emis);
        for (int i = 0; i < sides; i++){
            Vector3 p1 = Vector3Add(cen, Vector3Scale(ring[i], cr));
            Vector3 p2 = Vector3Add(cen, Vector3Scale(ring[i + 1], cr));
            if (end) emit_tri(p1, p2, cen, cc);
            else     emit_tri(p2, p1, cen, cc);
        }
    }
}

void geo_cone(Vector3 base, Vector3 axis, float r, float h, int sides, Color col, float emis){
    if (sides < 3 || sides > 15) return;
    Vector3 ax = Vector3Normalize(axis);
    Vector3 tip = Vector3Add(base, Vector3Scale(ax, h));
    Vector3 ref = fabsf(ax.y) < 0.9f ? (Vector3){ 0, 1, 0 } : (Vector3){ 1, 0, 0 };
    Vector3 u = Vector3Normalize(Vector3CrossProduct(ax, ref));
    Vector3 w = Vector3CrossProduct(ax, u);
    Vector3 ring[16];
    for (int j = 0; j <= sides; j++){
        float ang = 6.2832f*(float)j/(float)sides;
        ring[j] = Vector3Add(Vector3Scale(u, cosf(ang)), Vector3Scale(w, sinf(ang)));
    }
    Vector3 prev = base;
    for (int i = 0; i <= sides; i++){
        Vector3 p = Vector3Add(base, Vector3Scale(ring[i], r));
        if (i > 0) emit_tri(prev, p, tip, shade_vert(ring[i], p, col, emis));
        prev = p;
    }
}

// -------- textures built at init --------

static void build_sign_atlas(void){
    Image img = GenImageColor(1024, 384, (Color){ 0, 0, 0, 0 });
    for (int sa = 0; sa < SA_COUNT; sa++)
        if (SA_PANEL[sa].bg.a > 0)
            ImageDrawRectangle(&img, SA_PANEL[sa].x, SA_PANEL[sa].y, SA_PANEL[sa].w, SA_PANEL[sa].h, SA_PANEL[sa].bg);

    ImageDrawRectangleLines(&img, (Rectangle){ 4, 4, 332, 114 }, 4, WHITE);
    ImageDrawText(&img, "ZOOM CITY", 22, 16, 30, WHITE);
    ImageDrawText(&img, "24", 288, 16, 30, WHITE);
    ImageDrawText(&img, "Meridian", 22, 62, 26, WHITE);
    ImageDrawText(&img, "2", 296, 62, 26, WHITE);

    ImageDrawRectangleLines(&img, (Rectangle){ 354, 4, 292, 96 }, 3, WHITE);
    ImageDrawText(&img, "Meridian", 368, 8, 26, WHITE);
    ImageDrawText(&img, "4", 606, 8, 26, WHITE);
    ImageDrawText(&img, "EXIT 12", 368, 44, 26, WHITE);
    ImageDrawText(&img, "ZOOMPILOT HQ", 368, 76, 20, WHITE);

    ImageDrawCircle(&img, 719, 59, 56, WHITE);
    ImageDrawCircle(&img, 719, 59, 51, (Color){ 20, 20, 24, 255 });
    ImageDrawText(&img, "SPEED", 693, 28, 14, WHITE);
    ImageDrawText(&img, "65", 700, 44, 30, WHITE);

    ImageDrawRectangleLines(&img, (Rectangle){ 794, 4, 110, 110 }, 3, WHITE);
    ImageDrawText(&img, "FUEL", 806, 40, 28, WHITE);
    ImageDrawText(&img, "1.89", 812, 74, 20, (Color){ 200, 220, 255, 255 });

    ImageDrawCircle(&img, 70, 188, 34, (Color){ 255, 150, 60, 255 });
    ImageDrawRectangle(&img, 130, 172, 300, 8, (Color){ 120, 200, 255, 255 });
    ImageDrawText(&img, "O R B I T", 130, 190, 30, WHITE);

    ImageDrawRectangle(&img, 540, 150, 8, 76, (Color){ 160, 120, 255, 255 });
    ImageDrawRectangle(&img, 560, 158, 6, 60, (Color){ 90, 200, 180, 255 });
    ImageDrawText(&img, "N O V A", 596, 176, 34, WHITE);
    ImageDrawText(&img, "drive calm", 596, 214, 16, (Color){ 180, 180, 190, 255 });

    // ZoomPilot billboard: radar glyph, name, tagline
    ImageDrawCircleLines(&img, 78, 316, 44, (Color){ 128, 138, 158, 255 });
    ImageDrawCircleLines(&img, 78, 316, 28, (Color){ 160, 170, 190, 255 });
    ImageDrawCircleLines(&img, 78, 316, 12, (Color){ 130, 215, 255, 255 });
    ImageDrawCircle(&img, 78, 316, 4, (Color){ 255, 180, 90, 255 });
    ImageDrawLine(&img, 78, 316, 118, 278, (Color){ 130, 215, 255, 255 });
    ImageDrawCircle(&img, 120, 276, 5, (Color){ 140, 220, 255, 255 });
    ImageDrawText(&img, "Z O O M P I L O T", 150, 288, 28, WHITE);
    ImageDrawText(&img, "always driving", 150, 330, 16, (Color){ 205, 212, 224, 255 });

    signAtlas = LoadTextureFromImage(img);
    GenTextureMipmaps(&signAtlas);
    SetTextureFilter(signAtlas, TEXTURE_FILTER_TRILINEAR);
    UnloadImage(img);
}

void render_init(void){
    for (int i = 0; i < SA_COUNT; i++){
        SA_UV[i][0] = (float)SA_PANEL[i].x/1024.0f;
        SA_UV[i][1] = (float)SA_PANEL[i].y/384.0f;
        SA_UV[i][2] = (float)SA_PANEL[i].w/1024.0f;
        SA_UV[i][3] = (float)SA_PANEL[i].h/384.0f;
    }

    Image w = GenImageColor(1, 1, WHITE);
    whiteTex = LoadTextureFromImage(w);
    UnloadImage(w);

    Image g = GenImageColor(64, 64, (Color){ 0, 0, 0, 0 });
    Color *px = (Color *)g.data;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++){
            float dx = (x - 31.5f)/32.0f, dy = (y - 31.5f)/32.0f;
            float r = sqrtf(dx*dx + dy*dy);
            float a = clampf(1.0f - r, 0.0f, 1.0f);
            a = a*a*a;
            px[y*64 + x] = (Color){ 255, 255, 255, cuc((int)(a*255.0f)) };
        }
    glowTex = LoadTextureFromImage(g);
    SetTextureFilter(glowTex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(g);

    build_sign_atlas();
}

// -------- pass flushes --------

static void flush_signs(void){
    for (int i = 0; i < nSign; i++){
        SignQ *s = &signs[i];
        const float *uv = SA_UV[s->atlas];
        float u0 = uv[0], v0 = uv[1], u1 = uv[0] + uv[2], v1 = uv[1] + uv[3];
        Vector3 n, cen;
        quad_frame(s->b, s->r, s->t, s->l, &n, &cen);
        // billboards are backlit at night; road signs keep the dimmer tint
        Color c = shade_vert(n, cen, (Color){ 255, 255, 255, 255 }, SA_PANEL[s->atlas].emis);
        emit_tex6(s->b, s->r, s->t, s->l, c, u0, v0, u1, v1);
    }
}

// camera-facing additive quad for one glow entry
static void draw_glow_billboard(const Glow *g, Color ca){
    Vector3 r = Vector3Scale(shRight, g->size);
    Vector3 u = Vector3Scale(shUp, g->size*g->ys);
    Vector3 a = Vector3Add(Vector3Subtract(g->p, r), Vector3Negate(u));
    Vector3 b = Vector3Add(Vector3Add(g->p, r), Vector3Negate(u));
    Vector3 c = Vector3Add(Vector3Add(g->p, r), u);
    Vector3 d = Vector3Add(Vector3Subtract(g->p, r), u);
    emit_tex6(a, b, c, d, ca, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void flush_glow_pass(void){
    for (int i = 0; i < nFlat; i++){
        Flat *f = &flats[i];
        Vector3 a = Vector3Add(f->p, Vector3Add(Vector3Scale(f->fwd, -f->hl), Vector3Scale(f->right, -f->hw)));
        Vector3 b = Vector3Add(f->p, Vector3Add(Vector3Scale(f->fwd, -f->hl), Vector3Scale(f->right,  f->hw)));
        Vector3 c = Vector3Add(f->p, Vector3Add(Vector3Scale(f->fwd,  f->hl), Vector3Scale(f->right,  f->hw)));
        Vector3 d = Vector3Add(f->p, Vector3Add(Vector3Scale(f->fwd,  f->hl), Vector3Scale(f->right, -f->hw)));
        emit_tex6(a, b, c, d, col_a(f->c, f->alpha), 0.0f, 0.0f, 1.0f, 1.0f);
    }
    for (int i = 0; i < nGlow; i++)
        draw_glow_billboard(&glows[i], col_a(glows[i].c, glows[i].alpha));
    // wet-road reflections: vertical additive streaks on the asphalt. The
    // entries sit just above the road plane, so the opaque road never
    // depth-occludes them, and the whole pass is skipped on a dry road.
    float wet = weather_wet();
    if (wet > 0.12f){
        for (int i = 0; i < nRefl; i++){
            Glow *g = &refls[i];
            float dist = Vector3Length(Vector3Subtract(g->p, shPos));
            if (dist > 240.0f) continue;
            float fall = 1.0f - 0.4f*clampf(dist/240.0f, 0.0f, 1.0f);
            draw_glow_billboard(g, col_a(g->c, g->alpha*wet*fall));
        }
    }
}

// -------- ADAS overlay --------

static struct { float a; float t; float pulse; } adas;

void render_adas_time(float simTime){ adas.t = simTime; }

static VehInfo  veh[32];
static int      vehN;
static int      leadIdx = -1;

static void draw_corner_ticks(Rectangle rec, float len, float thick, Color c){
    Vector2 p[4] = {
        { rec.x, rec.y }, { rec.x + rec.width, rec.y },
        { rec.x, rec.y + rec.height }, { rec.x + rec.width, rec.y + rec.height }
    };
    static const int dx[4] = { 1, -1, 1, -1 }, dy[4] = { 1, 1, -1, -1 };
    for (int i = 0; i < 4; i++){
        Vector2 ex = { p[i].x + dx[i]*len, p[i].y };
        Vector2 ey = { p[i].x, p[i].y + dy[i]*len };
        DrawLineEx(p[i], ex, thick, c);
        DrawLineEx(p[i], ey, thick, c);
    }
}

static void adas_project(float dt){
    float ph = fmodf(adas.t, 28.0f);
    float onT = 0.0f;
    if (adas.t > 8.0f) onT = (ph < 16.0f) ? 1.0f : 0.0f;
    if (onT > adas.a && adas.t > 9.5f) adas.a = onT;              // snap on after startup
    else adas.a += (onT - adas.a)*clampf(dt*1.4f, 0.0f, 1.0f);    // fade otherwise
    adas.pulse = 0.88f + 0.12f*sinf(adas.t*3.1f);

    vehN = traffic_query(veh, 32);
    leadIdx = -1;
    float best = 1e9f;
    float egoLaneLat = (traffic_ego_target_lane() == 1) ? LANE_SLOW : LANE_FAST;
    for (int i = 0; i < vehN; i++){
        if (veh[i].dir < 0) continue;
        if (fabsf(veh[i].lat - egoLaneLat) > 2.4f) continue;
        if (veh[i].distAhead < 2.0f || veh[i].distAhead > 130.0f) continue;
        if (veh[i].distAhead < best){
            best = veh[i].distAhead;
            leadIdx = i;
        }
    }
}

static void adas_draw2d(Camera3D cam, float sCam){
    int w = GetScreenWidth(), h = GetScreenHeight();
    float scale = (float)h/720.0f;

    if (adas.a < 0.02f) return;
    float A = adas.a*adas.pulse;

    static const Color cLane = { 110, 205, 255, 255 };
    static const Color cBox  = { 140, 225, 255, 255 };

    // lane edge dashes
    float egoLat = traffic_ego_lat();
    for (int side = -1; side <= 1; side += 2){
        for (int k = 0; k + 1 < 16; k += 2){
            float s1 = sCam + 8.0f + (float)k*6.0f;
            Vector3 p1 = road_point(s1, egoLat + side*LANE_W*0.5f, 0.10f);
            Vector3 p2 = road_point(s1 + 6.5f, egoLat + side*LANE_W*0.5f, 0.10f);
            Vector3 d1 = Vector3Subtract(p1, cam.position);
            Vector3 d2 = Vector3Subtract(p2, cam.position);
            if (Vector3DotProduct(d1, shFwd) < 1.0f) continue;
            if (Vector3DotProduct(d2, shFwd) < 1.0f) continue;
            Vector2 sp1 = GetWorldToScreen(p1, cam);
            Vector2 sp2 = GetWorldToScreen(p2, cam);
            float u = (float)k/16.0f;
            DrawLineEx(sp1, sp2, 1.6f*scale, col_a(cLane, A*0.28f*(1.0f - u)));
        }
    }

    // vehicle boxes
    Font f = GetFontDefault();
    for (int i = 0; i < vehN; i++){
        VehInfo *v = &veh[i];
        if (v->detect < 0.03f) continue;
        float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
        int ok = 0;
        for (int cx = -1; cx <= 1; cx += 2)
            for (int cy = 0; cy <= 1; cy++)
                for (int cz = -1; cz <= 1; cz += 2){
                    Vector3 p = Vector3Add(v->pos,
                        Vector3Add(Vector3Scale(v->fwd, cz*v->len*0.5f),
                        Vector3Add(Vector3Scale(v->right, cx*v->w*0.5f), (Vector3){ 0, cy*v->h, 0 })));
                    Vector3 d = Vector3Subtract(p, cam.position);
                    if (Vector3DotProduct(d, shFwd) < 0.5f) continue;
                    Vector2 sp = GetWorldToScreen(p, cam);
                    if (sp.x < minX) minX = sp.x;
                    if (sp.y < minY) minY = sp.y;
                    if (sp.x > maxX) maxX = sp.x;
                    if (sp.y > maxY) maxY = sp.y;
                    ok++;
                }
        if (!ok || maxX < 0 || minX > (float)w) continue;
        float ahead = v->distAhead > 0.0f ? v->distAhead : 0.0f;
        float fade = clampf(1.0f - ahead/200.0f, 0.25f, 1.0f);
        float isLead = (i == leadIdx) ? 1.35f : 1.0f;
        Rectangle rec = { minX - 3, minY - 3, (maxX - minX) + 6, (maxY - minY) + 6 };
        if (rec.width < 6) rec.width = 6;
        if (rec.height < 6) rec.height = 6;
        DrawRectangleLinesEx(rec, 1.2f, col_a(cBox, v->detect*A*0.55f*fade*isLead));
        draw_corner_ticks(rec, 7.0f*scale, 1.8f, col_a(cBox, v->detect*A*0.8f*fade*isLead));
        if (i == leadIdx && v->distAhead > 2.0f){
            const char *txt = TextFormat("LEAD  %d m", (int)(v->distAhead + 0.5f));
            Vector2 ts = MeasureTextEx(f, txt, 10, 1);
            DrawTextEx(f, txt, (Vector2){ rec.x, rec.y - ts.y - 3 }, 10, 1,
                       col_a((Color){ 220, 240, 255, 255 }, A*0.7f));
        }
    }

    // occasional small labels
    float ph = fmodf(adas.t, 30.0f);
    const char *label = NULL;
    Vector2 lpos = { (float)w*0.5f, (float)h*0.66f };
    if (ph > 13.0f && ph < 19.0f){
        label = "LANE";
        Vector3 p = road_point(sCam + 30.0f, egoLat + LANE_W*0.5f, 0.1f);
        Vector2 sp = GetWorldToScreen(p, cam);
        lpos = (Vector2){ sp.x + 10, sp.y - 6 };
    } else if (ph > 23.0f && ph < 28.0f && leadIdx >= 0){
        label = "TRACK";
        lpos = (Vector2){ (float)w*0.5f - 20, (float)h*0.42f };
    }
    if (label)
        DrawTextEx(f, label, lpos, 10, 2, col_a((Color){ 200, 235, 255, 255 }, A*0.55f));

    // small autopilot readout; the rounded MPH rarely changes at cruise
    static char autoTxt[20];
    static int autoMph = -1;
    int mph = (int)(traffic_ego_v()*2.237f + 0.5f);
    if (mph != autoMph){
        autoMph = mph;
        snprintf(autoTxt, sizeof(autoTxt), "AUTO   %d MPH", mph);
    }
    DrawTextEx(f, autoTxt, (Vector2){ 20, (float)h - 30 }, 12, 1,
               col_a((Color){ 190, 215, 235, 255 }, A*0.4f));
}

// -------- city glow on the horizon --------

static void draw_city_glow(Camera3D cam, Vector3 pos, float amt){
    if (amt < 0.03f) return;
    Vector2 sp = GetWorldToScreen(pos, cam);
    int h = GetScreenHeight();
    if (sp.y < -100 || sp.y > (float)(h + 100)) return;
    rlSetBlendMode(RL_BLEND_ADDITIVE);
    DrawCircleGradient(sp, 120.0f,
                       col_a((Color){ 255, 186, 120, 255 }, 26.0f*amt), (Color){ 0, 0, 0, 0 });
    DrawCircleGradient((Vector2){ sp.x + 40, sp.y - 8 }, 60.0f,
                       col_a((Color){ 150, 190, 255, 255 }, 20.0f*amt), (Color){ 0, 0, 0, 0 });
    rlSetBlendMode(RL_BLEND_ALPHA);
}

void render_frame(Camera3D cam, float sCam, float dt){
    Matrix m = GetCameraMatrix(cam);
    shPos   = cam.position;
    shRight = (Vector3){ m.m0, m.m4, m.m8 };
    shUp    = (Vector3){ m.m1, m.m5, m.m9 };
    shFwd   = Vector3Scale((Vector3){ m.m2, m.m6, m.m10 }, -1.0f);
    {
        Vector3 p, f, r, u;
        road_frame(sCam, &p, &f, &r, &u);
        shConeFwd = Vector3Normalize((Vector3){ f.x, 0.0f, f.z });
        shConeRight = Vector3Normalize((Vector3){ r.x, 0.0f, r.z });
    }

    ClearBackground(envl.fogColor);

    Vector3 cityPos; float cityAmt;
    world_city_glow(sCam, &cityPos, &cityAmt);
    env_draw_sky(cam, sCam, dt);
    draw_city_glow(cam, cityPos, cityAmt);

    nGlow = nFlat = nSign = nRefl = 0;

    BeginMode3D(cam);
    // face windings are mixed (geo_box inside-out, cylinder sides outward),
    // so culling eats body panels; the depth buffer alone must do occlusion
    rlDisableBackfaceCulling();
    // BeginMode3D hardcodes a 0.01 near plane; that flattens depth precision
    // at range until wheels bleed through body panels. Nothing legit renders
    // closer than 0.5 m, so tighten the near plane for the whole 3D pass.
    rlSetMatrixProjection(MatrixPerspective(cam.fovy*DEG2RAD,
        (float)GetScreenWidth()/(float)GetScreenHeight(), 0.5f, 1000.0f));
    rlSetTexture(whiteTex.id);
    rlBegin(RL_TRIANGLES);
    road_draw(sCam);
    world_draw(sCam);
    traffic_draw();
    events_draw();
    rlEnd();

    if (nSign > 0){
        rlSetTexture(signAtlas.id);
        rlBegin(RL_TRIANGLES);
        flush_signs();
        rlEnd();
    }

    rlSetTexture(glowTex.id);
    rlSetBlendMode(RL_BLEND_ADDITIVE);
    rlBegin(RL_TRIANGLES);
    flush_glow_pass();
    rlEnd();
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlSetTexture(0);

    adas_project(dt);
    EndMode3D();

    adas_draw2d(cam, sCam);
    weather_draw(cam);
}
