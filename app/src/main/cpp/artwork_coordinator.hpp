#pragma once

#include "artwork_image_loader.hpp"
#include "artwork_pipeline.hpp"
#include "home_image_disk_cache.hpp"

#include <mutex>
#include <string>
#include <utility>

template <typename TaskRunnerLike, typename MutexLike>
class ArtworkCoordinator {
public:
    ArtworkCoordinator(TaskRunnerLike& tasks, MutexLike& stateMutex)
        : tasks_(tasks), stateMutex_(stateMutex) {}

    void setDataPath(std::string dataPath) {
        homeDiskCache_.setDataPath(std::move(dataPath));
    }

    template <typename RendererLike, typename Download, typename Decode>
    bool loadPoster(
        const std::string& key,
        RendererLike& renderer,
        Download&& download,
        Decode&& decode
    ) {
        return queueLoad(
            poster_,
            key,
            renderer,
            [download = std::forward<Download>(download), decode = std::forward<Decode>(decode)]() mutable {
                return ArtworkImageLoader::load(download, decode);
            }
        );
    }

    template <typename RendererLike, typename Download, typename Decode>
    bool loadProfile(
        const std::string& key,
        RendererLike& renderer,
        Download&& download,
        Decode&& decode
    ) {
        return queueLoad(
            profile_,
            key,
            renderer,
            [download = std::forward<Download>(download), decode = std::forward<Decode>(decode)]() mutable {
                return ArtworkImageLoader::load(download, decode);
            }
        );
    }

    template <typename RendererLike, typename Download, typename Decode, typename Observe>
    bool loadHome(
        const std::string& key,
        RendererLike& renderer,
        Download&& download,
        Decode&& decode,
        Observe&& observe
    ) {
        return queueLoad(
            home_,
            key,
            renderer,
            [this,
             key,
             download = std::forward<Download>(download),
             decode = std::forward<Decode>(decode),
             observe = std::forward<Observe>(observe)]() mutable {
                ArtworkLoadResult loaded = ArtworkImageLoader::loadCached(
                    homeDiskCache_,
                    key,
                    download,
                    decode
                );
                observe(loaded);
                return loaded;
            }
        );
    }

    template <typename RendererLike, typename Download, typename Decode>
    bool loadBackdrop(
        const std::string& key,
        RendererLike& renderer,
        Download&& download,
        Decode&& decode
    ) {
        return queueLoad(
            backdrop_,
            key,
            renderer,
            [download = std::forward<Download>(download), decode = std::forward<Decode>(decode)]() mutable {
                return ArtworkImageLoader::load(download, decode);
            }
        );
    }

    template <typename RendererLike, typename Download, typename Decode>
    bool loadLogo(
        const std::string& key,
        RendererLike& renderer,
        Download&& download,
        Decode&& decode
    ) {
        return queueLoad(
            logo_,
            key,
            renderer,
            [download = std::forward<Download>(download), decode = std::forward<Decode>(decode)]() mutable {
                return ArtworkImageLoader::load(download, decode);
            }
        );
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
    HomeImageDiskCache homeDiskCache_;
    ArtworkPipeline backdrop_{8};
    ArtworkPipeline logo_{12};
};
