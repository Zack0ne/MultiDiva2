#include "log.h"
#include "mod.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <share.h>

namespace md {

static std::mutex s_mutex;
static FILE* s_file = nullptr;

void LogInit()
{
    std::lock_guard lock(s_mutex);
    if (s_file)
        return;
    // The previous session's log stays as multidiva.old.log: after a crash, that's the one that matters.
    std::wstring path = ModDir() + L"\\multidiva.log";
    MoveFileExW(path.c_str(), (ModDir() + L"\\multidiva.old.log").c_str(), MOVEFILE_REPLACE_EXISTING);
    // Deny writes only, so the log can be read while the game is running.
    s_file = _wfsopen(path.c_str(), L"w", _SH_DENYWR);
}

void LogShutdown()
{
    std::lock_guard lock(s_mutex);
    if (s_file) {
        fclose(s_file);
        s_file = nullptr;
    }
}

void Log(const char* fmt, ...)
{
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    SYSTEMTIME t;
    GetLocalTime(&t);

    std::lock_guard lock(s_mutex);
    printf("[MultiDiva] %s\n", msg);
    if (s_file) {
        fprintf(s_file, "%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, msg);
        fflush(s_file);
    }
}

void LogNoLock(const char* fmt, ...)
{
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    if (s_file) {
        fprintf(s_file, "%s\n", msg);
        fflush(s_file);
    }
}

} // namespace md
