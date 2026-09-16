#pragma once

#include "artwork_pipeline.hpp"

#include <mutex>
#include <string>
#include <utility>

template <typename TaskRunnerLike, typename MutexLike>
class ArtworkCoordinator {
public:
    ArtworkCoordinator(TaskRunnerLike& tasks, MutexLike& stateMutex)
        : tasks_(tasks), stateMutex_(stateMutex) {}

    template <typename RendererLike, typename Load>
    bool loadPoster(const std::string& key, RendererLike& renderer, Load&& load) {
        return queueLoad(poster_, key, renderer, std::forward<Load>(load));
    }

    template <typename RendererLike, typename Load>
    bool loadProfile(const std::string& key, RendererLike& renderer, Load&& load) {
        return queueLoad(profile_, key, renderer, std::forward<Load>(load));
    }

    template <typename RendererLike, typename Load, typename Observe>
    bool loadHome(
        const std::string& key,
        RendererLike& renderer,
        Load&& load,
        Observe&& observe
    ) {
        return queueLoad(
            home_,
            key,
            renderer,
            [load = std::forward<Load>(load), observe = std::forward<Observe>(observe)]() mutable {
                ArtworkLoadResult loaded = load();
                observe(loaded);
                return loaded;
            }
        );
    }

    template <typename RendererLike, typename Load>
    bool loadBackdrop(const std::string& key, RendererLike& renderer, Load&& load) {
        return queueLoad(backdrop_, key, renderer, std::forward<Load>(load));
    }

    template <typename RendererLike, typename Load>
    bool loadLogo(const std::string& key, RendererLike& renderer, Load&& load) {
        return queueLoad(logo_, key, renderer, std::forward<Load>(load));
    }

    template <typename RendererLike, typename Request>
    ArtworkEntry* posterTexture(const std::string& key, RendererLike& renderer, Request&& request) {
        return poster_.readyTexture(key, renderer, std::forward<Request>(request));
    }

    template <typename RendererLike, typename Request>
    ArtworkEntry* profileTexture(const std::string& key, RendererLike& renderer, Request&& request) {
        return profile_.readyTexture(key, renderer, std::forward<Request>(request));
    }

    template <typename RendererLike, typename Request>
    ArtworkEntry* homeTexture(const std::string& key, RendererLike& renderer, Request&& request) {
        return home_.readyTexture(key, renderer, std::forward<Request>(request));
    }

    template <typename RendererLike, typename Request>
    ArtworkEntry* backdropTexture(const std::string& key, RendererLike& renderer, Request&& request) {
        return backdrop_.readyTexture(key, renderer, std::forward<Request>(request));
    }

    template <typename RendererLike, typename Request>
    ArtworkEntry* logoTexture(const std::string& key, RendererLike& renderer, Request&& request) {
        return logo_.readyTexture(key, renderer, std::forward<Request>(request));
    }

    template <typename RendererLike>
    void eraseProfile(const std::string& key, RendererLike& renderer) {
        profile_.erase(key, renderer);
    }

    template <typename RendererLike>
    void clearSession(RendererLike& renderer) {
        poster_.clear(renderer);
        home_.clear(renderer);
        backdrop_.clear(renderer);
        logo_.clear(renderer);
    }

private:
    template <typename RendererLike, typename Load>
    bool queueLoad(
        ArtworkPipeline& pipeline,
        const std::string& key,
        RendererLike& renderer,
        Load&& load
    ) {
        if (!pipeline.beginLoad(key, renderer)) return false;
        return tasks_.submit([
            this,
            &pipeline,
            key,
            load = std::forward<Load>(load)
        ]() mutable {
            ArtworkLoadResult loaded = load();
            std::scoped_lock lock(stateMutex_);
            pipeline.completeLoad(key, std::move(loaded));
        });
    }

    TaskRunnerLike& tasks_;
    MutexLike& stateMutex_;
    ArtworkPipeline poster_{30};
    ArtworkPipeline profile_;
    ArtworkPipeline home_{48};
    ArtworkPipeline backdrop_{8};
    ArtworkPipeline logo_{12};
};
