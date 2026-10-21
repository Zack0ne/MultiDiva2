#include "pause_hint.h"
#include "leaderboard.h"
#include "overlay.h"
#include "../game/pause_guard.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <imgui.h>
#include <windows.h>

namespace md::ui {

static const ImVec4 Teal(0.22f, 0.77f, 0.73f, 1.0f);
static const ImVec4 Dim(0.62f, 0.62f, 0.66f, 1.0f);

float HintTitleFontPx(float screenHeight) { return std::round(screenHeight * 0.024f); }
float HintLineFontPx(float screenHeight) { return std::round(screenHeight * 0.0175f); }

// "Pausing is off" hint at the right edge after a blocked pause, mirroring the leaderboard cards.
void DrawPauseHint()
{
    pause::State ps = pause::GetState();
    uint32_t age = GetTickCount() - ps.hintTick;
    if (ps.hint == pause::Hint::None || ps.paused || age > pause::ConfirmWindowMs)
        return;

    const Fonts& fonts = GetFonts();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float cardH = CardHeight(vp->Size.y);
    const float bigPx = HintTitleFontPx(vp->Size.y);
    const float smallPx = HintLineFontPx(vp->Size.y);
    const float pad = std::round(cardH * 0.36f);
    const char* line1 = "Pausing is off in multiplayer";
    const char* line2 = "Press again to pause. The others keep playing.";
    float w = std::max(fonts.hintTitle->CalcTextSizeA(bigPx, FLT_MAX, 0, line1).x,
                  fonts.hintLine->CalcTextSizeA(smallPx, FLT_MAX, 0, line2).x) + pad * 2;
    float h = bigPx + smallPx + pad * 2.4f;

    // Fade out over the last half second of the confirm window.
    float remaining = (float)(pause::ConfirmWindowMs - age) / 1000.0f;
    float alpha = remaining < 0.5f ? remaining / 0.5f : 1.0f;
    bool armed = age >= pause::ConfirmAfterMs;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 b(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y * 0.30f + h);
    ImVec2 a(b.x - w, b.y - h);
    float rounding = cardH * 0.16f;
    dl->AddRectFilled(a, b, ImGui::GetColorU32(ImVec4(0.06f, 0.09f, 0.16f, 0.82f * alpha)), rounding, ImDrawFlags_RoundCornersLeft);
    dl->AddText(fonts.hintTitle, bigPx, ImVec2(a.x + pad, a.y + pad * 0.8f), ImGui::GetColorU32(ImVec4(1, 1, 1, alpha)), line1);
    ImVec4 c2 = armed ? ImVec4(Teal.x, Teal.y, Teal.z, alpha) : ImVec4(Dim.x, Dim.y, Dim.z, 0.8f * alpha);
    dl->AddText(fonts.hintLine, smallPx, ImVec2(a.x + pad, b.y - pad - smallPx), ImGui::GetColorU32(c2), line2);

    // Thin bar: how long the second press stays valid.
    float t = 1.0f - (float)age / pause::ConfirmWindowMs;
    dl->AddRectFilled(ImVec2(a.x + rounding, b.y - 3), ImVec2(a.x + rounding + (w - rounding) * t, b.y),
        ImGui::GetColorU32(ImVec4(Teal.x, Teal.y, Teal.z, 0.7f * alpha)));
}

} // namespace md::ui
