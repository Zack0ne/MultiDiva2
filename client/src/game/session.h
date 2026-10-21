#pragma once
#include <deque>
#include <vector>
#include "events.h"
#include "memory.h"
#include "note_grade.h"

namespace md {

// Everything the client knows about the local game, updated once per frame on the render thread.
struct Session {
    bool inSong = false;
    bool practice = false;
    int32_t pvId = -1; // from the SongStarted hook argument

    GameState state{};
    StageState stage{};
    NoteGradeTracker grades;
    NoteGrade lastGrade = NoteGrade::Cool;
    bool hasLastGrade = false;

    std::deque<GameEvent> recentEvents; // newest first, capped
    std::vector<GameEvent> frameEvents; // events handled in the latest SessionTick, oldest first
};

extern Session g_session;

// Drains hook events, reads game memory and runs the note-grade tracker.
void SessionTick();

} // namespace md
