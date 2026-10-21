#include "leaderboard.h"
#include "avatars.h"
#include "overlay.h"
#include "../core/config.h"

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <unordered_map>

namespace md::ui {

// Proportions measured from osu!'s multiplayer leaderboard, relative to the card height.
constexpr float CardHeightOfScreen = 0.060f; // ~65 px at 1080p
constexpr float NameOfCard = 0.42f;
constexpr float SmallOfCard = 0.31f;
constexpr float WidthOfCard = 4.6f;          // card incl. avatar
constexpr float GapOfCard = 0.12f;
constexpr float RoundingOfCard = 0.16f;
constexpr float TopOfScreen = 0.097f;        // just below the life bar: notes reach the screen edges lower down

float CardHeight(float screenHeight) { return std::round(screenHeight * CardHeightOfScreen); }
float CardNameFontPx(float screenHeight) { return std::round(CardHeight(screenHeight) * NameOfCard); }
float CardSmallFontPx(float screenHeight) { return std::round(CardHeight(screenHeight) * SmallOfCard); }

namespace {

// Per-player animation state, keyed by player id.
struct Row {
    float y = -1;          // current slot position (in rows), animates toward the rank
    double shownScore = 0; // rolling number
    int lastRank = -1;
    int lastCombo = 0;
    float overtake = 0;    // 1 -> 0 glow after moving up
    float comboBreak = 0;  // 1 -> 0 red flash after a combo break
};

std::unordered_map<int, Row> g_rows;

constexpr float SlideSpeed = 10.0f; // higher = snappier position swaps
constexpr float ScoreSpeed = 12.0f; // how fast rolling scores catch up
constexpr float FlashFade = 1.8f;   // flash lifetime ~0.55 s
constexpr int ComboBreakMin = 20;   // combos below this breaking aren't dramatic

const ImVec4 Teal(0.45f, 0.86f, 0.86f, 1.0f);
const ImVec4 Gold(1.0f, 0.82f, 0.30f, 1.0f);
const ImVec4 Red(1.0f, 0.36f, 0.40f, 1.0f);
const ImVec4 White(1.0f, 1.0f, 1.0f, 1.0f);

float Approach(float t, float dt) { return 1.0f - std::exp(-t * dt); }

ImU32 Col(ImVec4 c, float alpha) { return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha)); }

// 3033270 -> "3,033,270"
void FormatThousands(char* buf, size_t size, long long v)
{
    char digits[32];
    int n = snprintf(digits, sizeof(digits), "%lld", v < 0 ? -v : v);
    size_t o = 0;
    for (int i = 0; i < n && o + 2 < size; i++) {
        if (i > 0 && (n - i) % 3 == 0)
            buf[o++] = ',';
        buf[o++] = digits[i];
    }
    buf[o] = '\0';
}

ID3D11ShaderResourceView* PlayerPicture(const mp::State& s, int id)
{
    for (const mp::Player& p : s.players)
        if (p.id == id)
            return PlayerAvatar(p);
    return nullptr;
}

// Stable color per name, for players without an avatar.
ImVec4 NameColor(const std::string& name)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : name)
        h = (h ^ c) * 16777619u;
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB((h % 360) / 360.0f, 0.40f, 0.62f, r, g, b);
    return ImVec4(r, g, b, 1.0f);
}

void Text(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text, const ImVec4* clip = nullptr)
{
    dl->AddText(font, size, ImVec2(std::round(pos.x), std::round(pos.y)), color, text, nullptr, 0.0f, clip);
}

ImVec2 Measure(ImFont* font, float size, const char* text) { return font->CalcTextSizeA(size, FLT_MAX, 0, text); }

} // namespace

void ResetLeaderboard() { g_rows.clear(); }

