#include "host.h"
#include "../core/log.h"
#include "../core/mod.h"

#include <windows.h>
#include <softpub.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <wintrust.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace md::host {

static std::mutex s_mutex;
static Status s_status;
// Never destroyed: a joinable std::thread destroyed at exit (game closed while hosting) calls std::terminate.
static std::thread& s_thread = *new std::thread;
static std::atomic<bool> s_stop{false};
static HANDLE s_serverProc = nullptr;
static HANDLE s_tunnelProc = nullptr;
static Options s_options; // set by Start before the worker runs

static const wchar_t* CloudflaredUrl =
    L"https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-windows-amd64.exe";
static const wchar_t* CloudflareSigner = L"Cloudflare, Inc.";

Status GetStatus()
{
    std::lock_guard lock(s_mutex);
    return s_status;
}

bool IsActive()
{
    Phase p = GetStatus().phase;
    return p != Phase::Off && p != Phase::Failed;
}

static void Set(Phase phase, std::string message, float progress = 0)
{
    std::lock_guard lock(s_mutex);
    s_status.phase = phase;
    s_status.message = std::move(message);
    s_status.progress = progress;
    if (phase == Phase::Failed)
        Log("host: %s", s_status.message.c_str());
}

static std::wstring BinDir() { return ModDir() + L"\\bin"; }

// ---- HTTP ----

