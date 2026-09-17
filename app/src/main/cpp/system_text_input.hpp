#pragma once

#include "unicode_text.hpp"

#include <string>
#include <utility>

enum class SystemTextInputPhase {
    Changed,
    Done,
    Cancelled,
};

struct SystemTextInputEvent {
    SystemTextInputPhase phase = SystemTextInputPhase::Changed;
    int mode = -1;
    std::string value;
};

inline SystemTextInputEvent systemTextInputEvent(SystemTextInputPhase phase, int mode, std::string value) {
    return {
        .phase = phase,
        .mode = mode,
        .value = truncateUtf8Bytes(std::move(value), 160),
    };
}
