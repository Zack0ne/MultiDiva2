#include "bug_report.h"
#include "../core/crash.h"
#include "../net/report.h"

#include <string>
#include <imgui.h>

namespace md::ui {

static const ImVec4 Accent(0.22f, 0.77f, 0.73f, 1.0f);
static const ImVec4 Dim(0.62f, 0.62f, 0.66f, 1.0f);
static const ImVec4 Bad(1.0f, 0.42f, 0.42f, 1.0f);

static bool s_open = false;
static bool s_afterCrash = false;
static bool s_askedAfterCrash = false;
static char s_note[1000];
static std::string s_preview;

void OpenBugReport()
{
    if (!report::Enabled())
        return;
    s_open = true;
    s_afterCrash = false;
    s_preview.clear();
    report::ResetState();
}

static void Close()
{
    s_open = false;
    s_note[0] = '\0';
    s_preview.clear();
    if (s_afterCrash)
        crash::ForgetPreviousCrash();
}

void DrawBugReport()
{
    if (!report::Enabled())
        return;
    // Once per launch after a crash, unprompted.
    if (!s_askedAfterCrash && crash::PreviousSessionCrashed()) {
        s_askedAfterCrash = true;
        s_open = true;
        s_afterCrash = true;
        report::ResetState();
    }
    if (!s_open)
        return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    if (!ImGui::Begin(s_afterCrash ? "MultiDiva2 crash report" : "MultiDiva2 bug report", &open,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    report::State state = report::GetState();
    if (state == report::State::Sent) {
        ImGui::TextColored(Accent, "Sent, thank you! It goes straight to the developer.");
        if (ImGui::Button("Close", ImVec2(120, 0)))
            open = false;
    } else {
        ImGui::PushTextWrapPos(500);
        if (s_afterCrash)
            ImGui::TextUnformatted("The game closed unexpectedly last time. Send the crash report to the MultiDiva2 "
                                   "developer? It helps a lot with fixing it.");
        else
            ImGui::TextUnformatted("Something not working? Send a report to the MultiDiva2 developer.");
        ImGui::TextColored(Dim, "It holds the mod's logs and your list of enabled mods. Names, server addresses and "
                                "Steam IDs are removed.");
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::TextUnformatted(s_afterCrash ? "What were you doing? (optional)" : "What happened? (optional)");
        ImGui::InputTextMultiline("##note", s_note, sizeof(s_note), ImVec2(500, 70));

        if (ImGui::TreeNode("What gets sent")) {
            if (s_preview.empty() || ImGui::IsItemToggledOpen())
                s_preview = report::Preview(s_note, s_afterCrash);
            ImGui::InputTextMultiline("##preview", s_preview.data(), s_preview.size() + 1, ImVec2(500, 200),
                ImGuiInputTextFlags_ReadOnly);
            ImGui::TreePop();
        }

        if (state == report::State::Failed)
            ImGui::TextColored(Bad, "Sending failed. Check your internet connection and try again.");
        bool sending = state == report::State::Sending;
        if (sending)
            ImGui::BeginDisabled();
        if (ImGui::Button(sending ? "Sending..." : "Send report", ImVec2(160, 0)))
            report::Send(s_note, s_afterCrash);
        ImGui::SameLine();
        if (ImGui::Button(s_afterCrash ? "Don't send" : "Cancel", ImVec2(120, 0)))
            open = false;
        if (sending)
            ImGui::EndDisabled();
    }
    ImGui::End();
    if (!open)
        Close();
}

} // namespace md::ui
