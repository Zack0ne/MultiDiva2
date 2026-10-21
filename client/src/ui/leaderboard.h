#pragma once
#include "../mp/multiplayer.h"

namespace md::ui {

// Live leaderboard at the left edge during a round, styled after osu!'s: avatar + card per
// player, rows slide when ranks swap, scores roll, overtakes glow and combo breaks flash red.
void DrawLeaderboard(const mp::State& s);

// Forget animation state (call when a round starts).
void ResetLeaderboard();

// Card metrics scale with the screen height. The overlay loads the card fonts at these sizes.
float CardHeight(float screenHeight);
float CardNameFontPx(float screenHeight);
float CardSmallFontPx(float screenHeight);

} // namespace md::ui
