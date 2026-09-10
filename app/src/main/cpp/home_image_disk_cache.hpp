#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

class HomeImageDiskCache {
public:
    void setDataPath(std::string dataPath) {
        std::scoped_lock lock(mutex_);
        dataPath_ = std::move(dataPath);
        usageKnown_ = false;
    }

    std::optional<std::string> read(const std::string& key) {
        std::scoped_lock lock(mutex_);
        const auto path = pathForKey(key);
        if (path.empty()) return std::nullopt;
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) return std::nullopt;
        const std::streamsize size = input.tellg();
        if (size <= 0) return std::nullopt;
        std::string bytes(static_cast<size_t>(size), '\0');
        input.seekg(0, std::ios::beg);
        if (!input.read(bytes.data(), size)) return std::nullopt;
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
        ensureUsageLocked();
        const bool existed = std::filesystem::is_regular_file(path, ec);
        const uintmax_t previousSize = existed ? std::filesystem::file_size(path, ec) : 0;
        if (ec) ec.clear();

        const std::filesystem::path temporaryPath = path.string() + ".tmp";
        std::remove(temporaryPath.c_str());
        {
            std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
            if (!output) return;
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            output.flush();
            if (!output) {
                output.close();
                std::remove(temporaryPath.c_str());
                return;
            }
        }
        if (std::rename(temporaryPath.c_str(), path.c_str()) != 0) {
            std::remove(temporaryPath.c_str());
            return;
        }

        if (usageKnown_) {
            totalBytes_ = previousSize > totalBytes_ ? bytes.size() : totalBytes_ - previousSize + bytes.size();
            if (!existed) ++fileCount_;
        }
        trimLocked();
    }

    void erase(const std::string& key) {
        std::scoped_lock lock(mutex_);
        const auto path = pathForKey(key);
        if (path.empty()) return;
        std::error_code ec;
        const uintmax_t size = usageKnown_ && std::filesystem::is_regular_file(path, ec)
            ? std::filesystem::file_size(path, ec)
            : 0;
        ec.clear();
        if (std::filesystem::remove(path, ec) && usageKnown_) {
            totalBytes_ = size > totalBytes_ ? 0 : totalBytes_ - size;
            if (fileCount_ > 0) --fileCount_;
        }
    }

private:
    static constexpr uintmax_t kMaxDiskBytes = 48ULL * 1024ULL * 1024ULL;
    static constexpr size_t kMaxDiskFiles = 256;
    std::filesystem::path pathForKey(const std::string& key) const {
        if (dataPath_.empty()) return {};
        uint64_t hash = 1469598103934665603ULL;
        for (const unsigned char value : key) {
            hash ^= value;
            hash *= 1099511628211ULL;
        }
        std::array<char, 16> hashText{};
        const auto [end, error] = std::to_chars(hashText.data(), hashText.data() + hashText.size(), hash, 16);
        if (error != std::errc{}) return {};
        std::string name(hashText.data(), end);
        name += ".img";
        return std::filesystem::path(dataPath_) / "home-image-cache" / name;
    }

    void ensureUsageLocked() {
        if (usageKnown_ || dataPath_.empty()) return;
        namespace fs = std::filesystem;
        const fs::path directory = fs::path(dataPath_) / "home-image-cache";
        std::error_code ec;
        totalBytes_ = 0;
        fileCount_ = 0;
        if (fs::exists(directory, ec)) {
            for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
                if (!it->is_regular_file(ec)) continue;
                const uintmax_t size = it->file_size(ec);
                if (ec) {
                    ec.clear();
                    continue;
                }
                totalBytes_ += size;
                ++fileCount_;
            }
        }
        usageKnown_ = !ec;
    }

    void trimLocked() {
        if (!usageKnown_ || (totalBytes_ <= kMaxDiskBytes && fileCount_ <= kMaxDiskFiles)) return;
        namespace fs = std::filesystem;
        const fs::path directory = fs::path(dataPath_) / "home-image-cache";
        std::error_code ec;

        struct CachedFile {
            fs::path path;
            uintmax_t size = 0;
            fs::file_time_type modified{};
        };
        std::vector<CachedFile> files;
        uintmax_t scannedBytes = 0;
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
            scannedBytes += file.size;
            files.push_back(std::move(file));
        }
        if (ec) {
            usageKnown_ = false;
            return;
        }
        totalBytes_ = scannedBytes;
        fileCount_ = files.size();
        if (totalBytes_ <= kMaxDiskBytes && fileCount_ <= kMaxDiskFiles) return;
        std::sort(files.begin(), files.end(), [](const CachedFile& left, const CachedFile& right) {
            return left.modified < right.modified;
        });
        size_t index = 0;
        while (index < files.size() && (totalBytes_ > kMaxDiskBytes || fileCount_ > kMaxDiskFiles)) {
            if (fs::remove(files[index].path, ec)) {
                totalBytes_ = files[index].size > totalBytes_ ? 0 : totalBytes_ - files[index].size;
                if (fileCount_ > 0) --fileCount_;
            } else {
                ec.clear();
            }
            ++index;
        }
    }

    std::mutex mutex_;
    std::string dataPath_;
    bool usageKnown_ = false;
    uintmax_t totalBytes_ = 0;
    size_t fileCount_ = 0;
};
