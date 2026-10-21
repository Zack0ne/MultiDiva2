#include "gpu_capture.h"
#include "../core/log.h"
#include "../core/mod.h"

#include <cmath>
#include <cstdio>
#include <detours.h>
#include <map>
#include <string>
#include <vector>
#include <windows.h>

namespace md::ui::capture {

// ID3D11DeviceContext vtable slots.
constexpr int SlotDrawIndexed = 12, SlotDraw = 13, SlotMap = 14, SlotUnmap = 15, SlotDrawIndexedInstanced = 20,
              SlotDrawInstanced = 21, SlotUpdateSubresource = 48;

using DrawIndexedFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
using DrawFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
using MapFn = HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*);
using UnmapFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT);
using DrawIndexedInstancedFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, INT, UINT);
using DrawInstancedFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, UINT);
using UpdateSubresourceFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT, const D3D11_BOX*, const void*, UINT, UINT);

static DrawIndexedFn s_drawIndexed;
static DrawFn s_draw;
static MapFn s_map;
static UnmapFn s_unmap;
static DrawIndexedInstancedFn s_drawIndexedInstanced;
static DrawInstancedFn s_drawInstanced;
static UpdateSubresourceFn s_updateSubresource;

static bool s_requested = false, s_active = false;
static FILE* s_out = nullptr;
static int s_draws = 0;
static std::map<ID3D11Resource*, std::vector<uint8_t>> s_buffers; // latest CPU-side contents seen this frame
static std::map<ID3D11Resource*, void*> s_mapped;
static std::map<ID3D11Resource*, int> s_textures; // texture -> dump index
static std::wstring s_dir;
static constexpr int MaxDraws = 4000;

// ---- helpers ----

static size_t RowBytes(DXGI_FORMAT f, UINT width, UINT* rows, UINT height)
{
    *rows = height;
    switch (f) {
    case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC1_UNORM_SRGB: case DXGI_FORMAT_BC4_UNORM: case DXGI_FORMAT_BC4_SNORM:
        *rows = (height + 3) / 4;
        return ((width + 3) / 4) * 8;
    case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC2_UNORM_SRGB: case DXGI_FORMAT_BC3_UNORM: case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_UNORM: case DXGI_FORMAT_BC5_SNORM: case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB:
        *rows = (height + 3) / 4;
        return ((width + 3) / 4) * 16;
    case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: case DXGI_FORMAT_R32_FLOAT:
        return (size_t)width * 4;
    case DXGI_FORMAT_R8_UNORM: return width;
    case DXGI_FORMAT_R8G8_UNORM: return (size_t)width * 2;
    default: return 0;
    }
}

// One mip of a 2D texture as a DDS file (DX10 header), via a staging copy. Sprite sheets are YCbCr: BC5 with
// luma+alpha in mip 0 and chroma in mip 1 (see docs/re-notes.md), so both get dumped.
static void DumpTexture(ID3D11DeviceContext* ctx, ID3D11Texture2D* tex, int index, UINT mip = 0)
{
    D3D11_TEXTURE2D_DESC d;
    tex->GetDesc(&d);
    if (mip >= d.MipLevels)
        return;
    d.Width = d.Width >> mip ? d.Width >> mip : 1;
    d.Height = d.Height >> mip ? d.Height >> mip : 1;
    UINT rows = 0;
    size_t rowBytes = RowBytes(d.Format, d.Width, &rows, d.Height);
    if (!rowBytes || d.SampleDesc.Count != 1)
        return;
    D3D11_TEXTURE2D_DESC sd = d;
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MiscFlags = 0;
    ID3D11Device* dev = nullptr;
    ctx->GetDevice(&dev);
    ID3D11Texture2D* staging = nullptr;
    if (!dev || FAILED(dev->CreateTexture2D(&sd, nullptr, &staging))) {
        if (dev) dev->Release();
        return;
    }
    dev->Release();
    ctx->CopySubresourceRegion(staging, 0, 0, 0, 0, tex, mip, nullptr); // array slice 0: subresource = mip
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(s_map(ctx, staging, 0, D3D11_MAP_READ, 0, &m))) {
        wchar_t name[96];
        swprintf(name, 96, mip ? L"\\tex_%03d_%ux%u_f%d_m1.dds" : L"\\tex_%03d_%ux%u_f%d.dds", index, d.Width, d.Height,
            (int)d.Format);
        FILE* f = _wfopen((s_dir + name).c_str(), L"wb");
        if (f) {
            uint32_t header[32] = {};
            header[0] = 124;                 // size
            header[1] = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000; // caps, height, width, pixelformat, linear size
            header[2] = d.Height;
            header[3] = d.Width;
            header[4] = (uint32_t)(rowBytes * rows);
            header[6] = 1;                   // mips
            header[18] = 32;                 // pixel format size
            header[19] = 0x4;                // FOURCC
            header[20] = 0x30315844;         // 'DX10'
            header[26] = 0x1000;             // caps: texture
            uint32_t dx10[5] = { (uint32_t)d.Format, 3 /* 2D */, 0, 1, 0 };
            fwrite("DDS ", 1, 4, f);
            fwrite(header, 4, 31, f);
            fwrite(dx10, 4, 5, f);
            for (UINT r = 0; r < rows; r++)
                fwrite((uint8_t*)m.pData + (size_t)r * m.RowPitch, 1, rowBytes, f);
            fclose(f);
        }
        s_unmap(ctx, staging, 0);
    }
    staging->Release();
}

