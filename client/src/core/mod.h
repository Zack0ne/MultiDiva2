#pragma once
#include <windows.h>
#include <string>

namespace md {

// Handle of MultiDiva.dll itself, set in DllMain.
extern HMODULE g_module;

// Folder that contains MultiDiva.dll (e.g. <game>\mods\MultiDiva), no trailing slash.
const std::wstring& ModDir();

// %LOCALAPPDATA%\MultiDiva2, no trailing slash (created if missing): settings and downloads live here so they
// survive the mod folder being replaced by an update. Falls back to ModDir() if it can't be created.
const std::wstring& DataDir();

// DataDir() + L"\\" + name. The first time, a copy left in the mod folder by an older version is copied over.
std::wstring DataFile(const wchar_t* name);

// ModDir() + L"\\" + relative, as UTF-8 (for stb_image / ImGui font loading).
std::string ModPathUtf8(const wchar_t* relative);

} // namespace md
