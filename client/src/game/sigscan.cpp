#include "sigscan.h"

#include <cstring>
#include <windows.h>

namespace md::sigscan {

struct Image {
    const uint8_t* begin = nullptr;
    size_t size = 0;
};

// The game's own module, bounds from its PE header.
static Image GameImage()
{
    static Image image = [] {
        Image out;
        auto base = (const uint8_t*)GetModuleHandleW(nullptr);
        auto dos = (const IMAGE_DOS_HEADER*)base;
        auto nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        out.begin = base;
        out.size = nt->OptionalHeader.SizeOfImage;
        return out;
    }();
    return image;
}

// "48 ?? 05" -> bytes {0x48, 0, 0x05}, known {1, 0, 1}.
static bool Parse(const char* pattern, std::vector<uint8_t>& bytes, std::vector<bool>& known)
{
    for (const char* p = pattern; *p;) {
        if (*p == ' ') {
            p++;
        } else if (p[0] == '?' && p[1] == '?') {
            bytes.push_back(0);
            known.push_back(false);
            p += 2;
        } else if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1])) {
            char hex[3] = { p[0], p[1], 0 };
            bytes.push_back((uint8_t)strtoul(hex, nullptr, 16));
            known.push_back(true);
            p += 2;
        } else {
            return false;
        }
    }
    return !bytes.empty() && known[0];
}

std::vector<uintptr_t> FindAll(const char* pattern)
{
    std::vector<uintptr_t> out;
    std::vector<uint8_t> bytes;
    std::vector<bool> known;
    if (!Parse(pattern, bytes, known))
        return out;

    const Image image = GameImage();
    const size_t n = bytes.size();
    if (image.size < n)
        return out;
    // memchr finds candidates for the (always known) first byte; the rest is compared from there.
    const uint8_t* end = image.begin + image.size - n + 1;
    for (const uint8_t* at = image.begin; at < end;) {
        at = (const uint8_t*)memchr(at, bytes[0], end - at);
        if (!at)
            break;
        size_t i = 1;
        while (i < n && (!known[i] || at[i] == bytes[i]))
            i++;
        if (i == n)
            out.push_back((uintptr_t)at);
        at++;
    }
    return out;
}

uintptr_t RipTarget(uintptr_t dispAt, uintptr_t instructionEnd)
{
    int32_t disp;
    memcpy(&disp, (const void*)dispAt, sizeof(disp));
    return instructionEnd + disp;
}

} // namespace md::sigscan
