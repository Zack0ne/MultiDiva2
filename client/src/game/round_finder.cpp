#include "round_finder.h"
#include "memory.h"
#include "../core/log.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include <windows.h>

namespace md::finder {

static constexpr size_t BlockFrom = 0x13C; // stage entry: the result block starts at +0x140 (grade)
static constexpr size_t BlockTo = 0x260;
static constexpr size_t MaxValues = 40;
static constexpr size_t MaxHitsPerValue = 8;

struct Range {
    uintptr_t begin, end;
};

// The executable's writable, non-code sections (its static data).
static std::vector<Range> StaticData()
{
    std::vector<Range> out;
    auto base = (const uint8_t*)GetModuleHandleW(nullptr);
    auto nt = (const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        DWORD c = sec->Characteristics;
        if ((c & IMAGE_SCN_MEM_WRITE) && !(c & IMAGE_SCN_MEM_EXECUTE))
            out.push_back({ (uintptr_t)base + sec->VirtualAddress, (uintptr_t)base + sec->VirtualAddress + sec->Misc.VirtualSize });
    }
    return out;
}

// One aligned pass over [begin, end): every dword equal to one of `values` (SEH-guarded, no C++ objects inside).
static size_t ScanDwords(uintptr_t begin, uintptr_t end, const int32_t* values, size_t count, const uint8_t* filter,
    uintptr_t* hits, int* which, size_t room)
{
    size_t found = 0;
    __try {
        for (const int32_t* p = (const int32_t*)begin; (uintptr_t)(p + 1) <= end && found < room; p++) {
            uint32_t v = (uint32_t)*p;
            if (!(filter[(v & 0xFFFF) >> 3] & (1 << (v & 7))))
                continue;
            for (size_t k = 0; k < count; k++)
                if (*p == values[k]) {
                    hits[found] = (uintptr_t)p;
                    which[found++] = (int)k;
                    break;
                }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return found;
}

static void DumpDwords(const char* what, uintptr_t at, size_t bytes)
{
    for (size_t row = 0; row < bytes; row += 32) {
        int32_t v[8] = {};
        if (!SafeCopy(v, (const void*)(at + row), sizeof(v)))
            return;
        Log("finder: %s %llX: %d %d %d %d %d %d %d %d", what, (unsigned long long)(at + row),
            v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
    }
}

void OnResultShown()
{
    int32_t index = 0;
    SafeRead(addr::StageData + stage::IndexOffset, index);
    const uintptr_t entry = StageEntryAddress(index);
    Log("finder: stage %d entry %llX, result block:", index, (unsigned long long)entry);
    for (size_t off = BlockFrom; off < BlockTo; off += 4) {
        int32_t i = 0;
        float f = 0;
        if (SafeRead(entry + off, i) && SafeRead(entry + off, f) && i != 0)
            Log("finder:   +0x%03zX = %d (float %g)", off, i, f);
    }

    // Distinctive values only (small ones are everywhere).
    std::vector<int32_t> values;
    for (size_t off = BlockFrom; off < BlockTo && values.size() < MaxValues; off += 4) {
        int32_t v = 0;
        if (SafeRead(entry + off, v) && v >= 100 && v < 100000000
            && std::find(values.begin(), values.end(), v) == values.end())
            values.push_back(v);
    }
    uint8_t filter[0x2000] = {};
    for (int32_t v : values)
        filter[((uint32_t)v & 0xFFFF) >> 3] |= 1 << ((uint32_t)v & 7);

    static uintptr_t hits[4096];
    static int which[4096];
    size_t total = 0;
    for (const Range& r : StaticData())
        total += ScanDwords(r.begin, r.end, values.data(), values.size(), filter, hits + total, which + total,
            4096 - total);

    // Per value, where else it lives (outside the stage data itself).
    const uintptr_t stageBegin = addr::StageData, stageEnd = addr::StageData + 0x30 + stage::EntryCount * stage::EntryStride;
    std::vector<uintptr_t> windows;
    for (size_t k = 0; k < values.size(); k++) {
        size_t shown = 0;
        for (size_t h = 0; h < total && shown < MaxHitsPerValue; h++) {
            if (which[h] != (int)k || (hits[h] >= stageBegin && hits[h] < stageEnd))
                continue;
            Log("finder: value %d also at %llX", values[k], (unsigned long long)hits[h]);
            shown++;
            uintptr_t w = hits[h] & ~(uintptr_t)0xFF;
            if (std::find(windows.begin(), windows.end(), w) == windows.end())
                windows.push_back(w);
        }
    }
    // The memory around the hits, one 0x100 window each (with its neighbours).
    for (uintptr_t w : windows)
        DumpDwords("around", w - 0x100, 0x300);
    Log("finder: done (%zu values, %zu hits, %zu windows)", values.size(), total, windows.size());
}

} // namespace md::finder
