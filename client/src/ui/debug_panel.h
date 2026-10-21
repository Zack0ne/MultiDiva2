#pragma once

namespace md::ui {

// F7 window: signature status, hook events and a live dump of every GameState field,
// used to verify the legacy v1.02 memory addresses against the in-game HUD on v1.03.
void DrawDebugPanel(bool* open);

} // namespace md::ui
