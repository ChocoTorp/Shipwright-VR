#pragma once

// Engine-independent input lifecycle. Never retain actor pointers or a queue of swaps.
class VrItemSelectionState {
  public:
    static constexpr int NoRequest = -2;

    void Request(int slot, int item) {
        if (slot >= -1 && slot <= 3) {
            pendingSlot = slot;
            pendingItem = item;
        }
    }
    int PendingSlot() const { return pendingSlot; }
    int PendingItem() const { return pendingItem; }
    void CancelRequest() { pendingSlot = NoRequest; }
    void Finish() {
        CancelRequest();
        triggerArmed[0] = triggerArmed[1] = false;
    }
    void ObserveTrigger(int hand, bool held) {
        if (hand >= 0 && hand < 2 && !held) {
            triggerArmed[hand] = true;
        }
    }
    bool TriggerArmed(int hand) const {
        return hand >= 0 && hand < 2 && triggerArmed[hand];
    }

  private:
    int pendingSlot = NoRequest;
    int pendingItem = 0;
    bool triggerArmed[2] = { false, false };
};
