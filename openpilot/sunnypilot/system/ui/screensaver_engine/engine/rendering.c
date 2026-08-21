#include "rendering.h"
#include "rlgl.h"
#include <stddef.h>
#include "util.h"
#include "environment.h"
#include "weather.h"
#include "road.h"
#include "world.h"
#include "traffic.h"

// -------- per-frame shading context --------
static Vector3 shPos, shFwd, shRight, shUp;       // camera basis (glow billboards)
static Vector3 shConeFwd, shConeRight;            // road basis (headlight cone)
static Camera3D sCam3;

// -------- glow / flatspot / sign queues --------
typedef struct { Vector3 p; Color c; float size, ys, alpha; } Glow;
typedef struct { Vector3 p, right, fwd; float hw, hl; Color c; float alpha; } Flat;
typedef struct { int atlas; Vector3 b, r, t, l; } SignQ;

#define GLOW_MAX 320
#define FLAT_MAX 160
#define SIGN_MAX 64

static Glow  glows[GLOW_MAX];  static int nGlow;
static Flat  flats[FLAT_MAX];   static int nFlat;
static SignQ signs[SIGN_MAX];   static int nSign;

static Texture2D whiteTex, glowTex, signAtlas;
static const float SA_UV[SA_COUNT][4] = {
    { 0.0f/1024.0f,   0.0f/256.0f, 340.0f/1024.0f, 122.0f/256.0f },
    { 350.0f/1024.0f, 0.0f/256.0f, 300.0f/1024.0f, 104.0f/256.0f },
    { 660.0f/1024.0f, 0.0f/256.0f, 118.0f/1024.0f, 118.0f/256.0f },
    { 790.0f/1024.0f, 0.0f/256.0f, 118.0f/1024.0f, 118.0f/256.0f },
    { 0.0f/1024.0f,   128.0f/256.0f, 500.0f/1024.0f, 120.0f/256.0f },
    { 510.0f/1024.0f, 128.0f/256.0f, 500.0f/1024.0f, 120.0f/256.0f }
};

