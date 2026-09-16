#include "artwork_provider.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {
ApiValueResult<std::string> bytesResult(std::string value) {
    return {{true, {}}, std::move(value)};
}

struct FakeTaskRunner {
    std::vector<std::function<void()>> queued;

    bool submit(std::function<void()> task) {
        queued.push_back(std::move(task));
        return true;
    }

    void runNext() {
        assert(!queued.empty());
        auto task = std::move(queued.front());
        queued.erase(queued.begin());
        task();
    }
};

struct FakeRenderer {
    uint64_t textureGeneration = 11;
    int createCalls = 0;
    std::vector<uint32_t> deleted;

    [[nodiscard]] uint64_t generation() const { return textureGeneration; }

    uint32_t createTexture(int width, int height, const uint8_t* pixels) {
        assert(width == 2);
        assert(height == 3);
        assert(pixels != nullptr);
        ++createCalls;
        return static_cast<uint32_t>(100 + createCalls);
    }

    void deleteTexture(uint32_t texture) {
        deleted.push_back(texture);
    }
};

struct FakeJellyfin {
    int posterDownloads = 0;
    int profileDownloads = 0;
    int homeDownloads = 0;
    int backdropDownloads = 0;
    int logoDownloads = 0;

    ApiValueResult<std::string> downloadPrimaryImage(
        const JellyfinSession& session,
        const std::string& itemId,
        const std::string& imageTag,
        int width,
        int height
    ) {
        ++posterDownloads;
        assert(session.userId == "user-1");
        assert(itemId == "movie-1");
        assert(imageTag == "primary-tag");
        assert(width == 384 && height == 576);
        return bytesResult("poster");
    }

    ApiValueResult<std::string> downloadUserImage(const JellyfinSession& session, int width, int height) {
        ++profileDownloads;
        assert(session.userId == "user-1");
        assert(width == 180 && height == 180);
        return bytesResult("profile");
    }

    ApiValueResult<std::string> downloadHomeImage(
        const JellyfinSession& session,
        const std::string& itemId,
        const ArtworkReference& artwork,
        int width,
        int height
    ) {
        ++homeDownloads;
        assert(session.userId == "user-1");
        assert(itemId == "movie-1");
        assert(artwork.itemId == "movie-1");
        assert(artwork.tag == "thumb-tag");
        assert(artwork.kind == ArtworkKind::Thumb);
        assert(width == 480 && height == 270);
        return bytesResult("home");
    }

    ApiValueResult<std::string> downloadBackdropImage(
        const JellyfinSession& session,
        const std::string& itemId,
        const std::string& tag,
        int width,
        int height
    ) {
        ++backdropDownloads;
        assert(session.userId == "user-1");
        assert(itemId == "backdrop-owner");
        assert(tag == "backdrop-tag");
        assert(width == 1920 && height == 1080);
        return bytesResult("backdrop");
    }

    ApiValueResult<std::string> downloadLogoImage(
        const JellyfinSession& session,
        const std::string& itemId,
        const std::string& tag,
        int width,
        int height
    ) {
        ++logoDownloads;
        assert(session.userId == "user-1");
        assert(itemId == "logo-owner");
        assert(tag == "logo-tag");
        assert(width == 800 && height == 240);
        return bytesResult("logo");
    }
};

struct FakeSeerr {
    int downloads = 0;
    std::string lastUrl;

    ApiValueResult<std::string> downloadImage(const std::string& url) {
        ++downloads;
        lastUrl = url;
        return bytesResult("external");
    }
};

struct FakeDecoder {
    DecodedImage decode(const std::string& encoded, std::string&) {
        assert(!encoded.empty());
        DecodedImage image;
        image.width = 2;
        image.height = 3;
        image.rgba.resize(24, 0xff);
        return image;
    }
};

int homeObservations = 0;