// The bytes of a buffer: what the game wrote this frame, else a staging read-back.
static bool ReadBuffer(ID3D11DeviceContext* ctx, ID3D11Buffer* buf, std::vector<uint8_t>& out)
{
    auto it = s_buffers.find(buf);
    if (it != s_buffers.end()) {
        out = it->second;
        return true;
    }
    D3D11_BUFFER_DESC d;
    buf->GetDesc(&d);
    if (d.ByteWidth > (16u << 20))
        return false;
    D3D11_BUFFER_DESC sd = {};
    sd.ByteWidth = d.ByteWidth;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Device* dev = nullptr;
    ctx->GetDevice(&dev);
    ID3D11Buffer* staging = nullptr;
    bool ok = false;
    if (dev && SUCCEEDED(dev->CreateBuffer(&sd, nullptr, &staging))) {
        ctx->CopyResource(staging, buf);
        D3D11_MAPPED_SUBRESOURCE m;
        if (SUCCEEDED(s_map(ctx, staging, 0, D3D11_MAP_READ, 0, &m))) {
            out.assign((uint8_t*)m.pData, (uint8_t*)m.pData + d.ByteWidth);
            s_unmap(ctx, staging, 0);
            ok = true;
        }
        staging->Release();
    }
    if (dev)
        dev->Release();
    s_buffers[buf] = out; // read once per frame
    return ok;
}

