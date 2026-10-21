#pragma once
#include <cstdint>

// Synchronized start: hold a song's loading sequence at a chosen step (with NOW LOADING still on
// screen) until released, so every player's song can begin at the same moment.
namespace md::sync {

struct PvGameState {
    bool exists;
    int32_t outer; // 0 = loading, 1 = playing, 2 = ending
    int32_t inner; // loading step while outer == 0
};

PvGameState ReadState();

// Loading step 3 is entered right after the assets finished loading; it ends NOW LOADING and
// starts the song clock. Holding there keeps the loading screen up with everything ready.
constexpr int32_t BarrierStep = 3;

// Hold at loading step `step` (-1 = never hold).
void SetHoldStep(int32_t step);
int32_t HoldStep();
bool IsHolding();   // a load is currently stopped at the hold step
void Release();     // let the held load continue (also disarms the hold)

// Called by the PvGameParentCtrl hook every frame (game thread). Returns true to skip this
// frame's state machine update (the load stays at its current step).
bool OnParentCtrl();

} // namespace md::sync
