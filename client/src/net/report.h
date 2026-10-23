#pragma once
#include <string>

// Bug and crash reports, sent with one click to the MultiDiva2 report Worker (tools/report-worker), which posts
// them to the developer's private Discord channel. Only sent when the player clicks Send.
namespace md::report {

// False when no report address is built in: the UI hides everything about reports.
bool Enabled();

enum class State { Idle, Sending, Sent, Failed };
State GetState();
void ResetState(); // back to Idle for a new report (not while sending)

// What a report contains, exactly as it would be sent: the mod version, the enabled mods, this session's log and
// the previous one's, with addresses, IPs, Steam IDs, the player's name and Windows user name removed.
std::string Preview(const std::string& note, bool afterCrash);

// Sends it in the background; GetState() follows.
void Send(const std::string& note, bool afterCrash);

} // namespace md::report
