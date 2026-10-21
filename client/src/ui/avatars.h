#pragma once
#include <cstdint>
#include <d3d11.h>

#include "../mp/multiplayer.h"

namespace md::ui {

// Steam avatar texture for a player, fetched in the background and cached. Null until it is
// available (or forever for players without Steam); callers draw a fallback meanwhile.
ID3D11ShaderResourceView* Avatar(uint64_t steamId);

// A player's picture: their Steam avatar, else the custom one the server holds for them (bots).
ID3D11ShaderResourceView* PlayerAvatar(const mp::Player& p);

} // namespace md::ui
