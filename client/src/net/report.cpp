#include "report.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../core/mod.h"
#include "../core/version.h"

#include <atomic>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>

namespace md::report {

// The deployed report Worker (tools/report-worker). Empty: reporting is off.
static const wchar_t* ReportUrl = L"";

static constexpr size_t MaxLogBytes = 150 * 1024; // the end of each log, where crashes are
static constexpr size_t MaxNoteBytes = 2000;

static std::atomic<State> s_state{State::Idle};

bool Enabled() { return ReportUrl[0] != 0; }
State GetState() { return s_state; }

void ResetState()
{
    if (s_state != State::Sending)
        s_state = State::Idle;
}

static std::string ReadTail(const std::wstring& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "(none)";
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    if (text.size() > MaxLogBytes)
        text = "(...)\n" + text.substr(text.size() - MaxLogBytes);
    return text;
}

static void ReplaceWord(std::string& text, const std::string& word, const char* with)
{
    if (word.size() < 2)
        return;
    auto isWordChar = [](char c) { return isalnum((unsigned char)c) || c == '_'; };
    for (size_t at = text.find(word); at != std::string::npos; at = text.find(word, at)) {
        bool startOk = at == 0 || !isWordChar(text[at - 1]);
        bool endOk = at + word.size() >= text.size() || !isWordChar(text[at + word.size()]);
        if (startOk && endOk) {
            text.replace(at, word.size(), with);
            at += strlen(with);
        } else {
            at += word.size();
        }
    }
}

// Nothing that points at a person or a server leaves the PC.
static std::string Scrub(std::string text)
{
    using std::regex;
    text = std::regex_replace(text, regex(R"((wss?|https?)://[^\s"'<>]+)", regex::icase), "<address>");
    text = std::regex_replace(text, regex(R"(\b\d{1,3}(\.\d{1,3}){3}\b)"), "<ip>");
    text = std::regex_replace(text, regex(R"(7656119\d{10})"), "<steam id>");
    text = std::regex_replace(text, regex(R"(([A-Za-z]:\\Users\\)[^\\\s]+)", regex::icase), "$1<user>");
    text = std::regex_replace(text, regex(R"((kicked from room ).*)"), "$1<room>");
    ReplaceWord(text, GetConfig().name, "<name>");
    return text;
}

// Folder names of the enabled mods, in DivaModLoader's priority order (conflicts with other mods are common).
static std::vector<std::string> EnabledMods()
{
    std::vector<std::string> mods;
    std::wstring modsDir = ModDir().substr(0, ModDir().find_last_of(L"\\/"));
    std::wstring gameDir = modsDir.substr(0, modsDir.find_last_of(L"\\/"));
    std::ifstream in(gameDir + L"\\config.toml");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("priority", 0) != 0)
            continue;
        std::smatch m;
        static const std::regex quoted(R"re("([^"]*)")re");
        for (auto it = line.cbegin(); std::regex_search(it, line.cend(), m, quoted); it = m[0].second) {
            std::string name = m[1];
            std::ifstream modConfig(modsDir + L"\\" + std::wstring(name.begin(), name.end()) + L"\\config.toml");
            std::string l;
            bool enabled = true;
            while (std::getline(modConfig, l))
                if (l.rfind("enabled", 0) == 0)
                    enabled = l.find("true") != std::string::npos;
            if (enabled)
                mods.push_back(name);
        }
    }
    return mods;
}

static nlohmann::json Build(const std::string& note, bool afterCrash)
{
    return {
        { "version", MULTIDIVA_VERSION },
        { "afterCrash", afterCrash },
        { "note", Scrub(note.substr(0, MaxNoteBytes)) },
        { "mods", EnabledMods() },
        { "log", Scrub(ReadTail(ModDir() + L"\\multidiva.log")) },
        { "oldLog", Scrub(ReadTail(ModDir() + L"\\multidiva.old.log")) },
    };
}

std::string Preview(const std::string& note, bool afterCrash)
{
    nlohmann::json j = Build(note, afterCrash);
    std::string mods;
    for (const auto& m : j["mods"])
        mods += (mods.empty() ? "" : ", ") + m.get<std::string>();
    return "MultiDiva2 " + j["version"].get<std::string>() + (afterCrash ? ", after a crash" : "") + "\n"
        + "Note: " + j["note"].get<std::string>() + "\n"
        + "Enabled mods: " + mods + "\n\n"
        + "--- this session's log ---\n" + j["log"].get<std::string>() + "\n"
        + "--- previous session's log ---\n" + j["oldLog"].get<std::string>();
}

static bool Post(const std::string& body)
{
    URL_COMPONENTS uc = { sizeof(uc) };
    wchar_t host[256] = {}, path[1024] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(ReportUrl, 0, 0, &uc))
        return false;

    bool ok = false;
    HINTERNET session = WinHttpOpen(L"MultiDiva2", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    HINTERNET connect = session ? WinHttpConnect(session, host, uc.nPort, 0) : nullptr;
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"POST", path, nullptr, nullptr, nullptr,
                                      uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                                : nullptr;
    if (request) {
        const wchar_t* headers = L"Content-Type: application/json\r\n";
        DWORD status = 0, size = sizeof(status);
        ok = WinHttpSendRequest(request, headers, (DWORD)-1L, (void*)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0)
            && WinHttpReceiveResponse(request, nullptr)
            && WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr)
            && status >= 200 && status < 300;
        WinHttpCloseHandle(request);
    }
    if (connect)
        WinHttpCloseHandle(connect);
    if (session)
        WinHttpCloseHandle(session);
    return ok;
}

void Send(const std::string& note, bool afterCrash)
{
    if (!Enabled() || s_state == State::Sending)
        return;
    s_state = State::Sending;
    // Log lines can hold text in the system code page: replace anything that isn't UTF-8 instead of throwing.
    std::string body = Build(note, afterCrash).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    std::thread([body = std::move(body)] {
        bool ok = Post(body);
        Log("report: %s", ok ? "sent" : "sending failed");
        s_state = ok ? State::Sent : State::Failed;
    }).detach();
}

} // namespace md::report
