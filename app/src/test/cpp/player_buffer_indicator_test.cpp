#include "player_buffer_indicator.hpp"

#include <cassert>
#include <chrono>

int main() {
    PlayerBufferIndicator b;
    const auto t = PlayerBufferIndicator::Clock::now();
    b.observe(true, false, false, false, false, 0, 0, true, t);
    assert(b.phase() == PlayerBufferPhase::Resolving);
    assert(b.statusText().find("Remux") != std::string::npos);
    b.observe(false, true, false, false, false, 0, 0, true, t);
    assert(b.phase() == PlayerBufferPhase::Connecting);
    assert(b.statusText(t + std::chrono::seconds(14)).find("Waiting") != std::string::npos);
    b.observe(false, false, true, false, false, 3000, 90000, true, t + std::chrono::seconds(14));
    assert(b.phase() == PlayerBufferPhase::Playing);
    b.observe(false, false, true, false, true, 3000, 0, true, t + std::chrono::seconds(18));
    assert(b.phase() == PlayerBufferPhase::Buffering && b.interrupted());
    b.observe(false, false, true, false, true, 3000, 0, true, t + std::chrono::seconds(32));
    assert(b.phase() == PlayerBufferPhase::Stalled);
    b.observe(false, false, true, false, false, 4000, 65000, true, t + std::chrono::seconds(33));
    assert(b.phase() == PlayerBufferPhase::Playing && !b.interrupted());
    b.observe(false, false, false, true, false, 4000, 65000, true, t + std::chrono::seconds(40));
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
    b.reset();
    assert(b.cacheAheadMs() == 0);
    return 0;
}
