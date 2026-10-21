#pragma once
#include <windows.h>
#include <Psapi.h>

const MODULEINFO& getModuleInfo();

// Scans the game executable's image. '?' in mask = wildcard byte. Returns the first match or nullptr.
void* sigScan(const char* signature, const char* mask);

// Returns how many times the pattern matches in the image (used to verify a signature is unique).
size_t sigCount(const char* signature, const char* mask);
