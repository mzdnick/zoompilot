#include "vehicle.h"
#include "rendering.h"
#include "util.h"

static const float DIMS[VT_COUNT][3] = {
    { 4.70f, 1.84f, 1.42f },   // sedan
    { 4.90f, 1.95f, 1.78f },   // suv
    { 5.50f, 1.98f, 1.85f },   // pickup
    { 14.2f, 2.50f, 3.95f },   // truck
};

void vehicle_dims(int type, float *len, float *w, float *h){
    *len = DIMS[type][0]; *w = DIMS[type][1]; *h = DIMS[type][2];
}

static void box(Vector3 pos, Vector3 r, Vector3 u, Vector3 f,
                float cx, float cy, float cz, float hx, float hy, float hz, Color c, float e){
    Vector3 ctr = Vector3Add(pos,
        Vector3Add(Vector3Scale(r, cx), Vector3Add(Vector3Scale(u, cy), Vector3Scale(f, cz))));
    geo_box(ctr, r, u, f, hx, hy, hz, c, e);
}

static void wheels(Vector3 pos, Vector3 r, Vector3 u, Vector3 f,
                   float w, float rr, const float *zs, int nAxles){
    float wx = w*0.5f - 0.16f;
    Color wc = { 26, 28, 30, 255 };
    Vector3 ax = Vector3Scale(r, 0.18f);
    float xs[2] = { wx, -wx };
    for (int i = 0; i < 2; i++)
        for (int k = 0; k < nAxles; k++){
            Vector3 p = Vector3Add(pos,
                Vector3Add(Vector3Scale(r, xs[i]), Vector3Add(Vector3Scale(u, rr), Vector3Scale(f, zs[k]))));
            geo_cylinder(Vector3Subtract(p, Vector3Scale(ax, 0.5f)),
                         Vector3Add(p, Vector3Scale(ax, 0.5f)), rr, rr, 6, wc, 0.0f);
        }
}

