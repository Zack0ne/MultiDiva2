#pragma once

// Crash catching for bug reports: a crash writes where it happened into the log, and the next launch knows the
// previous one didn't end normally (see ui/bug_report for the "send the report?" prompt).
namespace md::crash {

// Call early in Init, then again once the game runs (it may set its own unhandled exception filter meanwhile).
void Install();

// The previous session ended without a normal exit (a crash, or the game was killed).
bool PreviousSessionCrashed();
void ForgetPreviousCrash(); // the report was sent or declined

// The game is closing normally (its window is going away, or the process exits).
void MarkCleanExit();

} // namespace md::crash
