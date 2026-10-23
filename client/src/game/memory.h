#pragma once
#include <cstddef>
#include <cstdint>
#include "../core/helpers.h"

namespace md {

// Absolute addresses for game v1.03, found with our own static analysis (docs/re-notes.md, tools/re).
// The executable has no ASLR (ImageBase 0x140000000), so absolute addresses are stable per version.
namespace addr {
// Stage data (ReDIVA: struc_717, returned by the getter at 0x14040B2A0).
constexpr uintptr_t StageData            = 0x1416E2B90;
// Mode state (returned by the getter at 0x1402C4810), written by shift_next_mode / shift_next_mode_sub.
constexpr uintptr_t ModeState            = 0x14CC61078;
// pv_game* (ReDIVA: pv_game_ptr), null outside a song; read by pv_game_parent::ctrl (0x140265E60).
constexpr uintptr_t PvGamePtr            = 0x14CC105A0;
// The song's live play data (ReDIVA: pv_game_data from life_gauge on). Found with game/round_finder: the only static
// places holding the round's final score in every round, laid out like ReDIVA's fields (docs/re-notes.md).
constexpr uintptr_t PlayData             = 0x1412EF564;
} // namespace addr

// Fields of addr::PlayData (offsets from life_gauge).
namespace play {
constexpr size_t Score      = 0x04;  // int32, ReDIVA score_final, updated as notes are hit
constexpr size_t Combo      = 0x14;  // int32, current combo
constexpr size_t Completion = 0xD0;  // float, percent shown on the HUD
} // namespace play

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
// After the launch block, the round's result (ReDIVA struc_662, written before the result screen opens).
constexpr size_t Grade         = 0x140; // int32, see md::FinalGrade
constexpr size_t ResultCounts  = 0x170; // int32[5] cool, fine, safe, sad, worst as the result screen shows them
constexpr size_t MaxCombo      = 0x198; // int32
constexpr size_t Score         = 0x1A4; // int32, ReDIVA score_final
constexpr size_t Percentage    = 0x1A8; // int32, completion x100
} // namespace stage

// Mode state fields (int32 index into the struct at addr::ModeState), mapped from in-game logs:
// [0] current mode, [1] next, [2] previous, [5] frames, [7] current sub, [8] next sub, [9] previous sub.
namespace mode {
constexpr int CurrentMode = 0;
constexpr int CurrentSub = 7;
constexpr int NextSub = 8;
constexpr int32_t RhythmGame = 2;
constexpr int32_t SubSongSelect = 5;
constexpr int32_t SubGameMain = 7;
} // namespace mode

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

// The local game's score data. A field is only meaningful if its *Ok flag is true (the read didn't fault).
struct GameState {
    bool liveOk;      // while a song plays
    int32_t liveScore;
    int32_t liveCombo;
    float completion; // percent

    bool resultOk;    // once the result screen opened
    int32_t score;
    int32_t maxCombo;
    int32_t grade;    // FinalGrade
    float resultCompletion;
    uint32_t cool, fine, safe, sad, worst;
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
