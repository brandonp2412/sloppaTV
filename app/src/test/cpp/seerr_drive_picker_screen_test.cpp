#include "seerr_drive_picker_screen.hpp"

#include <cassert>

int main() {
    assert(seerrDrivePickerSubtitle(std::nullopt) == "Choose where Seerr should place this request");
    SeerrMediaItem item;
    item.name = "Example Movie";
    assert(seerrDrivePickerSubtitle(item) == "Choose storage for Example Movie");

    assert(seerrDrivePickerFirstVisible(0, 7) == 0);
    assert(seerrDrivePickerFirstVisible(2, 7) == 0);
    assert(seerrDrivePickerFirstVisible(4, 7) == 2);
    assert(seerrDrivePickerFirstVisible(6, 7) == 2);
    assert(seerrDrivePickerFirstVisible(3, 4) == 0);

    SeerrStorageTarget target;
    target.serviceName = "Radarr";
    target.path = "/mnt/media/movies/";
    target.totalSpace = 1000LL * 1000000000LL;
    target.freeSpace = 100LL * 1000000000LL;

    auto row = seerrDrivePickerRow(target);
    assert(row.name == "media · movies · Radarr");
    assert(row.secondaryText == "900 / 1000 GB");
    assert(row.statusText == "90% full");
    assert(row.usedPercent == 90);
    assert(row.hasCapacity);
    assert(row.nearFull);

    target.totalSpace = 0;
    target.freeSpace = 125LL * 1000000000LL;
    row = seerrDrivePickerRow(target);
    assert(row.secondaryText == "/mnt/media/movies/");
    assert(row.statusText == "125 GB free");
    assert(!row.hasCapacity);
    assert(!row.nearFull);

    target.path.clear();
    target.serviceName = "Sonarr";
    target.freeSpace = 0;
    row = seerrDrivePickerRow(target);
    assert(row.name == "Sonarr");
    assert(row.statusText == "Free space unknown");

    assert(formatSeerrStorageBytes(1500LL * 1000000000LL) == "1.5 TB");
    assert(formatSeerrStorageBytes(15000LL * 1000000000LL) == "15 TB");

    std::vector<SeerrStorageTarget> choices;
    for (int index = 0; index < 7; ++index) {
        SeerrStorageTarget choice;
        choice.serviceName = "Drive " + std::to_string(index);
        choice.path = "/mnt/drive" + std::to_string(index);
        choices.push_back(std::move(choice));
    }
    SeerrMediaItem pending;
    pending.name = "Example Show";
    const auto model = seerrDrivePickerViewModel(pending, choices, 5);
    assert(model.subtitle == "Choose storage for Example Show");
    assert(model.rows.size() == 5);
    assert(model.rows.front().row.name == "mnt · drive2 · Drive 2");
    assert(!model.rows.front().focused);
    assert(model.rows[3].row.name == "mnt · drive5 · Drive 5");
    assert(model.rows[3].focused);

    const auto emptyModel = seerrDrivePickerViewModel(std::nullopt, {}, 0);
    assert(emptyModel.rows.empty());
    assert(emptyModel.subtitle == "Choose where Seerr should place this request");
    return 0;
}
