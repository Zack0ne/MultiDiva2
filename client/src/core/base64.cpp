#include "base64.h"

namespace md {

static const char* Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const uint8_t* data, size_t size)
{
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    for (size_t i = 0; i < size; i += 3) {
        uint32_t v = data[i] << 16;
        if (i + 1 < size) v |= data[i + 1] << 8;
        if (i + 2 < size) v |= data[i + 2];
        out += Alphabet[(v >> 18) & 63];
        out += Alphabet[(v >> 12) & 63];
        out += i + 1 < size ? Alphabet[(v >> 6) & 63] : '=';
        out += i + 2 < size ? Alphabet[v & 63] : '=';
    }
    return out;
}

static int Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

bool Base64Decode(const std::string& text, std::vector<uint8_t>& out)
{
    out.clear();
    if (text.size() % 4 != 0)
        return false;
    for (size_t i = 0; i < text.size(); i += 4) {
        int v[4];
        int pad = 0;
        for (int j = 0; j < 4; j++) {
            char c = text[i + j];
            if (c == '=' && i + 4 == text.size() && j >= 2) {
                v[j] = 0;
                pad++;
            } else if (pad || (v[j] = Value(c)) < 0) {
                return false;
            }
        }
        uint32_t n = (v[0] << 18) | (v[1] << 12) | (v[2] << 6) | v[3];
        out.push_back((n >> 16) & 0xFF);
        if (pad < 2) out.push_back((n >> 8) & 0xFF);
        if (pad < 1) out.push_back(n & 0xFF);
    }
    return true;
}

} // namespace md
