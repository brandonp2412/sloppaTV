#include "subtitle_sync_correction_cache.hpp"
#include <cassert>

int main() {
    const std::vector<SubtitleCue> cues{{1000, 2000, "first phrase"}, {3000, 4000, "next phrase"}};
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
}
