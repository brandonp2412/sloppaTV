#include "player_buffer_indicator.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;
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

    // A resumed item starts at a nonzero timestamp but has not yet advanced.
    PlayerBufferIndicator coldResume;
    coldResume.observe(true, false, false, false, false, 500000, 0, true, false, t);
    coldResume.observe(false, true, false, false, false, 500000, 0, true, false, t + 1s);
    assert(coldResume.phase() == PlayerBufferPhase::Connecting);
    coldResume.observe(true, false, false, false, false, 500000, 0, true, false, t + 2s);
    assert(coldResume.phase() == PlayerBufferPhase::Resolving);

    // Midstream source resolution must say buffering, not opening, after progress.
    PlayerBufferIndicator reconnect;
    reconnect.observe(false, true, false, false, false, 100000, 0, true, false, t);
    reconnect.observe(false, false, true, false, false, 100000, 2000, true, false, t + 1s);
    reconnect.observe(false, false, true, false, false, 100250, 2000, true, false, t + 2s);
    reconnect.observe(true, false, false, false, false, 100250, 0, true, false, t + 3s);
    assert(reconnect.phase() == PlayerBufferPhase::Buffering);
    assert(reconnect.statusText(t + 3s) == "Buffering from Remux");
    reconnect.observe(false, true, false, false, false, 100250, 0, true, false, t + 19s);
    assert(reconnect.phase() == PlayerBufferPhase::Stalled);
    reconnect.observe(false, false, true, false, false, 100500, 2000, true, false, t + 20s);
    assert(reconnect.phase() == PlayerBufferPhase::Playing);
    reconnect.reset();
    reconnect.observe(true, false, false, false, false, 0, 0, true, false, t + 21s);
    assert(reconnect.phase() == PlayerBufferPhase::Resolving);

    b.reset();
    assert(b.cacheAheadMs() == 0);
    return 0;
}
