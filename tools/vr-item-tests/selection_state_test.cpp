#include "../../soh/soh/Enhancements/vr-combat/VrItemSelectionState.h"
#include <cassert>
#include <cstdio>

int main() {
    VrItemSelectionState state;
    assert(state.PendingSlot() == VrItemSelectionState::NoRequest);
    // A held trigger at entry is not a new activation.
    state.ObserveTrigger(0, true);
    assert(!state.TriggerArmed(0));
    state.ObserveTrigger(0, false);
    assert(state.TriggerArmed(0));
    assert(!state.TriggerArmed(1));

    // Unsafe transitions wait; a newer choice replaces both slot and identity.
    state.Request(1, 10);
    state.Request(3, 20);
    assert(state.PendingSlot() == 3 && state.PendingItem() == 20);
    state.Request(9, 99);
    assert(state.PendingSlot() == 3 && state.PendingItem() == 20);
    state.Request(-1, 255);
    assert(state.PendingSlot() == -1);

    // Completing a transition requires a release on each hand independently.
    state.Finish();
    assert(state.PendingSlot() == VrItemSelectionState::NoRequest);
    assert(!state.TriggerArmed(0) && !state.TriggerArmed(1));
    state.ObserveTrigger(0, true);
    state.ObserveTrigger(1, false);
    assert(!state.TriggerArmed(0) && state.TriggerArmed(1));
    assert(!state.TriggerArmed(-1) && !state.TriggerArmed(2));

    // Cancelling a request must not synthesize release on the active bow.
    state.Request(0, 3);
    state.CancelRequest();
    assert(state.TriggerArmed(1));
    assert(state.PendingSlot() == VrItemSelectionState::NoRequest);
    std::puts("VR item selection lifecycle: passed");
}
