// MultiDiva client: a DivaModLoader DLL mod. DML calls the exports below; see
// https://github.com/blueskythlikesclouds/DivaModLoader for the loader contract.
#include <windows.h>
#include <d3d11.h>

#include "core/config.h"
#include "core/log.h"
#include "core/mod.h"
#include "game/hooks.h"
#include "game/pause_guard.h"
#include "game/songs.h"
#include "game/session.h"
#include "game/sigs.h"
#include "game/titles.h"
#include "mp/multiplayer.h"
#include "ui/overlay.h"

#define MULTIDIVA_VERSION "2.1.1"

extern "C" {

__declspec(dllexport) void Init()
{
    md::LogInit();
    md::Log("MultiDiva %s loading from %ls", MULTIDIVA_VERSION, md::ModDir().c_str());
    md::LoadConfig();
    md::titles::Load();
    md::sigs::Resolve();
    md::hooks::Install();
    md::pause::Install();
    md::songs::Install();
}

__declspec(dllexport) void D3DInit(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
    md::ui::OverlayInit(swapChain, device, deviceContext);
}

__declspec(dllexport) void OnFrame(IDXGISwapChain* swapChain)
{
    md::SessionTick();
    for (const md::GameEvent& e : md::g_session.frameEvents)
        md::mp::OnGameEvent(e);
    md::mp::Tick();
    md::ui::OverlayFrame(swapChain);
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        md::g_module = module;
        DisableThreadLibraryCalls(module);
        break;
    case DLL_PROCESS_DETACH:
        // On process exit (reserved != nullptr) the game's D3D device may already be gone,
        // so only tear down the overlay on an explicit FreeLibrary.
        if (!reserved)
            md::ui::OverlayShutdown();
        md::LogShutdown();
        break;
    }
    return TRUE;
}
