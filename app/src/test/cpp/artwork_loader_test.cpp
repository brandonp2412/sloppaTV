#include "artwork_loader.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace {
ApiValueResult<std::string> bytesResult(std::string value) {
    return {{true, {}}, std::move(value)};
}

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
        const std::string& sourceItemId,
        const ArtworkReference& artwork,
        int width,
        int height
    ) {
        ++homeDownloads;
        assert(session.userId == "user-1");
        assert(sourceItemId == "movie-1");
        assert(artwork.itemId == "movie-1");
        assert(artwork.tag == "thumb-tag");
        assert(artwork.kind == ArtworkKind::Thumb);
        assert(width == 480 && height == 270);
        return bytesResult("home");
    }

    ApiValueResult<std::string> downloadBackdropImage(
        const JellyfinSession& session,
        const std::string& artworkItemId,
        const std::string& artworkTag,
        int width,
        int height
    ) {
        ++backdropDownloads;
        assert(session.userId == "user-1");
        assert(artworkItemId == "backdrop-owner");
        assert(artworkTag == "backdrop-tag");
        assert(width == 1920 && height == 1080);
        return bytesResult("backdrop");
    }

    ApiValueResult<std::string> downloadLogoImage(
        const JellyfinSession& session,
        const std::string& artworkItemId,
        const std::string& artworkTag,
        int width,
        int height
    ) {
        ++logoDownloads;
        assert(session.userId == "user-1");
        assert(artworkItemId == "logo-owner");
        assert(artworkTag == "logo-tag");
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
    int decodes = 0;
    std::vector<std::string> values;

    DecodedImage decode(const std::string& encoded, std::string&) {
        ++decodes;
        values.push_back(encoded);
        DecodedImage image;
        image.width = 2;
        image.height = 3;
        image.rgba.resize(24, 0xff);
        return image;
    }
};
}

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example";
    session.userId = "user-1";
    session.token = "token";

    JellyfinItem item;
    item.id = "movie-1";
    item.name = "Large media object payload";
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
    ArtworkLoader<FakeJellyfin, FakeSeerr, FakeDecoder> loader(jellyfin, seerr, decoder);

    const std::filesystem::path cacheRoot =
        std::filesystem::temp_directory_path() / "sloppatv-artwork-loader-test";
    std::error_code error;
    std::filesystem::remove_all(cacheRoot, error);
    loader.setDataPath(cacheRoot.string());

    const PosterArtworkRequest poster = posterArtworkRequest(session, item, false);
    assert(loader.loadPoster(session, poster).ok());
    assert(jellyfin.posterDownloads == 1);

    assert(loader.loadProfile(session).ok());
    assert(jellyfin.profileDownloads == 1);

    const HomeArtworkRequest home = homeArtworkRequest(session, item, false);
    ArtworkLoadResult homeNetwork = loader.loadHome(session, home);
    assert(homeNetwork.ok());
    assert(homeNetwork.source == ArtworkLoadSource::Network);
    assert(jellyfin.homeDownloads == 1);

    ArtworkLoadResult homeCached = loader.loadHome(session, home);
    assert(homeCached.ok());
    assert(homeCached.source == ArtworkLoadSource::DiskCache);
    assert(jellyfin.homeDownloads == 1);

    const BackdropArtworkRequest backdrop = backdropArtworkRequest(session, item, 2);
    assert(loader.loadBackdrop(session, backdrop).ok());
    assert(jellyfin.backdropDownloads == 1);

    const LogoArtworkRequest logo = logoArtworkRequest(session, item);
    assert(loader.loadLogo(session, logo).ok());
    assert(jellyfin.logoDownloads == 1);

    item.externalPosterUrl = "https://images.example/poster.jpg";
    const PosterArtworkRequest externalPoster = posterArtworkRequest(session, item, true);
    assert(loader.loadPoster(session, externalPoster).ok());
    assert(jellyfin.posterDownloads == 1);
    assert(seerr.downloads == 1);
    assert(seerr.lastUrl == item.externalPosterUrl);

    item.externalBackdropUrl = "https://images.example/backdrop.jpg";
    const HomeArtworkRequest externalHome = homeArtworkRequest(session, item, true);
    assert(loader.loadHome(session, externalHome).ok());
    assert(jellyfin.homeDownloads == 1);
    assert(seerr.downloads == 2);
    assert(seerr.lastUrl == item.externalBackdropUrl);

    assert(decoder.decodes == 8);
    std::filesystem::remove_all(cacheRoot, error);
    return 0;
}
