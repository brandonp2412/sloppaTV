#include "system_text_input.hpp"

#include <cassert>
#include <string>

int main() {
    auto changed = systemTextInputEvent(SystemTextInputPhase::Changed, 4, "query");
    assert(changed.phase == SystemTextInputPhase::Changed);
    assert(changed.mode == 4);
    assert(changed.value == "query");

    std::string oversized(200, 'a');
    auto done = systemTextInputEvent(SystemTextInputPhase::Done, 7, oversized);
    assert(done.phase == SystemTextInputPhase::Done);
    assert(done.mode == 7);
    assert(done.value.size() == 160);

    auto cancelled = systemTextInputEvent(SystemTextInputPhase::Cancelled, 2, "cancelled");
    assert(cancelled.phase == SystemTextInputPhase::Cancelled);
    assert(cancelled.mode == 2);
    assert(cancelled.value == "cancelled");
    return 0;
}
