extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h" // gMtxClear
#include "objects/gameplay_keep/gameplay_keep.h"
#include "textures/parameter_static/parameter_static.h" // HUD counter digits
extern PlayState* gPlayState;
}
#include "VrCombat.h"

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/frame_interpolation.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <vr_interface.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

// Physical archery (VR first person, selector mode; slingshot first, bow inherits): the weapon
// rides the OFF hand (bow/slingshot are right-hand models under motion hands), and the string
// hand pinches the configured input (default trigger) NEAR the weapon to nock. While nocked,
// the weapon's own item button reads held-down in padmgr, so the entire vanilla draw pipeline
// runs untouched — projectile spawned at nock, ammo charged at release, elemental magic already
// deferred to release by the lifecycle work. Releasing the pinch past the minimum draw drops
// the button, and the vanilla release fires the shot along the string-hand -> bow-hand line
// (Player_VrAimHeldProjectile consumes VrArchery_AimSegment). A release short of the minimum
// draw cancels through Player_VrCancelPreparedItem: arrow killed, ammo and magic preserved.
//
// Deliberate fallbacks: shooting galleries, bombchu bowling and horseback keep their own
// schemes (Covers is false there), and gVrPhysArchery=0 restores the previous behavior where
// the weapon hand's trigger mirrors the item button.

namespace {

bool sNocked = false;
bool sPinchPrev = true; // require a fresh pinch after entry/reset
float sDrawM = 0.0f;    // current string-hand draw distance, meters

// The firing release is processed by the vanilla pad path a frame AFTER the tick clears the
// nock — and draws in between would fall back to the one-hand ray and overwrite the shot
// direction (the "shots ignore the pull" bug). So the last live pull line is latched at
// release and served for a few ticks, exactly long enough for the vanilla release to fire.
float sAimLatch[6] = {};
bool sAimLatchLive = false; // a nock has produced a valid latch this draw cycle
int sAimLatchTicks = 0;     // >0: keep serving the latch after the nock ended

int StringHand() {
    return CVarGetInteger("gVrLeftHanded", 0) ? VR_HAND_LEFT : VR_HAND_RIGHT;
}

int BowHand() {
    return 1 - StringHand();
}

uint16_t PinchMask() {
    return (uint16_t)CVarGetInteger("gVrArcheryNockInput", VR_BTN_TRIGGER);
}

float WorldScale() {
    const float ws = VR_GetWorldScale();
    return ws < 1.0f ? 35.0f : ws;
}

// Rotate a vector by a unit quaternion (x, y, z, w) — same layout the runtime hands out.
void QuatRot(const float q[4], const float v[3], float out[3]) {
    const float tx = 2.0f * (q[1] * v[2] - q[2] * v[1]);
    const float ty = 2.0f * (q[2] * v[0] - q[0] * v[2]);
    const float tz = 2.0f * (q[0] * v[1] - q[1] * v[0]);
    out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
    out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
    out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

// The nock anchor: where the string physically lives on the weapon — the weapon-hand
// controller plus the user-tuned grip-local offset (cm sliders; right/up/forward in the
// controller's own frame, forward = -Z as OpenXR defines it). The marker shows this point,
// reach and draw distance measure against it, and the shot flies string-hand -> anchor.
bool NockAnchorWorld(float* out3) {
    float pos[3], rot[4];
    if (!VR_GetHandPose(BowHand(), pos, rot)) {
        return false;
    }
    const float u = 0.01f * WorldScale(); // cm -> game units
    float mirror = CVarGetInteger("gVrLeftHanded", 0) ? -1.0f : 1.0f;
    // Defaults are the headset-tuned slingshot values (September 14, 2026).
    const float local[3] = { CVarGetFloat("gVrArcheryAnchorRight", 4.0f) * u * mirror,
                             CVarGetFloat("gVrArcheryAnchorUp", -5.0f) * u,
                             -CVarGetFloat("gVrArcheryAnchorFwd", 17.0f) * u };
    float off[3];
    QuatRot(rot, local, off);
    for (int i = 0; i < 3; i++) {
        out3[i] = pos[i] + off[i];
    }
    return true;
}

// String hand <-> nock anchor gap in meters; negative when either is unavailable.
float DrawGapM() {
    float s[3], rot[4], a[3];
    if (!VR_GetHandPose(StringHand(), s, rot) || !NockAnchorWorld(a)) {
        return -1.0f;
    }
    float d2 = 0.0f;
    for (int i = 0; i < 3; i++) {
        const float d = s[i] - a[i];
        d2 += d * d;
    }
    return std::sqrt(d2) / WorldScale();
}

bool NearBow() {
    const float gap = DrawGapM();
    return gap >= 0.0f && gap <= CVarGetFloat("gVrArcheryNockRadius", 20.0f) * 0.01f;
}

} // namespace

extern "C" bool VrArchery_Covers(Player* player) {
    if (!VrItemSelect_ModeActive() || !CVarGetInteger("gVrPhysArchery", 1) || player == NULL ||
        player->actor.category != ACTORCAT_PLAYER || gPlayState == NULL) {
        return false;
    }
    if (!VrItemSelect_SelectionAllowed()) {
        return false; // scripted control, transitions, ocarina: no nock, no button mask
    }
    if (gPlayState->shootingGalleryStatus != 0 || gPlayState->bombchuBowlingStatus != 0 ||
        (player->stateFlags1 & PLAYER_STATE1_ON_HORSE)) {
        return false; // galleries, bowling and horseback keep their dedicated schemes
    }
    return (player->heldItemAction >= PLAYER_IA_BOW && player->heldItemAction <= PLAYER_IA_BOW_0E) ||
           player->heldItemAction == PLAYER_IA_SLINGSHOT;
}

extern "C" void VrArchery_Reset(void) {
    sNocked = false;
    sPinchPrev = true;
    sDrawM = 0.0f;
    sAimLatchLive = false;
    sAimLatchTicks = 0;
}

static Vtx sAmmoVtx[3 * 4]; // QuestShip: ammo digits at the nock point

// Nock-point marker: the ammo counter, rendered in-world at the nock anchor while the
// bow/slingshot is out and no nock is drawn. It grows when the string hand is in pinch reach. DELIBERATELY minimal gates
// (mode + cvar + weapon out, none of the input-side availability checks): the icon is a
// tuning target for the anchor sliders and a liveness diagnostic — it must show even when
// the input gates are the thing that is broken. extern "C" linkage is load-bearing for the
// block-scope FrameInterpolation declarations inside OPEN_DISPS (see VrItemSelect_Draw).
extern "C" void VrArchery_DrawNockIcon(void) {
    if (gPlayState == NULL || !CVarGetInteger("gVrPhysArchery", 1) || !VrItemSelect_ModeActive() || sNocked) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == NULL ||
        !((player->heldItemAction >= PLAYER_IA_BOW && player->heldItemAction <= PLAYER_IA_BOW_0E) ||
          player->heldItemAction == PLAYER_IA_SLINGSHOT)) {
        return;
    }
    float anchor[3];
    if (!NockAnchorWorld(anchor)) {
        return;
    }

