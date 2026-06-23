#include "image.hpp"
#include <cstring>
#include <cmath>

static uint16_t read_u16_be(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) << 8 | p[1];
}

static uint32_t read_u32_be(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) << 24 |
           static_cast<uint32_t>(p[1]) << 16 |
           static_cast<uint32_t>(p[2]) << 8 |
           p[3];
}

static uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[3]) << 24 |
           static_cast<uint32_t>(p[2]) << 16 |
           static_cast<uint32_t>(p[1]) << 8 |
           p[0];
}

static uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[1]) << 8 | p[0];
}

static void parse_jpeg_exif(const uint8_t* data, size_t size, ImageMeta& meta) {
    size_t pos = 2;
    while (pos + 4 < size) {
        if (data[pos] != 0xFF) break;
        uint8_t marker = data[pos + 1];
        if (marker == 0xD8 || marker == 0xD9 || marker == 0x00) { pos++; continue; }
        uint16_t seg_len = read_u16_be(data + pos + 2);
        if (seg_len < 2) break;
        size_t seg_start = pos + 2;
        size_t seg_end = seg_start + seg_len;
        if (seg_end > size) break;

        if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
            if (seg_start + 7 <= seg_end) {
                meta.height = read_u16_be(data + seg_start + 3);
                meta.width = read_u16_be(data + seg_start + 5);
            }
        } else if (marker == 0xE1 && seg_len >= 8) {
            if (std::memcmp(data + seg_start, "Exif\0\0", 6) == 0) {
                size_t tiff = seg_start + 6;
                if (tiff + 8 > seg_end) { pos = seg_end; continue; }

                bool le = (data[tiff] == 'I' && data[tiff+1] == 'I');
                bool be = (data[tiff] == 'M' && data[tiff+1] == 'M');
                if (!le && !be) { pos = seg_end; continue; }
                if (read_u16_be(data + tiff + 2) != 0x002A) { pos = seg_end; continue; }

                auto r16 = le ? read_u16_le : read_u16_be;
                auto r32 = le ? read_u32_le : read_u32_be;

                uint32_t ifd0_off = r32(data + tiff + 4);
                if (ifd0_off + 2 > seg_end - tiff) { pos = seg_end; continue; }
                size_t ifd0 = tiff + ifd0_off;
                uint16_t entries = r16(data + ifd0);
                ifd0 += 2;

                for (uint16_t e = 0; e < entries && ifd0 + 12 <= seg_end; e++) {
                    uint16_t tag = r16(data + ifd0);
                    uint16_t type = r16(data + ifd0 + 2);
                    ifd0 += 12;

                    auto read_str = [&](uint32_t off, int max) -> std::string {
                        if (off + 2 > seg_end - tiff) return {};
                        max = std::min(max, int(seg_end - tiff - off));
                        return std::string(reinterpret_cast<const char*>(data + tiff + off), max);
                    };

                    if (tag == 0x010F && type == 2) meta.camera_make = read_str(r32(data + ifd0 - 4), 64);
                    if (tag == 0x0110 && type == 2) meta.camera_model = read_str(r32(data + ifd0 - 4), 64);
                    if (tag == 0x0112 && type == 3) meta.orientation = r16(data + ifd0 - 4);
                    if (tag == 0x0132 && type == 2) { /* date/time string */ }
                    if (tag == 0x9003 && type == 2) { /* date digitized */ }
                    if (tag == 0x9004 && type == 2) { /* date original */ }

                    // GPS IFD pointer
                    if (tag == 0x8825 && type == 4) {
                        uint32_t gps_off = r32(data + ifd0 - 4);
                        size_t gps_ifd = tiff + gps_off;
                        if (gps_ifd + 2 <= seg_end) {
                            uint16_t gps_entries = r16(data + gps_ifd);
                            size_t gp = gps_ifd + 2;
                            for (uint16_t ge = 0; ge < gps_entries && gp + 12 <= seg_end; ge++) {
                                uint16_t gt = r16(data + gp);
                                gp += 12;
                                if (gt == 0x0002 && type == 3) {
                                    meta.has_gps = true;
                                    // GPS lat ref
                                }
                                if (gt == 0x0001 && type == 3) {
                                    // GPS lat ref
                                }
                                if (gt == 0x0004 && type == 3) {
                                    // GPS lon ref
                                }
                            }
                        }
                    }
                }
            }
        }
        pos = seg_end;
    }
}

