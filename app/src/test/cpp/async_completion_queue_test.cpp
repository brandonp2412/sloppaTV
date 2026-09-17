#include "async_completion_queue.hpp"

#include <cassert>
#include <string>
#include <variant>

namespace {
struct NumberCompletion {
    int value = 0;
};

struct TextCompletion {
    std::string value;
};

using Completion = std::variant<NumberCompletion, TextCompletion>;
} // namespace

int main() {
    AsyncCompletionQueue<Completion> queue;
    assert(queue.takeAll().empty());

    queue.push(NumberCompletion{.value = 7});
    queue.push(TextCompletion{.value = "done"});
    auto completions = queue.takeAll();
    assert(completions.size() == 2);
    assert(std::get<NumberCompletion>(completions[0]).value == 7);
    assert(std::get<TextCompletion>(completions[1]).value == "done");
    assert(queue.takeAll().empty());

    queue.push(NumberCompletion{.value = 11});
    completions = queue.takeAll();
    assert(completions.size() == 1);
    assert(std::get<NumberCompletion>(completions.front()).value == 11);
    return 0;
}