void observeHome(const HomeArtworkRequest& request, const ArtworkLoadResult& loaded) {
    ++homeObservations;
    assert(request.itemId == "movie-1");
    assert(request.itemType == "Movie");
    assert(loaded.ok());
    assert(loaded.source == ArtworkLoadSource::Network);
}
}

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example";
    session.userId = "user-1";
    session.token = "token";

    JellyfinItem item;
    item.id = "movie-1";
    item.type = "Movie";
    item.imageTag = "primary-tag";
    item.thumbTag = "thumb-tag";
    item.backdropTag = "backdrop-tag";
    item.backdropItemId = "backdrop-owner";
    item.logoTag = "logo-tag";
    item.logoItemId = "logo-owner";

    FakeJellyfin jellyfin;
    FakeSeerr seerr;
    FakeDecoder decoder;
    FakeTaskRunner tasks;
    std::recursive_mutex stateMutex;
    FakeRenderer renderer;
    ArtworkProvider<FakeJellyfin, FakeSeerr, FakeDecoder, FakeTaskRunner, std::recursive_mutex> artwork(
        jellyfin,
        seerr,
        decoder,
        tasks,
        stateMutex,
        observeHome
    );

    const std::filesystem::path cacheRoot =
        std::filesystem::temp_directory_path() / "sloppatv-artwork-provider-test";
    std::error_code error;
    std::filesystem::remove_all(cacheRoot, error);
    artwork.setDataPath(cacheRoot.string());

    JellyfinSession invalidSession;
    assert(artwork.posterTexture(invalidSession, item, false, renderer) == nullptr);
    assert(artwork.homeTexture(invalidSession, item, false, renderer) == nullptr);
    assert(tasks.queued.empty());

    assert(artwork.posterTexture(session, item, false, renderer) == nullptr);
    assert(tasks.queued.size() == 1);
    assert(artwork.posterTexture(session, item, false, renderer) == nullptr);
    assert(tasks.queued.size() == 1);
    tasks.runNext();
    assert(jellyfin.posterDownloads == 1);
    assert(renderer.createCalls == 0);
    ArtworkEntry* poster = artwork.posterTexture(session, item, false, renderer);
    assert(poster && poster->texture == 101);
    assert(renderer.createCalls == 1);

    assert(artwork.requestHome(session, item, false, renderer));
    tasks.runNext();
    assert(jellyfin.homeDownloads == 1);
    assert(homeObservations == 1);
    ArtworkEntry* home = artwork.homeTexture(session, item, false, renderer);
    assert(home && home->texture == 102);

    assert(artwork.profileTexture(session, renderer) == nullptr);
    tasks.runNext();
    assert(jellyfin.profileDownloads == 1);
    ArtworkEntry* profile = artwork.profileTexture(session, renderer);
    assert(profile && profile->texture == 103);

    assert(artwork.backdropTexture(session, item, 0, renderer) == nullptr);
    assert(tasks.queued.empty());
    assert(artwork.backdropTexture(session, item, 2, renderer) == nullptr);
    tasks.runNext();
    assert(jellyfin.backdropDownloads == 1);
    ArtworkEntry* backdrop = artwork.backdropTexture(session, item, 2, renderer);
    assert(backdrop && backdrop->texture == 104);

    assert(artwork.logoTexture(session, item, renderer) == nullptr);
    tasks.runNext();
    assert(jellyfin.logoDownloads == 1);
    ArtworkEntry* logo = artwork.logoTexture(session, item, renderer);
    assert(logo && logo->texture == 105);

    JellyfinItem external = item;
    external.externalPosterUrl = "https://images.example/poster.jpg";
    assert(artwork.posterTexture(session, external, true, renderer) == nullptr);
    tasks.runNext();
    assert(jellyfin.posterDownloads == 1);
    assert(seerr.downloads == 1);
    assert(seerr.lastUrl == external.externalPosterUrl);
    ArtworkEntry* externalPoster = artwork.posterTexture(session, external, true, renderer);
    assert(externalPoster && externalPoster->texture == 106);

    artwork.eraseProfile(session, renderer);
    assert(std::find(renderer.deleted.begin(), renderer.deleted.end(), 103) != renderer.deleted.end());
    artwork.clearSession(renderer);
    assert(renderer.deleted.size() == 6);

    std::filesystem::remove_all(cacheRoot, error);
    return 0;
}
