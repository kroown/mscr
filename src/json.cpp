#include "json.hpp"
#include <sstream>
#include <iomanip>

static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::ostringstream hex;
                    hex << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
                    out += hex.str();
                } else {
                    out += c;
                }
        }
    }
    return out;
}

static void indent(std::ostringstream& os, int depth, bool pretty) {
    if (pretty) os << std::string(depth * 2, ' ');
}

static void open_brace(std::ostringstream& os, int depth, bool pretty) {
    os << "{";
    if (pretty) os << "\n";
}

static void close_brace(std::ostringstream& os, int depth, bool pretty, bool comma) {
    indent(os, depth, pretty);
    os << "}";
    if (comma) os << ",";
    if (pretty) os << "\n";
}

static void kv(std::ostringstream& os, const std::string& key, const std::string& val,
               int depth, bool pretty, bool comma) {
    indent(os, depth, pretty);
    os << "\"" << key << "\": \"" << json_escape(val) << "\"";
    if (comma) os << ",";
    if (pretty) os << "\n";
}

static void kv_int(std::ostringstream& os, const std::string& key, uint64_t val,
                   int depth, bool pretty, bool comma) {
    indent(os, depth, pretty);
    os << "\"" << key << "\": " << val;
    if (comma) os << ",";
    if (pretty) os << "\n";
}

static void kv_bool(std::ostringstream& os, const std::string& key, bool val,
                    int depth, bool pretty, bool comma) {
    indent(os, depth, pretty);
    os << "\"" << key << "\": " << (val ? "true" : "false");
    if (comma) os << ",";
    if (pretty) os << "\n";
}

static void kv_double(std::ostringstream& os, const std::string& key, double val,
                      int depth, bool pretty, bool comma) {
    indent(os, depth, pretty);
    os << "\"" << key << "\": " << std::fixed << std::setprecision(6) << val;
    if (comma) os << ",";
    if (pretty) os << "\n";
}

static std::string perms_to_str(uint32_t perms) {
    std::string s;
    s += (perms & 0400 ? 'r' : '-');
    s += (perms & 0200 ? 'w' : '-');
    s += (perms & 0100 ? 'x' : '-');
    s += (perms & 0040 ? 'r' : '-');
    s += (perms & 0020 ? 'w' : '-');
    s += (perms & 0010 ? 'x' : '-');
    s += (perms & 0004 ? 'r' : '-');
    s += (perms & 0002 ? 'w' : '-');
    s += (perms & 0001 ? 'x' : '-');
    return s;
}

static void serialize_file(std::ostringstream& os, const FileMetadata& md,
                           int depth, bool pretty, bool comma) {
    auto& f = md.file;
    open_brace(os, depth, pretty);

    kv(os, "path", f.path, depth + 1, pretty, true);
    kv(os, "name", f.name, depth + 1, pretty, true);
    kv(os, "extension", f.extension, depth + 1, pretty, true);
    kv_int(os, "size", f.size, depth + 1, pretty, true);
    kv_int(os, "inode", f.inode, depth + 1, pretty, true);
    kv(os, "permissions", perms_to_str(f.permissions), depth + 1, pretty, true);
    kv_int(os, "permissions_octal", f.permissions, depth + 1, pretty, true);
    kv(os, "owner", f.owner_name, depth + 1, pretty, true);
    kv(os, "group", f.group_name, depth + 1, pretty, true);
    kv_int(os, "uid", f.uid, depth + 1, pretty, true);
    kv_int(os, "gid", f.gid, depth + 1, pretty, true);
    kv_int(os, "mtime_ns", f.mtime, depth + 1, pretty, true);
    kv_int(os, "atime_ns", f.atime, depth + 1, pretty, true);
    kv_int(os, "ctime_ns", f.ctime, depth + 1, pretty, true);
    kv_bool(os, "is_directory", f.is_directory, depth + 1, pretty, true);
    kv_bool(os, "is_symlink", f.is_symlink, depth + 1, pretty, true);

    if (f.is_symlink) {
        kv(os, "symlink_target", f.symlink_target, depth + 1, pretty, true);
    }

    // image metadata
    if (md.image.is_image) {
        indent(os, depth + 1, pretty);
        os << "\"image\": ";
        open_brace(os, depth + 1, pretty);

        kv(os, "format", md.image.format, depth + 2, pretty, true);
        kv_int(os, "width", md.image.width, depth + 2, pretty, true);
        kv_int(os, "height", md.image.height, depth + 2, pretty, true);
        if (!md.image.camera_make.empty())
            kv(os, "camera_make", md.image.camera_make, depth + 2, pretty, true);
        if (!md.image.camera_model.empty())
            kv(os, "camera_model", md.image.camera_model, depth + 2, pretty, true);
        kv_int(os, "orientation", md.image.orientation, depth + 2, pretty, true);
        kv_bool(os, "has_gps", md.image.has_gps, depth + 2, pretty, true);
        if (md.image.has_gps) {
            kv_double(os, "gps_lat", md.image.gps_lat, depth + 2, pretty, true);
            kv_double(os, "gps_lon", md.image.gps_lon, depth + 2, pretty, false);
        } else {
            close_brace(os, depth + 2, pretty, true);
        }
        close_brace(os, depth + 1, pretty, true);
    }

    // text metadata
    if (md.text.is_text) {
        indent(os, depth + 1, pretty);
        os << "\"text\": ";
        open_brace(os, depth + 1, pretty);

        kv_int(os, "lines", md.text.line_count, depth + 2, pretty, true);
        kv_int(os, "words", md.text.word_count, depth + 2, pretty, true);
        kv_int(os, "characters", md.text.char_count, depth + 2, pretty, true);
        kv_int(os, "bytes", md.text.byte_count, depth + 2, pretty, true);
        kv_bool(os, "is_ascii", md.text.is_ascii, depth + 2, pretty, true);
        kv_bool(os, "is_utf8", md.text.is_utf8, depth + 2, pretty, false);

        close_brace(os, depth + 1, pretty, true);
    }

    close_brace(os, depth, pretty, comma);
}

std::string to_json(const std::vector<FileMetadata>& results, bool pretty) {
    std::ostringstream os;
    os << "[";
    if (pretty) os << "\n";

    for (size_t i = 0; i < results.size(); i++) {
        serialize_file(os, results[i], 1, pretty, i + 1 < results.size());
    }

    os << "]";
    if (pretty) os << "\n";
    return os.str();
}
