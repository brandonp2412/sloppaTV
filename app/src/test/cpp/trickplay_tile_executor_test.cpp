#include "trickplay_tile_executor.hpp"

#include <cassert>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {
struct ImmediateTaskRunner {
    bool submit(std::function<void()> task) {
        if (!accept) return false;
        ++submissions;
        task();
        return true;
    }

    bool accept = true;
    int submissions = 0;
};

struct CompletionSink {
    void push(TrickplayTileCompletion completion) { events.push_back(std::move(completion)); }

    std::vector<TrickplayTileCompletion> events;
};

struct FakeClient {
    ApiValueResult<std::string> downloadTrickplayTile(const JellyfinSession&, const JellyfinItem& item, int tileIndex) {
        lastItem = item;
        lastTileIndex = tileIndex;
        return response;
    }

    ApiValueResult<std::string> response;
    JellyfinItem lastItem;
    int lastTileIndex = -1;
};

struct FakeDecoder {
    DecodedImage decode(const std::string& encoded, std::string& error) {
        ++calls;
        lastEncoded = encoded;
        error = decodeError;
        return decoded;
    }

    int calls = 0;
    std::string lastEncoded;
    std::string decodeError;
    DecodedImage decoded;
};

TrickplayTileRequest request(int tileIndex = 4) {
    return TrickplayTileRequest{
        .itemId = "episode-1",
        .trickplay =
            JellyfinTrickplayInfo{
                .mediaSourceId = "source-1",
                .width = 320,
                .height = 180,
                .tileWidth = 160,
                .tileHeight = 90,
                .thumbnailCount = 20,
                .intervalMs = 10'000,
            },
        .tileIndex = tileIndex,
    };
}
} // namespace

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example.nz";
    session.userId = "user-1";

    FakeClient client;
    FakeDecoder decoder;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    TrickplayTileExecutor executor(client, decoder, tasks, completions);

    client.response.ok = true;
    client.response.value = "jpeg-bytes";
    decoder.decoded.width = 2;
    decoder.decoded.height = 1;
    decoder.decoded.rgba.resize(8);
    assert(executor.load(session, request()));
    assert(tasks.submissions == 1);
    assert(client.lastItem.id == "episode-1");
    assert(client.lastItem.trickplay.mediaSourceId == "source-1");
    assert(client.lastItem.trickplay.width == 320);
    assert(client.lastTileIndex == 4);
    assert(decoder.calls == 1);
    assert(decoder.lastEncoded == "jpeg-bytes");
    assert(completions.events.back().itemId == "episode-1");
    assert(completions.events.back().tileIndex == 4);
    assert(completions.events.back().decoded.valid());
    assert(completions.events.back().error.empty());

    client.response.ok = false;
    client.response.value.clear();
    client.response.error = "download failed";
    const int decodeCalls = decoder.calls;
    assert(executor.load(session, request(5)));
    assert(decoder.calls == decodeCalls);
    assert(!completions.events.back().decoded.valid());
    assert(completions.events.back().error == "download failed");

    client.response.ok = true;
    client.response.value = "bad-image";
    client.response.error.clear();
    decoder.decoded = {};
    decoder.decodeError = "decode failed";
    assert(executor.load(session, request(6)));
    assert(!completions.events.back().decoded.valid());
    assert(completions.events.back().error == "decode failed");

    tasks.accept = false;
    const size_t completionCount = completions.events.size();
    assert(!executor.load(session, request(7)));
    assert(completions.events.size() == completionCount);

    return 0;
}
