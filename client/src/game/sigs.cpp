#include "sigs.h"
#include "memory.h"
#include "sigscan.h"
#include "../core/log.h"

#include <cstring>

namespace md::sigs {

// How each function was found is in docs/re-notes.md; patterns come from tools/re/mksig.py.
struct Definition {
    const char* name;
    uintptr_t known103;
    const char* pattern;
    // Optional: turns a match into the address we want (nullptr: the match itself).
    uintptr_t (*resolve)(uintptr_t match);
};

// The match is GameResultTask's constructor storing its vtable: `lea rax, [vtable]; mov [task_ptr], rbx;
// lea rdx, ["GameResultTask"]; mov [rbx], rax`. Another task is built the same way, so the name tells them apart.
static uintptr_t ResolveResultVtable(uintptr_t match)
{
    char name[sizeof("GameResultTask")] = {};
    if (!SafeCopy(name, (const void*)sigscan::RipTarget(match + 17, match + 21), sizeof(name))
        || memcmp(name, "GameResultTask", sizeof(name)) != 0)
        return 0;
    return sigscan::RipTarget(match + 3, match + 7);
}

static const Definition s_defs[Count] = {
    // task_pv_game_init_pv (ReDIVA name): reads the current stage entry and opens the PVGAME task.
    // Found via the only xref to the "PVGAME" task name (task_pv_game_open, 0x1405DA0D0).
    { "PvGameInitPv", 0x1404414B0,
      "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55 41 54 41 55 41 56 41 57 48 8D AC 24 60 FE FF FF "
      "48 81 EC A0 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33" },
    // shift_next_mode(int mode) (ReDIVA name): sets mode state +0x04 = mode, +0x10 = 1.
    // Called by ModeGamePS4_PvselImpl (song select) when a song is confirmed.
    { "ShiftNextMode", 0x1402C4BB0,
      "48 83 EC 28 E8 ?? ?? ?? ?? 83 78 04 05 75 06 80 78 10 00 75 0B 89 48 04 C6 40 10 01" },
    // shift_next_mode_sub(int sub): sets mode state +0x20 = sub, +0x2C = 1.
    { "ShiftNextModeSub", 0x1402C4BE0,
      "48 83 EC 28 E8 ?? ?? ?? ?? 83 78 04 05 75 06 80 78 10 00 75 07 89 48 20 C6 40 2C 01" },
    // pv_game_parent::ctrl (ReDIVA name), the song's per-frame state machine (pv_game_parent_data.update_func):
    // `if (pv_game_ptr) goto 0x140268100` (the real body, whose steps are virtualized).
    { "PvGameParentCtrl", 0x140265E60,
      "48 83 3D ?? ?? ?? ?? 00 0F 85 92 22 00 00 C3" },
    // The result screen task's vtable, from its constructor (the only xref to the "GameResultTask" string).
    { "GameResultTaskVtable", 0x140C821F0,
      "48 8D 05 ?? ?? ?? ?? 48 89 1D ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 48 89 03 48 8B CB", ResolveResultVtable },
};

static Result s_results[Count];
static int s_resolved = 0;

int Resolve()
{
    s_resolved = 0;
    for (int i = 0; i < Count; i++) {
        const Definition& def = s_defs[i];
        Result& res = s_results[i];
        res = { def.name, nullptr, def.known103 };

        for (uintptr_t match : sigscan::FindAll(def.pattern)) {
            uintptr_t addr = def.resolve ? def.resolve(match) : match;
            if (addr) {
                res.address = (void*)addr;
                break;
            }
        }

        if (res.address) {
            s_resolved++;
            uintptr_t addr = (uintptr_t)res.address;
            Log("sig %-20s -> 0x%llX%s", def.name, (unsigned long long)addr,
                addr == def.known103 ? "" : "  (differs from known v1.03 address!)");
        } else {
            Log("sig %-20s -> NOT FOUND, hook disabled", def.name);
        }
    }
    Log("%d/%d signatures resolved", s_resolved, (int)Count);
    return s_resolved;
}

const Result& Get(Id id) { return s_results[id]; }
int ResolvedCount() { return s_resolved; }

} // namespace md::sigs
