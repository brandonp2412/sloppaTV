#pragma once

#include "browse_screen.hpp"
#include "jellyfin_types.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct BrowsePageCompletion {
    std::string containerId;
    int startIndex = 0;
    bool append = false;
    uint64_t generation = 0;
    ApiValueResult<std::vector<JellyfinItem>> result;
};

struct BrowsePageRequest {
    JellyfinSession session;
    JellyfinItem container;
    int startIndex = 0;
    bool append = false;
    uint64_t generation = 0;
    BrowseContentMode mode = BrowseContentMode::All;
    std::string genre;
    std::string letter;
    bool nested = false;
    int pageSize = 60;
};

template <typename Client, typename TaskRunner, typename CompletionSink>
class BrowseAsyncExecutor {
public:
    BrowseAsyncExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions)
        : client_(client), tasks_(tasks), completions_(completions) {}

    bool load(BrowsePageRequest request) {
        return tasks_.submit([this, request = std::move(request)]() mutable {
            ApiValueResult<std::vector<JellyfinItem>> result;
            if (request.nested || request.mode == BrowseContentMode::All) {
                result =
                    client_.browseLibrary(request.session, request.container.id, request.startIndex, request.pageSize);
                if (result.ok && result.value.empty() && request.startIndex == 0 &&
                    request.container.type == "BoxSet") {
                    auto fallback = client_.browseCollectionMembersFallback(request.session, request.container);
                    if (fallback.ok) result = std::move(fallback);
                }
            } else if (request.mode == BrowseContentMode::Favorites) {
                result = client_.browseVideoFilter(request.session, request.container, request.startIndex,
                                                   request.pageSize, true);
            } else if (request.mode == BrowseContentMode::Genres) {
                result = client_.listGenres(request.session, request.container, 100);
            } else if (request.mode == BrowseContentMode::GenreItems) {
                result = client_.browseVideoFilter(request.session, request.container, request.startIndex,
                                                   request.pageSize, false, request.genre);
            } else if (request.mode == BrowseContentMode::LetterItems) {
                result = client_.browseVideoFilter(request.session, request.container, request.startIndex,
                                                   request.pageSize, false, {}, request.letter);
            } else if (request.mode == BrowseContentMode::Collections) {
                result = client_.browseCollections(request.session, request.startIndex, request.pageSize);
            } else {
                result.ok = true;
            }

            completions_.push(BrowsePageCompletion{
                .containerId = std::move(request.container.id),
                .startIndex = request.startIndex,
                .append = request.append,
                .generation = request.generation,
                .result = std::move(result),
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
};
