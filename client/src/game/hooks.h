#pragma once

namespace md::hooks {

// Attaches a Detours hook for every resolved signature. Unresolved ones are skipped.
// Returns the number of hooks installed.
int Install();

} // namespace md::hooks
