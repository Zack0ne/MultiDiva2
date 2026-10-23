#pragma once
#include <deque>
#include <vector>
#include "events.h"
#include "memory.h"

namespace md {

// Everything the client knows about the local game, updated once per frame on the render thread.
struct Session {
    bool inSong = false;
    bool inResult = false; // on the result screen of the last song
    int32_t pvId = -1;     // the launched song (stage entry, at task_pv_game_init_pv)

    GameState state{};
    StageState stage{};

    std::deque<GameEvent> recentEvents; // newest first, capped
    std::vector<GameEvent> frameEvents; // events handled in the latest SessionTick, oldest first
};

extern Session g_session;

// Drains hook events and reads game memory.
void SessionTick();

} // namespace md
