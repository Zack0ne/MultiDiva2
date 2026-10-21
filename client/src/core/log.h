#pragma once

namespace md {

// Opens <ModDir>\multidiva.log (truncated each launch).
void LogInit();
void LogShutdown();

// printf-style. Thread-safe. Writes to the log file and stdout (the DML console when enabled).
void Log(const char* fmt, ...);

} // namespace md
