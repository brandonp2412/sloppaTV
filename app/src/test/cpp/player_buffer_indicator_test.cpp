#include "player_buffer_indicator.hpp"

#include <cassert>
#include <chrono>

int main() {
    PlayerBufferIndicator b;
    const auto t = PlayerBufferIndicator::Clock::now();
    b.observe(true, false, false, false, false, 0, 0, true, true, t);
    assert(b.phase() == PlayerBufferPhase::Resolving);
    assert(b.statusText().find("Remux") != std::string::npos);
    b.observe(true, false, false, false, false, 0, 0, true, false, t);
    assert(b.statusText(t) == "Resolving Remux stream");
    b.observe(false, true, false, false, false, 0, 0, true, true, t);
    assert(b.phase() == PlayerBufferPhase::Connecting);
    assert(b.statusText(t + std::chrono::seconds(14)).find("Waiting") != std::string::npos);
    b.observe(false, false, true, false, false, 3000, 90000, true, true, t + std::chrono::seconds(14));
    assert(b.phase() == PlayerBufferPhase::Playing);
    b.observe(false, false, true, false, true, 3000, 0, true, true, t + std::chrono::seconds(18));
    assert(b.phase() == PlayerBufferPhase::Buffering && b.interrupted());
    b.observe(false, false, true, false, true, 3000, 0, true, true, t + std::chrono::seconds(32));
    assert(b.phase() == PlayerBufferPhase::Stalled);
    b.observe(false, false, true, false, false, 4000, 65000, true, true, t + std::chrono::seconds(33));
    assert(b.phase() == PlayerBufferPhase::Playing && !b.interrupted());
    b.observe(false, false, false, true, false, 4000, 65000, true, true, t + std::chrono::seconds(40));
    assert(b.phase() == PlayerBufferPhase::Paused && !b.interrupted());
    RemuxPrefetchStatus progress;
    progress.currentCachedBytes = 500;
    progress.currentTotalBytes = 1000;
    progress.nextCachedBytes = 200;
    progress.nextTotalBytes = 800;
    progress.livePeers = 2;
    progress.downloadMiBps = 1.6;
    b.setRemotePrefetch(progress, t + std::chrono::seconds(41));
    assert(b.remotePrefetchText(t + std::chrono::seconds(42)).find("current 50%") != std::string::npos);
    assert(b.remotePrefetchText(t + std::chrono::seconds(42)).find("next 25%") != std::string::npos);
    assert(b.remotePrefetchText(t + std::chrono::seconds(65)).empty());
    // Peers downloading other pieces must not disguise a stalled startup
    // range as useful buffering; the label follows selected readable bytes.
    PlayerBufferIndicator sourceProgress;
    sourceProgress.observe(false, true, false, false, false, 0, 0, true, false, t);
    RemuxPrefetchStatus source;
    source.livePeers = 4;
    source.downloadMiBps = 8.0;
    source.startupRequiredBytes = 1000;
    sourceProgress.setRemotePrefetch(source, t + std::chrono::seconds(1));
    assert(sourceProgress.statusText(t + std::chrono::seconds(15)).find("Waiting for Remux stream data") !=
           std::string::npos);
    source.startupReadableBytes = 100;
    sourceProgress.setRemotePrefetch(source, t + std::chrono::seconds(16));
    assert(sourceProgress.statusText(t + std::chrono::seconds(16)).find("Buffering—downloading slowly") !=
           std::string::npos);
    sourceProgress.setRemotePrefetch(source, t + std::chrono::seconds(37));
    assert(sourceProgress.statusText(t + std::chrono::seconds(37)).find("Waiting for Remux stream data") !=
           std::string::npos);

    // A 60 Hz render loop advances by less than 125 ms per observation.
    // It must stay Playing instead of falsely reporting a stall after 15 s.
    PlayerBufferIndicator smooth;
    for (int ms = 0; ms <= 20000; ms += 16) {
        smooth.observe(false, false, true, false, false, ms, 30000, true, true, t + std::chrono::milliseconds(ms));
        assert(smooth.phase() == PlayerBufferPhase::Playing);
    }
    smooth.observe(false, false, true, false, false, 20000, 0, true, true, t + std::chrono::seconds(24));
    assert(smooth.phase() == PlayerBufferPhase::Buffering);
    smooth.observe(false, false, true, false, false, 20000, 0, true, true, t + std::chrono::seconds(36));
    assert(smooth.phase() == PlayerBufferPhase::Stalled);

    // Torrent totals alone are not a startup ETA. Only consecutive readable
    // bytes from Remux's actual startup-range check can produce one.
    PlayerBufferIndicator startup;
    startup.observe(false, true, false, false, false, 0, 0, true, false, t);
    RemuxPrefetchStatus measure;
    measure.currentTotalBytes = 100000000;
    measure.currentCachedBytes = 80000000;
    measure.downloadMiBps = 4.0;
    measure.livePeers = 8;
    startup.setRemotePrefetch(measure, t + std::chrono::seconds(1));
    assert(startup.statusText(t + std::chrono::seconds(1)).find("remaining") == std::string::npos);
    measure.startupRequiredBytes = 10000000;
    measure.startupReadableBytes = 1000000;
    startup.setRemotePrefetch(measure, t + std::chrono::seconds(3));
    measure.startupReadableBytes = 2000000;
    startup.setRemotePrefetch(measure, t + std::chrono::seconds(5));
    assert(startup.statusText(t + std::chrono::seconds(5)).find("remaining") == std::string::npos);
    measure.startupReadableBytes = 3000000;
    startup.setRemotePrefetch(measure, t + std::chrono::seconds(7));
    assert(startup.statusText(t + std::chrono::seconds(7)).find("~15s remaining") != std::string::npos);
    assert(startup.startupProgress(t + std::chrono::seconds(7)).has_value());
    assert(*startup.startupProgress(t + std::chrono::seconds(7)) == 0.3f);
    assert(!startup.startupProgress(t + std::chrono::seconds(16)).has_value());
    assert(startup.statusText(t + std::chrono::seconds(16)).find("remaining") == std::string::npos);
    measure.startupReady = true;
    startup.setRemotePrefetch(measure, t + std::chrono::seconds(17));
    assert(startup.statusText(t + std::chrono::seconds(17)).find("remaining") == std::string::npos);
    startup.clearRemotePrefetch();
    assert(startup.statusText(t + std::chrono::seconds(18)).find("remaining") == std::string::npos);

    // Successful startups teach the resolve and player-start phases. A cold
    // start without sufficient history still shows a plain stage message.
    PlayerBufferIndicator learned;
    for (int sample = 0; sample < 3; ++sample) {
        const auto start = t + std::chrono::seconds(100 * sample + 1);
        learned.observe(true, false, false, false, false, 0, 0, true, false, start);
        learned.observe(false, true, false, false, false, 0, 0, true, false, start + std::chrono::seconds(15));
        learned.observe(false, false, true, false, false, 1000, 0, true, false, start + std::chrono::seconds(23));
        learned.reset();
    }
    const auto next = t + std::chrono::seconds(305);
    learned.observe(true, false, false, false, false, 0, 0, true, false, next);
    assert(learned.statusText(next + std::chrono::seconds(2)).find("remaining") != std::string::npos);
    learned.observe(false, true, false, false, false, 0, 0, true, false, next + std::chrono::seconds(15));
    assert(learned.statusText(next + std::chrono::seconds(16)).find("remaining") != std::string::npos);

    // A replacement stream must not inherit the previous item's stalled clock.
    PlayerBufferIndicator replacement;
    replacement.observe(false, false, true, false, false, 1000, 0, true, false, t);
    replacement.observe(true, false, false, false, false, 0, 0, true, false, t + std::chrono::seconds(30));
    replacement.observe(false, true, false, false, false, 0, 0, true, false, t + std::chrono::seconds(31));
    replacement.observe(false, false, true, false, false, 0, 0, true, false, t + std::chrono::seconds(32));
    assert(replacement.phase() == PlayerBufferPhase::Playing);

    // The direct connecting path also starts a new progress clock.
    replacement.observe(false, true, false, false, false, 0, 0, true, false, t + std::chrono::seconds(60));
    replacement.observe(false, false, true, false, false, 0, 0, true, false, t + std::chrono::seconds(61));
    assert(replacement.phase() == PlayerBufferPhase::Playing);

    b.reset();
    assert(b.cacheAheadMs() == 0);
    return 0;
}
