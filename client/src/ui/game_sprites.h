#pragma once
#include <cstdint>
#include <d3d11.h>
#include <imgui.h>

// The game's own 2D art (menu buttons, plates, glows, labels...) drawn in our overlay.
//
// Sprite sets are read from the game's memory once the game has loaded them (no files, no keys): its sprite set
// map gives each set's sprite table (name, texture, rectangle) and textures. A set is copied the first time it is
// seen loaded and its textures are kept alive (AddRef), so its sprites stay usable after the game unloads it.
// See docs/re-notes.md "UI assets".
namespace md::ui::sprites {

namespace set {
constexpr uint32_t MainMenu = 2976; // spr_nswgam_menu
constexpr uint32_t Common = 2970;   // spr_nswgam_cmn (dialogs, key icons)
constexpr uint32_t SongSelect = 2981;
constexpr uint32_t FontThin = 1907; // spr_fnt_36latin9 (sets are keyed by their spr_db index)
constexpr uint32_t FontBold = 1913; // spr_fnt_bold36latin9
} // namespace set

struct Sprite {
    ID3D11ShaderResourceView* srv = nullptr;
    ImVec2 uv0, uv1;  // in the texture (already flipped: the game stores sheets bottom-up)
    ImVec2 size;      // in pixels, as authored for 1080p
    bool ycbcr = false; // two-channel sheet: needs the YCbCr pixel shader
};

// Copies any wanted set the game has loaded right now. Cheap, call once per frame (render thread).
void Tick();
bool Ready(uint32_t setId);

// A sprite by its short name (e.g. "SELECTOR_RHYTHMGAME_EFF"), null until its set has been seen loaded.
const Sprite* Find(uint32_t setId, const char* name);

// A song's jacket (SONG_JK, 502x502) from its select set, which is requested from the game the first time and
// copied once loaded (works for songs the player has, mod songs too). Null until then, or if the song has none.
// Call every frame it's wanted; the last few stay cached.
const Sprite* Jacket(int pv);

// Draws a sprite into `a`..`b` (any size; tint multiplies). Two-channel sheets switch ImGui to our shader.
void Draw(ImDrawList* dl, const Sprite& s, ImVec2 a, ImVec2 b, ImU32 tint = IM_COL32_WHITE);

// Draws a grey sprite recolored: black stays black, grey `g0` (0..1) becomes `low`, grey `g1` and lighter become
// `high`. E.g. song select's star-less grey song bar as a bar of any color (white top, colored band).
void DrawRecolored(ImDrawList* dl, const Sprite& s, ImVec2 a, ImVec2 b, ImU32 low, ImU32 high, float g0, float g1,
    ImU32 tint = IM_COL32_WHITE);

// ---- text in the game's own font (36 px sheets + the game's per-letter widths, read from its memory) ----
enum class Font { Thin, Bold }; // song select titles use Bold

// Song select's title look at 1080p: the bold font at ~43 px with ~4.5 px between letters.
constexpr float TitleSpacingOfSize = 0.105f;

// False until the font sheet has been seen loaded and its width table read (draw a fallback until then).
bool TextReady(Font f);
// Width of `text` (UTF-8) at `px` pixels tall, with `spacing` extra pixels after each letter.
float TextWidth(Font f, const char* text, float px, float spacing = 0);
// Draws `text` with its top-left at `pos`. Letters the font lacks are skipped (a space-wide gap).
void DrawText(ImDrawList* dl, Font f, ImVec2 pos, const char* text, float px, ImU32 col, float spacing = 0);

#ifdef MD_SPRITE_PACK
// tools/ui-preview: loads the sets and font tables from a local folder instead of the game's memory.
bool LoadPack(const char* dir);
#endif

// Logs how many sprites of each wanted set are known (Ctrl+F8).
void LogStatus();

// Ctrl+F8 test: draws a few main menu sprites on screen.
void SetTest(bool on);
bool TestEnabled();
void DrawTest();

} // namespace md::ui::sprites
