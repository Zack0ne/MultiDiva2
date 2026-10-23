#include "memory.h"
#include <cstring>

namespace md {

bool SafeCopy(void* dst, const void* src, size_t size)
{
    __try {
        memcpy(dst, src, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

const char* ToString(Difficulty d)
{
    switch (d) {
    case Difficulty::Easy: return "Easy";
    case Difficulty::Normal: return "Normal";
    case Difficulty::Hard: return "Hard";
    case Difficulty::Extreme: return "Extreme";
    case Difficulty::ExExtreme: return "ExExtreme";
    }
    return "?";
}

const char* ToString(FinalGrade g)
{
    switch (g) {
    case FinalGrade::Failed: return "Failed";
    case FinalGrade::Cheap: return "Cheap";
    case FinalGrade::Standard: return "Standard";
    case FinalGrade::Great: return "Great";
    case FinalGrade::Excellent: return "Excellent";
    case FinalGrade::Perfect: return "Perfect";
    }
    return "?";
}

uintptr_t StageEntryAddress(int32_t index)
{
    // Mirrors the game: out-of-range indices fall back to entry 2.
    if (index < 0 || index >= stage::EntryCount)
        index = 2;
    return addr::StageData + stage::EntriesOffset + (size_t)index * stage::EntryStride;
}

void ReadStageState(StageState& out)
{
    out.ok = SafeRead(addr::StageData + stage::IndexOffset, out.index);
    for (int32_t i = 0; i < stage::EntryCount && out.ok; i++) {
        uintptr_t e = StageEntryAddress(i);
        StageEntry& entry = out.entries[i];
        out.ok = SafeRead(e + stage::PvId, entry.pvId)
              && SafeRead(e + stage::Difficulty, entry.difficulty)
              && SafeRead(e + stage::Edition, entry.edition);
    }
}

void ReadGameState(GameState& s)
{
    s = GameState{};
    s.liveOk = SafeRead(addr::PlayData + play::Score, s.liveScore) && SafeRead(addr::PlayData + play::Combo, s.liveCombo)
        && SafeRead(addr::PlayData + play::Completion, s.completion);

    int32_t index = 0, counts[5] = {}, percentage = 0;
    uintptr_t entry = SafeRead(addr::StageData + stage::IndexOffset, index) ? StageEntryAddress(index) : 0;
    s.resultOk = entry && SafeRead(entry + stage::Grade, s.grade) && SafeRead(entry + stage::ResultCounts, counts)
        && SafeRead(entry + stage::MaxCombo, s.maxCombo) && SafeRead(entry + stage::Score, s.score)
        && SafeRead(entry + stage::Percentage, percentage);
    s.cool = counts[0];
    s.fine = counts[1];
    s.safe = counts[2];
    s.sad = counts[3];
    s.worst = counts[4];
    s.resultCompletion = percentage / 100.0f;
}

} // namespace md