// GET http://127.0.0.1:port/healthz == "ok"
static bool ServerAlive()
{
    HINTERNET session = WinHttpOpen(L"MultiDiva", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
    if (!session)
        return false;
    WinHttpSetTimeouts(session, 1000, 1000, 1000, 1000);
    bool ok = false;
    HINTERNET connect = WinHttpConnect(session, L"127.0.0.1", LocalPort, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", L"/healthz", nullptr, nullptr, nullptr, 0) : nullptr;
    if (request && WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr)) {
        char buf[8] = {};
        DWORD read = 0;
        ok = WinHttpReadData(request, buf, 2, &read) && read == 2 && buf[0] == 'o' && buf[1] == 'k';
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

// Downloads url to path (via path + ".part"), reporting progress. WinHTTP follows GitHub's redirect.
static bool Download(const wchar_t* url, const std::wstring& path)
{
    URL_COMPONENTS uc = { sizeof(uc) };
    wchar_t host[256], urlPath[1024];
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(url, 0, 0, &uc))
        return false;

    HINTERNET session = WinHttpOpen(L"MultiDiva", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    HINTERNET connect = session ? WinHttpConnect(session, host, uc.nPort, 0) : nullptr;
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", urlPath, nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
    bool ok = false;
    std::wstring part = path + L".part";
    if (request && WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
        DWORD total = 0;
        size = sizeof(total);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &total, &size, nullptr);

        HANDLE file = status == 200 ? CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr)
                                    : INVALID_HANDLE_VALUE;
        if (file != INVALID_HANDLE_VALUE) {
            std::vector<char> buf(64 * 1024);
            DWORD read = 0, done = 0;
            ok = true;
            while (!s_stop) {
                if (!WinHttpReadData(request, buf.data(), (DWORD)buf.size(), &read)) {
                    ok = false;
                    break;
                }
                if (read == 0)
                    break;
                DWORD written = 0;
                if (!WriteFile(file, buf.data(), read, &written, nullptr) || written != read) {
                    ok = false;
                    break;
                }
                done += read;
                if (total)
                    Set(Phase::Downloading, "Downloading cloudflared (first time only)...", (float)done / total);
            }
            ok = ok && !s_stop && (!total || done == total);
            CloseHandle(file);
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);

    if (ok)
        ok = MoveFileExW(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
    if (!ok)
        DeleteFileW(part.c_str());
    return ok;
}

// ---- trust ----

// The file must carry a valid Authenticode signature from Cloudflare, Inc.
static bool SignedByCloudflare(const std::wstring& path)
{
    WINTRUST_FILE_INFO file = { sizeof(file) };
    file.pcwszFilePath = path.c_str();
    WINTRUST_DATA data = { sizeof(data) };
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    bool ok = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data) == ERROR_SUCCESS;
    if (ok) {
        ok = false;
        CRYPT_PROVIDER_DATA* provider = WTHelperProvDataFromStateData(data.hWVTStateData);
        CRYPT_PROVIDER_SGNR* signer = provider ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : nullptr;
        CRYPT_PROVIDER_CERT* cert = signer ? WTHelperGetProvCertFromChain(signer, 0) : nullptr;
        wchar_t name[256] = {};
        if (cert && CertGetNameStringW(cert->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name, 256))
            ok = wcscmp(name, CloudflareSigner) == 0;
    }
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
    return ok;
}

// ---- processes ----

static HANDLE Job()
{
    // Never closed: when the game exits, Windows closes it and kills our child processes.
    // A function-local static: the hosting worker and the UI thread (bot) may both get here first.
    static HANDLE job = [] {
        HANDLE j = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(j, JobObjectExtendedLimitInformation, &info, sizeof(info));
        return j;
    }();
    return job;
}

// Starts a hidden process in our job. If `output` is given, stdout+stderr go to a pipe returned there.
static HANDLE Spawn(std::wstring cmdline, HANDLE* output)
{
    HANDLE readEnd = nullptr, writeEnd = nullptr;
    STARTUPINFOW si = { sizeof(si) };
    if (output) {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        if (!CreatePipe(&readEnd, &writeEnd, &sa, 0))
            return nullptr;
        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = writeEnd;
        si.hStdError = writeEnd;
    }
    PROCESS_INFORMATION pi = {};
    std::wstring dir = BinDir();
    BOOL ok = CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, output != nullptr, CREATE_NO_WINDOW | CREATE_SUSPENDED,
        nullptr, dir.c_str(), &si, &pi);
    if (writeEnd)
        CloseHandle(writeEnd);
    if (!ok) {
        if (readEnd)
            CloseHandle(readEnd);
        return nullptr;
    }
    AssignProcessToJobObject(Job(), pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    if (output)
        *output = readEnd;
    return pi.hProcess;
}

static void Kill(HANDLE& proc)
{
    if (proc) {
        TerminateProcess(proc, 0);
        CloseHandle(proc);
        proc = nullptr;
    }
}

// ---- worker ----

static void Run()
{
    // 1. Server: reuse one that is already running on the port, else start the bundled one.
    Set(Phase::StartingServer, "Starting the server...");
    if (!ServerAlive()) {
        std::wstring exe = BinDir() + L"\\multidiva-server.exe";
        if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES)
            return Set(Phase::Failed, "multidiva-server.exe is missing from mods\\MultiDiva\\bin.");
        s_serverProc = Spawn(L"\"" + exe + L"\" -addr 127.0.0.1:" + std::to_wstring(LocalPort), nullptr);
        if (!s_serverProc)
            return Set(Phase::Failed, "Could not start the server.");
        for (int i = 0; i < 50 && !s_stop && !ServerAlive(); i++)
            Sleep(100);
        if (!ServerAlive())
            return Set(Phase::Failed, "The server did not start (is port 3939 in use?).");
    }

    // 2. cloudflared: download once, and always check Cloudflare's signature before running it.
    std::wstring tunnelExe = BinDir() + L"\\cloudflared.exe";
    CreateDirectoryW(BinDir().c_str(), nullptr);
    if (GetFileAttributesW(tunnelExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Set(Phase::Downloading, "Downloading cloudflared (first time only)...");
        if (!Download(CloudflaredUrl, tunnelExe))
            return Set(Phase::Failed, s_stop ? "Stopped." : "Downloading cloudflared failed. Check your internet connection.");
    }
    if (!SignedByCloudflare(tunnelExe)) {
        DeleteFileW(tunnelExe.c_str());
        return Set(Phase::Failed, "cloudflared.exe is not signed by Cloudflare, deleted it. Try again.");
    }

    // 3. Tunnel: read its log until it prints the public address and registers a connection.
    Set(Phase::StartingTunnel, "Opening the tunnel...");
    HANDLE output = nullptr;
    std::string url;
    if (!s_options.token.empty()) {
        // Named tunnel: its public hostname and target are configured in the Cloudflare dashboard.
        // The token goes through the environment (TUNNEL_TOKEN), not the visible command line.
        url = s_options.permanentUrl;
        std::wstring token(s_options.token.begin(), s_options.token.end());
        SetEnvironmentVariableW(L"TUNNEL_TOKEN", token.c_str());
        s_tunnelProc = Spawn(L"\"" + tunnelExe + L"\" tunnel --no-autoupdate run", &output);
        SetEnvironmentVariableW(L"TUNNEL_TOKEN", nullptr);
        std::lock_guard lock(s_mutex);
        s_status.publicUrl = url;
    } else {
        s_tunnelProc = Spawn(L"\"" + tunnelExe + L"\" tunnel --no-autoupdate --url http://127.0.0.1:" + std::to_wstring(LocalPort),
            &output);
    }
    if (!s_tunnelProc)
        return Set(Phase::Failed, "Could not start cloudflared.");

    std::string log;
    char buf[4096];
    DWORD read = 0;
    while (!s_stop && ReadFile(output, buf, sizeof(buf), &read, nullptr) && read > 0) {
        log.append(buf, read);
        if (url.empty()) {
            size_t end = log.find(".trycloudflare.com");
            size_t start = end == std::string::npos ? end : log.rfind("https://", end);
            if (start != std::string::npos) {
                url = "wss://" + log.substr(start + 8, end + 18 - (start + 8));
                std::lock_guard lock(s_mutex);
                s_status.publicUrl = url;
            }
        }
        if (!url.empty() && log.find("Registered tunnel connection") != std::string::npos
            && GetStatus().phase != Phase::Ready) {
            Set(Phase::Ready, s_options.token.empty() ? "Hosting. New addresses can take ~30 s to work for friends."
                                                      : "Hosting on your permanent address.");
            Log("host: ready at %s", url.c_str());
        }
        if (!s_options.token.empty() && (log.find("Unauthorized") != std::string::npos
                                            || log.find("Invalid tunnel secret") != std::string::npos)) {
            Kill(s_tunnelProc);
            CloseHandle(output);
            return Set(Phase::Failed, "Cloudflare rejected the tunnel token. Check it in the permanent address settings.");
        }
        if (log.size() > 64 * 1024)
            log.erase(0, log.size() - 4096); // keep the tail, the address was already found
    }
    CloseHandle(output);
    if (!s_stop)
        Set(Phase::Failed, "The tunnel stopped (PC asleep or network change?). Click Host again.");
}

void Start(const Options& options)
{
    if (IsActive())
        return;
    Stop();
    s_stop = false;
    s_options = options;
    {
        std::lock_guard lock(s_mutex);
        s_status = Status{};
    }
    s_thread = std::thread(Run);
}

// ---- Miku Bot ----

static HANDLE s_botProc = nullptr; // UI thread only

// One argument quoted for CommandLineToArgvW / the Go runtime: backslashes before a quote are doubled.
static std::wstring QuoteArg(const std::string& utf8)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring arg(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, arg.data(), n);
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            slashes++;
            continue;
        }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    return out + L"\"";
}

