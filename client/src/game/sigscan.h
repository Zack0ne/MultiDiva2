#pragma once
#include <cstdint>
#include <vector>

// Byte patterns in the game executable, written as text: "48 8B 05 ?? ?? ?? ??" (?? = any byte).
// tools/re/mksig.py makes them from the exe.
namespace md::sigscan {

// Every match in the executable's image, in address order. Empty if the pattern is malformed.
std::vector<uintptr_t> FindAll(const char* pattern);

// The target of the rip-relative operand whose 32-bit displacement starts at `dispAt`, for an instruction that
// ends at `instructionEnd` (e.g. a lea).
uintptr_t RipTarget(uintptr_t dispAt, uintptr_t instructionEnd);

} // namespace md::sigscan
