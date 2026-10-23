#pragma once

namespace md::ui {

// The game reads the keyboard through DirectInput and GetAsyncKeyState, not window messages,
// so typing in an overlay text box would also drive the game. These hooks report "no keys down"
// to the game while keyboard blocking is on.
void InstallInputBlock();

// Called once per frame by the overlay (true while an ImGui text field has focus, or a full-screen
// multiplayer screen is open).
void SetKeyboardBlocked(bool blocked);

// The real key state, also while the game's keyboard is blocked (our own hotkeys: F8, Ctrl+F8, F9).
bool RealKeyDown(int vk);

// Esc for an overlay window (round results): while captured, an Esc press is hidden from the game (until
// the key is released) and reported to the overlay instead.
void WantEscape();     // call every frame the window that Esc closes is drawn
bool EscapePressed();  // true once per captured press
void EndFrameEscape(); // overlay, after drawing: capture stays on only if WantEscape() was called

} // namespace md::ui
