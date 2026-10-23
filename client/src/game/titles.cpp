#include "titles.h"
#include "session.h"
#include "songs.h"
#include "sync_start.h"
#include "../core/log.h"
#include "../core/mod.h"

#include <fstream>
#include <map>

namespace md::titles {

static std::map<int32_t, std::string> s_titles;
static int32_t s_recordedPv = -1; // PV whose title was already checked during this song

static std::wstring FilePath() { return DataFile(L"titles.txt"); }

void Load()
{
    std::ifstream in(FilePath());
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        int32_t pv = atoi(line.substr(0, eq).c_str());
        std::string title = line.substr(eq + 1);
        if (!title.empty() && title.back() == '\r')
            title.pop_back();
        if (pv > 0 && !title.empty())
            s_titles[pv] = title;
    }
}

static void Save()
{
    std::ofstream out(FilePath(), std::ios::trunc);
    for (const auto& [pv, title] : s_titles)
        out << pv << "=" << title << "\n";
}

std::string Find(int32_t pvId)
{
    // The game's song list knows every installed song; the cache keeps titles of songs seen in play
    // (e.g. a host's song this player doesn't have, once it was learned on another occasion).
    std::string title = songs::Title(pvId);
    if (!title.empty())
        return title;
    auto it = s_titles.find(pvId);
    return it == s_titles.end() ? std::string() : it->second;
}

std::string Describe(int32_t pvId)
{
    std::string title = Find(pvId);
    return title.empty() ? "PV " + std::to_string(pvId) : title;
}

void Tick()
{
    const Session& s = g_session;
    if (!s.inSong) {
        s_recordedPv = -1;
        return;
    }
    if (s.pvId <= 0 || s_recordedPv == s.pvId)
        return;

    // Once the song plays, its title comes from the game's song list (songs::Title).
    if (sync::ReadState().outer != 1)
        return;

    s_recordedPv = s.pvId;
    std::string title = songs::Title(s.pvId);
    if (!title.empty() && s_titles[s.pvId] != title) {
        s_titles[s.pvId] = title;
        Save();
        Log("title: PV %d = %s", s.pvId, title.c_str());
    }
}

} // namespace md::titles
