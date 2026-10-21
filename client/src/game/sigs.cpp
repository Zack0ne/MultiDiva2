#include "sigs.h"
#include "sigscan.h"
#include "../core/log.h"

namespace md::sigs {

struct Pattern {
    const char* bytes;
    const char* mask;
};

// Each signature can list several patterns (tried in order) so a future game update
// only needs a new pattern appended, not a code change.
struct Definition {
    const char* name;
    uintptr_t known103;
    Pattern patterns[2];
};

static const Definition s_defs[Count] = {
    // 1.02: 0x14043B2D0, 1.03: 0x14043B310 (Braasileiro)
    { "SongStart", 0x14043B310, {
        { "\x8B\xD1\xE9\xA9\xE8\xFF\xFF\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xE9", "xxxxxxx?????????x" },
    } },
    // 1.02: 0x1401E7A60, 1.03: 0x1401E7A70 (Braasileiro)
    { "SongPracticeStart", 0x1401E7A70, {
        { "\xE9\x00\x00\x00\x00\x58\x3C\xB4", "x????xxx" },
    } },
    // 1.02: 0x14043B000 (Braasileiro)
    { "SongEnd", 0x14043B040, {
        { "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8D\x0D\xCC\xCC\xCC\xCC\xE8\xCC\xCC\xCC\xCC\x48\x8B\x3D\xCC\xCC\xCC\xCC\x48\x8B\x1F\x48\x3B\xDF",
          "xxxxxxxxxxxxx????x????xxx????xxxxxx" },
    } },
    // Result screen / score trigger (RocketRacer)
    { "PrintResult", 0x140649E10, {
        { "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8B\xEC\x48\x83\xEC\x60\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x45\xF8\x48\x8B\xF9\x80\xB9\x00\x00\x00\x00\x00\x0F\x85\x00\x00\x00\x00",
          "xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx?????xx????" },
    } },
    // task_pv_game_init_pv (ReDIVA name): reads the current stage entry and opens the PVGAME task.
    // Found via the only xref to the "PVGAME" task name (task_pv_game_open, 0x1405DA0D0).
    { "PvGameInitPv", 0x1404414B0, {
        { "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x48\x89\x7C\x24\x18\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x60\xFE\xFF\xFF\x48\x81\xEC\xA0\x02\x00\x00\x48\x8B\x05\x8A\xAE\x95\x00\x48\x33",
          "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xx" },
    } },
    // shift_next_mode(int mode) (ReDIVA name): sets mode state +0x04 = mode, +0x10 = 1.
    // Called by ModeGamePS4_PvselImpl (song select) when a song is confirmed.
    { "ShiftNextMode", 0x1402C4BB0, {
        { "\x48\x83\xEC\x28\xE8\x57\xFC\xFF\xFF\x83\x78\x04\x05\x75\x06\x80\x78\x10\x00\x75\x0B\x89\x48\x04\xC6\x40\x10\x01",
          "xxxxx????xxxxxxxxxxxxxxxxxxx" },
    } },
    // shift_next_mode_sub(int sub): sets mode state +0x20 = sub, +0x2C = 1.
    { "ShiftNextModeSub", 0x1402C4BE0, {
        { "\x48\x83\xEC\x28\xE8\x27\xFC\xFF\xFF\x83\x78\x04\x05\x75\x06\x80\x78\x10\x00\x75\x07\x89\x48\x20\xC6\x40\x2C\x01",
          "xxxxx????xxxxxxxxxxxxxxxxxxx" },
    } },
    // pv_game_parent::ctrl (ReDIVA name), the song's per-frame state machine (pv_game_parent_data.update_func):
    // `if (pv_game_ptr) goto 0x140268100` (the real body, whose steps are virtualized).
    { "PvGameParentCtrl", 0x140265E60, {
        { "\x48\x83\x3D\x38\xA7\x9A\x0C\x00\x0F\x85\x92\x22\x00\x00\xC3", "xxx????xxxxxxxx" },
    } },
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

        for (const Pattern& p : def.patterns) {
            if (!p.bytes)
                break;
            if ((res.address = sigScan(p.bytes, p.mask)))
                break;
        }

        if (res.address) {
            s_resolved++;
            uintptr_t addr = (uintptr_t)res.address;
            Log("sig %-18s -> 0x%llX%s", def.name, (unsigned long long)addr,
                addr == def.known103 ? "" : "  (differs from known v1.03 address!)");
        } else {
            Log("sig %-18s -> NOT FOUND, hook disabled", def.name);
        }
    }
    Log("%d/%d signatures resolved", s_resolved, (int)Count);
    return s_resolved;
}

const Result& Get(Id id) { return s_results[id]; }
int ResolvedCount() { return s_resolved; }

} // namespace md::sigs