// One draw: bound texture (slot 0), vertex stride, and the vertices it uses as floats.
static void Record(ID3D11DeviceContext* ctx, const char* kind, UINT count, UINT start, INT base, bool indexed)
{
    if (!s_active || !s_out || s_draws++ >= MaxDraws)
        return;
    // Pixel shader textures, slots 0-3 (sprites can be split: luma+alpha in one, chroma in another).
    char texInfo[256] = "";
    int texLen = 0;
    ID3D11ShaderResourceView* srvs[4] = {};
    ctx->PSGetShaderResources(0, 4, srvs);
    for (int slot = 0; slot < 4; slot++) {
        ID3D11ShaderResourceView* srv = srvs[slot];
        if (!srv)
            continue;
        ID3D11Resource* res = nullptr;
        srv->GetResource(&res);
        ID3D11Texture2D* tex = nullptr;
        if (res && SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex))) {
            D3D11_TEXTURE2D_DESC d;
            tex->GetDesc(&d);
            int texIndex;
            auto it = s_textures.find(res);
            if (it == s_textures.end()) {
                texIndex = (int)s_textures.size();
                s_textures[res] = texIndex;
                DumpTexture(ctx, tex, texIndex);
                if (d.Format == DXGI_FORMAT_BC5_UNORM && d.MipLevels == 2)
                    DumpTexture(ctx, tex, texIndex, 1);
            } else {
                texIndex = it->second;
            }
            texLen += snprintf(texInfo + texLen, sizeof(texInfo) - texLen, " t%d=%d(%ux%u f%d)", slot, texIndex, d.Width,
                d.Height, (int)d.Format);
            tex->Release();
        }
        if (res) res->Release();
        srv->Release();
    }
    ID3D11Buffer* vb = nullptr;
    UINT stride = 0, offset = 0;
    ctx->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
    fprintf(s_out, "draw %d %s count=%u start=%u base=%d stride=%u%s\n", s_draws, kind, count, start, base, stride, texInfo);
    if (vb && stride >= 8 && stride <= 128) {
        std::vector<uint8_t> vdata, idata;
        std::vector<uint32_t> verts;
        if (indexed) {
            ID3D11Buffer* ib = nullptr;
            DXGI_FORMAT ifmt;
            UINT ioff = 0;
            ctx->IAGetIndexBuffer(&ib, &ifmt, &ioff);
            if (ib && ReadBuffer(ctx, ib, idata)) {
                size_t isz = ifmt == DXGI_FORMAT_R16_UINT ? 2 : 4;
                for (UINT i = 0; i < count && i < 96; i++) {
                    size_t at = ioff + (size_t)(start + i) * isz;
                    if (at + isz > idata.size())
                        break;
                    uint32_t v = isz == 2 ? *(uint16_t*)&idata[at] : *(uint32_t*)&idata[at];
                    verts.push_back(v + base);
                }
            }
            if (ib) ib->Release();
        } else {
            for (UINT i = 0; i < count && i < 96; i++)
                verts.push_back(start + i);
        }
        if (ReadBuffer(ctx, vb, vdata)) {
            int shown = 0;
            uint32_t last = 0xFFFFFFFF;
            for (uint32_t v : verts) {
                if (v == last || shown >= 24)
                    continue;
                last = v;
                size_t at = offset + (size_t)v * stride;
                if (at + stride > vdata.size())
                    break;
                fprintf(s_out, "  v%u:", v);
                // Sprite vertices are x y z, a packed RGBA color, u v: print values that aren't plain floats as hex.
                for (UINT k = 0; k + 4 <= stride; k += 4) {
                    float f = *(float*)&vdata[at + k];
                    if (std::isfinite(f) && std::fabs(f) < 1e7f && (f == 0 || std::fabs(f) > 1e-6f))
                        fprintf(s_out, " %g", f);
                    else
                        fprintf(s_out, " #%08X", *(uint32_t*)&vdata[at + k]);
                }
                fprintf(s_out, "\n");
                shown++;
            }
        }
    }
    if (vb) vb->Release();
}

// ---- hooks ----