void vehicle_draw(Vector3 pos, Vector3 fwd, Vector3 right, int type,
                  Color col, Color col2, float lights, float brake,
                  float wet, int oncoming){
    float l, w, h;
    vehicle_dims(type, &l, &w, &h);
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, fwd));
    Color glass = { 34, 42, 52, 255 };

    switch (type){
    case VT_SEDAN: {
        float zs[2] = { 1.45f, -1.45f };
        box(pos, right, up, fwd, 0, 0.55f, 0, 0.92f, 0.31f, l*0.475f, col, 0);
        box(pos, right, up, fwd, 0, 1.07f, -0.15f, 0.79f, 0.27f, 1.32f, glass, 0);
        wheels(pos, right, up, fwd, w, 0.33f, zs, 2);
        break; }
    case VT_SUV: {
        float zs[2] = { 1.50f, -1.50f };
        box(pos, right, up, fwd, 0, 0.66f, 0, 0.95f, 0.36f, l*0.48f, col, 0);
        box(pos, right, up, fwd, 0, 1.30f, -0.10f, 0.86f, 0.33f, 1.40f, glass, 0);
        wheels(pos, right, up, fwd, w, 0.37f, zs, 2);
        break; }
    case VT_PICKUP: {
        float zs[2] = { 1.60f, -1.60f };
        // low body slab, cab glass above the middle, bed walls behind
        box(pos, right, up, fwd, 0, 0.58f, 0.20f, 0.95f, 0.30f, 2.35f, col, 0);
        box(pos, right, up, fwd, 0, 1.18f, -0.05f, 0.85f, 0.30f, 0.85f, glass, 0);
        box(pos, right, up, fwd, 0, 1.02f, -1.55f, 0.92f, 0.16f, 0.75f, col_scale(col, 0.85f), 0);
        wheels(pos, right, up, fwd, w, 0.38f, zs, 2);
        break; }
    case VT_TRUCK: {
        float zs[4] = { 5.50f, 3.30f, -4.60f, -6.10f };
        // cab at the nose, thin windshield on its face, trailer with a gap;
        // trailer skirt reaches down over the wheels so they sit in wells
        box(pos, right, up, fwd, 0, 1.80f, 5.55f, 1.20f, 1.25f, 1.45f, col, 0);
        box(pos, right, up, fwd, 0, 2.45f, 6.99f, 0.92f, 0.42f, 0.05f, glass, 0);
        for (int s = -1; s <= 1; s += 2)
            box(pos, right, up, fwd, s*(w*0.5f - 0.04f), 2.35f, 5.55f, 0.03f, 0.32f, 0.90f, glass, 0);
        box(pos, right, up, fwd, 0, 3.14f, 5.30f, 0.85f, 0.16f, 1.05f, col_scale(col, 0.9f), 0);
        box(pos, right, up, fwd, 0, 2.35f, -1.55f, 1.22f, 1.60f, 5.50f, col2, 0);
        wheels(pos, right, up, fwd, w, 0.50f, zs, 4);
        break; }
    default: break;
    }

    float frontY = (type == VT_TRUCK) ? 1.10f : 0.75f;
    float rearY  = (type == VT_TRUCK) ? 1.35f : 0.80f;
    float hx = w*0.5f - 0.30f;
    float hz = l*0.5f - 0.06f;
    float headEmis = lights > 0.02f ? 1.0f : 0.12f;
    float tailEmis = brake > 0.5f ? 1.0f : lights*0.75f;

    Color headC = { 255, 244, 214, 255 };
    Color tailC = { 255, 64, 48, 255 };
    box(pos, right, up, fwd,  hx, frontY,  hz, 0.11f, 0.08f, 0.03f, headC, headEmis);
    box(pos, right, up, fwd, -hx, frontY,  hz, 0.11f, 0.08f, 0.03f, headC, headEmis);
    box(pos, right, up, fwd,  hx*0.95f, rearY, -hz, 0.12f, 0.07f, 0.03f, tailC, tailEmis);
    box(pos, right, up, fwd, -hx*0.95f, rearY, -hz, 0.12f, 0.07f, 0.03f, tailC, tailEmis);
    if (type == VT_TRUCK){
        // marker lights along the trailer top edge and rear corners
        Color mk = { 255, 176, 64, 255 };
        for (int s = -1; s <= 1; s += 2){
            box(pos, right, up, fwd, s*(w*0.5f - 0.06f), 3.97f, -1.55f, 0.03f, 0.04f, 1.6f, mk, lights);
            box(pos, right, up, fwd, s*(w*0.5f - 0.10f), 3.80f, -hz, 0.04f, 0.04f, 0.04f, mk, lights);
        }
    }

    if (lights > 0.03f){
        Color hg = { 255, 240, 200, 255 };
        Color tg = { 255, 70, 50, 255 };
        float hsize = oncoming ? 0.85f : 0.36f;
        for (int s = -1; s <= 1; s += 2){
            Vector3 hp = Vector3Add(pos,
                Vector3Add(Vector3Scale(right, s*hx), Vector3Add(Vector3Scale(up, frontY), Vector3Scale(fwd, hz))));
            glow_add(hp, hg, hsize, wet > 0.25f ? 2.6f : 1.0f, oncoming ? 0.34f : 0.16f);
            Vector3 tp = Vector3Add(pos,
                Vector3Add(Vector3Scale(right, s*hx*0.95f), Vector3Add(Vector3Scale(up, rearY), Vector3Scale(fwd, -hz))));
            glow_add(tp, tg, brake > 0.5f ? 0.50f : 0.22f, wet > 0.25f ? 3.0f : 1.0f,
                     brake > 0.5f ? 0.30f : 0.15f);
        }
        if (!oncoming){
            Vector3 ap = Vector3Add(pos, Vector3Add(Vector3Scale(up, 0.06f), Vector3Scale(fwd, hz + 5.5f)));
            flatspot_add(ap, right, fwd, 3.2f, 6.0f, (Color){ 255, 236, 190, 255 }, 0.10f*lights);
        }
    }
}
