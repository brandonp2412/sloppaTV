#pragma once

#include "seerr_media.hpp"
#include "seerr_storage.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

struct SeerrDrivePickerRow {
    std::string name;
    std::string secondaryText;
    std::string statusText;
    int usedPercent = 0;
    bool hasCapacity = false;
    bool nearFull = false;
};

struct SeerrDrivePickerVisibleRow {
    SeerrDrivePickerRow row;
    bool focused = false;
};

struct SeerrDrivePickerViewModel {
    std::string subtitle;
    std::vector<SeerrDrivePickerVisibleRow> rows;
};

inline std::string seerrDrivePickerSubtitle(const std::optional<SeerrMediaItem>& pendingRequest) {
    return !pendingRequest || pendingRequest->name.empty() ? "Choose where Seerr should place this request"
                                                           : "Choose storage for " + pendingRequest->name;
}

inline int seerrDrivePickerFirstVisible(int selection, int count, int visibleRows = 5) {
    if (count <= 0 || visibleRows <= 0) return 0;
    return std::clamp(selection - 2, 0, std::max(0, count - visibleRows));
}

inline double seerrStorageBytesToGb(int64_t bytes) {
    return static_cast<double>(std::max<int64_t>(0, bytes)) / 1000000000.0;
}

inline std::string formatSeerrStorageBytes(int64_t bytes) {
    const double gb = seerrStorageBytesToGb(bytes);
    std::ostringstream value;
    if (gb >= 1000.0) {
        value << std::fixed << std::setprecision(gb >= 10000.0 ? 0 : 1) << (gb / 1000.0) << " TB";
    } else {
        value << std::fixed << std::setprecision(gb >= 100.0 ? 0 : 1) << gb << " GB";
    }
    return value.str();
}

inline SeerrDrivePickerRow seerrDrivePickerRow(const SeerrStorageTarget& target) {
    std::string normalizedPath = target.path;
    while (normalizedPath.size() > 1 && normalizedPath.back() == '/') normalizedPath.pop_back();

    std::string name = normalizedPath;
    const size_t slash = normalizedPath.find_last_of('/');
    if (slash != std::string::npos && slash + 1 < normalizedPath.size()) {
        const std::string leaf = normalizedPath.substr(slash + 1);
        std::string parent;
        if (slash > 0) {
            const size_t parentSlash = normalizedPath.find_last_of('/', slash - 1);
            const size_t parentStart = parentSlash == std::string::npos ? 0 : parentSlash + 1;
            parent = normalizedPath.substr(parentStart, slash - parentStart);
        }
        name = parent.empty() ? leaf : parent + " · " + leaf;
    }
    if (name.empty()) name = target.serviceName.empty() ? "Storage" : target.serviceName;
    if (!target.serviceName.empty() && name != target.serviceName) name += " · " + target.serviceName;

    const bool hasCapacity = target.totalSpace > 0;
    const int percent = hasCapacity ? std::clamp(target.usedPercent(), 0, 100) : 0;
    std::string secondaryText;
    if (hasCapacity) {
        const double usedGb = seerrStorageBytesToGb(target.totalSpace - target.freeSpace);
        const double totalGb = seerrStorageBytesToGb(target.totalSpace);
        std::ostringstream usage;
        usage << std::fixed << std::setprecision(totalGb >= 100.0 ? 0 : 1) << usedGb << " / " << totalGb << " GB";
        secondaryText = usage.str();
    } else {
        secondaryText = target.path;
    }

    const std::string statusText =
        hasCapacity
            ? std::to_string(percent) + "% full"
            : (target.freeSpace > 0 ? formatSeerrStorageBytes(target.freeSpace) + " free" : "Free space unknown");

    return {
        .name = std::move(name),
        .secondaryText = std::move(secondaryText),
        .statusText = statusText,
        .usedPercent = percent,
        .hasCapacity = hasCapacity,
        .nearFull = hasCapacity && percent >= 90,
    };
}

inline SeerrDrivePickerViewModel seerrDrivePickerViewModel(const std::optional<SeerrMediaItem>& pendingRequest,
                                                           const std::vector<SeerrStorageTarget>& driveChoices,
                                                           int selection, int visibleRows = 5) {
    SeerrDrivePickerViewModel model;
    model.subtitle = seerrDrivePickerSubtitle(pendingRequest);
    if (driveChoices.empty() || visibleRows <= 0) return model;

    const int first = seerrDrivePickerFirstVisible(selection, static_cast<int>(driveChoices.size()), visibleRows);
    const int end = std::min(first + visibleRows, static_cast<int>(driveChoices.size()));
    model.rows.reserve(static_cast<size_t>(end - first));
    for (int index = first; index < end; ++index) {
        model.rows.push_back({
            .row = seerrDrivePickerRow(driveChoices[static_cast<size_t>(index)]),
            .focused = index == selection,
        });
    }
    return model;
}
