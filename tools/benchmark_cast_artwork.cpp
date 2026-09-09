#include "jellyfin_types.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

static size_t consume(std::string_view id, std::string_view imageTag) {
    return id.size() + imageTag.size();
}

static size_t baseline(const JellyfinPerson& person) {
    JellyfinItem item;
    item.id = person.id;
    item.name = person.name;
    item.type = "Person";
    item.imageTag = person.imageTag;
    return consume(item.id, item.imageTag);
}

static size_t optimized(const JellyfinPerson& person) {
    return consume(person.id, person.imageTag);
}

template <typename Function>
double measure(const std::vector<JellyfinPerson>& people, int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (const auto& person : people) checksum += function(person);
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 5000000;
    std::vector<JellyfinPerson> people;
    for (int index = 0; index < 10; ++index) {
        JellyfinPerson person;
        person.id = "person-id-" + std::to_string(index) + "-0123456789abcdef";
        person.name = "Representative Performer Name " + std::to_string(index);
        person.role = "Representative Character Name " + std::to_string(index);
        person.imageTag = "image-tag-0123456789abcdef" + std::to_string(index);
        people.push_back(std::move(person));
    }
    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measure(people, iterations, baseline, baselineChecksum);
    const double optimizedMs = measure(people, iterations, optimized, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