    // QuestShip: the nock marker shows the AMMO LEFT (seeds / arrows) in the HUD's own digit font,
    // instead of a Deku Nut icon. Always readable: translucent, no depth test. Red at zero.
    const int ammo = (player->heldItemAction == PLAYER_IA_SLINGSHOT) ? AMMO(ITEM_SLINGSHOT) : AMMO(ITEM_BOW);
    int digits[3];
    int nd = 0;
    {
        int v = ammo < 0 ? 0 : (ammo > 999 ? 999 : ammo);
        do {
            digits[nd++] = v % 10;
            v /= 10;
        } while (v > 0 && nd < 3);
    }
    static const char* kDigitTex[10] = { gCounterDigit0Tex, gCounterDigit1Tex, gCounterDigit2Tex, gCounterDigit3Tex,
                                         gCounterDigit4Tex, gCounterDigit5Tex, gCounterDigit6Tex, gCounterDigit7Tex,
                                         gCounterDigit8Tex, gCounterDigit9Tex };
    // Digit cells in texture pixels (8x16); the matrix scales a 16-px glyph to the chosen height.
    for (int i = 0; i < nd; i++) {
        const int x0 = (nd * 8) / 2 - (i + 1) * 8; // digits[0] is the ones place, rightmost
        Vtx* v = &sAmmoVtx[i * 4];
        const s16 xs[4] = { (s16)x0, (s16)(x0 + 8), (s16)x0, (s16)(x0 + 8) };
        const s16 ys[4] = { 8, 8, -8, -8 };
        const s16 ss[4] = { 0, 8 << 5, 0, 8 << 5 };
        const s16 ts[4] = { 0, 0, 16 << 5, 16 << 5 };
        for (int k = 0; k < 4; k++) {
            v[k].v.ob[0] = xs[k];
            v[k].v.ob[1] = ys[k];
            v[k].v.ob[2] = 0;
            v[k].v.flag = 0;
            v[k].v.tc[0] = ss[k];
            v[k].v.tc[1] = ts[k];
            v[k].v.cn[0] = v[k].v.cn[1] = v[k].v.cn[2] = v[k].v.cn[3] = 255;
        }
    }
    const float heightUnits = CVarGetFloat("gVrArcheryAmmoSize", 3.0f) * 0.01f * WorldScale();

    OPEN_DISPS(gPlayState->state.gfxCtx);
    FrameInterpolation_RecordOpenChild((const void*)&sNocked, 0);
    Matrix_Translate(anchor[0], anchor[1], anchor[2], MTXMODE_NEW);
    Matrix_ReplaceRotation(&gPlayState->billboardMtxF);
    const float sc = heightUnits / 16.0f * (NearBow() ? 1.25f : 1.0f);
    Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);
    gSPVrPhysMask(POLY_XLU_DISP++, 1);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    if (ammo <= 0) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 80, 60, 255);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    }
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH | G_LIGHTING | G_FOG);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    {
        // QuestShip: weld the counter to the slingshot/bow hand at headset rate. Its position was
        // taken from the hand at this 20 Hz tick, so as a plain world matrix it trailed the live
        // hand when walking or moving the arm. Register it as a live CHILD of the hand (like the
        // bowstring): local = inverse(hand now) x this matrix, re-composed with the live hand pose
        // every frame by the renderer.
        Mtx* ammoMtx = MATRIX_NEWMTX(gPlayState->state.gfxCtx);
        float hm[4][4];
        if (VR_GetHandMatrix(BowHand(), hm)) {
            MtxF hand;
            MtxF handInv;
            MtxF cur;
            MtxF local;
            memcpy(hand.mf, hm, sizeof(hand.mf));
            if (SkinMatrix_Invert(&hand, &handInv) == 0) {
                Matrix_Get(&cur);
                SkinMatrix_MtxFMtxFMult(&handInv, &cur, &local);
                VR_RegisterHandChildMatrix((const void*)ammoMtx, BowHand(), &local.mf[0][0]);
            }
        }
        gSPMatrix(POLY_XLU_DISP++, ammoMtx, G_MTX_MODELVIEW | G_MTX_LOAD);
    }
    for (int i = 0; i < nd; i++) {
        gDPLoadTextureBlock(POLY_XLU_DISP++, kDigitTex[digits[i]], G_IM_FMT_I, G_IM_SIZ_8b, 8, 16, 0,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)&sAmmoVtx[i * 4], 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 2, 1, 0, 1, 2, 3, 0);
    }
    gSPVrPhysMask(POLY_XLU_DISP++, 0);
    FrameInterpolation_RecordCloseChild();
    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

