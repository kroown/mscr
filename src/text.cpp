#include "text.hpp"
#include <cstring>
#include <cctype>

TextMeta analyze_text(const std::vector<uint8_t>& data) {
    TextMeta meta;
    meta.byte_count = data.size();
    if (data.empty()) return meta;

    bool all_ascii = true;
    bool valid_utf8 = true;
    uint64_t lines = 0;
    uint64_t words = 0;
    uint64_t chars = 0;
    bool in_word = false;
    int utf8_remaining = 0;

    for (size_t i = 0; i < data.size(); i++) {
        uint8_t c = data[i];

        if (c >= 128) all_ascii = false;

        if (valid_utf8) {
            if (utf8_remaining > 0) {
                if ((c & 0xC0) != 0x80) valid_utf8 = false;
                utf8_remaining--;
            } else if (c >= 0xF5) {
                valid_utf8 = false;
            } else if (c >= 0xF0) {
                utf8_remaining = 3;
            } else if (c >= 0xE0) {
                utf8_remaining = 2;
            } else if (c >= 0xC0) {
                utf8_remaining = 1;
            }
        }

        if (c == '\n') {
            lines++;
            if (in_word) { words++; in_word = false; }
        } else if (std::isspace(c)) {
            if (in_word) { words++; in_word = false; }
        } else if (c >= 32 || c == '\t') {
            in_word = true;
            if (c < 128) chars++;
        }
    }
    if (in_word) words++;

    // detect binary: check for null bytes and high ratio of control chars
    int null_count = 0;
    int control_count = 0;
    for (size_t i = 0; i < std::min(data.size(), size_t(4096)); i++) {
        uint8_t c = data[i];
        if (c == 0) null_count++;
        if (c < 8 || (c > 13 && c < 26) || c == 27) control_count++;
    }
    size_t check_len = std::min(data.size(), size_t(4096));
    if (check_len > 0) {
        double null_ratio = static_cast<double>(null_count) / check_len;
        double ctrl_ratio = static_cast<double>(control_count) / check_len;
        if (null_ratio < 0.01 && ctrl_ratio < 0.05) meta.is_text = true;
    }

    meta.is_ascii = all_ascii;
    meta.is_utf8 = valid_utf8;
    meta.line_count = lines;
    meta.word_count = words;
    meta.char_count = chars;

    return meta;
}
