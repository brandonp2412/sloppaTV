#pragma once

#include "artwork_pipeline.hpp"

#include <cstdint>
#include <string>
#include <utility>

enum class ArtworkPipelineKind : uint8_t {
    Poster,
    Profile,
    Home,
    Backdrop,
    Logo,
};

struct ArtworkLoadCompletion {
    ArtworkPipelineKind pipeline = ArtworkPipelineKind::Poster;
    std::string key;
    ArtworkLoadResult loaded;
};

template <typename TaskRunnerLike, typename CompletionSink> class ArtworkCoordinator {
public:
    ArtworkCoordinator(TaskRunnerLike& tasks, CompletionSink& completions) : tasks_(tasks), completions_(completions) {}

    template <typename RendererLike, typename Load>
    bool loadPoster(const std::string& key, RendererLike& renderer, Load&& load) {
        return queueLoad(ArtworkPipelineKind::Poster, poster_, key, renderer, std::forward<Load>(load));
    }

    template <typename RendererLike, typename Load>
    bool loadProfile(const std::string& key, RendererLike& renderer, Load&& load) {
        return queueLoad(ArtworkPipelineKind::Profile, profile_, key, renderer, std::forward<Load>(load));
    }

    template <typename RendererLike, typename Load, typename Observe>
    bool loadHome(const std::string& key, RendererLike& renderer, Load&& load, Observe&& observe) {
        return queueLoad(ArtworkPipelineKind::Home, home_, key, renderer,
                         [load = std::forward<Load>(load), observe = std::forward<Observe>(observe)]() mutable {
                             ArtworkLoadResult loaded = load();
                             observe(loaded);
                             return loaded;
                         });
    }

    template <typename RendererLike, typename Load>
    bool loadBackdrop(const std::string& key, RendererLike& renderer, Load&& load) {
        return queueLoad(ArtworkPipelineKind::Backdrop, backdrop_, key, renderer, std::forward<Load>(load));
    }

    template <typename RendererLike, typename Load>
    bool loadLogo(const std::string& key, RendererLike& renderer, Load&& load) {
        return queueLoad(ArtworkPipelineKind::Logo, logo_, key, renderer, std::forward<Load>(load));
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

    template <typename RendererLike> void eraseProfile(const std::string& key, RendererLike& renderer) {
        profile_.erase(key, renderer);
    }

    template <typename RendererLike> void clearSession(RendererLike& renderer) {
        poster_.clear(renderer);
        home_.clear(renderer);
        backdrop_.clear(renderer);
        logo_.clear(renderer);
    }

    void applyCompletion(ArtworkLoadCompletion completion) {
        switch (completion.pipeline) {
        case ArtworkPipelineKind::Poster:
            poster_.completeLoad(completion.key, std::move(completion.loaded));
            break;
        case ArtworkPipelineKind::Profile:
            profile_.completeLoad(completion.key, std::move(completion.loaded));
            break;
        case ArtworkPipelineKind::Home:
            home_.completeLoad(completion.key, std::move(completion.loaded));
            break;
        case ArtworkPipelineKind::Backdrop:
            backdrop_.completeLoad(completion.key, std::move(completion.loaded));
            break;
        case ArtworkPipelineKind::Logo:
            logo_.completeLoad(completion.key, std::move(completion.loaded));
            break;
        }
    }

private:
    template <typename RendererLike, typename Load>
    bool queueLoad(ArtworkPipelineKind kind, ArtworkPipeline& pipeline, const std::string& key, RendererLike& renderer,
                   Load&& load) {
        if (!pipeline.beginLoad(key, renderer)) return false;
        return tasks_.submit([this, kind, key, load = std::forward<Load>(load)]() mutable {
            completions_.push(ArtworkLoadCompletion{
                .pipeline = kind,
                .key = key,
                .loaded = load(),
            });
        });
    }

    TaskRunnerLike& tasks_;
    CompletionSink& completions_;
    ArtworkPipeline poster_{30};
    ArtworkPipeline profile_;
    ArtworkPipeline home_{48};
    ArtworkPipeline backdrop_{8};
    ArtworkPipeline logo_{12};
};
