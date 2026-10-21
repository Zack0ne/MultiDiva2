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

// MSVC x64 std::string: { union { char buf[16]; char* ptr; }; size_t size; size_t capacity; }
static void GuessMsvcString(const uint8_t raw[32], char* out, size_t outSize)
{
    out[0] = '\0';
    uint64_t size, capacity;
    memcpy(&size, raw + 16, 8);
    memcpy(&capacity, raw + 24, 8);
    if (capacity < 15 || size > capacity || size > 4096)
        return;

    const char* text = (const char*)raw;
    if (capacity > 15) {
        uint64_t ptr;
        memcpy(&ptr, raw, 8);
        text = (const char*)ptr;
    }

    size_t n = (size_t)size < outSize - 1 ? (size_t)size : outSize - 1;
    if (!SafeCopy(out, text, n)) {
        out[0] = '\0';
        return;
    }
    out[n] = '\0';
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
    s.scoreOk = SafeRead(addr::MainScore, s.score);
    s.liveOk = SafeRead(addr::LiveScore, s.liveScore) && SafeRead(addr::LiveCombo, s.liveCombo);
    s.completionOk = SafeRead(addr::Completion, s.completion);
    s.pvOk = SafeRead(addr::PvId, s.pvId)
          && SafeRead(addr::PvDiffVanilla, s.diffVanilla)
          && SafeRead(addr::PvDiffSongLimitPatch, s.diffSongLimitPatch);
    s.gradeOk = SafeRead(addr::PvGrade, s.grade);
    s.worstOk = SafeRead(addr::WorstCounter, s.worstCounter);

    if (SafeRead(addr::PvTitle, s.titleRaw))
        GuessMsvcString(s.titleRaw, s.titleGuess, sizeof(s.titleGuess));
    else
        s.titleGuess[0] = '\0';
}

} // namespace md
