#include "strip.hpp"
#include <cstdint>
#include <vector>
#include <cstdio>
#include <cstring>
#include <string>

static uint16_t read_u16_be(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) << 8 | p[1];
}

static uint32_t read_u32_be(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) << 24 |
           static_cast<uint32_t>(p[1]) << 16 |
           static_cast<uint32_t>(p[2]) << 8 |
           p[3];
}

static bool write_all(FILE* f, const uint8_t* data, size_t len) {
    return fwrite(data, 1, len, f) == len;
}

static bool strip_jpeg(const std::string& path) {
    FILE* in = fopen(path.c_str(), "rb");
    if (!in) return false;
    fseek(in, 0, SEEK_END);
    long fsize = ftell(in);
    if (fsize < 2) { fclose(in); return false; }
    rewind(in);

    std::vector<uint8_t> buf(fsize);
    if (fread(buf.data(), 1, fsize, in) != size_t(fsize)) {
        fclose(in); return false;
    }
    fclose(in);

    if (buf[0] != 0xFF || buf[1] != 0xD8) return false;

    std::vector<uint8_t> out;
    out.reserve(buf.size());

    // keep SOI
    out.push_back(0xFF);
    out.push_back(0xD8);

    size_t pos = 2;
    while (pos + 2 <= buf.size()) {
        if (buf[pos] != 0xFF) break;
        uint8_t marker = buf[pos + 1];

        if (marker == 0xD9) { // EOI
            out.push_back(0xFF);
            out.push_back(0xD9);
            break;
        }

        if (marker == 0x00 || marker == 0xD0 || marker == 0xD1 ||
            marker == 0xD2 || marker == 0xD3 || marker == 0xD4 ||
            marker == 0xD5 || marker == 0xD6 || marker == 0xD7 ||
            marker == 0xD8) {
            // no segment data
            if (marker == 0xD8) { pos++; continue; }
            out.push_back(0xFF);
            out.push_back(marker);
            pos += 2;
            continue;
        }

        if (pos + 4 > buf.size()) break;
        uint16_t seg_len = read_u16_be(buf.data() + pos + 2);
        if (seg_len < 2 || pos + 2 + seg_len > buf.size()) break;

        // keep only essential markers
        bool keep = false;
        if (marker == 0xE0) keep = true;  // APP0/JFIF
        if (marker >= 0xC0 && marker <= 0xCF) keep = true; // SOF, DHT, DQT, etc.
        if (marker == 0xDA) keep = true;  // SOS
        if (marker == 0xDB) keep = true;  // DQT
        if (marker == 0xC4) keep = true;  // DHT
        if (marker == 0xDD) keep = true;  // DRI

        if (keep) {
            out.insert(out.end(), buf.data() + pos, buf.data() + pos + 2 + seg_len);
        }
        // else drop it (APP1=EXIF, APP2, COM, etc.)

        pos += 2 + seg_len;

        if (marker == 0xDA) {
            // SOS: rest of file is entropy-coded data until EOI
            size_t data_start = pos;
            // find EOI marker (FFD9) not preceded by FF00
            for (size_t i = data_start; i + 1 < buf.size(); i++) {
                if (buf[i] == 0xFF && buf[i+1] == 0xD9) {
                    out.insert(out.end(), buf.data() + data_start, buf.data() + i);
                    out.push_back(0xFF);
                    out.push_back(0xD9);
                    pos = i + 2;
                    goto done;
                }
            }
            out.insert(out.end(), buf.data() + data_start, buf.data() + buf.size());
            break;
        }
    }
done:
    ;

    if (out.size() == buf.size() && std::memcmp(out.data(), buf.data(), out.size()) == 0) return false;
    FILE* out_f = fopen(path.c_str(), "wb");
    if (!out_f) return false;
    bool ok = write_all(out_f, out.data(), out.size());
    fclose(out_f);
    return ok;
}


