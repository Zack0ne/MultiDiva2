#include "glyphs.h"
#include "../core/log.h"

#include <cstdio>
#include <mutex>
#include <set>
#include <vector>
#include <windows.h>

// Only used to ask fonts which characters they have; ImGui keeps its own (also static) copy for rendering.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <imstb_truetype.h>

namespace md::ui::glyphs {

namespace {

struct FontFile {
    const char* name;
    bool tried = false;
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    std::vector<ImWchar> ranges; // built by BuildRanges
};

// Segoe UI first (the overlay's font), then fallbacks: Japanese, Chinese, Korean, symbols.
FontFile g_fonts[] = {
    { "segoeui.ttf" }, { "YuGothM.ttc" }, { "meiryo.ttc" }, { "msgothic.ttc" }, { "msyh.ttc" },
    { "simsun.ttc" }, { "malgun.ttf" }, { "seguisym.ttf" },
};
constexpr int FontCount = sizeof(g_fonts) / sizeof(g_fonts[0]);

std::mutex g_mutex;
std::set<ImWchar> g_requested;
bool g_dirty = false;

bool Load(FontFile& f)
{
    if (f.tried)
        return !f.data.empty();
    f.tried = true;
    char path[MAX_PATH];
    GetWindowsDirectoryA(path, MAX_PATH);
    strcat_s(path, "\\Fonts\\");
    strcat_s(path, f.name);
    FILE* file = fopen(path, "rb");
    if (!file)
        return false;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    f.data.resize(size > 0 ? size : 0);
    bool ok = size > 0 && fread(f.data.data(), 1, size, file) == (size_t)size;
    fclose(file);
    int offset = ok ? stbtt_GetFontOffsetForIndex(f.data.data(), 0) : -1;
    if (offset < 0 || !stbtt_InitFont(&f.info, f.data.data(), offset)) {
        f.data.clear();
        return false;
    }
    return true;
}

bool Has(FontFile& f, ImWchar c) { return Load(f) && stbtt_FindGlyphIndex(&f.info, c) != 0; }

void AddRange(std::vector<ImWchar>& ranges, ImWchar c)
{
    if (ranges.size() >= 2 && ranges.back() + 1 == c)
        ranges.back() = c;
    else
        ranges.insert(ranges.end(), { c, c });
}

} // namespace

void RequestChar(unsigned int c)
{
    // Latin-1 is always in the atlas; ImWchar is 16-bit, so nothing beyond the BMP (emoji) fits.
    if (c <= 0xFF || c > 0xFFFF || (c >= 0xD800 && c <= 0xDFFF))
        return;
    std::lock_guard lock(g_mutex);
    if (g_requested.insert((ImWchar)c).second)
        g_dirty = true;
}

void RequestText(const std::string& s)
{
    for (size_t i = 0; i < s.size();) {
        unsigned char b = (unsigned char)s[i];
        if (b < 0x80) {
            i++;
            continue;
        }
        int len = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;
        unsigned int c = len == 4 ? b & 0x07 : len == 3 ? b & 0x0F : b & 0x1F;
        for (int k = 1; k < len && i + k < s.size(); k++)
            c = (c << 6) | ((unsigned char)s[i + k] & 0x3F);
        if (len > 1)
            RequestChar(c);
        i += len;
    }
}

bool TakeDirty()
{
    std::lock_guard lock(g_mutex);
    bool d = g_dirty;
    g_dirty = false;
    return d;
}

void BuildRanges()
{
    std::set<ImWchar> requested;
    {
        std::lock_guard lock(g_mutex);
        requested = g_requested;
    }
    for (FontFile& f : g_fonts)
        f.ranges.clear();
    g_fonts[0].ranges = { 0x0020, 0x00FF };

    int missing = 0;
    for (ImWchar c : requested) {
        int i = 0;
        while (i < FontCount && !Has(g_fonts[i], c))
            i++;
        if (i < FontCount)
            AddRange(g_fonts[i].ranges, c);
        else
            missing++;
    }
    for (FontFile& f : g_fonts)
        if (!f.ranges.empty())
            f.ranges.push_back(0);
    Log("fonts: %zu extra character(s) requested, %d not in any font", requested.size(), missing);
}

const ImWchar* PrimaryRanges() { return g_fonts[0].ranges.empty() ? nullptr : g_fonts[0].ranges.data(); }

int FallbackCount() { return FontCount - 1; }

Fallback GetFallback(int i)
{
    FontFile& f = g_fonts[i + 1];
    if (f.ranges.empty() || f.data.empty())
        return {};
    return { f.data.data(), (int)f.data.size(), f.ranges.data() };
}

} // namespace md::ui::glyphs
