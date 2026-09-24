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

Vtx sAmmoVtx[3 * 4]; // QuestShip: ammo digits at the nock point

// Nock-point icon: a miniature Deku Nut (the classic drop model, gameplay_keep so it is
// always loaded) rendered in-world at the nock anchor while the bow/slingshot is out and no
// nock is drawn. It grows when the string hand is in pinch reach. DELIBERATELY minimal gates
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

// QuestShip: predicted flight path while the string is drawn — a thin translucent ribbon that
// follows the projectile's REAL motion (EnArrow_Shoot/EnArrow_Fly): launched along the aim line at
// 80 (seed) / 150 (arrow) units per tick, moved 1.5x velocity per update, gravity -0.4 only once
// the flight timer (15 / 12) drops below 7.2, killed at 0. Stops at the first surface it would
// hit. Camera-facing, fading toward the end, no depth write, masked out of combat collision.
constexpr int kTrajMaxPts = 16;
Vtx sTrajVtx[kTrajMaxPts * 3];
Gfx sTrajDl[96];

extern "C" void VrArchery_DrawTrajectory(void) {
    if (gPlayState == NULL || !CVarGetInteger("gVrArcheryTrajectory", 1) || !sNocked) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == NULL || !VrArchery_Covers(player)) {
        return;
    }
    float seg[6];
    if (!VrArchery_AimSegment(seg)) {
        return;
    }
    const bool seed = player->heldItemAction == PLAYER_IA_SLINGSHOT;
    const float speed = seed ? 80.0f : 150.0f;
    int timer = seed ? 15 : 12;
    Vec3f pos = { seg[0], seg[1], seg[2] };
    float vx = seg[3] * speed, vy = seg[4] * speed, vz = seg[5] * speed;
    float gravity = 0.0f;
    Vec3f pts[kTrajMaxPts];
    int n = 0;
    pts[n++] = pos;
    while (n < kTrajMaxPts) {
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
        Vec3f hit;
        CollisionPoly* poly = NULL;
        s32 bgId = 0;
        if (BgCheck_EntityLineTest1(&gPlayState->colCtx, &pos, &next, &hit, &poly, true, true, true, true, &bgId)) {
            pts[n++] = hit;
            break;
        }
        pts[n++] = next;
        pos = next;
    }
    if (n < 2) {
        return;
    }

    float eye[3], fwd[3], up[3];
    VR_GetCameraPose(eye, fwd, up);
    // A thin 3-sided TUBE (triangular prism) rather than a camera-facing strip: a flat strip
    // turns edge-on as the head moves between 20 Hz updates (and differs per eye), so it
    // flickered thinner/thicker. A tube reads the same from every angle. Radius still scales
    // with distance so it stays a thin line on screen.
    const float widthK = CVarGetFloat("gVrArcheryTrajectoryWidth", 0.0012f);
    const int alpha0 = CVarGetInteger("gVrArcheryTrajectoryAlpha", 120);
    for (int i = 0; i < n; i++) {
        const Vec3f& a = pts[i > 0 ? i - 1 : 0];
        const Vec3f& b = pts[i > 0 ? i : 1];
        float tx = b.x - a.x, ty = b.y - a.y, tz = b.z - a.z;
        const float tl = sqrtf(tx * tx + ty * ty + tz * tz);
        if (tl > 1e-4f) {
            tx /= tl;
            ty /= tl;
            tz /= tl;
        }
        // n1 = tangent x worldUp (fallback X), n2 = tangent x n1
        float n1x = -tz, n1y = 0.0f, n1z = tx;
        float n1l = sqrtf(n1x * n1x + n1z * n1z);
        if (n1l < 1e-3f) {
            n1x = 1.0f;
            n1z = 0.0f;
            n1l = 1.0f;
        }
        n1x /= n1l;
        n1z /= n1l;
        const float n2x = ty * n1z - tz * n1y, n2y = tz * n1x - tx * n1z, n2z = tx * n1y - ty * n1x;
        const float ex = eye[0] - pts[i].x, ey = eye[1] - pts[i].y, ez = eye[2] - pts[i].z;
        const float r = fmaxf(0.03f, sqrtf(ex * ex + ey * ey + ez * ez) * widthK);
        const u8 al = (u8)(alpha0 * (1.0f - (float)i / (float)(n - 1)));
        static const float kC[3] = { 1.0f, -0.5f, -0.5f };
        static const float kS[3] = { 0.0f, 0.8660254f, -0.8660254f };
        for (int k = 0; k < 3; k++) {
            Vtx& v = sTrajVtx[i * 3 + k];
            v.v.ob[0] = (s16)(pts[i].x + (n1x * kC[k] + n2x * kS[k]) * r);
            v.v.ob[1] = (s16)(pts[i].y + (n1y * kC[k] + n2y * kS[k]) * r);
            v.v.ob[2] = (s16)(pts[i].z + (n1z * kC[k] + n2z * kS[k]) * r);
            v.v.flag = 0;
            v.v.tc[0] = v.v.tc[1] = 0;
            v.v.cn[0] = 255;
            v.v.cn[1] = 255;
            v.v.cn[2] = 235;
            v.v.cn[3] = al;
        }
    }

    Gfx* p = sTrajDl;
    gSPVrPhysMask(p++, 1);
    gDPPipeSync(p++);
    gDPSetCycleType(p++, G_CYC_1CYCLE);
    gDPSetRenderMode(p++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    gDPSetCombineMode(p++, G_CC_SHADE, G_CC_SHADE);
    gSPTexture(p++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gSPClearGeometryMode(p++, G_CULL_BOTH | G_LIGHTING | G_FOG);
    gSPSetGeometryMode(p++, G_SHADE | G_SHADING_SMOOTH);
    {
        // QuestShip: vertices are world-space at this 20 Hz tick; weld them to the bow hand (live
        // hand x inverse(hand at this tick)) so the line doesn't trail while walking.
        Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
        Mtx* lineMtx = MATRIX_NEWMTX(gPlayState->state.gfxCtx);
        float hm[4][4];
        if (VR_GetHandMatrix(BowHand(), hm)) {
            MtxF hand;
            MtxF handInv;
            memcpy(hand.mf, hm, sizeof(hand.mf));
            if (SkinMatrix_Invert(&hand, &handInv) == 0) {
                VR_RegisterHandChildMatrix((const void*)lineMtx, BowHand(), &handInv.mf[0][0]);
            }
        }
        gSPMatrix(p++, lineMtx, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    }
    for (int i = 0; i + 1 < n; i++) {
        // two rings (6 verts): 0-2 this point, 3-5 next; three quads around the tube
        gSPVertex(p++, (uintptr_t)&sTrajVtx[i * 3], 6, 0);
        gSP2Triangles(p++, 0, 1, 3, 0, 1, 4, 3, 0);
        gSP2Triangles(p++, 1, 2, 4, 0, 2, 5, 4, 0);
        gSP2Triangles(p++, 2, 0, 5, 0, 0, 3, 5, 0);
    }
    gSPVrPhysMask(p++, 0);
    gSPEndDisplayList(p++);

    OPEN_DISPS(gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sTrajDl);
    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

// QuestShip: the string (pouch) hand, for drawing the nocked seed in it.
extern "C" int VrArchery_StringHand(void) {
    return StringHand();
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
        sPinchPrev = (VR_GetControllerButton(StringHand()) & PinchMask()) != 0;
        return;
    }

    if (sAimLatchTicks > 0 && !sNocked) {
        sAimLatchTicks--;
        if (sAimLatchTicks == 0) {
            sAimLatchLive = false;
        }
    }

    const bool pinch = (VR_GetControllerButton(StringHand()) & PinchMask()) != 0;
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
