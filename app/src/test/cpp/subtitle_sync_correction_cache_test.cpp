#include "subtitle_sync_correction_cache.hpp"
#include "subtitle_scene_cache.hpp"
#include <cassert>
#include <chrono>

int main() {
    const std::vector<SubtitleCue> cues{{1000, 2000, "first phrase"}, {3000, 4000, "next phrase"}};
    auto advertised = cues;
    advertised.push_back(
        {6000, 7000, "Support us and become VIP member\nto remove all ads from www.OpenSubtitles.org"});
    auto rotatedAds = advertised;
    rotatedAds.back().text = "Watch Online Movies and Series for FREE\nwww.osdb.link/lm";
    assert(subtitleCueFingerprint(advertised) != subtitleCueFingerprint(rotatedAds));
    assert(subtitleCueFingerprint(advertised, true) == subtitleCueFingerprint(rotatedAds, true));
    rotatedAds.front().text = "different spoken dialogue";
    assert(subtitleCueFingerprint(advertised, true) != subtitleCueFingerprint(rotatedAds, true));
    rotatedAds = advertised;
    ++rotatedAds.back().startMs;
    assert(subtitleCueFingerprint(advertised, true) != subtitleCueFingerprint(rotatedAds, true));
    const auto hash = subtitleCueFingerprint(cues);
    assert(hash && hash == subtitleCueFingerprint(cues));
    auto changed = cues;
    changed[0].startMs += 10;
    assert(subtitleCueFingerprint(changed) != hash);
    changed = cues;
    changed[0].text += " changed";
    assert(subtitleCueFingerprint(changed) != hash);
    assert(subtitleCueFingerprint({}) == 0);
    SubtitleSyncContext context{1,      "item", "source", "release", "session", "https://fixture/stream.m3u8",
                                385000, 4,      6,        hash,      381405};
    SubtitleSyncCorrectionCache cache;
    assert(!cache.find(context));
    cache.store(context, {true, 5212, 1.0, 1.0});
    assert(cache.find(context)->offsetMs == 5212);
    auto different = context;
    ++different.generation;
    assert(!cache.find(different));
    different = context;
    different.itemId = "other";
    assert(!cache.find(different));
    different = context;
    different.sourceId = "other";
    assert(!cache.find(different));
    different = context;
    different.sourceFingerprint = "other";
    assert(!cache.find(different));
    different = context;
    different.playSessionId = "other";
    assert(!cache.find(different));
    different = context;
    different.mediaUrl += "?restart=1";
    assert(!cache.find(different));
    different = context;
    different.timelineBaseMs += 1000;
    assert(!cache.find(different));
    different = context;
    ++different.audioIndex;
    assert(!cache.find(different));
    different = context;
    ++different.subtitleIndex;
    assert(!cache.find(different));
    different = context;
    ++different.cueFingerprint;
    assert(!cache.find(different));
    different = context;
    different.timelineOriginMs = 380000;
    assert(!cache.find(different));
    different = context;
    different.timelineOriginMs.reset();
    assert(!cache.find(different));
    cache.store(context, {false, 9999, 0.0, 1.0});
    assert(cache.find(context)->offsetMs == 5212);
    different = context;
    different.sourceFingerprint.clear();
    cache.store(different, {true, 9999, 1.0, 1.0});
    assert(!cache.find(different));
    assert(cache.find(context)->offsetMs == 5212);
    SubtitleSyncDecision scene{true, 0, 1.0, 1.0, {{1000, 60000, 3300, 1.0, 8}}};
    cache.store(context, scene);
    assert(cache.find(context)->regions.size() == 1);
    different = context;
    different.sourceFingerprint = "another-release";
    assert(!cache.find(different));
    scene.regions.front().scale = std::numeric_limits<double>::quiet_NaN();
    cache.store(context, scene);
    assert(cache.find(context)->regions.front().scale == 1.0);
    SubtitleSceneEvidenceCache evidence;
    const std::vector<SubtitleTimelineRegion> sourceRegions{{1000, 60000, 3300, 1.0, 8}};
    evidence.store("server/account", context, sourceRegions);
    auto restarted = context;
    ++restarted.generation;
    restarted.playSessionId = "restarted-session";
    restarted.mediaUrl = "https://fixture/restarted.m3u8";
    restarted.timelineBaseMs = 900000;
    restarted.timelineOriginMs = 899000;
    assert(evidence.find("server/account", restarted).size() == 1); // evidence only, requires revalidation
    assert(evidence.find("another-server/account", restarted).empty());
    restarted.sourceFingerprint = "different-release";
    assert(evidence.find("server/account", restarted).empty());
    restarted = context;
    ++restarted.subtitleIndex;
    assert(evidence.find("server/account", restarted).empty());
    restarted = context;
    ++restarted.audioIndex;
    assert(evidence.find("server/account", restarted).empty());
    restarted = context;
    ++restarted.cueFingerprint;
    assert(evidence.find("server/account", restarted).empty());
    const auto file = std::filesystem::temp_directory_path() /
                      ("sloppa-scene-evidence-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    evidence.setFile(file.string());
    evidence.store("server/account", context, sourceRegions);
    SubtitleSceneEvidenceCache loaded;
    loaded.setFile(file.string());
    assert(loaded.find("server/account", context).size() == 1);
    {
        std::ofstream corrupt(file);
        corrupt << "{broken";
    }
    SubtitleSceneEvidenceCache broken;
    broken.setFile(file.string());
    assert(broken.find("server/account", context).empty());
    std::filesystem::remove(file);
}
