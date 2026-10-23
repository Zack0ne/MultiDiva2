#pragma once

namespace md::ui {

// The report window: asks once after a crash whether to send the report, or opens from F8's "Report a bug".
// Draws nothing when reports are off (no report address built in). Call every frame.
void DrawBugReport();
void OpenBugReport();

} // namespace md::ui
