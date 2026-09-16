#pragma once

#include "home_screen.hpp"
#include "jellyfin_types.hpp"

#include <string>

struct PosterArtworkRequest {
    std::string key;
    std::string itemId;
    std::string imageTag;
    std::string externalUrl;
    bool external = false;

    [[nodiscard]] JellyfinItem jellyfinItem() const {
        JellyfinItem item;
        item.id = itemId;
        item.imageTag = imageTag;
        return item;
    }
};

struct HomeArtworkRequest {
    std::string key;
    std::string itemId;
    std::string itemType;
    std::string imageTag;
    std::string seriesId;
    std::string seriesPrimaryImageTag;
    std::string thumbTag;
    std::string backdropTag;
    std::string backdropItemId;
    std::string externalUrl;
    bool external = false;

    [[nodiscard]] JellyfinItem jellyfinItem() const {
        JellyfinItem item;
        item.id = itemId;
        item.type = itemType;
        item.imageTag = imageTag;
        item.seriesId = seriesId;
        item.seriesPrimaryImageTag = seriesPrimaryImageTag;
        item.thumbTag = thumbTag;
        item.backdropTag = backdropTag;
        item.backdropItemId = backdropItemId;
        return item;
    }
};

struct BackdropArtworkRequest {
    std::string key;
    std::string itemId;
    std::string backdropItemId;
    std::string backdropTag;

    [[nodiscard]] JellyfinItem jellyfinItem() const {
        JellyfinItem item;
        item.id = itemId;
        item.backdropItemId = backdropItemId;
        item.backdropTag = backdropTag;
        return item;
    }
};

struct LogoArtworkRequest {
    std::string key;
    std::string itemId;
    std::string logoItemId;
    std::string logoTag;

    [[nodiscard]] JellyfinItem jellyfinItem() const {
        JellyfinItem item;
        item.id = itemId;
        item.logoItemId = logoItemId;
        item.logoTag = logoTag;
        return item;
    }
};

inline std::string profileArtworkKey(const JellyfinSession& session) {
    std::string key;
    key.reserve(session.server.size() + session.userId.size() + 6);
    key.append(session.server).append(":user:").append(session.userId);
    return key;
}

inline std::string posterArtworkKey(
    const JellyfinSession& session,
    const JellyfinItem& item,
    bool external
) {
    if (external) return "seerr:poster:" + item.externalPosterUrl;

    std::string key;
    key.reserve(session.server.size() + session.userId.size() + item.id.size() + item.imageTag.size() + 16);
    key.append(session.server)
        .append(":user:")
        .append(session.userId)
        .push_back(':');
    key.append(item.id).append(":primary:").append(item.imageTag);
    return key;
}

inline std::string backdropArtworkKey(
    const JellyfinSession& session,
    const JellyfinItem& item,
    int backdropMode
) {
    const std::string& artworkItemId = item.backdropItemId.empty() ? item.id : item.backdropItemId;
    const std::string mode = std::to_string(backdropMode);
    std::string key;
    key.reserve(session.server.size() + session.userId.size() + artworkItemId.size() + item.backdropTag.size() + mode.size() + 24);
    key.append(session.server)
        .append(":user:")
        .append(session.userId)
        .push_back(':');
    key.append(artworkItemId)
        .append(":backdrop:")
        .append(item.backdropTag)
        .append(":mode:")
        .append(mode);
    return key;
}

inline std::string logoArtworkKey(const JellyfinSession& session, const JellyfinItem& item) {
    const std::string& artworkItemId = item.logoItemId.empty() ? item.id : item.logoItemId;
    std::string key;
    key.reserve(session.server.size() + session.userId.size() + artworkItemId.size() + item.logoTag.size() + 13);
    key.append(session.server)
        .append(":user:")
        .append(session.userId)
        .push_back(':');
    key.append(artworkItemId).append(":logo:").append(item.logoTag);
    return key;
}

inline std::string homeArtworkKey(
    const JellyfinSession& session,
    const JellyfinItem& item,
    bool external
) {
    if (external) {
        const std::string& source = item.externalBackdropUrl.empty()
            ? item.externalPosterUrl
            : item.externalBackdropUrl;
        return "seerr:home:" + source;
    }

    const ArtworkReference artwork = homeArtworkReference(
        item.id,
        item.imageTag,
        item.seriesId,
        item.seriesPrimaryImageTag,
        preferHomeLandscapeArtwork(item.type),
        item.thumbTag,
        item.backdropTag,
        item.backdropItemId
    );
    const std::string kind = std::to_string(static_cast<int>(artwork.kind));
    std::string key;
    key.reserve(session.server.size() + session.userId.size() + artwork.itemId.size() + artwork.tag.size() + kind.size() + 31);
    key.append(session.server)
        .append(":user:")
        .append(session.userId)
        .push_back(':');
    key.append(artwork.itemId)
        .append(":home:v5-480x270:")
        .append(kind)
        .push_back(':');
    key.append(artwork.tag);
    return key;
}

inline PosterArtworkRequest posterArtworkRequest(
    const JellyfinSession& session,
    const JellyfinItem& item,
    bool external
) {
    return {
        .key = posterArtworkKey(session, item, external),
        .itemId = item.id,
        .imageTag = item.imageTag,
        .externalUrl = external ? item.externalPosterUrl : std::string{},
        .external = external,
    };
}

inline BackdropArtworkRequest backdropArtworkRequest(
    const JellyfinSession& session,
    const JellyfinItem& item,
    int backdropMode
) {
    return {
        .key = backdropArtworkKey(session, item, backdropMode),
        .itemId = item.id,
        .backdropItemId = item.backdropItemId,
        .backdropTag = item.backdropTag,
    };
}

inline LogoArtworkRequest logoArtworkRequest(const JellyfinSession& session, const JellyfinItem& item) {
    return {
        .key = logoArtworkKey(session, item),
        .itemId = item.id,
        .logoItemId = item.logoItemId,
        .logoTag = item.logoTag,
    };
}

inline HomeArtworkRequest homeArtworkRequest(
    const JellyfinSession& session,
    const JellyfinItem& item,
    bool external
) {
    return {
        .key = homeArtworkKey(session, item, external),
        .itemId = item.id,
        .itemType = item.type,
        .imageTag = item.imageTag,
        .seriesId = item.seriesId,
        .seriesPrimaryImageTag = item.seriesPrimaryImageTag,
        .thumbTag = item.thumbTag,
        .backdropTag = item.backdropTag,
        .backdropItemId = item.backdropItemId,
        .externalUrl = external
            ? (item.externalBackdropUrl.empty() ? item.externalPosterUrl : item.externalBackdropUrl)
            : std::string{},
        .external = external,
    };
}
