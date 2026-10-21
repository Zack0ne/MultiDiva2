#pragma once
#include <imgui.h>
#include <string>

// Glyphs beyond Segoe UI's (Japanese, Chinese, Korean, symbols) are added to the font atlas on demand:
// text that may contain them is passed to RequestText(), and the overlay rebuilds its fonts when new
// characters turn up. Each character comes from the first Windows font that has it, so the atlas only ever
// holds the few hundred CJK characters actually seen instead of tens of thousands.
namespace md::ui::glyphs {

void RequestText(const std::string& utf8);
void RequestChar(unsigned int codepoint);

// True (once) when characters were requested since the last BuildRanges().
bool TakeDirty();

// Computes the ranges for the next font build from everything requested so far. The pointers stay valid
// until the next BuildRanges() call.
void BuildRanges();

const ImWchar* PrimaryRanges(); // for Segoe UI: Latin-1 plus requested characters it has

struct Fallback {
    const void* data = nullptr; // whole font file, owned here (not by the atlas)
    int size = 0;
    const ImWchar* ranges = nullptr;
};
int FallbackCount();
Fallback GetFallback(int i); // only fallbacks with at least one character to provide

} // namespace md::ui::glyphs
