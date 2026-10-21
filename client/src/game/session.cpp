#include "session.h"
#include "pause_guard.h"
#include "titles.h"
#include "../core/log.h"

namespace md {

Session g_session;

static constexpr size_t MaxRecentEvents = 32;

static void LogResult(const GameState& s)
{
    Log("result: total=%u combo=%u cool=%u fine=%u safe=%u sad=%u worst=%u worstCounter=%u",
        s.score.totalScore, s.score.combo, s.score.cool, s.score.fine, s.score.safe, s.score.sad,
        s.score.worst, s.worstCounter);
    Log("result: pv=%d diff(vanilla)=%d diff(SLP)=%d grade=%d completion=%.2f title=\"%s\"",
        s.pvId, s.diffVanilla, s.diffSongLimitPatch, s.grade, s.completion, s.titleGuess);
}

static void Handle(const GameEvent& e)
{
    if (e.type == GameEventType::PvGameInit || e.type == GameEventType::SongEnded)
        pause::Reset();

    Session& s = g_session;
    switch (e.type) {
    case GameEventType::SongStarted:
        Log("SongStarted pvId=%d", e.pvId);
        s.inSong = true;
        s.practice = false;
        s.pvId = e.pvId;
        s.grades.Reset();
        s.hasLastGrade = false;
        break;
    case GameEventType::PracticeStarted:
        Log("PracticeStarted");
        s.practice = true;
        break;
    case GameEventType::SongEnded:
        Log("SongEnded");
        s.inSong = false;
        break;
    case GameEventType::ModeShift: {
        int32_t m[12] = {};
        SafeRead(addr::ModeState, m);
        Log("ModeShift %s -> %d (caller %llX)  state: %d %d %d %d | %d %d %d %d | %d %d %d %d",
            e.edition ? "sub" : "mode", e.pvId, (unsigned long long)e.caller,
            m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11]);
        break;
    }
    case GameEventType::PvGameState:
        Log("PvGameState outer=%d inner=%d", e.pvId, e.difficulty);
        break;
    case GameEventType::PvGameInit:
        if (e.overridden)
            Log("PvGameInit stage=%d pv=%d diff=%d edition=%d (OVERRIDDEN, game had picked pv=%d)",
                e.stageIndex, e.pvId, e.difficulty, e.edition, e.originalPvId);
        else
            Log("PvGameInit stage=%d pv=%d diff=%d edition=%d", e.stageIndex, e.pvId, e.difficulty, e.edition);
        Log("  modules: %d %d %d %d %d %d", e.modules[0], e.modules[1], e.modules[2], e.modules[3], e.modules[4], e.modules[5]);
        break;
    case GameEventType::ResultShown:
        Log("ResultShown");
        s.inSong = false;
        ReadGameState(s.state);
        LogResult(s.state);
        break;
    }

    s.recentEvents.push_front(e);
    if (s.recentEvents.size() > MaxRecentEvents)
        s.recentEvents.pop_back();
}

void SessionTick()
{
    g_session.frameEvents.clear();
    GameEvent e;
    while (g_gameEvents.TryPop(e)) {
        Handle(e);
        g_session.frameEvents.push_back(e);
    }

    Session& s = g_session;
    ReadGameState(s.state);
    ReadStageState(s.stage);

    if (s.inSong && s.state.liveOk) {
        NoteGrade grade;
        if (s.grades.Update(s.state.liveScore, s.state.liveCombo, grade)) {
            s.lastGrade = grade;
            s.hasLastGrade = true;
        }
    }

    titles::Tick();
}

} // namespace md