static bool strip_png(const std::string& path) {
    FILE* in = fopen(path.c_str(), "rb");
    if (!in) return false;
    fseek(in, 0, SEEK_END);
    long fsize = ftell(in);
    if (fsize < 8) { fclose(in); return false; }
    rewind(in);

    std::vector<uint8_t> buf(fsize);
    if (fread(buf.data(), 1, fsize, in) != size_t(fsize)) {
        fclose(in); return false;
    }
    fclose(in);

    const uint8_t sig[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (std::memcmp(buf.data(), sig, 8) != 0) return false;

    std::vector<uint8_t> out;
    out.reserve(buf.size());
    out.insert(out.end(), sig, sig + 8);

    size_t pos = 8;
    while (pos + 12 <= buf.size()) {
        uint32_t len = read_u32_be(buf.data() + pos);
        char type[5] = {};
        std::memcpy(type, buf.data() + pos + 4, 4);
        size_t chunk_end = pos + 12 + len;
        if (chunk_end > buf.size()) break;

        bool keep = false;
        if (std::memcmp(type, "IHDR", 4) == 0) keep = true;
        else if (std::memcmp(type, "PLTE", 4) == 0) keep = true;
        else if (std::memcmp(type, "IDAT", 4) == 0) { keep = true; }
        else if (std::memcmp(type, "IEND", 4) == 0) keep = true;

        if (keep) {
            out.insert(out.end(), buf.data() + pos, buf.data() + chunk_end);
        }

        pos = chunk_end;
        if (std::memcmp(type, "IEND", 4) == 0) break;
    }

    if (out.size() == buf.size() && std::memcmp(out.data(), buf.data(), out.size()) == 0) return false;
    FILE* out_f = fopen(path.c_str(), "wb");
    if (!out_f) return false;
    bool ok = write_all(out_f, out.data(), out.size());
    fclose(out_f);
    return ok;
}

static bool strip_gif(const std::string& path) {
    FILE* in = fopen(path.c_str(), "rb");
    if (!in) return false;
    fseek(in, 0, SEEK_END);
    long fsize = ftell(in);
    if (fsize < 6) { fclose(in); return false; }
    rewind(in);

    std::vector<uint8_t> buf(fsize);
    if (fread(buf.data(), 1, fsize, in) != size_t(fsize)) {
        fclose(in); return false;
    }
    fclose(in);

    if (std::memcmp(buf.data(), "GIF87a", 6) != 0 &&
        std::memcmp(buf.data(), "GIF89a", 6) != 0) return false;

    std::vector<uint8_t> out;
    out.reserve(buf.size());

    size_t pos = 0;
    // copy header
    out.push_back(buf[0]); out.push_back(buf[1]); out.push_back(buf[2]);
    out.push_back(buf[3]); out.push_back(buf[4]); out.push_back(buf[5]);

    // logical screen descriptor (7 bytes)
    out.push_back(buf[6]); out.push_back(buf[7]); out.push_back(buf[8]);
    out.push_back(buf[9]); out.push_back(buf[10]); out.push_back(buf[11]);

    uint8_t packed = buf[10];
    int gct_size = 0;
    if (packed & 0x80) {
        gct_size = 3 * (1 << ((packed & 0x07) + 1));
    }
    out.insert(out.end(), buf.data() + 13, buf.data() + 13 + gct_size);
    pos = 13 + gct_size;

    // skip extensions, keep only image descriptors and data
    while (pos < buf.size()) {
        if (buf[pos] == 0x2C) { // image descriptor
            // copy it
            size_t img_start = pos;
            pos++; // skip 0x2C
            // image descriptor: 9 bytes fixed + local color table
            if (pos + 9 > buf.size()) break;
            out.insert(out.end(), buf.data() + img_start, buf.data() + pos + 9);
            uint8_t lct_packed = buf[pos + 8];
            int lct_size = 0;
            if (lct_packed & 0x80) {
                lct_size = 3 * (1 << ((lct_packed & 0x07) + 1));
            }
            out.insert(out.end(), buf.data() + pos + 9, buf.data() + pos + 9 + lct_size);
            pos += 9 + lct_size;

            // LZW min code size
            if (pos >= buf.size()) break;
            out.push_back(buf[pos]);
            pos++;

            // sub-blocks
            while (pos < buf.size()) {
                int block_size = buf[pos];
                out.push_back(buf[pos]);
                pos++;
                if (block_size == 0) break;
                if (pos + block_size > buf.size()) break;
                out.insert(out.end(), buf.data() + pos, buf.data() + pos + block_size);
                pos += block_size;
            }
        } else if (buf[pos] == 0x21) { // extension
            if (pos + 2 > buf.size()) break;
            uint8_t label = buf[pos + 1];
            // skip comment (0xFE), application (0xFF), plain text (0x01) extensions
            // keep graphics control (0xF9) for animation timing
            if (label == 0xF9) {
                // copy graphics control extension
                size_t ext_start = pos;
                pos += 2;
                if (pos >= buf.size()) break;
                int block_size = buf[pos];
                size_t ext_end = pos + 1 + block_size;
                while (ext_end < buf.size() && buf[ext_end] != 0) {
                    ext_end += 1 + buf[ext_end];
                }
                ext_end++; // trailing 0
                out.insert(out.end(), buf.data() + ext_start, buf.data() + ext_end);
                pos = ext_end;
            } else {
                // skip this extension
                pos += 2;
                while (pos < buf.size()) {
                    int block_size = buf[pos];
                    pos++;
                    if (block_size == 0) break;
                    pos += block_size;
                }
            }
        } else if (buf[pos] == 0x3B) { // trailer
            out.push_back(0x3B);
            break;
        } else {
            break;
        }
    }

    if (out.size() == buf.size() && std::memcmp(out.data(), buf.data(), out.size()) == 0) return false;
    FILE* out_f = fopen(path.c_str(), "wb");
    if (!out_f) return false;
    bool ok = write_all(out_f, out.data(), out.size());
    fclose(out_f);
    return ok;
}

static bool strip_text(const std::string& path) {
    FILE* in = fopen(path.c_str(), "rb");
    if (!in) return false;
    fseek(in, 0, SEEK_END);
    long fsize = ftell(in);
    rewind(in);

    std::vector<uint8_t> buf(fsize);
    if (fread(buf.data(), 1, fsize, in) != size_t(fsize)) {
        fclose(in); return false;
    }
    fclose(in);

    std::vector<uint8_t> out;
    out.reserve(buf.size());

    size_t pos = 0;
    // strip utf-8 BOM
    if (buf.size() >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF)
        pos = 3;

    bool prev_cr = false;
    for (; pos < buf.size(); pos++) {
        uint8_t c = buf[pos];
        if (c == '\r') {
            prev_cr = true;
            out.push_back('\n');
        } else if (c == '\n') {
            if (!prev_cr) out.push_back('\n');
            prev_cr = false;
        } else {
            prev_cr = false;
            out.push_back(c);
        }
    }

    if (out.size() == buf.size() && std::memcmp(out.data(), buf.data(), out.size()) == 0) return false;
    FILE* out_f = fopen(path.c_str(), "wb");
    if (!out_f) return false;
    bool ok = write_all(out_f, out.data(), out.size());
    fclose(out_f);
    return ok;
}

bool strip_file_metadata(const std::string& path, const std::string& ext) {
    if (ext == ".jpg" || ext == ".jpeg") return strip_jpeg(path);
    if (ext == ".png") return strip_png(path);
    if (ext == ".gif") return strip_gif(path);
    if (ext == ".txt" || ext == ".md" || ext == ".csv" ||
        ext == ".xml" || ext == ".html" || ext == ".htm" ||
        ext == ".json" || ext == ".yaml" || ext == ".yml" ||
        ext == ".conf" || ext == ".cfg" || ext == ".ini" ||
        ext == ".sh" || ext == ".py" || ext == ".js" ||
        ext == ".css" || ext == ".cpp" || ext == ".c" ||
        ext == ".hpp" || ext == ".h" || ext == ".rs" ||
        ext == ".go" || ext == ".java" || ext == ".toml") {
        return strip_text(path);
    }
    return false;
}
