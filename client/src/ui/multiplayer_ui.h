#pragma once

namespace md::ui {

// F8: the full-screen multiplayer screens (connect / room list, then the room), or the old ImGui window when
// "classic" is on (Ctrl+F8).
void DrawLobby(bool* open);
bool LobbyIsFullScreen(); // the last DrawLobby drew a full-screen screen (the game's keyboard is blocked meanwhile)
void SetClassicLobby(bool classic);
bool ClassicLobby();

// Live leaderboard during a round, results after it. Drawn every frame; they decide when to show.
void DrawRoundOverlays();

// Before NewFrame: passes every name, chat line and title on screen to glyphs::RequestText.
void RequestGlyphs();

} // namespace md::ui
