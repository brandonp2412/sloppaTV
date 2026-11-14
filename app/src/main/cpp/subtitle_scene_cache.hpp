#pragma once

#include "subtitle_sync_correction_cache.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

// This stores source evidence, never an authorization to correct a new session.
// A caller must independently revalidate fresh dialogue before using a hit.
// Access is confined to the single subtitle worker.
class SubtitleSceneEvidenceCache {
public:
    void setFile(const std::string& file) {
        if (file_ == file) return;
        file_ = file;
        entries_.clear();
        if (file.empty()) return;
        std::error_code error;
        if (std::filesystem::file_size(file, error) > 256U * 1024U || error) return;
        try {
            std::ifstream input(file);
            const auto data = nlohmann::json::parse(input);
            if (data.at("version") != 4 || !data.at("entries").is_array() || data.at("entries").size() > 8) return;
            for (const auto& row : data.at("entries")) {
                SubtitleSyncContext context;
                context.itemId = row.at("item").get<std::string>();
                context.sourceId = row.at("source").get<std::string>();
                context.sourceFingerprint = row.at("fingerprint").get<std::string>();
                context.audioIndex = row.at("audio").get<int>();
                context.subtitleIndex = row.at("subtitle").get<int>();
                context.cueFingerprint = row.at("cues").get<uint64_t>();
                const auto account = row.at("account").get<std::string>();
                if (account.empty() || context.itemId.empty() || context.sourceId.empty() ||
                    context.sourceFingerprint.empty() || context.audioIndex < 0 || context.subtitleIndex < 0 ||
                    context.cueFingerprint == 0 || row.at("regions").size() > 256)
                    continue;
                std::vector<SubtitleTimelineRegion> regions;
                for (const auto& r : row.at("regions"))
                    regions.push_back({r.at("first").get<int>(), r.at("last").get<int>(), r.at("offset").get<int>(),
                                       r.at("scale").get<double>(), r.at("anchors").get<int>()});
                if (subtitleTimelineValid(regions)) entries_.push_back({account, context, std::move(regions)});
            }
        } catch (const nlohmann::json::exception&) {
            entries_.clear();
        }
    }

    std::vector<SubtitleTimelineRegion> find(const std::string& account, const SubtitleSyncContext& context) const {
        if (account.empty() || !context.trustworthy()) return {};
        const auto key = sourceContext(context);
        for (const auto& entry : entries_)
            if (entry.account == account && entry.context == key) return entry.regions;
        return {};
    }

    void store(const std::string& account, const SubtitleSyncContext& context,
               const std::vector<SubtitleTimelineRegion>& regions) {
        if (account.empty() || !context.trustworthy() || !subtitleTimelineValid(regions)) return;
        const auto key = sourceContext(context);
        std::erase_if(entries_, [&](const auto& entry) { return entry.account == account && entry.context == key; });
        if (entries_.size() >= 8) entries_.erase(entries_.begin());
        entries_.push_back({account, key, regions});
        save();
    }

private:
    void save() const {
        if (file_.empty()) return;
        auto rows = nlohmann::json::array();
        for (const auto& entry : entries_) {
            auto regions = nlohmann::json::array();
            for (const auto& r : entry.regions)
                regions.push_back({{"first", r.firstSubtitleMs},
                                   {"last", r.lastSubtitleMs},
                                   {"offset", r.offsetMs},
                                   {"scale", r.scale},
                                   {"anchors", r.anchors}});
            rows.push_back({{"account", entry.account},
                            {"item", entry.context.itemId},
                            {"source", entry.context.sourceId},
                            {"fingerprint", entry.context.sourceFingerprint},
                            {"audio", entry.context.audioIndex},
                            {"subtitle", entry.context.subtitleIndex},
                            {"cues", entry.context.cueFingerprint},
                            {"regions", std::move(regions)}});
        }
        const std::string temporary = file_ + ".tmp";
        {
            std::ofstream output(temporary, std::ios::trunc);
            output << nlohmann::json{{"version", 4}, {"entries", std::move(rows)}}.dump();
            if (!output) return;
        }
        std::error_code error;
        std::filesystem::rename(temporary, file_, error);
        if (error) std::filesystem::remove(temporary, error);
    }

    static SubtitleSyncContext sourceContext(SubtitleSyncContext context) {
        context.generation = 0;
        context.playSessionId.clear();
        context.mediaUrl.clear();
        context.timelineBaseMs = 0;
        context.timelineOriginMs.reset();
        return context;
    }

    struct Entry {
        std::string account;
        SubtitleSyncContext context;
        std::vector<SubtitleTimelineRegion> regions;
    };

    std::vector<Entry> entries_;
    std::string file_;
};