bool BotRunning()
{
    if (s_botProc && WaitForSingleObject(s_botProc, 0) != WAIT_TIMEOUT)
        Kill(s_botProc); // exited by itself (connection lost)
    return s_botProc != nullptr;
}

bool StartBot(const std::string& url, const std::string& room)
{
    if (BotRunning())
        return true;
    std::wstring exe = BinDir() + L"\\multidiva-bot.exe";
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Log("bot: multidiva-bot.exe is missing from mods\\MultiDiva\\bin");
        return false;
    }
    std::wstring cmd = L"\"" + exe + L"\" -url " + QuoteArg(url) + L" -room " + QuoteArg(room)
        + L" -name \"Miku Bot\" -rival -load-delay 2s";
    s_botProc = Spawn(cmd, nullptr);
    Log("bot: %s", s_botProc ? "started" : "failed to start");
    return s_botProc != nullptr;
}

void StopBot()
{
    if (s_botProc)
        Log("bot: stopped");
    Kill(s_botProc);
}

void Stop()
{
    s_stop = true;
    Kill(s_tunnelProc); // ends the pipe read in the worker
    Kill(s_serverProc);
    if (s_thread.joinable())
        s_thread.join();
    std::lock_guard lock(s_mutex);
    s_status = Status{};
}

} // namespace md::host
