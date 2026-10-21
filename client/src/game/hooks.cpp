#include "hooks.h"
#include "events.h"
#include "launch.h"
#include "memory.h"
#include "sigs.h"
#include "sync_start.h"
#include "../core/helpers.h"
#include "../core/log.h"

#include <detours.h>
#include <intrin.h>

namespace md {

BoundedQueue<GameEvent, 256> g_gameEvents;

const char* ToString(GameEventType type)
{
    switch (type) {
    case GameEventType::SongStarted: return "SongStarted";
    case GameEventType::PracticeStarted: return "PracticeStarted";
    case GameEventType::SongEnded: return "SongEnded";
    case GameEventType::ResultShown: return "ResultShown";
    case GameEventType::PvGameInit: return "PvGameInit";
    case GameEventType::ModeShift: return "ModeShift";
    case GameEventType::PvGameState: return "PvGameState";
    }
    return "?";
}

// Runs on the game thread: must stay cheap and never block.
static void Emit(GameEventType type, int32_t pvId = -1)
{
    g_gameEvents.TryPush({ type, pvId, GetTickCount() });
}

// Locations are filled in at Install() from the resolved signatures.
HOOK(void, __fastcall, SongStart, nullptr, int pvId)
{
    Emit(GameEventType::SongStarted, pvId);
    originalSongStart(pvId);
}

// task_pv_game_init_pv: about to read the current stage entry and open the PVGAME task.
HOOK(void, __fastcall, PvGameInitPv, nullptr)
{
    GameEvent e{ GameEventType::PvGameInit, -1, GetTickCount() };
    SafeRead(addr::StageData + stage::IndexOffset, e.stageIndex);
    uintptr_t entry = StageEntryAddress(e.stageIndex);
    SafeRead(entry + stage::PvId, e.originalPvId);

    e.overridden = launch::OnPvGameInit(e.stageIndex);

    SafeRead(entry + stage::PvId, e.pvId);
    SafeRead(entry + stage::Difficulty, e.difficulty);
    SafeRead(entry + stage::Edition, e.edition);
    SafeRead(entry + stage::Modules, e.modules);
    g_gameEvents.TryPush(e);

    originalPvGameInitPv();
}

static void EmitModeShift(int32_t mode, bool sub, void* caller)
{
    GameEvent e{ GameEventType::ModeShift, mode, GetTickCount() };
    e.edition = sub ? 1 : 0;
    e.caller = (uintptr_t)caller;
    g_gameEvents.TryPush(e);
}

HOOK(void, __fastcall, ShiftNextMode, nullptr, int32_t mode)
{
    EmitModeShift(mode, false, _ReturnAddress());
    originalShiftNextMode(mode);
}

HOOK(void, __fastcall, ShiftNextModeSub, nullptr, int32_t sub)
{
    EmitModeShift(sub, true, _ReturnAddress());
    originalShiftNextModeSub(sub);
}

// Song state machine, once per frame. Skipping it holds the load at its current step.
HOOK(void, __fastcall, PvGameParentCtrl, nullptr, void* parent)
{
    if (sync::OnParentCtrl())
        return;
    originalPvGameParentCtrl(parent);
}

HOOK(__int64, __fastcall, SongPracticeStart, nullptr, __int64 a1, __int64 a2)
{
    Emit(GameEventType::PracticeStarted);
    return originalSongPracticeStart(a1, a2);
}

HOOK(__int64, __stdcall, SongEnd, nullptr)
{
    Emit(GameEventType::SongEnded);
    return originalSongEnd();
}

HOOK(int, __fastcall, PrintResult, nullptr, int a1)
{
    Emit(GameEventType::ResultShown);
    return originalPrintResult(a1);
}

namespace hooks {

static bool Attach(sigs::Id id, void** original, void* detour)
{
    const sigs::Result& sig = sigs::Get(id);
    if (!sig.address)
        return false;

    *original = sig.address;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    LONG attach = DetourAttach(original, detour);
    LONG commit = DetourTransactionCommit();
    if (attach != NO_ERROR || commit != NO_ERROR) {
        Log("hook %s FAILED (attach=%ld commit=%ld)", sig.name, attach, commit);
        *original = nullptr;
        return false;
    }
    return true;
}

int Install()
{
    int installed = 0;
    installed += Attach(sigs::SongStart, (void**)&originalSongStart, (void*)implOfSongStart);
    installed += Attach(sigs::SongPracticeStart, (void**)&originalSongPracticeStart, (void*)implOfSongPracticeStart);
    installed += Attach(sigs::SongEnd, (void**)&originalSongEnd, (void*)implOfSongEnd);
    installed += Attach(sigs::PrintResult, (void**)&originalPrintResult, (void*)implOfPrintResult);
    installed += Attach(sigs::PvGameInitPv, (void**)&originalPvGameInitPv, (void*)implOfPvGameInitPv);
    installed += Attach(sigs::ShiftNextMode, (void**)&originalShiftNextMode, (void*)implOfShiftNextMode);
    installed += Attach(sigs::ShiftNextModeSub, (void**)&originalShiftNextModeSub, (void*)implOfShiftNextModeSub);
    installed += Attach(sigs::PvGameParentCtrl, (void**)&originalPvGameParentCtrl, (void*)implOfPvGameParentCtrl);
    Log("%d/%d hooks installed", installed, (int)sigs::Count);
    return installed;
}

} // namespace hooks
} // namespace md
