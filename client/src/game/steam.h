#pragma once
#include <cstdint>
#include <vector>

// Steam avatars through the game's own steam_api64.dll (flat API, already initialised by the game).
namespace md::steam {

// 0 if Steam isn't available.
uint64_t MySteamId();

// Medium (64x64) avatar of ourselves or a Steam friend as RGBA (false for anyone else). Returns false while Steam is still fetching it
// (call again later) or if the user has none. Steam caches avatars, so polling is cheap.
bool AvatarRGBA(uint64_t steamId, std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height);

} // namespace md::steam
