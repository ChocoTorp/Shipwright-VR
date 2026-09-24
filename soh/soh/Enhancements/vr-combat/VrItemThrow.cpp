extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h" // gMtxClear
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h" // ARROW_NUT
extern PlayState* gPlayState;
}
#include "VrCombat.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <cmath>
#include <cstring>
#include <vr_interface.h>

namespace {
bool sGripPrev[2] = { true, true }; // require a fresh press on entry/reset
int sCarryHand = -1;                // hand that grabbed the current throwable, -1 = none
int sObservedItem = -1;
bool sReleaseHasGrip = false;

float WorldScale() {
    const float scale = VR_GetWorldScale();
    return scale < 1.0f ? 35.0f : scale;
}

int SwordHandIdx() {
    return CVarGetInteger("gVrLeftHanded", 0) ? VR_HAND_LEFT : VR_HAND_RIGHT;
}

// The hand that owns the current carry. A hold restored without an observed grab (save-state
// load) defaults to the sword hand; sReleaseHasGrip stays false there, so it drops, not throws.
int CarryHand() {
    return sCarryHand >= 0 ? sCarryHand : SwordHandIdx();
}

bool HasThrowable(Player* player) {
    return player && player->heldActor && (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) &&
           player->heldActor->parent == &player->actor &&
           (player->heldActor->id == ACTOR_EN_BOM || player->heldActor->id == ACTOR_EN_ARROW);
}

bool SwapChord() {
    const auto mask = (uint16_t)CVarGetInteger("gVrItemSelSwapInput", VR_BTN_GRIP);
    return mask && (VR_GetControllerButton(0) & mask) && (VR_GetControllerButton(1) & mask);
}

// Any-hand grabs (behavior plan): a hand is "about to grab" when it is within reach of the
// presented preview. Shared by the grab tick and the padmgr grip reservation, so a grip press
// only loses its normal binding when it would actually grab.
bool HandNearPreview(int hand) {
    float preview[3], position[3], rotation[4];
    if (!VrItemThrow_PreviewPosition(preview) || !VR_GetHandPose(hand, position, rotation)) {
        return false;
    }
    float distanceSq = 0.0f;
    for (int axis = 0; axis < 3; ++axis) {
        const float d = position[axis] - preview[axis];
        distanceSq += d * d;
    }
    const float reach = WorldScale() * 0.15f;
    return distanceSq <= reach * reach;
}
}

extern "C" bool VrItemThrow_Active(Player* player) {
    return VrItemSelect_ModeActive() && CVarGetInteger("gVrPhysicalItemThrows", 1) && player &&
           player->actor.category == ACTORCAT_PLAYER &&
           (player->heldItemAction == PLAYER_IA_BOMB || player->heldItemAction == PLAYER_IA_DEKU_NUT) &&
           gPlayState && gPlayState->bombchuBowlingStatus == 0 && gPlayState->shootingGalleryStatus == 0 &&
           !(player->stateFlags1 & (PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER));
}

extern "C" void VrItemThrow_Reset(void) {
    sGripPrev[0] = sGripPrev[1] = true;
    sCarryHand = -1;
    sObservedItem = -1;
    sReleaseHasGrip = false;
}

extern "C" bool VrItemThrow_PreviewPosition(float* position) {
    if (!gPlayState || !VrItemThrow_Active(GET_PLAYER(gPlayState)) ||
        !VrItemSelect_SelectionAllowed() || GET_PLAYER(gPlayState)->heldActor) {
        return false;
    }
    float eye[3], forward[3], up[3];
    VR_GetCameraPose(eye, forward, up);
    const float scale = WorldScale();
    // Stable chest-height presentation in front of the player; looking up/down must
    // not move the target out of reach. Exact distances remain headset-tunable.
    float length = std::sqrt(forward[0] * forward[0] + forward[2] * forward[2]);
    if (length < 0.01f) return false;
    position[0] = eye[0] + forward[0] / length * scale * 0.4f;
    position[1] = eye[1] - scale * 0.25f;
    position[2] = eye[2] + forward[2] / length * scale * 0.4f;
    return true;
}

