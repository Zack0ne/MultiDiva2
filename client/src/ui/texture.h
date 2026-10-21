#pragma once
#include <d3d11.h>

namespace md::ui {

struct Texture {
    ID3D11ShaderResourceView* srv = nullptr;
    int width = 0;
    int height = 0;
};

bool CreateTextureRGBA(ID3D11Device* device, const unsigned char* pixels, int width, int height, Texture& out);

// Loads a PNG/JPG (UTF-8 path) into an RGBA8 shader resource view.
bool LoadTextureFromFile(ID3D11Device* device, const char* path, Texture& out);
void ReleaseTexture(Texture& tex);

} // namespace md::ui
