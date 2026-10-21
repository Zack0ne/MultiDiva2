#pragma once
#include <cstdint>
#include "../core/event_queue.h"

namespace md {

enum class GameEventType : uint8_t {
    SongStarted,     // pvId valid
    PracticeStarted,
    SongEnded,
    ResultShown,
    ModeShift,       // pvId = requested mode id, edition = 0 (mode) / 1 (sub mode)
    PvGameState,     // pv_game state machine changed: pvId = outer state, difficulty = inner (loading) step
    PvGameInit,      // pvId/difficulty/edition = what the game launches; overridden = rewritten by launch::Arm
};

struct GameEvent {
    GameEventType type;
    int32_t pvId;
    uint32_t tick; // GetTickCount() when the hook fired
    int32_t stageIndex = -1;
    int32_t difficulty = -1;
    int32_t edition = -1;
    int32_t originalPvId = -1; // before the override, if any
    bool overridden = false;
    int32_t modules[6] = {}; // stage entry module ids per performer slot
    uintptr_t caller = 0;    // return address of the hooked call, for RE
};

const char* ToString(GameEventType type);

// Filled by game hooks (game thread), drained once per frame by the session tick.
extern BoundedQueue<GameEvent, 256> g_gameEvents;

} // namespace md
