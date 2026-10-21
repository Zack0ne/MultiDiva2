#pragma once
#include <string>

// "Host a server" from the game: runs the bundled MultiDiva server on this PC and a Cloudflare
// quick tunnel in front of it, so friends can join through a public wss:// address.
// Both child processes live in a job object and die with the game.
namespace md::host {

enum class Phase { Off, StartingServer, Downloading, StartingTunnel, Ready, Failed };

struct Status {
    Phase phase = Phase::Off;
    std::string message;   // what's happening / what went wrong
    std::string publicUrl; // wss://name.trycloudflare.com once ready
    float progress = 0;    // cloudflared download, 0..1
};

Status GetStatus();
bool IsActive(); // anything but Off / Failed

constexpr int LocalPort = 3939;

struct Options {
    // Both set: run the user's own named Cloudflare tunnel (fixed address) instead of a quick tunnel.
    std::string permanentUrl; // wss://multidiva.example.com
    std::string token;        // tunnel token from the Cloudflare dashboard
};

void Start(const Options& options); // background work; poll GetStatus()
void Stop();

// Miku Bot: the bundled test player (bin\multidiva-bot.exe), for trying MultiDiva without a friend.
// Joins `room` on the server at `url` (ws:// or wss://) and plays along; dies with the game. UI thread only.
bool StartBot(const std::string& url, const std::string& room);
void StopBot();
bool BotRunning();

} // namespace md::host
