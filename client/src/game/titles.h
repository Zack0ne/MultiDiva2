#pragma once
#include <cstdint>
#include <string>

// PV id -> song title, learned whenever a song plays locally (the game only exposes the title of
// the song being played) and kept in %LOCALAPPDATA%\MultiDiva2\titles.txt across sessions.
namespace md::titles {

void Load();

// Empty if this PV hasn't been played on this PC yet.
std::string Find(int32_t pvId);

// "Roshin Yukai" if known, else "PV 54".
std::string Describe(int32_t pvId);

// Called every frame by the session tick: records the playing song's title once it is reliable.
void Tick();

} // namespace md::titles
