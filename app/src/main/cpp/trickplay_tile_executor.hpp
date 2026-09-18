#pragma once

#include "decoded_image.hpp"
#include "jellyfin_types.hpp"

#include <string>
#include <utility>

struct TrickplayTileRequest {
    std::string itemId;
    JellyfinTrickplayInfo trickplay;
    int tileIndex = -1;
};

struct TrickplayTileCompletion {
    std::string itemId;
    int tileIndex = -1;
    DecodedImage decoded;
    std::string error;
};

template <typename Client, typename Decoder, typename TaskRunner, typename CompletionSink>
class TrickplayTileExecutor {
public:
    TrickplayTileExecutor(Client& client, Decoder& decoder, TaskRunner& tasks, CompletionSink& completions)
        : client_(client), decoder_(decoder), tasks_(tasks), completions_(completions) {}

    bool load(JellyfinSession session, TrickplayTileRequest request) {
        return tasks_.submit([this, session = std::move(session), request = std::move(request)] {
            JellyfinItem item;
            item.id = request.itemId;
            item.trickplay = request.trickplay;

            auto image = client_.downloadTrickplayTile(session, item, request.tileIndex);
            DecodedImage decoded;
            std::string error;
            if (image.ok) decoded = decoder_.decode(image.value, error);
            completions_.push(TrickplayTileCompletion{
                .itemId = request.itemId,
                .tileIndex = request.tileIndex,
                .decoded = std::move(decoded),
                .error = image.ok ? std::move(error) : std::move(image.error),
            });
        });
    }

private:
    Client& client_;
    Decoder& decoder_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
};
