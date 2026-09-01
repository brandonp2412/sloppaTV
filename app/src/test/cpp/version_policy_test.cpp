#include "version_policy.hpp"

#include <cassert>

int main() {
    assert(jellyfinServerCompatibility("10.10.0") == ServerCompatibility::Supported);
    assert(jellyfinServerCompatibility("10.11.2") == ServerCompatibility::Supported);
    assert(jellyfinServerCompatibility("10.9.11") == ServerCompatibility::TooOld);
    assert(jellyfinServerCompatibility("10.10") == ServerCompatibility::Unknown);
    assert(jellyfinServerCompatibility("dev") == ServerCompatibility::Unknown);
    return 0;
}
