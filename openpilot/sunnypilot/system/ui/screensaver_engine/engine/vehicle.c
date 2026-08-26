#include "vehicle.h"
#include "rendering.h"
#include "util.h"

static const float DIMS[VT_COUNT][3] = {
    { 4.70f, 1.84f, 1.42f },   // sedan
    { 4.90f, 1.95f, 1.78f },   // suv
    { 5.50f, 1.98f, 1.85f },   // pickup
    { 14.2f, 2.50f, 3.95f },   // truck
};

// sedan/suv/pickup share one shape recipe: painted slab, glass cabin, an
// optional pickup bed wall, then wheels. The truck keeps its own case.
typedef struct {
    float bodyY, bodyCz, bodyHx, bodyHy, bodyHz;   // slab; lenMul overrides bodyHz when > 0
    float lenMul;                                  // body half-length as a fraction of l
    float glassY, glassCz, glassHx, glassHy, glassHz;
    float bedY, bedCz, bedHx, bedHy, bedHz;        // bedHy <= 0: no bed wall
    float wheelR, zA, zB;                          // two wheel axles
} BodySpec;
static const BodySpec BODY[VT_COUNT] = {
    [VT_SEDAN]  = { .bodyY=0.55f, .bodyCz=0.00f, .bodyHx=0.92f, .bodyHy=0.31f, .lenMul=0.475f,
                    .glassY=1.07f, .glassCz=-0.15f, .glassHx=0.79f, .glassHy=0.27f, .glassHz=1.32f,
                    .bedHy=0.0f, .wheelR=0.33f, .zA=1.45f, .zB=-1.45f },
    [VT_SUV]    = { .bodyY=0.66f, .bodyCz=0.00f, .bodyHx=0.95f, .bodyHy=0.36f, .lenMul=0.48f,
                    .glassY=1.30f, .glassCz=-0.10f, .glassHx=0.86f, .glassHy=0.33f, .glassHz=1.40f,
                    .bedHy=0.0f, .wheelR=0.37f, .zA=1.50f, .zB=-1.50f },
    [VT_PICKUP] = { .bodyY=0.58f, .bodyCz=0.20f, .bodyHx=0.95f, .bodyHy=0.30f, .bodyHz=2.35f,
                    .glassY=1.18f, .glassCz=-0.05f, .glassHx=0.85f, .glassHy=0.30f, .glassHz=0.85f,
                    .bedY=1.02f, .bedCz=-1.55f, .bedHx=0.92f, .bedHy=0.16f, .bedHz=0.75f,
                    .wheelR=0.38f, .zA=1.60f, .zB=-1.60f },
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
                  float wet, int oncoming, int blinker){
    float l, w, h;
    vehicle_dims(type, &l, &w, &h);
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, fwd));
    Color glass = { 34, 42, 52, 255 };

    if (type != VT_TRUCK){
        const BodySpec *b = &BODY[type];
        float zs[2] = { b->zA, b->zB };
        float bodyHz = b->lenMul > 0.0f ? b->lenMul*l : b->bodyHz;
        box(pos, right, up, fwd, 0, b->bodyY, b->bodyCz, b->bodyHx, b->bodyHy, bodyHz, col, 0);
        box(pos, right, up, fwd, 0, b->glassY, b->glassCz, b->glassHx, b->glassHy, b->glassHz, glass, 0);
        if (b->bedHy > 0.0f)
            box(pos, right, up, fwd, 0, b->bedY, b->bedCz, b->bedHx, b->bedHy, b->bedHz,
                col_scale(col, 0.85f), 0);
        wheels(pos, right, up, fwd, w, b->wheelR, zs, 2);
    } else {
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
    }

    float frontY = (type == VT_TRUCK) ? 1.10f : 0.75f;
    float rearY  = (type == VT_TRUCK) ? 1.35f : 0.80f;
    float hx = w*0.5f - 0.30f;
    float hz = l*0.5f - 0.06f;
    // lamp corner positions, computed once for blinkers and glows
    Vector3 cfl = Vector3Add(pos, Vector3Add(Vector3Scale(right, hx),
                          Vector3Add(Vector3Scale(up, frontY), Vector3Scale(fwd, hz))));
    Vector3 cfr = Vector3Add(pos, Vector3Add(Vector3Scale(right, -hx),
                          Vector3Add(Vector3Scale(up, frontY), Vector3Scale(fwd, hz))));
    Vector3 crl = Vector3Add(pos, Vector3Add(Vector3Scale(right, hx*0.95f),
                          Vector3Add(Vector3Scale(up, rearY), Vector3Scale(fwd, -hz))));
    Vector3 crr = Vector3Add(pos, Vector3Add(Vector3Scale(right, -hx*0.95f),
                          Vector3Add(Vector3Scale(up, rearY), Vector3Scale(fwd, -hz))));
    float headEmis = lights > 0.02f ? 1.0f : 0.12f;
    float tailEmis = brake > 0.5f ? 1.0f : lights*0.75f;

    Color headC = { 255, 244, 214, 255 };
    Color tailC = { 255, 64, 48, 255 };
    box(pos, right, up, fwd,  hx, frontY,  hz, 0.11f, 0.08f, 0.03f, headC, headEmis);
    box(pos, right, up, fwd, -hx, frontY,  hz, 0.11f, 0.08f, 0.03f, headC, headEmis);
    box(pos, right, up, fwd,  hx*0.95f, rearY, -hz, 0.12f, 0.07f, 0.03f, tailC, tailEmis);
    box(pos, right, up, fwd, -hx*0.95f, rearY, -hz, 0.12f, 0.07f, 0.03f, tailC, tailEmis);
    if (blinker){
        int s = (blinker > 0) ? 1 : -1;
        Color bc = { 255, 178, 48, 255 };
        box(pos, right, up, fwd, s*hx, frontY, hz, 0.10f, 0.07f, 0.03f, bc, 1.0f);
        box(pos, right, up, fwd, s*hx*0.95f, rearY, -hz, 0.11f, 0.07f, 0.03f, bc, 1.0f);
        glow_add(s > 0 ? cfl : cfr, bc, 0.30f, 1.0f, 0.26f);
        glow_add(s > 0 ? crl : crr, bc, 0.34f, 1.0f, 0.30f);
    }
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
            Vector3 hp = (s > 0) ? cfl : cfr;
            glow_add(hp, hg, hsize, wet > 0.25f ? 2.6f : 1.0f, oncoming ? 0.34f : 0.16f);
            Vector3 tp = (s > 0) ? crl : crr;
            glow_add(tp, tg, brake > 0.5f ? 0.50f : 0.22f, wet > 0.25f ? 3.0f : 1.0f,
                     brake > 0.5f ? 0.30f : 0.15f);
            // wet asphalt: streak the visible lamps onto the road surface.
            // Same-direction cars show taillights; oncoming show headlight
            // glare - one pair per vehicle keeps the additive pass cheap.
            if (wet > 0.12f){
                if (oncoming)
                    refl_add((Vector3){ hp.x, pos.y + 0.06f, hp.z }, hg, hsize*1.15f, 3.4f, 0.19f);
                else
                    refl_add((Vector3){ tp.x, pos.y + 0.06f, tp.z }, tg,
                             (brake > 0.5f ? 0.50f : 0.22f)*1.1f, 2.8f,
                             (brake > 0.5f ? 0.30f : 0.15f)*0.55f);
            }
        }
        if (!oncoming){
            Vector3 ap = Vector3Add(pos, Vector3Add(Vector3Scale(up, 0.06f), Vector3Scale(fwd, hz + 5.5f)));
            flatspot_add(ap, right, fwd, 3.2f, 6.0f, (Color){ 255, 236, 190, 255 }, 0.10f*lights);
        }
    }
}
