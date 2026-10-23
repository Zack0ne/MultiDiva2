#pragma once
#include <d3d11.h>

// Research tool (Ctrl+F8): records one game frame at the Direct3D level, to learn which textures and sprite
// rectangles the game's menus are drawn with. Writes mods\MultiDiva\capture\: every texture the frame
// samples (as .dds, already decoded by the game) and draws.txt (each draw: texture, vertex data).
namespace md::ui::capture {

void Install(ID3D11DeviceContext* context); // hooks the context's draw / map functions (d3d11.dll, not game code)
void Request();                             // capture the next game frame
bool Busy();

// Called by the overlay: the game's frame lies between the end of one overlay frame and the start of the next.
void OnOverlayFrameStart();
void OnOverlayFrameEnd();

} // namespace md::ui::capture
