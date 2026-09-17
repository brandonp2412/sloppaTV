#include "async_completion_queue.hpp"

#include <cassert>
#include <string>
#include <thread>
#include <variant>
#include <vector>

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

    constexpr int producerCount = 4;
    constexpr int eventsPerProducer = 50;
    std::vector<std::thread> producers;
    for (int producer = 0; producer < producerCount; ++producer) {
        producers.emplace_back([&queue, producer] {
            for (int event = 0; event < eventsPerProducer; ++event) {
                queue.push(NumberCompletion{.value = producer * eventsPerProducer + event});
            }
        });
    }
    for (auto& producer : producers) producer.join();

    completions = queue.takeAll();
    assert(completions.size() == static_cast<size_t>(producerCount * eventsPerProducer));
    long long sum = 0;
    for (const auto& completion : completions) sum += std::get<NumberCompletion>(completion).value;
    const long long eventCount = producerCount * eventsPerProducer;
    assert(sum == (eventCount - 1) * eventCount / 2);
    assert(queue.takeAll().empty());
    return 0;
}
