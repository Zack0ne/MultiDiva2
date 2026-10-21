#pragma once

namespace md::ui {

// "Pausing is off in multiplayer" card at the right edge after a blocked pause (see game/pause_guard.h).
void DrawPauseHint();

// Font sizes of its two lines, ~26 / 19 px at 1080p (baked into the atlas at load, so the text stays sharp).
float HintTitleFontPx(float screenHeight);
float HintLineFontPx(float screenHeight);

} // namespace md::ui
