# Builds build\MultiDiva-<version>.zip: a "MultiDiva" mod folder ready for Diva Mod Manager
# (drag the zip onto its window) or for extracting into the game's "mods" folder.
# Run from the repo root after building:  powershell -ExecutionPolicy Bypass -File tools\package.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dll = Join-Path $root "build\x64-release\Release\MultiDiva.dll"
$server = Join-Path $root "server\bin\multidiva-server.exe"
$bot = Join-Path $root "server\bin\multidiva-bot.exe"
$dist = Join-Path $root "dist\mods\MultiDiva"

foreach ($f in @($dll, $server, $bot)) {
    if (-not (Test-Path $f)) { throw "missing $f (build the mod and the server first)" }
}

$version = (Select-String -Path (Join-Path $dist "config.toml") -Pattern '^version\s*=\s*"(.+)"').Matches[0].Groups[1].Value
$stage = Join-Path $root "build\package\MultiDiva"
if (Test-Path (Split-Path $stage)) { Remove-Item -Recurse -Force (Split-Path $stage) }
New-Item -ItemType Directory -Force (Join-Path $stage "bin") | Out-Null

# Only what a player needs: no personal settings (multidiva.ini holds names and tunnel tokens),
# logs, title cache or cloudflared (downloaded on first "Host a server").
Copy-Item -Recurse (Join-Path $dist "*") $stage
Copy-Item $dll $stage
Copy-Item $server (Join-Path $stage "bin")
Copy-Item $bot (Join-Path $stage "bin")

$zip = Join-Path $root "build\MultiDiva-$version.zip"
if (Test-Path $zip) { Remove-Item $zip }
# Entries written by hand: Windows PowerShell 5.1 (Compress-Archive, ZipFile) stores "\" separators,
# which some unzippers mishandle. Zip paths must use "/".
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$base = Split-Path $stage
$archive = [System.IO.Compression.ZipFile]::Open($zip, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    Get-ChildItem -Recurse -File $stage | ForEach-Object {
        $name = $_.FullName.Substring($base.Length + 1).Replace("\", "/")
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $_.FullName, $name,
            [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally {
    $archive.Dispose()
}
Write-Output "wrote $zip"
