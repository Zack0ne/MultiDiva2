#pragma once
#include <cstdint>

namespace md::sigs {

enum Id { SongStart, SongPracticeStart, SongEnd, PrintResult, PvGameInitPv, ShiftNextMode, ShiftNextModeSub, PvGameParentCtrl, Count };

struct Result {
    const char* name;
    void* address;     // nullptr if not found
    uintptr_t known103; // address found by static analysis of DivaMegaMix.exe v1.03
};

// Scans every signature once. Returns how many resolved. Call from Init (not DllMain).
int Resolve();

const Result& Get(Id id);
int ResolvedCount();

} // namespace md::sigs
