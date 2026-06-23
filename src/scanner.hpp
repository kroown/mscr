#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct ScannedFile {
    std::string path;
    std::string name;
    std::string extension;
    uint64_t size;
    uint64_t inode;
    uint32_t permissions;  // octal
    uint32_t uid;
    uint32_t gid;
    std::string owner_name;
    std::string group_name;
    uint64_t mtime;  // unix ns
    uint64_t atime;
    uint64_t ctime;
    uint64_t btime;   // birth time, 0 if unavailable
    bool is_directory;
    bool is_symlink;
    std::string symlink_target;
};

struct ImageMeta {
    bool is_image = false;
    int width = 0;
    int height = 0;
    std::string format;  // jpeg, png, gif, bmp
    std::string camera_make;
    std::string camera_model;
    uint64_t date_taken;  // unix timestamp, 0 if unknown
    double gps_lat = 0;
    double gps_lon = 0;
    bool has_gps = false;
    int orientation = 0;
};

struct TextMeta {
    bool is_text = false;
    uint64_t line_count = 0;
    uint64_t word_count = 0;
    uint64_t char_count = 0;
    uint64_t byte_count = 0;
    bool is_utf8 = false;
    bool is_ascii = false;
};

struct FileMetadata {
    ScannedFile file;
    ImageMeta image;
    TextMeta text;
};

std::vector<ScannedFile> scan_directory(const std::string& path);
FileMetadata collect_metadata(const ScannedFile& file);
