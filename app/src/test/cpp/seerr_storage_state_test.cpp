#include "seerr_storage_state.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {
SeerrMediaItem media(std::string type, int tmdbId) {
    SeerrMediaItem item;
    item.id = seerrMediaId(type, tmdbId);
    item.name = "Example";
    item.mediaType = std::move(type);
    item.tmdbId = tmdbId;
    return item;
}

SeerrStorageTarget target(std::string type, int serverId, bool isDefault = false) {
    SeerrStorageTarget value;
    value.mediaType = std::move(type);
    value.serviceName = "Service";
    value.path = "/media/" + std::to_string(serverId);
    value.serverId = serverId;
    value.freeSpace = 25;
    value.totalSpace = 100;
    value.isDefault = isDefault;
    return value;
}
} // namespace

int main() {
    using namespace std::chrono_literals;
    SeerrStorageState state;
    const auto start = SeerrStorageState::Clock::now();

    assert(state.empty());
    assert(!state.loading());
    assert(state.error().empty());
    assert(state.refreshDeadline() == SeerrStorageState::TimePoint{});
    assert(state.beginRefresh(false, start));
    assert(state.loading());
    assert(!state.beginRefresh(true, start));

    state.finishRefresh({target("movie", 1), target("movie", 2, true), target("movie", 3), target("tv", 4)}, start);
    assert(!state.loading());
    assert(state.targets().size() == 4);
    assert(state.targets().front().usedPercent() == 75);
    SeerrStorageTarget huge = target("movie", 99);
    huge.totalSpace = std::numeric_limits<int64_t>::max();
    huge.freeSpace = 0;
    assert(huge.usedPercent() == 100);
    huge.freeSpace = std::numeric_limits<int64_t>::max();
    assert(huge.usedPercent() == 0);
    huge.freeSpace = -1;
    assert(huge.usedPercent() == 100);
    huge.freeSpace = std::numeric_limits<int64_t>::max();
    huge.totalSpace = 100;
    assert(huge.usedPercent() == 0);
    assert(!state.beginRefresh(false, start + 59s));
    assert(state.beginRefresh(true, start + 59s));
    state.invalidateRefresh();
    assert(!state.loading());
    assert(state.refreshDeadline() == start + 60s);

    assert(state.beginRefresh(false, start + 60s));
    state.failRefresh("storage unavailable");
    assert(!state.loading());
    assert(state.error() == "storage unavailable");
    assert(state.refreshDeadline() == SeerrStorageState::TimePoint{});

    assert(state.beginRefresh(false, start + 61s));
    state.finishRefresh({target("movie", 1), target("movie", 2, true), target("movie", 3)}, start + 61s);
    assert(state.error().empty());

    const SeerrMediaItem movie = media("movie", 10);
    assert(state.preparePicker(movie) == SeerrStorageState::PickerStatus::Ready);
    assert(state.pendingRequest());
    assert(state.driveChoices().size() == 3);
    assert(state.driveChoices()[0].serverId == 2);
    assert(state.driveChoices()[1].serverId == 1);
    assert(state.driveChoices()[2].serverId == 3);
    assert(state.driveSelection() == 0);
    state.moveSelection(1);
    assert(state.driveSelection() == 1);
    state.moveSelection(100);
    assert(state.driveSelection() == 2);
    state.moveSelection(-100);
    assert(state.driveSelection() == 0);
    state.moveSelection(1);

    const auto selected = state.takeSelection();
    assert(selected);
    assert(selected->item.id == movie.id);
    assert(selected->target.serverId == 1);
    assert(!state.pendingRequest());
    assert(state.driveChoices().empty());

    const SeerrMediaItem series = media("tv", 20);
    assert(state.preparePicker(series) == SeerrStorageState::PickerStatus::Unavailable);
    assert(!state.pendingRequest());

    assert(state.beginRefresh(true, start + 62s));
    assert(state.preparePicker(series) == SeerrStorageState::PickerStatus::Loading);
    assert(state.pendingRequest());
    const auto pending = state.takePendingRequest();
    assert(pending && pending->id == series.id);
    assert(!state.pendingRequest());
    state.invalidateRefresh();

    assert(state.preparePicker(movie) == SeerrStorageState::PickerStatus::Ready);
    auto pickerCommand = state.handlePickerInput(SeerrStorageState::PickerInput::Down);
    assert(pickerCommand.type == SeerrStorageState::PickerCommandType::None);
    assert(state.driveSelection() == 1);
    pickerCommand = state.handlePickerInput(SeerrStorageState::PickerInput::Activate);
    assert(pickerCommand.type == SeerrStorageState::PickerCommandType::Selected);
    assert(pickerCommand.selection);
    assert(pickerCommand.selection->item.id == movie.id);
    assert(pickerCommand.selection->target.serverId == 1);
    assert(!state.pendingRequest());
    assert(state.driveChoices().empty());

    assert(state.preparePicker(movie) == SeerrStorageState::PickerStatus::Ready);
    pickerCommand = state.handlePickerInput(SeerrStorageState::PickerInput::Back);
    assert(pickerCommand.type == SeerrStorageState::PickerCommandType::Back);
    assert(!pickerCommand.selection);
    assert(!state.pendingRequest());
    assert(state.driveChoices().empty());

    state.clearTargets();
    assert(state.empty());
    state.clearRefreshDeadline();
    assert(state.refreshDeadline() == SeerrStorageState::TimePoint{});
    state.resetUnavailable();
    assert(!state.loading());
    return 0;
}