static void parse_png(const uint8_t* data, size_t size, ImageMeta& meta) {
    if (size < 24) return;
    if (std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) != 0) return;
    meta.format = "png";
    uint32_t len = read_u32_be(data + 8);
    if (len != 13 || std::memcmp(data + 12, "IHDR", 4) != 0) return;
    if (16 + 8 > size) return;
    meta.width = static_cast<int>(read_u32_be(data + 16));
    meta.height = static_cast<int>(read_u32_be(data + 20));
}

static void parse_gif(const uint8_t* data, size_t size, ImageMeta& meta) {
    if (size < 10) return;
    if (std::memcmp(data, "GIF87a", 6) == 0 || std::memcmp(data, "GIF89a", 6) == 0) {
        meta.format = "gif";
        meta.width = read_u16_le(data + 6);
        meta.height = read_u16_le(data + 8);
    }
}

static void parse_bmp(const uint8_t* data, size_t size, ImageMeta& meta) {
    if (size < 26) return;
    if (std::memcmp(data, "BM", 2) != 0) return;
    meta.format = "bmp";
    uint32_t data_offset = read_u32_le(data + 10);
    if (data_offset < 14 || data_offset + 8 > size) return;
    int hdr_size = static_cast<int>(read_u32_le(data + 14));
    if (hdr_size >= 40) {
        meta.width = std::abs(static_cast<int>(read_u32_le(data + 18)));
        meta.height = std::abs(static_cast<int>(read_u32_le(data + 22)));
    }
}

static void parse_webp(const uint8_t* data, size_t size, ImageMeta& meta) {
    if (size < 30) return;
    if (std::memcmp(data, "RIFF", 4) != 0) return;
    if (std::memcmp(data + 8, "WEBP", 4) != 0) return;
    meta.format = "webp";
    uint32_t chunk_size = read_u32_be(data + 4);
    if (chunk_size + 8 > size) return;

    size_t pos = 12;
    while (pos + 8 <= size) {
        uint32_t clen = read_u32_be(data + pos + 4);
        if (std::memcmp(data + pos, "VP8 ", 4) == 0) {
            if (clen >= 10 && pos + 16 <= size) {
                meta.width = read_u16_le(data + pos + 12) & 0x3FFF;
                meta.height = read_u16_le(data + pos + 14) & 0x3FFF;
            }
            break;
        }
        if (std::memcmp(data + pos, "VP8L", 4) == 0) {
            if (clen >= 5 && pos + 13 <= size) {
                uint32_t bits = read_u32_le(data + pos + 8);
                meta.width = (bits & 0x3FFF) + 1;
                meta.height = ((bits >> 14) & 0x3FFF) + 1;
            }
            break;
        }
        if (std::memcmp(data + pos, "VP8X", 4) == 0) {
            if (clen >= 10 && pos + 18 <= size) {
                uint32_t w = read_u32_le(data + pos + 12);
                uint32_t h = read_u32_le(data + pos + 16);
                meta.width = (w & 0x00FFFFFF) + 1;
                meta.height = (h & 0x00FFFFFF) + 1;
            }
            break;
        }
        pos += 8 + clen + (clen & 1);
        if (pos >= size) break;
    }
}

ImageMeta parse_image_metadata(const std::vector<uint8_t>& data, const std::string& ext) {
    ImageMeta meta;
    meta.is_image = true;

    if (ext == ".jpg" || ext == ".jpeg") {
        meta.format = "jpeg";
        parse_jpeg_exif(data.data(), data.size(), meta);
    } else if (ext == ".png") {
        parse_png(data.data(), data.size(), meta);
    } else if (ext == ".gif") {
        parse_gif(data.data(), data.size(), meta);
    } else if (ext == ".bmp") {
        parse_bmp(data.data(), data.size(), meta);
    } else if (ext == ".webp") {
        parse_webp(data.data(), data.size(), meta);
    }

    return meta;
}
