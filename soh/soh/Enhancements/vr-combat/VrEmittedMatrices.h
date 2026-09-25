#pragma once
// Include after z64.h / functions.h / variables.h (needs Gfx, Mtx, MtxF, gMtxClear, G_MTX*).

namespace VrCombat {

// QuestShip: every runtime LOAD modelview matrix emitted into a display list in [from, to), for
// re-anchoring freshly drawn models to a hand or a physical-space point. Skips gMtxClear and
// `skip` (pass the billboard matrix).
template <typename Fn> void ForEachEmittedLoadMatrix(Gfx* from, Gfx* to, const Mtx* skip, Fn&& fn) {
    for (Gfx* g = from; g < to; ++g) {
        if (((g->words.w0 >> 24) & 0xFF) != G_MTX) {
            continue;
        }
        const uint32_t params = (uint32_t)(g->words.w0 & 0xFF) ^ G_MTX_PUSH; // F3DEX2 gDma2p layout
        if (!(params & G_MTX_LOAD) || (params & G_MTX_PROJECTION)) {
            continue;
        }
        Mtx* m = (Mtx*)g->words.w1;
        if (m == NULL || m == &gMtxClear || m == skip) {
            continue;
        }
        MtxF mf;
        Matrix_MtxToMtxF(m, &mf);
        fn(m, mf);
    }
}

} // namespace VrCombat
