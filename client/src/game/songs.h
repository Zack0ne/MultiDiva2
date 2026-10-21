#pragma once
#include <cstdint>
#include <string>

// Which songs this player has, and which one song select has highlighted (see docs/re-notes.md "Songs").
namespace md::songs {

void Install(); // the song select highlight watch (hardware breakpoint, no code patch)
void Tick();    // once per frame

// True if the song is installed (base game, DLC or mod song). difficulty < 0: any chart; edition 2: any.
bool Has(int32_t pv, int32_t difficulty = -1, int32_t edition = 2);

// The song highlighted in song select, -1 when not on song select (or not known yet).
int32_t Hovered();

// The song's name from the game's song list (in the game's language); empty if it isn't installed.
// Game thread only.
std::string Title(int32_t pv);

// Research: logs every std::string inside the song's pv_db entry (to find the title fields).
void DumpEntryStrings(int32_t pv);

} // namespace md::songs
