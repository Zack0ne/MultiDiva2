#include "mod.h"

namespace md {

HMODULE g_module = nullptr;

const std::wstring& ModDir()
{
    static std::wstring dir = [] {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(g_module, path, MAX_PATH);
        std::wstring p(path);
        size_t slash = p.find_last_of(L"\\/");
        return slash == std::wstring::npos ? std::wstring(L".") : p.substr(0, slash);
    }();
    return dir;
}

const std::wstring& DataDir()
{
    static std::wstring dir = [] {
        wchar_t base[MAX_PATH] = {};
        DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
        if (n == 0 || n >= MAX_PATH)
            return ModDir();
        std::wstring d = std::wstring(base) + L"\\MultiDiva2";
        if (!CreateDirectoryW(d.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            return ModDir();
        return d;
    }();
    return dir;
}

std::wstring DataFile(const wchar_t* name)
{
    std::wstring path = DataDir() + L"\\" + name;
    if (DataDir() != ModDir() && GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        CopyFileW((ModDir() + L"\\" + name).c_str(), path.c_str(), TRUE);
    return path;
}

std::string ModPathUtf8(const wchar_t* relative)
{
    std::wstring full = ModDir() + L"\\" + relative;
    int len = WideCharToMultiByte(CP_UTF8, 0, full.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len > 0 ? len - 1 : 0, '\0');
    if (len > 1)
        WideCharToMultiByte(CP_UTF8, 0, full.c_str(), -1, out.data(), len, nullptr, nullptr);
    return out;
}

} // namespace md
