#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include <stb_image.h>

namespace md::ui {

bool CreateTextureRGBA(ID3D11Device* device, const unsigned char* pixels, int width, int height, Texture& out)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = pixels;
    data.SysMemPitch = desc.Width * 4;

    ID3D11Texture2D* texture = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &data, &texture)))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    HRESULT hr = device->CreateShaderResourceView(texture, &srvDesc, &out.srv);
    texture->Release();
    if (FAILED(hr))
        return false;

    out.width = width;
    out.height = height;
    return true;
}

bool LoadTextureFromFile(ID3D11Device* device, const char* path, Texture& out)
{
    int width = 0, height = 0;
    unsigned char* pixels = stbi_load(path, &width, &height, nullptr, 4);
    if (!pixels)
        return false;
    bool ok = CreateTextureRGBA(device, pixels, width, height, out);
    stbi_image_free(pixels);
    return ok;
}

void ReleaseTexture(Texture& tex)
{
    if (tex.srv) {
        tex.srv->Release();
        tex.srv = nullptr;
    }
}

} // namespace md::ui
