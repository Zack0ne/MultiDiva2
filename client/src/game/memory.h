#pragma once
#include <cstddef>
#include <cstdint>
#include "../core/helpers.h"

namespace md {

// Absolute addresses for game v1.03, verified in-game against the HUD and result screen.
// The executable has no ASLR (ImageBase 0x140000000), so absolute addresses are stable per version.
namespace addr {
// Stage data (ReDIVA: struc_717, returned by the getter at 0x14040B2A0).
constexpr uintptr_t StageData            = 0x1416E2B90;
// Mode state (returned by the getter at 0x1402C4810), written by shift_next_mode / shift_next_mode_sub.
constexpr uintptr_t ModeState            = 0x14CC61078;
// pv_game* (ReDIVA: pv_game_ptr), null outside a song; read by pv_game_parent::ctrl (0x140265E60).
constexpr uintptr_t PvGamePtr            = 0x14CC105A0;
constexpr uintptr_t MainScore            = 0x1412EF568; // DivaScore (final result block)
constexpr uintptr_t LiveScore            = 0x1412EF56C; // score while playing (legacy: read as int64 - 2^32)
constexpr uintptr_t LiveCombo            = 0x1412EF578; // combo while playing
constexpr uintptr_t Completion           = 0x1412EF634; // float, percent
constexpr uintptr_t PvTitle              = 0x1412EF228; // type unknown; dumped raw + as std::string guess
constexpr uintptr_t PvId                 = 0x1412C2340;
constexpr uintptr_t PvDiffVanilla        = 0x1412B634C;
constexpr uintptr_t PvDiffSongLimitPatch = 0x1423157AC; // only meaningful with SongLimitPatch installed
constexpr uintptr_t PvGrade              = 0x1416E2D00;
constexpr uintptr_t WorstCounter         = 0x1416E2D40; // = result screen "Miss/Wrong/Almost" (DivaScore::worst is a subset)
} // namespace addr

struct DivaScore {
    uint32_t totalScore;
    uint32_t unknown[8];
    uint32_t combo;
    uint32_t preAdjustCool, preAdjustFine, preAdjustSafe, preAdjustSad, preAdjustWorst;
    uint32_t cool, fine, safe, sad, worst;
};
ASSERT_OFFSETOF(DivaScore, combo, 0x24);
ASSERT_OFFSETOF(DivaScore, preAdjustCool, 0x28);
ASSERT_OFFSETOF(DivaScore, cool, 0x3C);
ASSERT_OFFSETOF(DivaScore, worst, 0x4C);
ASSERT_SIZEOF(DivaScore, 0x50);

// Song launch parameters inside one stage entry (ReDIVA: struc_716 / struc_775).
// Layout taken from task_pv_game_init_pv (0x1404414B0), which reads these to start a song.
namespace stage {
constexpr size_t IndexOffset   = 0x08;  // int32 current stage index (0..3, else entry 2 is used)
constexpr size_t EntriesOffset = 0x30;
constexpr size_t EntryStride   = 0x6A8;
constexpr int    EntryCount    = 4;
constexpr size_t PvId          = 0x2C;  // int32
constexpr size_t Difficulty    = 0x34;  // int32, see md::Difficulty
constexpr size_t Edition       = 0x38;  // int32, 0 = normal, 1 = extra (EX chart)
constexpr size_t Modules       = 0x78;  // int32[6], module id per performer slot
} // namespace stage

// pv_game fields used by the loading state machine (0x140268100: outer switch on +0x10, inner on +0x14).
namespace pvgame {
constexpr size_t OuterState = 0x10; // 0 = loading, 1 = playing, 2 = ending
constexpr size_t InnerState = 0x14; // loading step 0..6
} // namespace pvgame

struct StageEntry {
    int32_t pvId, difficulty, edition;
};

struct StageState {
    bool ok;
    int32_t index;
    StageEntry entries[stage::EntryCount];
};

uintptr_t StageEntryAddress(int32_t index);
void ReadStageState(StageState& out);

enum class Difficulty : int32_t { Easy, Normal, Hard, Extreme, ExExtreme };
enum class FinalGrade : int32_t { Failed, Cheap, Standard, Great, Excellent, Perfect };

const char* ToString(Difficulty d);
const char* ToString(FinalGrade g);

struct GameState {
    // A field is only meaningful if its *Ok flag is true (the read didn't fault).
    bool scoreOk, liveOk, completionOk, pvOk, gradeOk, worstOk;

    DivaScore score;
    int32_t liveScore;
    int32_t liveCombo;
    float completion;
    int32_t pvId;
    int32_t diffVanilla;
    int32_t diffSongLimitPatch;
    int32_t grade;
    uint32_t worstCounter;

    uint8_t titleRaw[32];
    char titleGuess[64]; // empty if the bytes don't look like an MSVC std::string
};

// Copies `size` bytes from `src`, returning false instead of crashing on an access violation.
bool SafeCopy(void* dst, const void* src, size_t size);

template <typename T>
bool SafeRead(uintptr_t address, T& out)
{
    return SafeCopy(&out, (const void*)address, sizeof(T));
}

void ReadGameState(GameState& state);

} // namespace md
