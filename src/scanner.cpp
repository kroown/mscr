#include "scanner.hpp"
#include "image.hpp"
#include "text.hpp"
#include <filesystem>
#include <fstream>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cctype>

namespace fs = std::filesystem;

static std::string get_owner(uint32_t uid) {
    auto* pw = getpwuid(uid);
    return pw ? pw->pw_name : std::to_string(uid);
}

static std::string get_group(uint32_t gid) {
    auto* gr = getgrgid(gid);
    return gr ? gr->gr_name : std::to_string(gid);
}

static uint64_t timespec_to_ns(const struct timespec& ts) {
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000 +
           static_cast<uint64_t>(ts.tv_nsec);
}

static ScannedFile stat_to_scanned(const fs::path& path) {
    struct stat st;
    ScannedFile sf;
    if (lstat(path.c_str(), &st) < 0) return sf;
    sf.path = path.string();
    sf.name = path.filename().string();
    sf.extension = path.extension().string();
    sf.size = static_cast<uint64_t>(st.st_size);
    sf.inode = static_cast<uint64_t>(st.st_ino);
    sf.permissions = st.st_mode & 07777;
    sf.uid = st.st_uid;
    sf.gid = st.st_gid;
    sf.owner_name = get_owner(sf.uid);
    sf.group_name = get_group(sf.gid);
    sf.mtime = timespec_to_ns(st.st_mtim);
    sf.atime = timespec_to_ns(st.st_atim);
    sf.ctime = timespec_to_ns(st.st_ctim);
    sf.btime = 0;
    sf.is_directory = S_ISDIR(st.st_mode);
    sf.is_symlink = S_ISLNK(st.st_mode);
    if (sf.is_symlink) {
        char buf[4096];
        ssize_t len = readlink(path.c_str(), buf, sizeof(buf) - 1);
        if (len >= 0) { buf[len] = 0; sf.symlink_target = buf; }
    }
    return sf;
}

std::vector<ScannedFile> scan_directory(const std::string& path) {
    std::vector<ScannedFile> files;
    fs::path root(path);
    if (!fs::exists(root)) return files;

    if (fs::is_regular_file(root) || fs::is_symlink(root)) {
        files.push_back(stat_to_scanned(root));
        return files;
    }

    if (!fs::is_directory(root)) return files;

    for (auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        auto sf = stat_to_scanned(entry.path());
        if (sf.path.empty()) continue;
        files.push_back(std::move(sf));
    }
    return files;
}

FileMetadata collect_metadata(const ScannedFile& file) {
    FileMetadata md;
    md.file = file;

    if (file.is_directory || file.size == 0) return md;

    std::ifstream in(file.path, std::ios::binary);
    if (!in) return md;

    std::vector<uint8_t> buf(std::min(file.size, uint64_t(1024 * 1024)));
    in.read(reinterpret_cast<char*>(buf.data()), buf.size());
    size_t read_bytes = in.gcount();
    buf.resize(read_bytes);

    std::string ext;
    for (auto& c : file.extension) ext.push_back(std::tolower(c));

    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
        ext == ".gif" || ext == ".bmp" || ext == ".webp") {
        md.image = parse_image_metadata(buf, ext);
    }

    md.text = analyze_text(buf);

    return md;
}
