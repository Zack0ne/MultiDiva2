#include "sync_start.h"
#include "events.h"
#include "memory.h"

#include <atomic>
#include <windows.h>

namespace md::sync {

static std::atomic<int32_t> s_holdStep{-1};
static std::atomic<bool> s_holding{false};
static std::atomic<bool> s_released{false}; // the current load was released, don't hold it again
static int32_t s_lastOuter = -1, s_lastInner = -1;

PvGameState ReadState()
{
    PvGameState st{ false, -1, -1 };
    uintptr_t game = 0;
    if (!SafeRead(addr::PvGamePtr, game) || !game)
        return st;
    st.exists = SafeRead(game + pvgame::OuterState, st.outer) && SafeRead(game + pvgame::InnerState, st.inner);
    return st;
}

void SetHoldStep(int32_t step) { s_holdStep = step; }
int32_t HoldStep() { return s_holdStep; }
bool IsHolding() { return s_holding; }

void Release()
{
    s_holdStep = -1;
    if (s_holding) {
        s_released = true;
        s_holding = false;
    }
}

bool OnParentCtrl()
{
    PvGameState st = ReadState();
    if (!st.exists)
        return false;

    if (st.outer != s_lastOuter || st.inner != s_lastInner) {
        GameEvent e{ GameEventType::PvGameState, st.outer, GetTickCount() };
        e.difficulty = st.inner;
        g_gameEvents.TryPush(e);
        s_lastOuter = st.outer;
        s_lastInner = st.inner;
        if (st.outer != 0)
            s_released = false; // loading is over: the next load may be held again
    }

    bool hold = st.outer == 0 && st.inner == s_holdStep && !s_released;
    s_holding = hold;
    return hold;
}

} // namespace md::sync
