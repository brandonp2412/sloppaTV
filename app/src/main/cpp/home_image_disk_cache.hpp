#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

class HomeImageDiskCache {
public:
    void setDataPath(std::string dataPath) {
        std::scoped_lock lock(mutex_);
        dataPath_ = std::move(dataPath);
    }

    std::optional<std::string> read(const std::string& key) {
        std::scoped_lock lock(mutex_);
        const auto path = pathForKey(key);
        if (path.empty()) return std::nullopt;
        std::ifstream input(path, std::ios::binary);
        if (!input) return std::nullopt;
        std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (bytes.empty()) return std::nullopt;
        std::error_code ec;
        std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(), ec);
        return bytes;
    }

    void write(const std::string& key, const std::string& bytes) {
        if (bytes.empty()) return;
        std::scoped_lock lock(mutex_);
        const auto path = pathForKey(key);
        if (path.empty()) return;
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return;
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) return;
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        trimLocked();
    }

    void erase(const std::string& key) {
        std::scoped_lock lock(mutex_);
        const auto path = pathForKey(key);
        if (path.empty()) return;
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

private:
    std::filesystem::path pathForKey(const std::string& key) const {
        if (dataPath_.empty()) return {};
        uint64_t hash = 1469598103934665603ULL;
        for (const unsigned char value : key) {
            hash ^= value;
            hash *= 1099511628211ULL;
        }
        std::ostringstream name;
        name << std::hex << hash << ".img";
        return std::filesystem::path(dataPath_) / "home-image-cache" / name.str();
    }

    void trimLocked() {
        if (dataPath_.empty()) return;
        namespace fs = std::filesystem;
        const fs::path directory = fs::path(dataPath_) / "home-image-cache";
        std::error_code ec;
        if (!fs::exists(directory, ec)) return;

        struct CachedFile {
            fs::path path;
            uintmax_t size = 0;
            fs::file_time_type modified{};
        };
        std::vector<CachedFile> files;
        uintmax_t totalBytes = 0;
        for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            CachedFile file;
            file.path = it->path();
            file.size = it->file_size(ec);
            if (ec) {
                ec.clear();
                continue;
            }
            file.modified = it->last_write_time(ec);
            if (ec) {
                ec.clear();
                continue;
            }
            totalBytes += file.size;
            files.push_back(std::move(file));
        }

        constexpr uintmax_t kMaxDiskBytes = 48ULL * 1024ULL * 1024ULL;
        constexpr size_t kMaxDiskFiles = 256;
        if (totalBytes <= kMaxDiskBytes && files.size() <= kMaxDiskFiles) return;
        std::sort(files.begin(), files.end(), [](const CachedFile& left, const CachedFile& right) {
            return left.modified < right.modified;
        });
        size_t index = 0;
        while (index < files.size() && (totalBytes > kMaxDiskBytes || files.size() - index > kMaxDiskFiles)) {
            fs::remove(files[index].path, ec);
            if (!ec) totalBytes = files[index].size > totalBytes ? 0 : totalBytes - files[index].size;
            else ec.clear();
            ++index;
        }
    }

    std::mutex mutex_;
    std::string dataPath_;
};
