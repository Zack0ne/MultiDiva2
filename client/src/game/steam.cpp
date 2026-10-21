#include "steam.h"
#include "../core/log.h"

#include <set>
#include <windows.h>

namespace md::steam {

// Flat API exports of the game's steam_api64.dll (interface versions it ships with).
using GetInterfaceFn = void*(__cdecl*)();
using GetSteamIdFn = uint64_t(__cdecl*)(void* user);
using GetAvatarFn = int(__cdecl*)(void* friends, uint64_t steamId);
using RequestUserInfoFn = bool(__cdecl*)(void* friends, uint64_t steamId, bool nameOnly);
using GetImageSizeFn = bool(__cdecl*)(void* utils, int image, uint32_t* w, uint32_t* h);
using GetImageRgbaFn = bool(__cdecl*)(void* utils, int image, uint8_t* dest, int size);
using GetRelationshipFn = int(__cdecl*)(void* friends, uint64_t steamId);
constexpr int RelationshipFriend = 3; // k_EFriendRelationshipFriend

struct Api {
    bool ok = false;
    void* user = nullptr;
    void* friends = nullptr;
    void* utils = nullptr;
    GetSteamIdFn getSteamId = nullptr;
    GetAvatarFn getMediumAvatar = nullptr;
    RequestUserInfoFn requestUserInfo = nullptr;
    GetImageSizeFn getImageSize = nullptr;
    GetImageRgbaFn getImageRgba = nullptr;
    GetRelationshipFn getRelationship = nullptr; // optional
};

static const Api& GetApi()
{
    static Api api = [] {
        Api a;
        HMODULE dll = GetModuleHandleW(L"steam_api64.dll");
        if (!dll)
            return a;
        auto get = [&](const char* name) { return GetProcAddress(dll, name); };
        auto user = (GetInterfaceFn)get("SteamAPI_SteamUser_v021");
        auto friends = (GetInterfaceFn)get("SteamAPI_SteamFriends_v017");
        auto utils = (GetInterfaceFn)get("SteamAPI_SteamUtils_v010");
        a.getSteamId = (GetSteamIdFn)get("SteamAPI_ISteamUser_GetSteamID");
        a.getMediumAvatar = (GetAvatarFn)get("SteamAPI_ISteamFriends_GetMediumFriendAvatar");
        a.requestUserInfo = (RequestUserInfoFn)get("SteamAPI_ISteamFriends_RequestUserInformation");
        a.getImageSize = (GetImageSizeFn)get("SteamAPI_ISteamUtils_GetImageSize");
        a.getImageRgba = (GetImageRgbaFn)get("SteamAPI_ISteamUtils_GetImageRGBA");
        a.getRelationship = (GetRelationshipFn)get("SteamAPI_ISteamFriends_GetFriendRelationship");
        if (!user || !friends || !utils || !a.getSteamId || !a.getMediumAvatar || !a.requestUserInfo || !a.getImageSize
            || !a.getImageRgba)
            return a;
        a.user = user();
        a.friends = friends();
        a.utils = utils();
        a.ok = a.user && a.friends && a.utils;
        Log("steam: %s", a.ok ? "avatars available" : "interfaces missing, no avatars");
        return a;
    }();
    return api;
}

uint64_t MySteamId()
{
    const Api& api = GetApi();
    return api.ok ? api.getSteamId(api.user) : 0;
}

bool AvatarRGBA(uint64_t steamId, std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height)
{
    const Api& api = GetApi();
    if (!api.ok || !steamId)
        return false;
    // For strangers the client answers with the default "?" avatar instead of "still loading", so only
    // ask it about ourselves and friends; everyone else goes to the Steam Community fallback.
    if (api.getRelationship && steamId != api.getSteamId(api.user)
        && api.getRelationship(api.friends, steamId) != RelationshipFriend)
        return false;

    // Non-friends' avatars are only downloaded after asking for their persona once.
    static std::set<uint64_t> requested;
    if (requested.insert(steamId).second)
        api.requestUserInfo(api.friends, steamId, false);

    int image = api.getMediumAvatar(api.friends, steamId); // 0 = none, -1 = still loading
    if (image <= 0 || !api.getImageSize(api.utils, image, &width, &height) || !width || !height || width > 512 || height > 512)
        return false;
    rgba.resize((size_t)width * height * 4);
    return api.getImageRgba(api.utils, image, rgba.data(), (int)rgba.size());
}

} // namespace md::steam
