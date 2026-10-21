#include "sigscan.h"
#include <cstring>

const MODULEINFO& getModuleInfo()
{
    static MODULEINFO moduleInfo = [] {
        MODULEINFO info = {};
        GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &info, sizeof(MODULEINFO));
        return info;
    }();
    return moduleInfo;
}

static bool matchAt(const char* memory, const char* signature, const char* mask, size_t length)
{
    for (size_t j = 0; j < length; j++) {
        if (mask[j] != '?' && signature[j] != memory[j])
            return false;
    }
    return true;
}

void* sigScan(const char* signature, const char* mask)
{
    const MODULEINFO& info = getModuleInfo();
    const size_t length = strlen(mask);
    if (length == 0 || info.SizeOfImage < length)
        return nullptr;

    const char* base = (const char*)info.lpBaseOfDll;
    for (size_t i = 0; i <= info.SizeOfImage - length; i++) {
        if (matchAt(base + i, signature, mask, length))
            return (void*)(base + i);
    }
    return nullptr;
}

size_t sigCount(const char* signature, const char* mask)
{
    const MODULEINFO& info = getModuleInfo();
    const size_t length = strlen(mask);
    if (length == 0 || info.SizeOfImage < length)
        return 0;

    size_t count = 0;
    const char* base = (const char*)info.lpBaseOfDll;
    for (size_t i = 0; i <= info.SizeOfImage - length; i++) {
        if (matchAt(base + i, signature, mask, length))
            count++;
    }
    return count;
}