void glow_add(Vector3 p, Color c, float size, float yStretch, float alpha){
    if (nGlow >= GLOW_MAX) return;
    glows[nGlow++] = (Glow){ p, c, size, yStretch, alpha };
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

Color shade_vert(Vector3 n, Vector3 wp, Color alb, float emis){
    float ndl = Vector3DotProduct(n, envl.sunDir);
    if (ndl < 0.0f) ndl = 0.0f;
    float sr = (float)envl.ambient.r + (float)envl.sunCol.r*envl.sunI*ndl;
    float sg = (float)envl.ambient.g + (float)envl.sunCol.g*envl.sunI*ndl;
    float sb = (float)envl.ambient.b + (float)envl.sunCol.b*envl.sunI*ndl;

    // ego headlight cone, road-relative so it stays on the lane ahead
    Vector3 d = Vector3Subtract(wp, shPos);
    float dist = Vector3Length(d);
    float ahead = Vector3DotProduct(d, shConeFwd);
    if (envl.headlights > 0.02f && ahead > 1.0f && ahead < 150.0f){
        float latr = fabsf(Vector3DotProduct(d, shConeRight));
        if (latr < 12.0f){
            float cone = 1.0f - ahead/150.0f;
            cone *= cone;
            float side = 1.0f - latr/12.0f;
            float hl = envl.headlights*cone*side*1.6f;
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

static void emit_quad_shaded(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col, float emis){
    Vector3 n = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a));
    float len = Vector3Length(n);
    if (len > 1e-6f) n = Vector3Scale(n, 1.0f/len);
    Vector3 cen = Vector3Scale(Vector3Add(Vector3Add(a, b), Vector3Add(c, d)), 0.25f);
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
    Vector3 ax = Vector3Subtract(b, a);
    float len = Vector3Length(ax);
    if (len < 1e-6f) return;
    ax = Vector3Scale(ax, 1.0f/len);
    Vector3 ref = fabsf(ax.y) < 0.9f ? (Vector3){ 0, 1, 0 } : (Vector3){ 1, 0, 0 };
    Vector3 u = Vector3Normalize(Vector3CrossProduct(ax, ref));
    Vector3 w = Vector3CrossProduct(ax, u);
    Vector3 pa = a, pb = b;
    for (int i = 1; i <= sides; i++){
        float ang1 = 6.2832f*(float)(i - 1)/(float)sides;
        float ang2 = 6.2832f*(float)i/(float)sides;
        Vector3 d1 = Vector3Add(Vector3Scale(u, cosf(ang1)), Vector3Scale(w, sinf(ang1)));
        Vector3 d2 = Vector3Add(Vector3Scale(u, cosf(ang2)), Vector3Scale(w, sinf(ang2)));
        Vector3 qa = Vector3Add(a, Vector3Scale(d1, ra));
        Vector3 qb = Vector3Add(a, Vector3Scale(d2, ra));
        Vector3 vc = Vector3Add(b, Vector3Scale(d2, rb));
        Vector3 vd = Vector3Add(b, Vector3Scale(d1, rb));
        emit_quad_shaded(qa, qb, vc, vd, col, emis);
        pa = qa; pb = qb;
    }
    (void)pa; (void)pb;
    // cap both ends so cylinders never look hollow; wind outward to survive culling
    for (int end = 0; end < 2; end++){
        Vector3 cen = end ? b : a;
        float cr = end ? rb : ra;
        Vector3 n = end ? ax : Vector3Scale(ax, -1.0f);
        if (cr < 0.001f) continue;
        for (int i = 0; i < sides; i++){
            float a1 = 6.2832f*(float)i/(float)sides, a2 = 6.2832f*(float)(i + 1)/(float)sides;
            Vector3 d1 = Vector3Add(Vector3Scale(u, cosf(a1)), Vector3Scale(w, sinf(a1)));
            Vector3 d2 = Vector3Add(Vector3Scale(u, cosf(a2)), Vector3Scale(w, sinf(a2)));
            Color cc = shade_vert(n, cen, col, emis);
            Vector3 p1 = Vector3Add(cen, Vector3Scale(d1, cr));
            Vector3 p2 = Vector3Add(cen, Vector3Scale(d2, cr));
            if (end) emit_tri(p1, p2, cen, cc);
            else     emit_tri(p2, p1, cen, cc);
        }
    }
}

void geo_cone(Vector3 base, Vector3 axis, float r, float h, int sides, Color col, float emis){
    Vector3 ax = Vector3Normalize(axis);
    Vector3 tip = Vector3Add(base, Vector3Scale(ax, h));
    Vector3 ref = fabsf(ax.y) < 0.9f ? (Vector3){ 0, 1, 0 } : (Vector3){ 1, 0, 0 };
    Vector3 u = Vector3Normalize(Vector3CrossProduct(ax, ref));
    Vector3 w = Vector3CrossProduct(ax, u);
    Vector3 prev = base;
    for (int i = 0; i <= sides; i++){
        float ang = 6.2832f*(float)i/(float)sides;
        Vector3 dir = Vector3Add(Vector3Scale(u, cosf(ang)), Vector3Scale(w, sinf(ang)));
        Vector3 p = Vector3Add(base, Vector3Scale(dir, r));
        if (i > 0) emit_tri(prev, p, tip, shade_vert(dir, p, col, emis));
        prev = p;
    }
}

// -------- textures built at init --------

static void build_sign_atlas(void){
    Image img = GenImageColor(1024, 256, (Color){ 0, 0, 0, 0 });
    ImageDrawRectangle(&img, 0, 0, 340, 122, (Color){ 16, 96, 52, 255 });
    ImageDrawRectangleLines(&img, (Rectangle){ 4, 4, 332, 114 }, 4, WHITE);
    ImageDrawText(&img, "ZOOM CITY", 22, 16, 30, WHITE);
    ImageDrawText(&img, "24", 288, 16, 30, WHITE);
    ImageDrawText(&img, "Meridian", 22, 62, 26, WHITE);
    ImageDrawText(&img, "2", 296, 62, 26, WHITE);

    ImageDrawRectangle(&img, 350, 0, 300, 104, (Color){ 16, 96, 52, 255 });
    ImageDrawRectangleLines(&img, (Rectangle){ 354, 4, 292, 96 }, 3, WHITE);
    ImageDrawText(&img, "Meridian", 368, 12, 26, WHITE);
    ImageDrawText(&img, "4", 606, 12, 26, WHITE);
    ImageDrawText(&img, "EXIT 12", 368, 56, 26, WHITE);

    ImageDrawCircle(&img, 719, 59, 56, WHITE);
    ImageDrawCircle(&img, 719, 59, 51, (Color){ 20, 20, 24, 255 });
    ImageDrawText(&img, "SPEED", 693, 28, 14, WHITE);
    ImageDrawText(&img, "65", 700, 44, 30, WHITE);

    ImageDrawRectangle(&img, 790, 0, 118, 118, (Color){ 30, 70, 140, 255 });
    ImageDrawRectangleLines(&img, (Rectangle){ 794, 4, 110, 110 }, 3, WHITE);
    ImageDrawText(&img, "FUEL", 806, 40, 28, WHITE);
    ImageDrawText(&img, "1.89", 812, 74, 20, (Color){ 200, 220, 255, 255 });

    ImageDrawRectangle(&img, 0, 128, 500, 120, (Color){ 24, 26, 34, 255 });
    ImageDrawCircle(&img, 70, 188, 34, (Color){ 255, 150, 60, 255 });
    ImageDrawRectangle(&img, 130, 172, 300, 8, (Color){ 120, 200, 255, 255 });
    ImageDrawText(&img, "O R B I T", 130, 190, 30, WHITE);

    ImageDrawRectangle(&img, 510, 128, 500, 120, (Color){ 30, 24, 34, 255 });
    ImageDrawRectangle(&img, 540, 150, 8, 76, (Color){ 160, 120, 255, 255 });
    ImageDrawRectangle(&img, 560, 158, 6, 60, (Color){ 90, 200, 180, 255 });
    ImageDrawText(&img, "N O V A", 596, 176, 34, WHITE);
    ImageDrawText(&img, "drive calm", 596, 214, 16, (Color){ 180, 180, 190, 255 });

    signAtlas = LoadTextureFromImage(img);
    GenTextureMipmaps(&signAtlas);
    SetTextureFilter(signAtlas, TEXTURE_FILTER_TRILINEAR);
    UnloadImage(img);
}

void render_init(void){
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
        Vector3 n = Vector3CrossProduct(Vector3Subtract(s->r, s->b), Vector3Subtract(s->t, s->b));
        float len = Vector3Length(n);
        if (len > 1e-6f) n = Vector3Scale(n, 1.0f/len);
        Vector3 cen = Vector3Scale(Vector3Add(Vector3Add(s->b, s->r), Vector3Add(s->t, s->l)), 0.25f);
        Color c = shade_vert(n, cen, (Color){ 255, 255, 255, 255 }, 0.28f);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlTexCoord2f(u0, v1); rlVertex3f(s->b.x, s->b.y, s->b.z);
        rlTexCoord2f(u1, v1); rlVertex3f(s->r.x, s->r.y, s->r.z);
        rlTexCoord2f(u1, v0); rlVertex3f(s->t.x, s->t.y, s->t.z);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlTexCoord2f(u0, v1); rlVertex3f(s->b.x, s->b.y, s->b.z);
        rlTexCoord2f(u1, v0); rlVertex3f(s->t.x, s->t.y, s->t.z);
        rlTexCoord2f(u0, v0); rlVertex3f(s->l.x, s->l.y, s->l.z);
    }
}

static void flush_glow_pass(void){
    for (int i = 0; i < nFlat; i++){
        Flat *f = &flats[i];
        Vector3 a = Vector3Add(f->p, Vector3Add(Vector3Scale(f->fwd, -f->hl), Vector3Scale(f->right, -f->hw)));
        Vector3 b = Vector3Add(f->p, Vector3Add(Vector3Scale(f->fwd, -f->hl), Vector3Scale(f->right,  f->hw)));
        Vector3 c = Vector3Add(f->p, Vector3Add(Vector3Scale(f->fwd,  f->hl), Vector3Scale(f->right,  f->hw)));
        Vector3 d = Vector3Add(f->p, Vector3Add(Vector3Scale(f->fwd,  f->hl), Vector3Scale(f->right, -f->hw)));
        Color ca = col_a(f->c, f->alpha);
        rlColor4ub(ca.r, ca.g, ca.b, ca.a);
        rlTexCoord2f(0, 1); rlVertex3f(a.x, a.y, a.z);
        rlTexCoord2f(1, 1); rlVertex3f(b.x, b.y, b.z);
        rlTexCoord2f(1, 0); rlVertex3f(c.x, c.y, c.z);
        rlColor4ub(ca.r, ca.g, ca.b, ca.a);
        rlTexCoord2f(0, 1); rlVertex3f(a.x, a.y, a.z);
        rlTexCoord2f(1, 0); rlVertex3f(c.x, c.y, c.z);
        rlTexCoord2f(0, 0); rlVertex3f(d.x, d.y, d.z);
    }
    for (int i = 0; i < nGlow; i++){
        Glow *g = &glows[i];
        Vector3 r = Vector3Scale(shRight, g->size);
        Vector3 u = Vector3Scale(shUp, g->size*g->ys);
        Vector3 a = Vector3Add(Vector3Subtract(g->p, r), Vector3Negate(u));
        Vector3 b = Vector3Add(Vector3Add(g->p, r), Vector3Negate(u));
        Vector3 c = Vector3Add(Vector3Add(g->p, r), u);
        Vector3 d = Vector3Add(Vector3Subtract(g->p, r), u);
        Color ca = col_a(g->c, g->alpha);
        rlColor4ub(ca.r, ca.g, ca.b, ca.a);
        rlTexCoord2f(0, 1); rlVertex3f(a.x, a.y, a.z);
        rlTexCoord2f(1, 1); rlVertex3f(b.x, b.y, b.z);
        rlTexCoord2f(1, 0); rlVertex3f(c.x, c.y, c.z);
        rlColor4ub(ca.r, ca.g, ca.b, ca.a);
        rlTexCoord2f(0, 1); rlVertex3f(a.x, a.y, a.z);
        rlTexCoord2f(1, 0); rlVertex3f(c.x, c.y, c.z);
        rlTexCoord2f(0, 0); rlVertex3f(d.x, d.y, d.z);
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
    int dx[4] = { 1, -1, 1, -1 }, dy[4] = { 1, 1, -1, -1 };
    for (int i = 0; i < 4; i++){
        Vector2 ex = { p[i].x + dx[i]*len, p[i].y };
        Vector2 ey = { p[i].x, p[i].y + dy[i]*len };
        DrawLineEx(p[i], ex, thick, c);
        DrawLineEx(p[i], ey, thick, c);
    }
}

static void adas_project(Camera3D cam, float sCam, float dt){
    (void)cam; (void)sCam; (void)dt;
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

    Color cLane = { 110, 205, 255, 255 };
    Color cBox  = { 140, 225, 255, 255 };

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
        DrawTextEx(GetFontDefault(), label, lpos, 10, 2, col_a((Color){ 200, 235, 255, 255 }, A*0.55f));

    // small autopilot readout
    float mph = traffic_ego_v()*2.237f;
    const char *auto1 = TextFormat("AUTO   %d MPH", (int)(mph + 0.5f));
    DrawTextEx(f, auto1, (Vector2){ 20, (float)h - 30 }, 12, 1,
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
    sCam3 = cam;
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
    env_draw_sky(cam);
    draw_city_glow(cam, cityPos, cityAmt);

    nGlow = nFlat = nSign = 0;

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

    adas_project(cam, sCam, dt);
    EndMode3D();

    adas_draw2d(cam, sCam);
    weather_draw(cam);
}
