#include "seerr_drive_navigation_controller.hpp"

namespace {
SeerrStorageState::PickerInput pickerInput(ScreenNavigationKey key) {
    switch (key) {
    case ScreenNavigationKey::Back:
        return SeerrStorageState::PickerInput::Back;
    case ScreenNavigationKey::Up:
        return SeerrStorageState::PickerInput::Up;
    case ScreenNavigationKey::Down:
        return SeerrStorageState::PickerInput::Down;
    case ScreenNavigationKey::Activate:
    case ScreenNavigationKey::Submit:
        return SeerrStorageState::PickerInput::Activate;
    case ScreenNavigationKey::None:
    case ScreenNavigationKey::Search:
    case ScreenNavigationKey::Context:
    case ScreenNavigationKey::Left:
    case ScreenNavigationKey::Right:
        return SeerrStorageState::PickerInput::None;
    }
    return SeerrStorageState::PickerInput::None;
}
} // namespace

SeerrDriveNavigationAction SeerrDriveNavigationController::handle(SeerrStorageState& state, ScreenNavigationKey key) {
    auto command = state.handlePickerInput(pickerInput(key));
    if (command.type == SeerrStorageState::PickerCommandType::Back) {
        SeerrDriveNavigationAction result;
        result.type = SeerrDriveNavigationActionType::Back;
        return result;
    }
    if (command.type != SeerrStorageState::PickerCommandType::Selected || !command.selection) return {};

    SeerrDriveNavigationAction result;
    result.type = SeerrDriveNavigationActionType::Selected;
    result.selection = std::move(command.selection);
    return result;
}
