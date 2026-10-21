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
