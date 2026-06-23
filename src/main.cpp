#include "scanner.hpp"
#include "json.hpp"
#include "strip.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <cctype>

static void print_usage() {
    std::cout << "mscr v1.0 - metadata scraper\n"
              << "usage: mscr [options] <path...>\n"
              << "\n"
              << "options:\n"
              << "  --strip            strip metadata from files in-place\n"
              << "  --pretty           pretty-print json\n"
              << "  -v, --verbose      verbose output to stderr\n"
              << "  -t, --threads <n>  worker threads\n"
              << "  --help             show this help\n";
}

struct Options {
    std::vector<std::string> paths;
    bool pretty = false;
    bool verbose = false;
    bool strip = false;
    int threads = 0;
};

static std::string ext_lower(const std::string& s) {
    std::string out;
    for (auto c : s) out.push_back(std::tolower(c));
    return out;
}

int main(int argc, char* argv[]) {
    Options opts;
    opts.threads = static_cast<int>(std::thread::hardware_concurrency());

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { print_usage(); return 0; }
        else if (arg == "--pretty") opts.pretty = true;
        else if (arg == "--strip") opts.strip = true;
        else if (arg == "-v" || arg == "--verbose") opts.verbose = true;
        else if (arg == "-t" || arg == "--threads") {
            if (++i >= argc) { std::cerr << "mscr: --threads needs a number\n"; return 1; }
            opts.threads = std::max(1, std::atoi(argv[i]));
        } else {
            opts.paths.push_back(arg);
        }
    }

    if (opts.paths.empty()) {
        print_usage();
        return 1;
    }

    // strip mode: process and exit
    if (opts.strip) {
        std::vector<ScannedFile> all_files;
        for (auto& p : opts.paths) {
            auto files = scan_directory(p);
            all_files.insert(all_files.end(), std::make_move_iterator(files.begin()),
                             std::make_move_iterator(files.end()));
        }

        if (opts.verbose)
            std::cerr << "mscr: stripping " << all_files.size() << " files\n";

        int stripped = 0;
        for (auto& f : all_files) {
            if (f.is_directory) continue;
            auto ext = ext_lower(f.extension);
            if (strip_file_metadata(f.path, ext)) {
                stripped++;
                if (opts.verbose)
                    std::cerr << "  stripped: " << f.path << "\n";
            }
        }

        if (opts.verbose)
            std::cerr << "mscr: stripped " << stripped << "/" << all_files.size() << " files\n";
        return 0;
    }

    // scan mode
    std::vector<ScannedFile> all_files;
    for (auto& p : opts.paths) {
        auto files = scan_directory(p);
        all_files.insert(all_files.end(), std::make_move_iterator(files.begin()),
                         std::make_move_iterator(files.end()));
    }

    if (opts.verbose)
        std::cerr << "mscr: scanning " << all_files.size() << " files\n";

    std::vector<FileMetadata> results(all_files.size());
    std::mutex mtx;
    size_t next = 0;

    auto worker = [&]() {
        while (true) {
            size_t idx;
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (next >= all_files.size()) return;
                idx = next++;
            }
            results[idx] = collect_metadata(all_files[idx]);
            if (opts.verbose && (idx % 100 == 0))
                std::cerr << "mscr: " << idx << "/" << all_files.size() << "\r";
        }
    };

    std::vector<std::thread> threads;
    int nthreads = std::min(opts.threads, static_cast<int>(all_files.size()));
    nthreads = std::max(nthreads, 1);
    for (int i = 0; i < nthreads; i++)
        threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    std::cout << to_json(results, opts.pretty);

    return 0;
}