void DrawLeaderboard(const mp::State& s)
{
    const Fonts& fonts = GetFonts();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float dt = ImGui::GetIO().DeltaTime;

    const float cardH = CardHeight(vp->Size.y);
    const float nameSize = CardNameFontPx(vp->Size.y);
    const float smallSize = CardSmallFontPx(vp->Size.y);
    const float width = std::round(cardH * WidthOfCard);
    const float gap = std::round(cardH * GapOfCard);
    const float rounding = cardH * RoundingOfCard;
    const float pad = std::round(cardH * 0.16f);
    const int count = (int)s.scores.size();
    const bool showGaps = GetConfig().showGaps;

    int myScore = 0;
    for (const mp::LiveScore& p : s.scores)
        if (p.id == s.myId)
            myScore = p.score;

    // Update animation state from the latest server ranking (scores arrive sorted).
    for (int rank = 0; rank < count; rank++) {
        const mp::LiveScore& p = s.scores[rank];
        Row& r = g_rows[p.id];
        if (r.y < 0) {
            r.y = (float)rank; // first sight: no slide-in
            r.lastRank = rank;
        }
        if (rank < r.lastRank)
            r.overtake = 1.0f;
        if (p.combo == 0 && r.lastCombo >= ComboBreakMin)
            r.comboBreak = 1.0f;
        r.lastRank = rank;
        r.lastCombo = p.combo;

        r.y += ((float)rank - r.y) * Approach(SlideSpeed, dt);
        r.shownScore += ((double)p.score - r.shownScore) * Approach(ScoreSpeed, dt);
        if (std::fabs(p.score - r.shownScore) < 1.0)
            r.shownScore = p.score;
        r.overtake = r.overtake > 0 ? r.overtake - dt * FlashFade : 0;
        r.comboBreak = r.comboBreak > 0 ? r.comboBreak - dt * FlashFade : 0;
    }

    // Cards hug the left screen edge, like osu!.
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y * TopOfScreen), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width + 2, count * (cardH + gap) + 2), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;
    if (!ImGui::Begin("##leaderboard", nullptr, flags)) {
        ImGui::End();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetWindowPos();
    // A window clips half its padding off its edges, which would cut the rounded corners square.
    dl->PushClipRectFullScreen();

    // Draw bottom-up so a card sliding up passes over the one it overtakes.
    for (int rank = count - 1; rank >= 0; rank--) {
        const mp::LiveScore& p = s.scores[rank];
        Row& r = g_rows[p.id];
        bool me = p.id == s.myId;

        ImVec2 a(origin.x, std::round(origin.y + r.y * (cardH + gap)));
        ImVec2 b(a.x + width, a.y + cardH);
        ImVec2 avB(a.x + cardH, b.y);

        // Card body: translucent navy, rounded on the right; the avatar sits square on the left.
        ImVec4 navy = me ? ImVec4(0.10f, 0.17f, 0.26f, 0.80f) : ImVec4(0.06f, 0.09f, 0.16f, 0.66f);
        dl->AddRectFilled(a, b, ImGui::GetColorU32(navy), rounding, ImDrawFlags_RoundCornersRight);
        if (r.overtake > 0)
            dl->AddRectFilled(a, b, Col(me ? Teal : Gold, 0.28f * r.overtake), rounding, ImDrawFlags_RoundCornersRight);
        if (r.comboBreak > 0)
            dl->AddRectFilled(a, b, Col(Red, 0.38f * r.comboBreak), rounding, ImDrawFlags_RoundCornersRight);

        if (ID3D11ShaderResourceView* tex = PlayerPicture(s, p.id)) {
            dl->AddImage((ImTextureID)tex, a, avB);
        } else {
            dl->AddRectFilled(a, avB, ImGui::GetColorU32(NameColor(p.name)));
            char initial[2] = { p.name.empty() ? '?' : (char)toupper((unsigned char)p.name[0]), 0 };
            ImVec2 sz = Measure(fonts.cardName, nameSize * 1.3f, initial);
            Text(dl, fonts.cardName, nameSize * 1.3f, ImVec2(a.x + (cardH - sz.x) * 0.5f, a.y + (cardH - sz.y) * 0.5f),
                Col(White, 0.95f), initial);
        }

        char buf[48];
        const float textX = avB.x + pad;
        const float right = b.x - pad;
        const float alpha = me ? 1.0f : 0.88f;

        // Faint rank number, top right.
        snprintf(buf, sizeof(buf), "%d", rank + 1);
        ImVec2 rankSz = Measure(fonts.cardName, nameSize * 1.15f, buf);
        Text(dl, fonts.cardName, nameSize * 1.15f, ImVec2(right - rankSz.x, a.y + cardH * 0.04f), Col(White, 0.13f), buf);

        // Optional, first line right: how far ahead (+, red) or behind (-, teal) the others are.
        float nameRight = right - rankSz.x - pad * 0.5f;
        if (showGaps && !me && !p.finished && count > 1) {
            int diff = p.score - myScore;
            buf[0] = diff > 0 ? '+' : diff < 0 ? '-' : ' ';
            FormatThousands(buf + 1, sizeof(buf) - 1, diff < 0 ? -diff : diff);
            ImVec2 sz = Measure(fonts.cardSmall, smallSize, buf);
            float gx = nameRight - sz.x;
            Text(dl, fonts.cardSmall, smallSize, ImVec2(gx, a.y + cardH * 0.10f + (nameSize - smallSize) * 0.6f),
                Col(diff > 0 ? Red : Teal, 0.75f), buf);
            nameRight = gx - pad * 0.5f;
        }

        // Name on the first line (clipped before the rank number / gap).
        float nameY = a.y + cardH * 0.10f;
        ImVec4 clip(textX, a.y, nameRight, b.y);
        Text(dl, fonts.cardName, nameSize, ImVec2(textX, nameY), Col(White, alpha), p.name.c_str(), &clip);

        // Score on the second line, clear percentage and combo on its right.
        float lineY = b.y - cardH * 0.10f - smallSize;
        FormatThousands(buf, sizeof(buf), std::llround(r.shownScore));
        Text(dl, fonts.cardSmall, smallSize, ImVec2(textX, lineY), Col(White, 0.92f * alpha), buf);

        snprintf(buf, sizeof(buf), p.failed ? "failed" : p.finished ? "done" : "%dx", p.combo);
        ImVec2 comboSz = Measure(fonts.cardSmall, smallSize, buf);
        Text(dl, fonts.cardSmall, smallSize, ImVec2(right - comboSz.x, lineY),
            Col(p.failed || r.comboBreak > 0 ? Red : Teal, alpha), buf);

        snprintf(buf, sizeof(buf), "%.2f%%", std::floor(p.completion * 100.0 + 1e-6) / 100.0);
        ImVec2 pctSz = Measure(fonts.cardSmall, smallSize, buf);
        float pctX = right - comboSz.x - pad - pctSz.x;
        Text(dl, fonts.cardSmall, smallSize, ImVec2(pctX, lineY), Col(White, 0.70f * alpha), buf);
    }
    dl->PopClipRect();
    ImGui::End();
}

} // namespace md::ui
