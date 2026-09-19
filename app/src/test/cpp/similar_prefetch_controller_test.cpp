#include "similar_prefetch_controller.hpp"

#include <cassert>
#include <chrono>
#include <utility>

int main() {
    using namespace std::chrono_literals;

    SimilarPrefetchController controller;
    JellyfinSession session;
    session.server = "https://example.invalid";
    session.userId = "user-1";
    session.token = "token";

    JellyfinItem item;
    item.id = "movie-1";
    item.name = "Movie";
    item.type = "Movie";

    const auto now = SimilarPrefetchController::Clock::now();
    assert(controller.schedule(session, item, now));
    assert(controller.dueDeadline() == now + 350ms);
    assert(!controller.takeDue(session, true, now + 349ms));

    auto work = controller.takeDue(session, true, now + 350ms);
    assert(work);
    assert(work->itemId == item.id);
    assert(work->session.userId == session.userId);

    assert(!controller.schedule(session, item, now + 351ms));
    controller.submissionFailed(work->key);
    assert(controller.schedule(session, item, now + 400ms));

    work = controller.takeDue(session, true, now + 750ms);
    assert(work);

    JellyfinItem similar;
    similar.id = "movie-2";
    similar.name = "Similar";
    similar.type = "Movie";
    SimilarPrefetchCompletion completion{
        .session = session,
        .itemId = item.id,
        .key = work->key,
        .result = {},
    };
    completion.result.ok = true;
    completion.result.value.push_back(similar);

    auto completedItems = controller.complete(completion);
    assert(completedItems);
    assert(completedItems->size() == 1);
    assert(completedItems->front().id == similar.id);

    auto cached = controller.cached(session, item.id);
    assert(cached);
    assert(cached->front().id == similar.id);
    assert(!controller.schedule(session, item, now + 1s));

    JellyfinItem folder;
    folder.id = "folder-1";
    folder.type = "Folder";
    assert(!controller.schedule(session, folder, now + 2s));

    JellyfinItem other;
    other.id = "movie-3";
    other.type = "Movie";
    assert(controller.schedule(session, other, now + 3s));
    assert(!controller.takeDue(session, false, now + 4s));
    assert(controller.dueDeadline() == SimilarPrefetchController::Clock::time_point{});

    return 0;
}
