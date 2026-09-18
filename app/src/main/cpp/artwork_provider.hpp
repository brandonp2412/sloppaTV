#pragma once

#include "artwork_coordinator.hpp"
#include "artwork_loader.hpp"
#include "artwork_request.hpp"

#include <string>
#include <utility>

template <typename JellyfinLike, typename SeerrLike, typename DecoderLike, typename TaskRunnerLike, typename CompletionSink>
class ArtworkProvider {
public:
    using HomeLoadObserver = void (*)(const HomeArtworkRequest&, const ArtworkLoadResult&);

    ArtworkProvider(JellyfinLike& jellyfin, SeerrLike& seerr, DecoderLike& decoder, TaskRunnerLike& tasks,
                    CompletionSink& completions, HomeLoadObserver homeLoadObserver = nullptr)
        : loader_(jellyfin, seerr, decoder), coordinator_(tasks, completions), homeLoadObserver_(homeLoadObserver) {}

    void setDataPath(std::string dataPath) { loader_.setDataPath(std::move(dataPath)); }

    template <typename RendererLike>
    bool requestPoster(const JellyfinSession& session, const JellyfinItem& item, bool external,
                       RendererLike& renderer) {
        if (!session.valid() || item.id.empty()) return false;
        return queuePoster(session, posterArtworkRequest(session, item, external), renderer);
    }

    template <typename RendererLike>
    ArtworkEntry* posterTexture(const JellyfinSession& session, const JellyfinItem& item, bool external,
                                RendererLike& renderer) {
        if (item.id.empty()) return nullptr;
        const std::string key = posterArtworkKey(session, item, external);
        return coordinator_.posterTexture(key, renderer, [&] { requestPoster(session, item, external, renderer); });
    }

    template <typename RendererLike>
    ArtworkEntry* profileTexture(const JellyfinSession& session, RendererLike& renderer) {
        if (!session.valid()) return nullptr;
        const std::string key = profileArtworkKey(session);
        return coordinator_.profileTexture(key, renderer, [&] { queueProfile(session, key, renderer); });
    }

    template <typename RendererLike> void eraseProfile(const JellyfinSession& session, RendererLike& renderer) {
        coordinator_.eraseProfile(profileArtworkKey(session), renderer);
    }

    template <typename RendererLike>
    bool requestHome(const JellyfinSession& session, const JellyfinItem& item, bool external, RendererLike& renderer) {
        if (!session.valid() || item.id.empty()) return false;
        return queueHome(session, homeArtworkRequest(session, item, external), renderer);
    }

    template <typename RendererLike>
    ArtworkEntry* homeTexture(const JellyfinSession& session, const JellyfinItem& item, bool external,
                              RendererLike& renderer) {
        if (item.id.empty()) return nullptr;
        const std::string key = homeArtworkKey(session, item, external);
        return coordinator_.homeTexture(key, renderer, [&] { requestHome(session, item, external, renderer); });
    }

    template <typename RendererLike>
    ArtworkEntry* backdropTexture(const JellyfinSession& session, const JellyfinItem& item, int backdropMode,
                                  RendererLike& renderer) {
        if (backdropMode <= 0 || item.id.empty() || item.backdropTag.empty()) return nullptr;
        const std::string key = backdropArtworkKey(session, item, backdropMode);
        return coordinator_.backdropTexture(key, renderer, [&] {
            if (!session.valid()) return;
            queueBackdrop(session, backdropArtworkRequest(session, item, backdropMode), renderer);
        });
    }

    template <typename RendererLike>
    ArtworkEntry* logoTexture(const JellyfinSession& session, const JellyfinItem& item, RendererLike& renderer) {
        if (item.id.empty() || item.logoTag.empty()) return nullptr;
        const std::string key = logoArtworkKey(session, item);
        return coordinator_.logoTexture(key, renderer, [&] {
            if (!session.valid()) return;
            queueLogo(session, logoArtworkRequest(session, item), renderer);
        });
    }

    template <typename RendererLike> void clearSession(RendererLike& renderer) { coordinator_.clearSession(renderer); }

    void applyCompletion(ArtworkLoadCompletion completion) { coordinator_.applyCompletion(std::move(completion)); }

private:
    template <typename RendererLike>
    bool queuePoster(const JellyfinSession& session, PosterArtworkRequest request, RendererLike& renderer) {
        if (!session.valid()) return false;
        const JellyfinSession saved = session;
        const std::string key = request.key;
        return coordinator_.loadPoster(
            key, renderer, [this, saved, request = std::move(request)] { return loader_.loadPoster(saved, request); });
    }

    template <typename RendererLike>
    bool queueProfile(const JellyfinSession& session, const std::string& key, RendererLike& renderer) {
        const JellyfinSession saved = session;
        return coordinator_.loadProfile(key, renderer, [this, saved] { return loader_.loadProfile(saved); });
    }

    template <typename RendererLike>
    bool queueHome(const JellyfinSession& session, HomeArtworkRequest request, RendererLike& renderer) {
        if (!session.valid()) return false;
        const JellyfinSession saved = session;
        const std::string key = request.key;
        const HomeArtworkRequest observedRequest = request;
        return coordinator_.loadHome(
            key, renderer, [this, saved, request = std::move(request)] { return loader_.loadHome(saved, request); },
            [observer = homeLoadObserver_, observedRequest](const ArtworkLoadResult& loaded) {
                if (observer != nullptr) observer(observedRequest, loaded);
            });
    }

    template <typename RendererLike>
    bool queueBackdrop(const JellyfinSession& session, BackdropArtworkRequest request, RendererLike& renderer) {
        if (!session.valid()) return false;
        const JellyfinSession saved = session;
        const std::string key = request.key;
        return coordinator_.loadBackdrop(key, renderer, [this, saved, request = std::move(request)] {
            return loader_.loadBackdrop(saved, request);
        });
    }

    template <typename RendererLike>
    bool queueLogo(const JellyfinSession& session, LogoArtworkRequest request, RendererLike& renderer) {
        if (!session.valid()) return false;
        const JellyfinSession saved = session;
        const std::string key = request.key;
        return coordinator_.loadLogo(
            key, renderer, [this, saved, request = std::move(request)] { return loader_.loadLogo(saved, request); });
    }

    ArtworkLoader<JellyfinLike, SeerrLike, DecoderLike> loader_;
    ArtworkCoordinator<TaskRunnerLike, CompletionSink> coordinator_;
    HomeLoadObserver homeLoadObserver_ = nullptr;
};
