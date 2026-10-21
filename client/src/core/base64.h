#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace md {

std::string Base64Encode(const uint8_t* data, size_t size);

// Returns false on invalid input.
bool Base64Decode(const std::string& text, std::vector<uint8_t>& out);

} // namespace md