static void STDMETHODCALLTYPE HookDrawIndexed(ID3D11DeviceContext* c, UINT n, UINT s, INT b)
{
    Record(c, "DrawIndexed", n, s, b, true);
    s_drawIndexed(c, n, s, b);
}
static void STDMETHODCALLTYPE HookDraw(ID3D11DeviceContext* c, UINT n, UINT s)
{
    Record(c, "Draw", n, s, 0, false);
    s_draw(c, n, s);
}
static void STDMETHODCALLTYPE HookDrawIndexedInstanced(ID3D11DeviceContext* c, UINT n, UINT inst, UINT s, INT b, UINT si)
{
    Record(c, "DrawIndexedInstanced", n, s, b, true);
    s_drawIndexedInstanced(c, n, inst, s, b, si);
}
static void STDMETHODCALLTYPE HookDrawInstanced(ID3D11DeviceContext* c, UINT n, UINT inst, UINT s, UINT si)
{
    Record(c, "DrawInstanced", n, s, 0, false);
    s_drawInstanced(c, n, inst, s, si);
}
static HRESULT STDMETHODCALLTYPE HookMap(ID3D11DeviceContext* c, ID3D11Resource* r, UINT sub, D3D11_MAP type, UINT flags,
    D3D11_MAPPED_SUBRESOURCE* m)
{
    HRESULT hr = s_map(c, r, sub, type, flags, m);
    if (s_active && SUCCEEDED(hr) && m && type != D3D11_MAP_READ)
        s_mapped[r] = m->pData;
    return hr;
}
static void STDMETHODCALLTYPE HookUnmap(ID3D11DeviceContext* c, ID3D11Resource* r, UINT sub)
{
    if (s_active) {
        auto it = s_mapped.find(r);
        ID3D11Buffer* buf = nullptr;
        if (it != s_mapped.end() && SUCCEEDED(r->QueryInterface(__uuidof(ID3D11Buffer), (void**)&buf))) {
            D3D11_BUFFER_DESC d;
            buf->GetDesc(&d);
            s_buffers[r].assign((uint8_t*)it->second, (uint8_t*)it->second + d.ByteWidth);
            buf->Release();
        }
        if (it != s_mapped.end())
            s_mapped.erase(it);
    }
    s_unmap(c, r, sub);
}
static void STDMETHODCALLTYPE HookUpdateSubresource(ID3D11DeviceContext* c, ID3D11Resource* r, UINT sub, const D3D11_BOX* box,
    const void* data, UINT rowPitch, UINT depthPitch)
{
    if (s_active && data) {
        ID3D11Buffer* buf = nullptr;
        if (SUCCEEDED(r->QueryInterface(__uuidof(ID3D11Buffer), (void**)&buf))) {
            D3D11_BUFFER_DESC d;
            buf->GetDesc(&d);
            std::vector<uint8_t>& v = s_buffers[r];
            v.resize(d.ByteWidth);
            UINT from = box ? box->left : 0, to = box ? box->right : d.ByteWidth;
            if (to <= d.ByteWidth && from < to)
                memcpy(v.data() + from, data, to - from);
            buf->Release();
        }
    }
    s_updateSubresource(c, r, sub, box, data, rowPitch, depthPitch);
}

static void Attach(void** vtable, int slot, void** original, void* hook)
{
    *original = vtable[slot];
    DetourAttach(original, hook);
}

void Install(ID3D11DeviceContext* context)
{
    void** vt = *(void***)context;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    Attach(vt, SlotDrawIndexed, (void**)&s_drawIndexed, (void*)HookDrawIndexed);
    Attach(vt, SlotDraw, (void**)&s_draw, (void*)HookDraw);
    Attach(vt, SlotMap, (void**)&s_map, (void*)HookMap);
    Attach(vt, SlotUnmap, (void**)&s_unmap, (void*)HookUnmap);
    Attach(vt, SlotDrawIndexedInstanced, (void**)&s_drawIndexedInstanced, (void*)HookDrawIndexedInstanced);
    Attach(vt, SlotDrawInstanced, (void**)&s_drawInstanced, (void*)HookDrawInstanced);
    Attach(vt, SlotUpdateSubresource, (void**)&s_updateSubresource, (void*)HookUpdateSubresource);
    Log("capture: Direct3D hooks %s", DetourTransactionCommit() == NO_ERROR ? "installed" : "FAILED");
}

void Request() { s_requested = true; }
bool Busy() { return s_requested || s_active; }

void OnOverlayFrameEnd()
{
    if (!s_requested)
        return;
    s_requested = false;
    s_dir = ModDir() + L"\\capture";
    CreateDirectoryW(s_dir.c_str(), nullptr);
    s_out = _wfopen((s_dir + L"\\draws.txt").c_str(), L"w");
    s_draws = 0;
    s_buffers.clear();
    s_mapped.clear();
    s_textures.clear();
    s_active = s_out != nullptr;
    Log("capture: recording the next frame");
}

void OnOverlayFrameStart()
{
    if (!s_active)
        return;
    s_active = false;
    fclose(s_out);
    s_out = nullptr;
    Log("capture: %d draw(s), %zu texture(s) written to mods\\MultiDiva\\capture", s_draws, s_textures.size());
    s_buffers.clear();
    s_mapped.clear();
}

} // namespace md::ui::capture
