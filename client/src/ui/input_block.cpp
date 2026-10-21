#include "input_block.h"
#include "../core/log.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <detours.h>

#include <atomic>
#include <cstring>

namespace md::ui {

static std::atomic<bool> s_blocked{false};

void SetKeyboardBlocked(bool blocked) { s_blocked = blocked; }

static std::atomic<bool> s_escCapture{false}; // a window wants Esc
static std::atomic<bool> s_escWanted{false};  // WantEscape() called this frame
static std::atomic<bool> s_escHeld{false};    // a captured press is still held down: keep hiding it
static std::atomic<bool> s_escPressed{false}; // for EscapePressed()

void WantEscape() { s_escWanted = true; }
bool EscapePressed() { return s_escPressed.exchange(false); }

// Fed with the real Esc state on every read; returns whether to hide it from the game.
static bool FilterEscape(bool down)
{
    if (!down) {
        s_escHeld = false;
        return false;
    }
    if (s_escHeld)
        return true;
    if (!s_escCapture)
        return false;
    s_escHeld = true;
    s_escPressed = true;
    return true;
}

// IDirectInputDevice8 vtable slots (IUnknown 0-2, GetCapabilities 3 ... Unacquire 8).
constexpr int GetDeviceStateSlot = 9;
constexpr int GetDeviceDataSlot = 10;
constexpr DWORD KeyboardStateSize = 256;

using GetDeviceStateFn = HRESULT(STDMETHODCALLTYPE*)(void*, DWORD, LPVOID);
using GetDeviceDataFn = HRESULT(STDMETHODCALLTYPE*)(void*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
using GetAsyncKeyStateFn = SHORT(WINAPI*)(int);

// The A and W interfaces may use different functions, so each gets its own trampoline.
struct DeviceHooks {
    void* stateTarget = nullptr; // original addresses, to detect a shared implementation
    GetDeviceStateFn getState = nullptr; // trampolines after hooking
    GetDeviceDataFn getData = nullptr;
};
static DeviceHooks s_a, s_w;
static GetAsyncKeyStateFn s_getAsyncKeyState = GetAsyncKeyState;

template <DeviceHooks& H>
static HRESULT STDMETHODCALLTYPE HookGetDeviceState(void* self, DWORD size, LPVOID data)
{
    HRESULT hr = H.getState(self, size, data);
    // Only the keyboard state is 256 bytes; mice and pads pass through untouched.
    if (SUCCEEDED(hr) && size == KeyboardStateSize) {
        uint8_t* keys = (uint8_t*)data;
        if (FilterEscape((keys[DIK_ESCAPE] & 0x80) != 0))
            keys[DIK_ESCAPE] = 0;
        if (s_blocked)
            memset(data, 0, size);
    }
    return hr;
}

template <DeviceHooks& H>
static HRESULT STDMETHODCALLTYPE HookGetDeviceData(void* self, DWORD objSize, LPDIDEVICEOBJECTDATA data, LPDWORD inOut,
    DWORD flags)
{
    // Always drain the buffer, so blocked key presses don't replay once typing ends.
    HRESULT hr = H.getData(self, objSize, data, inOut, flags);
    if (FAILED(hr) || !inOut || !data)
        return hr;
    if (s_blocked) {
        *inOut = 0;
        return hr;
    }
    // Drop the Esc events the overlay takes (this is called for every buffered device, so check the key).
    DWORD kept = 0;
    for (DWORD i = 0; i < *inOut; i++) {
        auto* ev = (LPDIDEVICEOBJECTDATA)((uint8_t*)data + i * objSize);
        if (ev->dwOfs == DIK_ESCAPE && FilterEscape((ev->dwData & 0x80) != 0))
            continue;
        if (kept != i)
            memcpy((uint8_t*)data + kept * objSize, ev, objSize);
        kept++;
    }
    *inOut = kept;
    return hr;
}

static SHORT WINAPI HookGetAsyncKeyState(int key)
{
    SHORT state = s_getAsyncKeyState(key);
    if (key == VK_ESCAPE && FilterEscape((state & 0x8000) != 0))
        return 0;
    return s_blocked && key >= 0x08 && key != VK_LBUTTON && key != VK_RBUTTON && key != VK_MBUTTON ? 0 : state;
}

bool RealKeyDown(int vk) { return (s_getAsyncKeyState(vk) & 0x8000) != 0; }

void EndFrameEscape()
{
    s_escCapture = s_escWanted.exchange(false);
    if (!s_escCapture)
        s_escPressed = false;
    // Also read the key ourselves, in case the game doesn't poll it this frame.
    FilterEscape((s_getAsyncKeyState(VK_ESCAPE) & 0x8000) != 0);
}

static bool Attach(void** target, void* detour)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    LONG a = DetourAttach(target, detour);
    return a == NO_ERROR && DetourTransactionCommit() == NO_ERROR;
}

template <DeviceHooks& H>
static bool HookKeyboardVtable(void** vtable, DeviceHooks* other)
{
    void* state = vtable[GetDeviceStateSlot];
    void* data = vtable[GetDeviceDataSlot];
    // Both interfaces sharing one implementation: hook it once.
    if (!state || !data || (other && other->stateTarget == state))
        return false;
    H.stateTarget = state;
    H.getState = (GetDeviceStateFn)state;
    H.getData = (GetDeviceDataFn)data;
    return Attach((void**)&H.getState, (void*)HookGetDeviceState<H>) && Attach((void**)&H.getData, (void*)HookGetDeviceData<H>);
}

void InstallInputBlock()
{
    // Our own keyboard devices give us the vtables the game's devices share.
    HINSTANCE inst = GetModuleHandleW(nullptr);
    IDirectInput8W* diW = nullptr;
    IDirectInput8A* diA = nullptr;
    IDirectInputDevice8W* devW = nullptr;
    IDirectInputDevice8A* devA = nullptr;
    int hooked = 0;

    if (SUCCEEDED(DirectInput8Create(inst, DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&diW, nullptr))
        && SUCCEEDED(diW->CreateDevice(GUID_SysKeyboard, &devW, nullptr)))
        hooked += HookKeyboardVtable<s_w>(*(void***)devW, nullptr);
    if (SUCCEEDED(DirectInput8Create(inst, DIRECTINPUT_VERSION, IID_IDirectInput8A, (void**)&diA, nullptr))
        && SUCCEEDED(diA->CreateDevice(GUID_SysKeyboard, &devA, nullptr)))
        hooked += HookKeyboardVtable<s_a>(*(void***)devA, &s_w);

    if (devW) devW->Release();
    if (devA) devA->Release();
    if (diW) diW->Release();
    if (diA) diA->Release();

    bool async = Attach((void**)&s_getAsyncKeyState, (void*)HookGetAsyncKeyState);
    Log("input block: %d DirectInput keyboard interface(s) hooked, GetAsyncKeyState %s", hooked, async ? "hooked" : "FAILED");
}

} // namespace md::ui
