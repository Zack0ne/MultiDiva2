#pragma once
#include <windows.h>
#include <string>

namespace md {

// Handle of MultiDiva.dll itself, set in DllMain.
extern HMODULE g_module;

// Folder that contains MultiDiva.dll (e.g. <game>\mods\MultiDiva), no trailing slash.
const std::wstring& ModDir();

// ModDir() + L"\\" + relative, as UTF-8 (for stb_image / ImGui font loading).
std::string ModPathUtf8(const wchar_t* relative);

} // namespace md