// QuestShip: where the nocked shot will land. The flight is simulated with the projectile's REAL
// motion (EnArrow_Shoot/EnArrow_Fly): launched along the aim line at 80 (seed) / 150 (arrow) units
// per tick, moved 1.5x velocity per update, gravity -0.4 only once the flight timer (15 / 12) drops
// below 7.2, killed at 0, stopped by the first surface. Default display: a flat ring lying on the
// surface it would hit (nothing when it would hit nothing). gVrArcheryTrajectoryLine adds the
// flight path as a thin smoothed tube. Both are masked out of combat collision.
static constexpr int kTrajMaxPts = 16;
static constexpr int kTrajSub = 3; // Catmull-Rom sub-points per simulated tick segment
static constexpr int kTrajMaxRings = (kTrajMaxPts - 1) * kTrajSub + 1;
static Vtx sTrajVtx[kTrajMaxRings * 3];
static Gfx sTrajDl[16 + (kTrajMaxRings - 1) * 4];
static constexpr int kRingSegs = 12;
static constexpr int kDotSegs = 8, kDotRings = 3; // center sphere: 8 around, 3 latitude rings
static constexpr int kDotVerts = kDotSegs * kDotRings + 2;
static Vtx sRingVtx[kRingSegs * 2 + kDotVerts];
static Gfx sRingDl[64];

