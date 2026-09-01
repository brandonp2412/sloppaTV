#include "navigation_stack.hpp"

#include <cassert>

enum class TestScreen { Home, Browse, Details, Player };

int main() {
    NavigationStack<TestScreen> navigation(TestScreen::Home);
    assert(navigation.current() == TestScreen::Home);
    assert(navigation.depth() == 1);

    navigation.push(TestScreen::Browse);
    navigation.push(TestScreen::Details);
    assert(navigation.current() == TestScreen::Details);
    assert(navigation.previousOr(TestScreen::Home) == TestScreen::Browse);
    assert(navigation.depth() == 3);

    navigation.push(TestScreen::Details);
    assert(navigation.depth() == 3);

    navigation.replace(TestScreen::Player);
    assert(navigation.current() == TestScreen::Player);
    assert(navigation.depth() == 3);

    assert(navigation.popOr(TestScreen::Home) == TestScreen::Browse);
    assert(navigation.popOr(TestScreen::Home) == TestScreen::Home);
    assert(navigation.popOr(TestScreen::Home) == TestScreen::Home);

    navigation.reset(TestScreen::Details);
    assert(navigation.current() == TestScreen::Details);
    assert(navigation.depth() == 1);
    return 0;
}