extern "C" bool VrItemThrow_GripConsumed(int32_t hand, uint16_t mask) {
    if (!gPlayState || !(mask & VR_BTN_GRIP) || VrOcarina_InPlay()) {
        return false;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (!VrItemThrow_Active(player)) {
        return false;
    }
    // Carrying: the owning hand's grip is the release input. Preview up: only a hand close
    // enough to grab loses its binding — a grip press elsewhere keeps Z-target and friends.
    return HasThrowable(player) ? hand == CarryHand() : HandNearPreview(hand);
}

extern "C" void VrItemThrow_UpdateCarryPose(Player* player) {
    if (!VrItemThrow_Active(player) || !HasThrowable(player)) return;
    float position[3], rotation[4];
    if (VR_GetHandPose(CarryHand(), position, rotation)) {
        player->heldActor->world.pos = { position[0], position[1], position[2] };
    }
}

extern "C" void VrItemThrow_Tick(PlayState* play, Player* player) {
    if (!VrItemThrow_Active(player) || !VrItemSelect_SelectionAllowed() ||
        play->pauseCtx.state != 0 || player->unk_6AD != 0 || player->heldItemAction != player->itemAction) {
        VrItemThrow_Reset();
        return;
    }
    if (sObservedItem != player->heldItemId) {
        sObservedItem = player->heldItemId;
        sGripPrev[0] = sGripPrev[1] = true;
    }
    bool pressed[2], released[2];
    for (int hand = 0; hand < 2; ++hand) {
        const bool grip = (VR_GetControllerButton(hand) & VR_BTN_GRIP) != 0;
        pressed[hand] = grip && !sGripPrev[hand];
        released[hand] = !grip && sGripPrev[hand];
        sGripPrev[hand] = grip;
    }
    if (HasThrowable(player) && (VR_GetControllerButton(CarryHand()) & VR_BTN_GRIP)) {
        sReleaseHasGrip = true;
    }
    if (SwapChord() || VrItemSelect_PendingSlot() != -2) return;

    if (HasThrowable(player)) {
        VrItemThrow_UpdateCarryPose(player);
        const int hand = CarryHand();
        if (released[hand]) {
            float velocity[3] = {}, angular[3];
            // Runtime velocities exclude locomotion/snap-turn displacement. Use the
            // shared path snapshot if available, never drain the XR buffer again.
            const auto& path = VrCombat::GetTickPath(hand);
            int samples = 0;
            if (path.count) {
                const auto latest = path.samples[path.count - 1].timeNs;
                for (int i = 0; i < path.count; ++i) {
                    if (latest - path.samples[i].timeNs > 100000000ULL) continue;
                    for (int axis = 0; axis < 3; ++axis) velocity[axis] += path.samples[i].linVelMps[axis];
                    ++samples;
                }
            }
            if (samples) {
                for (float& component : velocity) component /= samples;
            } else {
                VR_GetHandVelocity(hand, velocity, angular);
            }
            // A restored/interrupted hold without an observed grip is a drop, never
            // a throw using motion samples from the previous timeline.
            if (!sReleaseHasGrip) velocity[0] = velocity[1] = velocity[2] = 0.0f;
            sReleaseHasGrip = false;
            const float scale = WorldScale();
            for (float& component : velocity) component *= scale * 1.4f / 20.0f;
            Player_VrReleaseItem(play, player, velocity);
            VR_TriggerHaptic(hand, 0.35f, 0.0f, 25.0f);
            sCarryHand = -1;
        }
    } else {
        sCarryHand = -1;
        // Any hand may claim the preview; sword hand checked first on a same-tick tie.
        const int first = SwordHandIdx();
        for (int i = 0; i < 2; ++i) {
            const int hand = i == 0 ? first : 1 - first;
            if (pressed[hand] && HandNearPreview(hand) && Player_VrGrabItem(play, player)) {
                sCarryHand = hand;
                VrItemThrow_UpdateCarryPose(player);
                VR_TriggerHaptic(hand, 0.5f, 0.0f, 35.0f);
                break;
            }
        }
    }
}

// ---- QuestShip: the Deku Nut (and slingshot Deku Seed) as real 3D models in VR ----
// Seeds follow the same rules: welded to the string (pouch) hand while nocked, tumbling in flight.
// Vanilla has no nut model for the projectile: in flight it is a near-black spinning sparkle
// (EnArrow_Draw, ARROW_NUT), and while carried it draws nothing (that path needs speedXZ != 0).
// In VR the nut is the get-item model instead: welded to the carrying hand at headset rate
// (live hand-child matrices, like the bowstring), and tumbling along its flight. Display-only:
// masked out of physical combat's visual-mesh collision.
namespace {
// Every runtime LOAD matrix emitted in [from, to): re-express it relative to the hand at this
// tick and register it as a live child of that hand.
void WeldEmittedToHand(Gfx* from, Gfx* to, int hand, MtxF* handInv) {
    for (Gfx* g = from; g < to; ++g) {
        if (((g->words.w0 >> 24) & 0xFF) != G_MTX) {
            continue;
        }
        const uint32_t params = (uint32_t)(g->words.w0 & 0xFF) ^ G_MTX_PUSH;
        if (!(params & G_MTX_LOAD) || (params & G_MTX_PROJECTION)) {
            continue;
        }
        Mtx* m = (Mtx*)g->words.w1;
        if (m == NULL || m == &gMtxClear || (gPlayState && m == gPlayState->billboardMtx)) {
            continue;
        }
        MtxF cur;
        MtxF local;
        Matrix_MtxToMtxF(m, &cur);
        SkinMatrix_MtxFMtxFMult(handInv, &cur, &local);
        VR_RegisterHandChildMatrix((const void*)m, hand, &local.mf[0][0]);
    }
}
} // namespace

extern "C" bool VrItemThrow_DrawNutModel(Actor* actor, PlayState* play) {
    if (actor == NULL || play == NULL || (actor->params != ARROW_NUT && actor->params != ARROW_SEED) ||
        !VR_IsInitialized() || !CVarGetInteger("gVrNutModel", 1)) {
        return false;
    }
    const bool isSeed = actor->params == ARROW_SEED;
    const s16 gid = isSeed ? GID_SEEDS : GID_NUTS;
    Player* player = GET_PLAYER(play);
    const bool held = player != NULL && player->heldActor == actor && actor->parent == &player->actor;
    const bool flying = actor->parent == NULL && (actor->speedXZ != 0.0f || actor->velocity.y != 0.0f);
    if (!held && !flying) {
        return false;
    }
    const float scale = isSeed ? CVarGetFloat("gVrSeedModelScale", 0.05f) : CVarGetFloat("gVrNutModelScale", 0.06f);

    OPEN_DISPS(play->state.gfxCtx);
    VrCombat_MeshMaskPush(play->state.gfxCtx);
    Gfx* opaStart = POLY_OPA_DISP;
    Gfx* xluStart = POLY_XLU_DISP;
    if (held) {
        const int hand = isSeed ? VrArchery_StringHand() : CarryHand();
        float pos[3] = { actor->world.pos.x, actor->world.pos.y, actor->world.pos.z };
        float rot[4];
        VR_GetHandPose(hand, pos, rot);
        Matrix_Translate(pos[0], pos[1], pos[2], MTXMODE_NEW);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        GetItem_Draw(play, gid);
        float hm[4][4];
        if (VR_GetHandMatrix(hand, hm)) {
            MtxF handMtx;
            MtxF handInv;
            memcpy(handMtx.mf, hm, sizeof(handMtx.mf));
            if (SkinMatrix_Invert(&handMtx, &handInv) == 0) {
                WeldEmittedToHand(opaStart, POLY_OPA_DISP, hand, &handInv);
                WeldEmittedToHand(xluStart, POLY_XLU_DISP, hand, &handInv);
            }
        }
    } else {
        const float spin = (float)((play->gameplayFrames & 0xFF) * 4000) * (float)(M_PI / 0x8000);
        Matrix_Translate(actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(spin, MTXMODE_APPLY);
        Matrix_RotateX(spin * 0.7f, MTXMODE_APPLY);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        GetItem_Draw(play, gid);
    }
    VrCombat_MeshMaskPop(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}
