#pragma once

namespace md {

// Opens <ModDir>\multidiva.log, new each launch; the previous one is kept as multidiva.old.log.
void LogInit();
void LogShutdown();

// printf-style. Thread-safe. Writes to the log file and stdout (the DML console when enabled).
void Log(const char* fmt, ...);

// For the crash handler only: no lock (the crash may have happened while holding it), no time stamp.
void LogNoLock(const char* fmt, ...);

} // namespace md
