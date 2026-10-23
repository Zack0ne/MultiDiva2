#include "crash.h"
#include "log.h"
#include "mod.h"

#include <windows.h>

namespace md::crash {

static LPTOP_LEVEL_EXCEPTION_FILTER s_previous = nullptr;
static bool s_installed = false;
static bool s_previousCrashed = false;

// Exists while the game runs; still there at the next launch means that session never closed normally.
static std::wstring MarkerPath() { return DataDir() + L"\\running.marker"; }

// "MultiDiva.dll+0x1A2B" for an address, or the raw address outside any module.
static void Describe(void* address, char* out, size_t size)
{
    HMODULE module = nullptr;
    wchar_t path[MAX_PATH] = {};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)address, &module) && GetModuleFileNameW(module, path, MAX_PATH)) {
        const wchar_t* name = wcsrchr(path, L'\\');
        name = name ? name + 1 : path;
        _snprintf_s(out, size, _TRUNCATE, "%ls+0x%llX", name, (unsigned long long)((char*)address - (char*)module));
    } else {
        _snprintf_s(out, size, _TRUNCATE, "0x%llX", (unsigned long long)address);
    }
}

// Only reached by exceptions nothing handled: the game is about to close. Logs without taking the log's lock
// (the crash may have happened while holding it), then lets the previous filter or Windows take over.
static LONG WINAPI OnUnhandled(EXCEPTION_POINTERS* info)
{
    const EXCEPTION_RECORD* rec = info->ExceptionRecord;
    char where[MAX_PATH + 32];
    Describe(rec->ExceptionAddress, where, sizeof(where));
    LogNoLock("CRASH: exception 0x%08lX at %s", rec->ExceptionCode, where);
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
        LogNoLock("CRASH: %s address 0x%llX", rec->ExceptionInformation[0] == 1 ? "writing" : "reading",
            (unsigned long long)rec->ExceptionInformation[1]);

    // Walk the crashing thread's stack from the exception's context.
    CONTEXT ctx = *info->ContextRecord;
    for (int i = 0; i < 24 && ctx.Rip; i++) {
        Describe((void*)ctx.Rip, where, sizeof(where));
        LogNoLock("CRASH:   #%02d %s", i, where);
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
        if (!fn) { // leaf function: the return address is on top of the stack
            if (IsBadReadPtr((void*)ctx.Rsp, sizeof(DWORD64)))
                break;
            ctx.Rip = *(DWORD64*)ctx.Rsp;
            ctx.Rsp += sizeof(DWORD64);
            continue;
        }
        void* handlerData = nullptr;
        DWORD64 frame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fn, &ctx, &handlerData, &frame, nullptr);
    }
    return s_previous ? s_previous(info) : EXCEPTION_CONTINUE_SEARCH;
}

void Install()
{
    if (!s_installed) {
        s_installed = true;
        std::wstring marker = MarkerPath();
        s_previousCrashed = GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES;
        HANDLE f = CreateFileW(marker.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE)
            CloseHandle(f);
        if (s_previousCrashed)
            Log("crash: the previous session didn't close normally (its log is multidiva.old.log)");
    }
    LPTOP_LEVEL_EXCEPTION_FILTER prev = SetUnhandledExceptionFilter(OnUnhandled);
    if (prev != OnUnhandled)
        s_previous = prev;
}

bool PreviousSessionCrashed() { return s_previousCrashed; }
void ForgetPreviousCrash() { s_previousCrashed = false; }

void MarkCleanExit()
{
    DeleteFileW(MarkerPath().c_str());
}

} // namespace md::crash
