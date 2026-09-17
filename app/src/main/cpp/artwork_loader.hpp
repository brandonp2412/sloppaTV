#pragma once

#include "artwork_image_loader.hpp"
#include "artwork_request.hpp"
#include "home_image_disk_cache.hpp"

#include <string>
#include <utility>

template <typename JellyfinLike, typename SeerrLike, typename DecoderLike> class ArtworkLoader {
public:
    ArtworkLoader(JellyfinLike& jellyfin, SeerrLike& seerr, DecoderLike& decoder)
        : jellyfin_(jellyfin), seerr_(seerr), decoder_(decoder) {}

    void setDataPath(std::string dataPath) { homeDiskCache_.setDataPath(std::move(dataPath)); }

    ArtworkLoadResult loadPoster(const JellyfinSession& session, const PosterArtworkRequest& request) {
        return ArtworkImageLoader::load(
            [&] {
                if (request.external) return seerr_.downloadImage(request.externalUrl);
                return jellyfin_.downloadPrimaryImage(session, request.itemId, request.imageTag, 384, 576);
            },
            [this](const std::string& encoded, std::string& error) { return decoder_.decode(encoded, error); });
    }

    ArtworkLoadResult loadProfile(const JellyfinSession& session) {
        return ArtworkImageLoader::load(
            [&] { return jellyfin_.downloadUserImage(session, 180, 180); },
            [this](const std::string& encoded, std::string& error) { return decoder_.decode(encoded, error); });
    }

    ArtworkLoadResult loadHome(const JellyfinSession& session, const HomeArtworkRequest& request) {
        return ArtworkImageLoader::loadCached(
            homeDiskCache_, request.key,
            [&] {
                if (request.external) return seerr_.downloadImage(request.externalUrl);
                return jellyfin_.downloadHomeImage(session, request.itemId, request.artwork, 480, 270);
            },
            [this](const std::string& encoded, std::string& error) { return decoder_.decode(encoded, error); });
    }

    ArtworkLoadResult loadBackdrop(const JellyfinSession& session, const BackdropArtworkRequest& request) {
        return ArtworkImageLoader::load(
            [&] {
                return jellyfin_.downloadBackdropImage(session, request.artworkItemId, request.artworkTag, 1920, 1080);
            },
            [this](const std::string& encoded, std::string& error) { return decoder_.decode(encoded, error); });
    }

    ArtworkLoadResult loadLogo(const JellyfinSession& session, const LogoArtworkRequest& request) {
        return ArtworkImageLoader::load(
            [&] { return jellyfin_.downloadLogoImage(session, request.artworkItemId, request.artworkTag, 800, 240); },
            [this](const std::string& encoded, std::string& error) { return decoder_.decode(encoded, error); });
    }

private:
    JellyfinLike& jellyfin_;
    SeerrLike& seerr_;
    DecoderLike& decoder_;
    HomeImageDiskCache homeDiskCache_;
};
