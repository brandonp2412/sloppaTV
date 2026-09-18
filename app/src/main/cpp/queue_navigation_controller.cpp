#include "queue_navigation_controller.hpp"

namespace {
QueueOverlayInput queueInput(ScreenNavigationKey key) {
    switch (key) {
    case ScreenNavigationKey::Back:
        return QueueOverlayInput::Back;
    case ScreenNavigationKey::Up:
        return QueueOverlayInput::Up;
    case ScreenNavigationKey::Down:
        return QueueOverlayInput::Down;
    case ScreenNavigationKey::Left:
        return QueueOverlayInput::Left;
    case ScreenNavigationKey::Right:
        return QueueOverlayInput::Right;
    case ScreenNavigationKey::Activate:
    case ScreenNavigationKey::Submit:
        return QueueOverlayInput::Activate;
    case ScreenNavigationKey::None:
    case ScreenNavigationKey::Search:
    case ScreenNavigationKey::Context:
        return QueueOverlayInput::None;
    }
    return QueueOverlayInput::None;
}

QueueNavigationAction changed() {
    QueueNavigationAction result;
    result.type = QueueNavigationActionType::QueueChanged;
    return result;
}
} // namespace

QueueNavigationAction QueueNavigationController::handle(PlaybackQueueState& state, ScreenNavigationKey key) {
    const QueueOverlayCommand command = state.handleOverlayInput(queueInput(key));
    if (command.type != QueueOverlayCommandType::ActivateAction) return {};

    if (command.action == 0 && queueCanPlayNow(command.selection, command.currentIndex, command.size)) {
        QueueNavigationAction result;
        result.type = QueueNavigationActionType::PlayIndex;
        result.index = command.selection;
        return result;
    }
    if (command.action == 1 && queueCanPlayNext(command.selection, command.currentIndex, command.size)) {
        return state.moveItem(command.selection, command.currentIndex + 1) ? changed() : QueueNavigationAction{};
    }
    if (command.action == 2 && queueCanMoveUp(command.selection, command.currentIndex, command.size)) {
        return state.moveItem(command.selection, command.selection - 1) ? changed() : QueueNavigationAction{};
    }
    if (command.action == 3 && queueCanMoveDown(command.selection, command.currentIndex, command.size)) {
        return state.moveItem(command.selection, command.selection + 1) ? changed() : QueueNavigationAction{};
    }
    if (command.action == 4) {
        return state.removeSelected() ? changed() : QueueNavigationAction{};
    }
    if (command.action == 5) {
        QueueNavigationAction result;
        result.type = QueueNavigationActionType::Shuffle;
        return result;
    }
    if (command.action == 6) {
        state.cycleRepeatMode();
        return changed();
    }
    return {};
}