namespace {

struct TrajSim {
    Vec3f pts[kTrajMaxPts];
    int n = 0;
    bool hit = false;
    Vec3f normal = { 0.0f, 1.0f, 0.0f };
};

// Segment a + t*d (t in [0,1]) against a sphere; fraction of the first entry, or 2.
float SegSphere(const Vec3f& a, float dx, float dy, float dz, const Vec3f& c, float r, Vec3f& normal) {
    const float ox = a.x - c.x, oy = a.y - c.y, oz = a.z - c.z;
    const float qa = dx * dx + dy * dy + dz * dz, qb = 2.0f * (ox * dx + oy * dy + oz * dz);
    const float qc = ox * ox + oy * oy + oz * oz - r * r;
    if (qc <= 0.0f || qa < 1e-6f) {
        return 2.0f; // starts inside (or no motion): ignore
    }
    const float disc = qb * qb - 4.0f * qa * qc;
    if (disc < 0.0f) {
        return 2.0f;
    }
    const float t = (-qb - sqrtf(disc)) / (2.0f * qa);
    if (t < 0.0f || t > 1.0f) {
        return 2.0f;
    }
    normal = { (ox + dx * t) / r, (oy + dy * t) / r, (oz + dz * t) / r };
    return t;
}

// Segment against a vertical cylinder (side wall and caps); fraction or 2.
float SegCylinder(const Vec3f& a, float dx, float dy, float dz, float cx, float cz, float y0, float y1, float r,
                  Vec3f& normal) {
    const float ox = a.x - cx, oz = a.z - cz;
    if (ox * ox + oz * oz < r * r && a.y >= y0 && a.y <= y1) {
        return 2.0f; // starts inside: ignore
    }
    float best = 2.0f;
    const float qa = dx * dx + dz * dz, qb = 2.0f * (ox * dx + oz * dz), qc = ox * ox + oz * oz - r * r;
    const float disc = qb * qb - 4.0f * qa * qc;
    if (qa > 1e-6f && disc >= 0.0f) {
        const float t = (-qb - sqrtf(disc)) / (2.0f * qa);
        const float y = a.y + dy * t;
        if (t >= 0.0f && t <= 1.0f && y >= y0 && y <= y1) {
            best = t;
            const float nx = ox + dx * t, nz = oz + dz * t, nl = sqrtf(nx * nx + nz * nz);
            normal = nl > 1e-4f ? Vec3f{ nx / nl, 0.0f, nz / nl } : Vec3f{ 0.0f, 1.0f, 0.0f };
        }
    }
    if (fabsf(dy) > 1e-6f) {
        for (int cap = 0; cap < 2; cap++) {
            const float cy = cap == 0 ? y1 : y0;
            const float t = (cy - a.y) / dy;
            const float px = ox + dx * t, pz = oz + dz * t;
            if (t >= 0.0f && t <= 1.0f && t < best && px * px + pz * pz <= r * r) {
                best = t;
                normal = { 0.0f, cap == 0 ? 1.0f : -1.0f, 0.0f };
            }
        }
    }
    return best;
}

// Nearest hit of segment a->b against the hurtboxes shots actually hit: this frame's active AC
// colliders (enemies, bosses, switches, ...), excluding Link's own. Returns the fraction, or 2.
float ActorLineHit(const Vec3f& a, const Vec3f& b, Vec3f& normal) {
    const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    const Player* player = GET_PLAYER(gPlayState);
    CollisionCheckContext* cc = &gPlayState->colChkCtx;
    float best = 2.0f;
    for (s32 i = 0; i < cc->colACCount; i++) {
        Collider* col = cc->colAC[i];
        if (col == NULL || !(col->acFlags & AC_ON) || col->actor == NULL || col->actor == &player->actor ||
            col->actor->category == ACTORCAT_PLAYER) {
            continue;
        }
        Vec3f n;
        if (col->shape == COLSHAPE_CYLINDER) {
            const ColliderCylinder* cyl = (const ColliderCylinder*)col;
            if (!(cyl->info.bumperFlags & BUMP_ON) || cyl->dim.radius <= 0) {
                continue;
            }
            const float y0 = cyl->dim.pos.y + cyl->dim.yShift;
            const float t = SegCylinder(a, dx, dy, dz, cyl->dim.pos.x, cyl->dim.pos.z, y0, y0 + cyl->dim.height,
                                        cyl->dim.radius, n);
            if (t < best) {
                best = t;
                normal = n;
            }
        } else if (col->shape == COLSHAPE_JNTSPH) {
            const ColliderJntSph* js = (const ColliderJntSph*)col;
            for (s32 k = 0; k < js->count; k++) {
                const ColliderJntSphElement* e = &js->elements[k];
                if (!(e->info.bumperFlags & BUMP_ON) || e->dim.worldSphere.radius <= 0) {
                    continue;
                }
                const Vec3f c = { (f32)e->dim.worldSphere.center.x, (f32)e->dim.worldSphere.center.y,
                                  (f32)e->dim.worldSphere.center.z };
                const float t = SegSphere(a, dx, dy, dz, c, e->dim.worldSphere.radius, n);
                if (t < best) {
                    best = t;
                    normal = n;
                }
            }
        }
    }
    return best;
}

bool SimulateShot(const Player* player, TrajSim& sim) {
    float seg[6];
    if (!VrArchery_AimSegment(seg)) {
        return false;
    }
    const bool seed = player->heldItemAction == PLAYER_IA_SLINGSHOT;
    const float speed = seed ? 80.0f : 150.0f;
    int timer = seed ? 15 : 12;
    Vec3f pos = { seg[0], seg[1], seg[2] };
    float vx = seg[3] * speed, vy = seg[4] * speed, vz = seg[5] * speed;
    float gravity = 0.0f;
    sim.pts[sim.n++] = pos;
    while (sim.n < kTrajMaxPts) {
        if (--timer <= 0) {
            break;
        }
        if (timer < 7.2f) {
            gravity = -0.4f;
        }
        vy += gravity;
        if (vy < -150.0f) {
            vy = -150.0f;
        }
        Vec3f next = { pos.x + vx * 1.5f, pos.y + vy * 1.5f, pos.z + vz * 1.5f };
        Vec3f hitPos;
        CollisionPoly* poly = NULL;
        s32 bgId = 0;
        const bool bgHit = BgCheck_EntityLineTest1(&gPlayState->colCtx, &pos, &next, &hitPos, &poly, true, true,
                                                   true, true, &bgId);
        const Vec3f segEnd = bgHit ? hitPos : next;
        // Whatever is nearest wins: an enemy in front of a wall takes the ring.
        Vec3f actorNormal;
        const float ta = ActorLineHit(pos, segEnd, actorNormal);
        if (ta <= 1.0f) {
            sim.pts[sim.n++] = { pos.x + (segEnd.x - pos.x) * ta, pos.y + (segEnd.y - pos.y) * ta,
                                 pos.z + (segEnd.z - pos.z) * ta };
            sim.hit = true;
            sim.normal = actorNormal;
            break;
        }
        if (bgHit) {
            sim.pts[sim.n++] = hitPos;
            if (poly != NULL) {
                sim.hit = true;
                sim.normal = { COLPOLY_GET_NORMAL(poly->normal.x), COLPOLY_GET_NORMAL(poly->normal.y),
                               COLPOLY_GET_NORMAL(poly->normal.z) };
            }
            break;
        }
        sim.pts[sim.n++] = next;
        pos = next;
    }
    return sim.n >= 2;
}

float DistToEye(const float eye[3], const Vec3f& p) {
    const float ex = eye[0] - p.x, ey = eye[1] - p.y, ez = eye[2] - p.z;
    return sqrtf(ex * ex + ey * ey + ez * ez);
}

void SetupTranslucentShade(Gfx*& p) {
    gSPVrPhysMask(p++, 1);
    gDPPipeSync(p++);
    gDPSetCycleType(p++, G_CYC_1CYCLE);
    gDPSetRenderMode(p++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    gDPSetCombineMode(p++, G_CC_SHADE, G_CC_SHADE);
    gSPTexture(p++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gSPClearGeometryMode(p++, G_CULL_BOTH | G_LIGHTING | G_FOG);
    // G_ZBUFFER: depth-test against the world (without it the state left by earlier draws decides,
    // and the marker showed through walls and enemies). XLU_SURF never writes depth.
    gSPSetGeometryMode(p++, G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);
}

} // namespace

// C linkage: OPEN_DISPS declares FrameInterpolation_* at block scope, which must resolve to the C
// functions (inside the anonymous namespace they would get internal C++ names and fail to link).
extern "C" {

// The landing ring: an annulus in its own unit frame (radius 100), placed by a matrix that is
// recorded for frame interpolation, so it glides between 20 Hz ticks instead of stepping.
static void DrawLandingRing(const TrajSim& sim, const float eye[3]) {
    static bool sBuilt = false;
    if (!sBuilt) {
        auto setVtx = [](Vtx& v, float x, float y, float z, u8 lum, u8 a) {
            v.v.ob[0] = (s16)lroundf(x);
            v.v.ob[1] = (s16)lroundf(y);
            v.v.ob[2] = (s16)lroundf(z);
            v.v.flag = 0;
            v.v.tc[0] = v.v.tc[1] = 0;
            v.v.cn[0] = lum;
            v.v.cn[1] = lum;
            v.v.cn[2] = (u8)(lum * 0.92f);
            v.v.cn[3] = a;
        };
        // Band: outer radius 100, inner 81 (a thin ring around the center dot).
        for (int k = 0; k < kRingSegs; k++) {
            const float a = (float)k * (2.0f * (float)M_PI / kRingSegs);
            setVtx(sRingVtx[k], cosf(a) * 100.0f, 0.0f, sinf(a) * 100.0f, 255, 200);
            setVtx(sRingVtx[kRingSegs + k], cosf(a) * 81.0f, 0.0f, sinf(a) * 81.0f, 255, 235);
        }
        // Center dot: a small sphere resting on the surface, shaded from above (no lighting pass).
        Vtx* dot = &sRingVtx[kRingSegs * 2];
        const float rad = 22.0f, cy = rad * 0.8f;
        auto lum = [](float ny) { return (u8)(175.0f + 80.0f * (0.5f + 0.5f * ny)); };
        setVtx(dot[0], 0.0f, cy + rad, 0.0f, lum(1.0f), 245);
        for (int r = 0; r < kDotRings; r++) {
            const float lat = (float)M_PI * (float)(r + 1) / (kDotRings + 1); // from the top
            const float ny = cosf(lat), ring = sinf(lat);
            for (int k = 0; k < kDotSegs; k++) {
                const float a = (float)k * (2.0f * (float)M_PI / kDotSegs);
                setVtx(dot[1 + r * kDotSegs + k], cosf(a) * ring * rad, cy + ny * rad, sinf(a) * ring * rad, lum(ny),
                       245);
            }
        }
        setVtx(dot[kDotVerts - 1], 0.0f, cy - rad, 0.0f, lum(-1.0f), 245);

        Gfx* p = sRingDl;
        SetupTranslucentShade(p);
        gSPVertex(p++, (uintptr_t)sRingVtx, kRingSegs * 2, 0);
        for (int k = 0; k < kRingSegs; k++) {
            const int o0 = k, o1 = (k + 1) % kRingSegs, i0 = kRingSegs + k, i1 = kRingSegs + o1;
            gSP2Triangles(p++, o0, o1, i1, 0, o0, i1, i0, 0);
        }
        gSPVertex(p++, (uintptr_t)dot, kDotVerts, 0);
        const int bottom = kDotVerts - 1;
        for (int k = 0; k < kDotSegs; k++) {
            const int k1 = (k + 1) % kDotSegs;
            const int lastRing = 1 + (kDotRings - 1) * kDotSegs;
            gSP2Triangles(p++, 0, 1 + k, 1 + k1, 0, bottom, lastRing + k1, lastRing + k, 0);
            for (int r = 0; r + 1 < kDotRings; r++) {
                const int a0 = 1 + r * kDotSegs + k, a1 = 1 + r * kDotSegs + k1;
                const int b0 = a0 + kDotSegs, b1 = a1 + kDotSegs;
                gSP2Triangles(p++, a0, b0, b1, 0, a0, b1, a1, 0);
            }
        }
        gSPVrPhysMask(p++, 0);
        gSPEndDisplayList(p++);
        assert(p <= sRingDl + ARRAY_COUNT(sRingDl));
        sBuilt = true;
    }

    // Basis: Y = surface normal, X/Z span the surface.
    const Vec3f& nrm = sim.normal;
    Vec3f t = fabsf(nrm.y) < 0.9f ? Vec3f{ nrm.z, 0.0f, -nrm.x } : Vec3f{ 0.0f, -nrm.z, nrm.y }; // n x (up|x)
    const float tl = sqrtf(t.x * t.x + t.y * t.y + t.z * t.z);
    if (tl < 1e-4f) {
        return;
    }
    t = { t.x / tl, t.y / tl, t.z / tl };
    const Vec3f b = { nrm.y * t.z - nrm.z * t.y, nrm.z * t.x - nrm.x * t.z, nrm.x * t.y - nrm.y * t.x };
    MtxF basis = {};
    basis.mf[0][0] = t.x, basis.mf[0][1] = t.y, basis.mf[0][2] = t.z;
    basis.mf[1][0] = nrm.x, basis.mf[1][1] = nrm.y, basis.mf[1][2] = nrm.z;
    basis.mf[2][0] = b.x, basis.mf[2][1] = b.y, basis.mf[2][2] = b.z;
    basis.mf[3][3] = 1.0f;

    // Constant physical size within 3 m, then growing gently (square root, capped at 2.5x) so far
    // targets stay findable without the ring ballooning.
    const Vec3f& at = sim.pts[sim.n - 1];
    const float ws = WorldScale();
    const float baseR = CVarGetFloat("gVrArcheryRingSize", 12.0f) * 0.01f * ws;
    const float grow = std::clamp(sqrtf(DistToEye(eye, at) / (3.0f * ws)), 1.0f, 2.5f);
    const float r = baseR * grow;
    const float lift = 1.5f; // off the surface, against z-fighting

    OPEN_DISPS(gPlayState->state.gfxCtx);
    FrameInterpolation_RecordOpenChild((const void*)sRingVtx, 0);
    Matrix_Translate(at.x + nrm.x * lift, at.y + nrm.y * lift, at.z + nrm.z * lift, MTXMODE_NEW);
    Matrix_Mult(&basis, MTXMODE_APPLY);
    Matrix_Scale(r / 100.0f, r / 100.0f, r / 100.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gPlayState->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, sRingDl);
    FrameInterpolation_RecordCloseChild();
    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

static Vec3f CatmullRom(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec3f& p3, float t) {
    const float t2 = t * t, t3 = t2 * t;
    const float a = -0.5f * t3 + t2 - 0.5f * t, b = 1.5f * t3 - 2.5f * t2 + 1.0f;
    const float c = -1.5f * t3 + 2.0f * t2 + 0.5f * t, d = 0.5f * t3 - 0.5f * t2;
    return { a * p0.x + b * p1.x + c * p2.x + d * p3.x, a * p0.y + b * p1.y + c * p2.y + d * p3.y,
             a * p0.z + b * p1.z + c * p2.z + d * p3.z };
}

// The flight path: a thin 3-sided tube (a flat strip turns edge-on as the head moves). Vertices are
// stored relative to the launch point and pre-scaled so the sub-unit radius survives the s16
// vertex format (plain world coordinates rounded it to 0 or 1 unit, which made the width jitter).
static void DrawFlightLine(const TrajSim& sim, const float eye[3]) {
    Vec3f rings[kTrajMaxRings];
    int nr = 0;
    for (int i = 0; i + 1 < sim.n; i++) {
        const Vec3f& p0 = sim.pts[i > 0 ? i - 1 : 0];
        const Vec3f& p3 = sim.pts[i + 2 < sim.n ? i + 2 : sim.n - 1];
        for (int k = 0; k < kTrajSub; k++) {
            rings[nr++] = CatmullRom(p0, sim.pts[i], sim.pts[i + 1], p3, (float)k / kTrajSub);
        }
    }
    rings[nr++] = sim.pts[sim.n - 1]; // exactly at the hit
    assert(nr <= kTrajMaxRings);

    const Vec3f& o = rings[0];
    float extent = 1.0f;
    float arc[kTrajMaxRings];
    arc[0] = 0.0f;
    for (int i = 0; i < nr; i++) {
        extent = fmaxf(extent, fmaxf(fabsf(rings[i].x - o.x), fmaxf(fabsf(rings[i].y - o.y), fabsf(rings[i].z - o.z))));
        if (i > 0) {
            const float dx = rings[i].x - rings[i - 1].x, dy = rings[i].y - rings[i - 1].y,
                        dz = rings[i].z - rings[i - 1].z;
            arc[i] = arc[i - 1] + sqrtf(dx * dx + dy * dy + dz * dz);
        }
    }
    const float q = fminf(64.0f, fmaxf(1.0f, floorf(30000.0f / (extent + 4.0f)))); // local units per world unit
    const float total = fmaxf(arc[nr - 1], 1e-3f);
    const float fadeIn = 0.06f * WorldScale(); // first ~6 cm fade in from the pouch
    const float widthK = CVarGetFloat("gVrArcheryTrajectoryWidth", 0.0012f);
    const int alpha0 = std::clamp(CVarGetInteger("gVrArcheryTrajectoryAlpha", 120), 0, 255);
    static const float kC[3] = { 1.0f, -0.5f, -0.5f };
    static const float kS[3] = { 0.0f, 0.8660254f, -0.8660254f };
    for (int i = 0; i < nr; i++) {
        const Vec3f& a = rings[i > 0 ? i - 1 : 0];
        const Vec3f& b = rings[i > 0 ? i : 1];
        float tx = b.x - a.x, ty = b.y - a.y, tz = b.z - a.z;
        const float tl = sqrtf(tx * tx + ty * ty + tz * tz);
        if (tl > 1e-4f) {
            tx /= tl, ty /= tl, tz /= tl;
        }
        // n1 = tangent x worldUp (fallback X), n2 = tangent x n1
        float n1x = -tz, n1z = tx;
        float n1l = sqrtf(n1x * n1x + n1z * n1z);
        if (n1l < 1e-3f) {
            n1x = 1.0f, n1z = 0.0f, n1l = 1.0f;
        }
        n1x /= n1l, n1z /= n1l;
        const float n2x = ty * n1z, n2y = tz * n1x - tx * n1z, n2z = -ty * n1x;
        const float r = fmaxf(0.03f, DistToEye(eye, rings[i]) * widthK) * q;
        const float fade = fminf(1.0f, arc[i] / fadeIn) * (1.0f - arc[i] / total);
        const u8 al = (u8)(alpha0 * fmaxf(0.0f, fade));
        const float lx = (rings[i].x - o.x) * q, ly = (rings[i].y - o.y) * q, lz = (rings[i].z - o.z) * q;
        for (int k = 0; k < 3; k++) {
            Vtx& v = sTrajVtx[i * 3 + k];
            v.v.ob[0] = (s16)lroundf(lx + (n1x * kC[k] + n2x * kS[k]) * r);
            v.v.ob[1] = (s16)lroundf(ly + (n2y * kS[k]) * r);
            v.v.ob[2] = (s16)lroundf(lz + (n1z * kC[k] + n2z * kS[k]) * r);
            v.v.flag = 0;
            v.v.tc[0] = v.v.tc[1] = 0;
            v.v.cn[0] = 255;
            v.v.cn[1] = 255;
            v.v.cn[2] = 235;
            v.v.cn[3] = al;
        }
    }

    Gfx* p = sTrajDl;
    SetupTranslucentShade(p);
    {
        // Vertices are sampled at this 20 Hz tick; weld them to the bow hand (live hand x
        // inverse(hand at this tick)) so the line doesn't trail while walking.
        Matrix_Translate(o.x, o.y, o.z, MTXMODE_NEW);
        Matrix_Scale(1.0f / q, 1.0f / q, 1.0f / q, MTXMODE_APPLY);
        Mtx* lineMtx = MATRIX_NEWMTX(gPlayState->state.gfxCtx);
        float hm[4][4];
        if (VR_GetHandMatrix(BowHand(), hm)) {
            MtxF hand, handInv, cur, local;
            memcpy(hand.mf, hm, sizeof(hand.mf));
            if (SkinMatrix_Invert(&hand, &handInv) == 0) {
                Matrix_Get(&cur);
                SkinMatrix_MtxFMtxFMult(&handInv, &cur, &local);
                VR_RegisterHandChildMatrix((const void*)lineMtx, BowHand(), &local.mf[0][0]);
            }
        }
        gSPMatrix(p++, lineMtx, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    }
    for (int i = 0; i + 1 < nr; i++) {
        // two rings (6 verts): 0-2 this point, 3-5 next; three quads around the tube
        gSPVertex(p++, (uintptr_t)&sTrajVtx[i * 3], 6, 0);
        gSP2Triangles(p++, 0, 1, 3, 0, 1, 4, 3, 0);
        gSP2Triangles(p++, 1, 2, 4, 0, 2, 5, 4, 0);
        gSP2Triangles(p++, 2, 0, 5, 0, 0, 3, 5, 0);
    }
    gSPVrPhysMask(p++, 0);
    gSPEndDisplayList(p++);
    assert(p <= sTrajDl + ARRAY_COUNT(sTrajDl));

    OPEN_DISPS(gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sTrajDl);
    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

} // extern "C"

extern "C" void VrArchery_DrawTrajectory(void) {
    if (gPlayState == NULL || !CVarGetInteger("gVrArcheryTrajectory", 1) || !sNocked) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == NULL || !VrArchery_Covers(player)) {
        return;
    }
    TrajSim sim;
    if (!SimulateShot(player, sim)) {
        return;
    }
    float eye[3], fwd[3], up[3];
    VR_GetCameraPose(eye, fwd, up);
    if (CVarGetInteger("gVrArcheryTrajectoryLine", 0)) {
        DrawFlightLine(sim, eye);
    }
    if (sim.hit) {
        DrawLandingRing(sim, eye);
    }
}

// True while a nock is drawn — the string presentation renders pulled to the string hand.
extern "C" bool VrArchery_StringNocked(void) {
    return sNocked && gPlayState != NULL && VrArchery_Covers(GET_PLAYER(gPlayState));
}

// The nock is a held item button: nonzero exactly while nocked, so padmgr ORs the weapon's
// button in as raw state and the vanilla draw/hold/release path just runs (the same trick the
// selector's trigger mirror uses). Mirrors z_player.c sItemButtons order.
extern "C" uint16_t VrArchery_ItemButtonMask(void) {
    if (!sNocked || gPlayState == NULL) {
        return 0;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (!VrArchery_Covers(player)) {
        return 0;
    }
    static const uint16_t kItemButtons[] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                             BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };
    const int slot = player->heldItemButton;
    return (slot >= 0 && slot < (int)(sizeof(kItemButtons) / sizeof(kItemButtons[0]))) ? kItemButtons[slot] : 0;
}

// The string hand's pinch input loses its normal binding only when it is about to nock (near
// the weapon) or currently drawn — a pinch elsewhere keeps whatever it is bound to.
extern "C" bool VrArchery_PinchConsumed(int32_t vrHand, uint16_t vrBtnMask) {
    if (gPlayState == NULL || vrHand != StringHand() || !(vrBtnMask & PinchMask()) || VrOcarina_InPlay()) {
        return false;
    }
    if (!VrArchery_Covers(GET_PLAYER(gPlayState))) {
        return false;
    }
    return sNocked || NearBow();
}

// While nocked: shot origin at the nock anchor, direction string hand -> anchor — the angle
// you pull the string back to is the angle the shot leaves at, exactly like a real string.
// False when idle or the pull is too short to define a stable line, which lets the caller
// fall back to the one-hand aim ray.
extern "C" bool VrArchery_AimSegment(float* outPosDir6) {
    if (gPlayState == NULL) {
        return false;
    }
    // Post-release window: the shot is leaving through the vanilla release path — keep
    // serving the pull line captured while drawn, or the fallback ray would overwrite it.
    if (!sNocked) {
        if (sAimLatchTicks > 0 && sAimLatchLive) {
            for (int i = 0; i < 6; i++) {
                outPosDir6[i] = sAimLatch[i];
            }
            return true;
        }
        return false;
    }
    if (!VrArchery_Covers(GET_PLAYER(gPlayState))) {
        return false;
    }
    float s[3], rot[4], a[3];
    if (!VR_GetHandPose(StringHand(), s, rot) || !NockAnchorWorld(a)) {
        return false;
    }
    float d2 = 0.0f;
    for (int i = 0; i < 3; i++) {
        const float d = a[i] - s[i];
        d2 += d * d;
    }
    const float minLine = 0.03f * WorldScale(); // hand at the anchor: no stable aim line
    if (d2 < minLine * minLine) {
        // Too short for a fresh line; the last good one (if any) still stands.
        if (sAimLatchLive) {
            for (int i = 0; i < 6; i++) {
                outPosDir6[i] = sAimLatch[i];
            }
            return true;
        }
        return false;
    }
    const float len = std::sqrt(d2);
    // QuestShip: the slingshot shot leaves from BETWEEN THE TINES, not from the fork's V where
    // the string is pinched: lift the origin along the weapon hand's pointing axis (the axis the
    // fork extends along; same frame as gVrArcheryAnchorFwd). Direction is unchanged, so aiming
    // by the pull line feels the same.
    float lift[3] = { 0.0f, 0.0f, 0.0f };
    if (GET_PLAYER(gPlayState)->heldItemAction == PLAYER_IA_SLINGSHOT) {
        float bp[3], br[4];
        if (VR_GetHandPose(BowHand(), bp, br)) {
            const float local[3] = { 0.0f, 0.0f, -CVarGetFloat("gVrSlingshotLaunchLift", 6.0f) * 0.01f * WorldScale() };
            QuatRot(br, local, lift);
        }
    }
    float dir[3] = { (a[0] - s[0]) / len, (a[1] - s[1]) / len, (a[2] - s[2]) / len };
    // QuestShip: slingshot aim felt like it tilted down; pitch the launch direction UP by
    // gVrSlingshotAimPitch degrees (about the horizontal axis across the shot). Shot and
    // trajectory line both use this direction.
    if (GET_PLAYER(gPlayState)->heldItemAction == PLAYER_IA_SLINGSHOT) {
        const float pitch = CVarGetFloat("gVrSlingshotAimPitch", 3.0f) * (float)(M_PI / 180.0);
        // up component perpendicular to dir
        float ux = -dir[1] * dir[0], uy = 1.0f - dir[1] * dir[1], uz = -dir[1] * dir[2];
        const float ul = std::sqrt(ux * ux + uy * uy + uz * uz);
        if (ul > 1e-3f) {
            ux /= ul;
            uy /= ul;
            uz /= ul;
            const float c = std::cos(pitch), sn = std::sin(pitch);
            dir[0] = dir[0] * c + ux * sn;
            dir[1] = dir[1] * c + uy * sn;
            dir[2] = dir[2] * c + uz * sn;
        }
    }
    for (int i = 0; i < 3; i++) {
        sAimLatch[i] = a[i] + lift[i];
        sAimLatch[3 + i] = dir[i];
        outPosDir6[i] = sAimLatch[i];
        outPosDir6[3 + i] = sAimLatch[3 + i];
    }
    sAimLatchLive = true;
    return true;
}

namespace {

void ArcheryTick() {
    if (gPlayState == NULL || !GameInteractor::IsSaveLoaded(true)) {
        VrArchery_Reset();
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (!VrArchery_Covers(player)) {
        VrArchery_Reset();
        return;
    }
    if (gPlayState->pauseCtx.state != 0) {
        // Absorb input edges across the pause; state (nocked/idle) resumes on unpause.
        sPinchPrev = (VR_GetGameButtons(StringHand()) & PinchMask()) != 0;
        return;
    }

    if (sAimLatchTicks > 0 && !sNocked) {
        sAimLatchTicks--;
        if (sAimLatchTicks == 0) {
            sAimLatchLive = false;
        }
    }

    const bool pinch = (VR_GetGameButtons(StringHand()) & PinchMask()) != 0;
    const bool pressed = pinch && !sPinchPrev;
    sPinchPrev = pinch;

    const float gap = DrawGapM();
    if (gap >= 0.0f) {
        sDrawM = gap;
    }

    if (!sNocked) {
        // Feelable affordance: a soft tick the moment the string hand enters nock reach
        // (pairs with the visual marker lighting up).
        static bool sWasNear = false;
        const bool near = NearBow();
        if (near && !sWasNear) {
            VR_TriggerHaptic(StringHand(), 0.2f, 0.0f, 15.0f);
        }
        sWasNear = near;
        if (pressed && near) {
            sNocked = true;
            VR_TriggerHaptic(StringHand(), 0.4f, 0.0f, 30.0f);
            VR_TriggerHaptic(BowHand(), 0.25f, 0.0f, 30.0f);
        }
        return;
    }

    if (pinch) {
        // Draw ramp: tension you can feel, growing toward full draw.
        const float full = CVarGetFloat("gVrArcheryFullDraw", 45.0f) * 0.01f;
        float norm = full > 0.01f ? sDrawM / full : 1.0f;
        if (norm > 1.0f) {
            norm = 1.0f;
        }
        VR_TriggerHaptic(StringHand(), 0.08f + 0.3f * norm, 0.0f, 15.0f + 25.0f * norm);
        return;
    }

    // Pinch is up: state-based release (edge may have been absorbed by a pause).
    if (sDrawM >= CVarGetFloat("gVrArcheryMinDraw", 10.0f) * 0.01f) {
        // The nock mask drops with sNocked this tick; the vanilla button release fires the
        // prepared shot NEXT tick — the aim latch keeps serving the drawn pull line through
        // that window so the fallback ray cannot overwrite the direction. Thunk both hands.
        sAimLatchTicks = 4;
        VR_TriggerHaptic(BowHand(), 0.8f, 0.0f, 40.0f);
        VR_TriggerHaptic(StringHand(), 0.6f, 0.0f, 40.0f);
    } else {
        // Too short to fire: clean cancel — arrow killed, ammo and magic preserved. No
        // latch: there is no shot in flight to protect.
        Player_VrCancelPreparedItem(gPlayState, player);
        sAimLatchLive = false;
    }
    sNocked = false;
}

void RegisterVrArchery() {
    COND_HOOK(OnPlayerUpdate, true, ArcheryTick);
    COND_HOOK(OnPlayDrawEnd, true, VrArchery_DrawNockIcon);
    COND_HOOK(OnPlayDrawEnd, true, VrArchery_DrawTrajectory);
}

static RegisterShipInitFunc initVrArchery(RegisterVrArchery);

} // namespace
