#pragma once
#include <cstdint>

// Debug tool: a hardware write watchpoint (debug register DR0) on one game address, on every thread.
// Each write is logged with the writing instruction and the game code return addresses on the stack,
// to find which game function writes a field. Off unless started from the F7 panel.
namespace md::watch {

bool Start(uintptr_t address, int maxHits = 24); // 4-byte write watch; false if it could not be set
void Stop();
bool Active();
int Hits();

} // namespace md::watch
