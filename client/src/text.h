// Text for the game's menu font, which only draws a small upper-case set (SPIKE S12 pins the exact set).
#pragma once
#include <cctype>
#include <string>

namespace rs64ap {

inline std::string menu_text(const std::string& in, size_t max_len = 39) {
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        if (out.size() >= max_len) break;
        if (std::isalnum(c)) {
            out += (char)std::toupper(c);
        } else if (c == ' ' || c == '.' || c == ',' || c == ':' || c == '!' || c == '?' || c == '-' || c == '\'' ||
                   c == '/' || c == '(' || c == ')') {
            out += (char)c;
        } else if (c >= 0x80) {
            // UTF-8 continuation/lead bytes: one placeholder per lead byte.
            if ((c & 0xC0) != 0x80) out += '?';
        } else {
            out += ' ';
        }
    }
    return out;
}

}  // namespace rs64ap
