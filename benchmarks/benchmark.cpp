#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "ShrdPtr.h"
#include "Stack.h"
#include "UnqPtr.h"

extern "C" void observe_pointer(const void*);
using Clock = std::chrono::steady_clock;
using Sum = std::uint64_t;

Sum expected(std::size_t n) {
    Sum result = 0;
    for (std::size_t i = 0; i < n; ++i) result += i % 97 + 1;
    return result;
}

template<class F> double timed(F&& function) {
    const auto start = Clock::now();
    function();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

struct Result { double ms; Sum checksum; std::size_t operations; };

template<class Pointer, bool MakeShared = false>
Pointer make_pointer(int value) {
    if constexpr (MakeShared) return std::make_shared<int>(value);
    else return Pointer(new int(value));
}

template<class Pointer, bool MakeShared = false>
Result scalar(const std::string& scenario, std::size_t n) {
    Sum sum = 0;
    if (scenario == "lifecycle") {
        const double ms = timed([&] {
            std::vector<Pointer> pointers(n);
            for (std::size_t i = 0; i < n; ++i)
                pointers[i] = make_pointer<Pointer, MakeShared>(int(i % 97 + 1));
            observe_pointer(pointers.data());
            for (const auto& pointer : pointers) sum += *pointer;
        });
        return {ms, sum, n};
    }
    std::vector<Pointer> pointers(n);
    if (scenario == "allocate") {
        const double ms = timed([&] {
            for (std::size_t i = 0; i < n; ++i)
                pointers[i] = make_pointer<Pointer, MakeShared>(int(i % 97 + 1));
            observe_pointer(pointers.data());
        });
        for (const auto& pointer : pointers) sum += *pointer;
        return {ms, sum, n};
    }
    for (std::size_t i = 0; i < n; ++i)
        pointers[i] = make_pointer<Pointer, MakeShared>(int(i % 97 + 1));
    observe_pointer(pointers.data());
    if (scenario == "read") {
        constexpr int passes = 32;
        const double ms = timed([&] {
            for (int pass = 0; pass < passes; ++pass) {
                observe_pointer(pointers.data());
                for (const auto& pointer : pointers) sum += *pointer;
            }
        });
        if (sum != expected(n) * passes) throw std::runtime_error("read checksum");
        return {ms, sum / passes, n * passes};
    }
    if (scenario == "move") {
        std::vector<Pointer> destination(n);
        constexpr int passes = 16;
        const double ms = timed([&] {
            for (int pass = 0; pass < passes; ++pass) {
                for (std::size_t i = 0; i < n; ++i)
                    destination[i] = std::move(pointers[i]);
                observe_pointer(destination.data());
                observe_pointer(pointers.data());
                for (std::size_t i = 0; i < n; ++i)
                    pointers[i] = std::move(destination[i]);
                observe_pointer(pointers.data());
                observe_pointer(destination.data());
            }
        });
        for (std::size_t i = 0; i < n; ++i) {
            if (destination[i].get() != nullptr) throw std::runtime_error("move source");
            sum += *pointers[i];
        }
        return {ms, sum, n * passes * 2};
    }
    if constexpr (std::is_copy_constructible_v<Pointer>) {
        if (scenario == "copy") {
            constexpr int passes = 16;
            const double ms = timed([&] {
                for (int pass = 0; pass < passes; ++pass) {
                    std::vector<Pointer> copies(pointers);
                    observe_pointer(copies.data());
                    observe_pointer(pointers.data());
                }
            });
            for (const auto& pointer : pointers) {
                if (pointer.use_count() != 1) throw std::runtime_error("copy count");
                sum += *pointer;
            }
            return {ms, sum, n * passes};
        }
    }
    throw std::runtime_error("unknown scenario");
}

template<class Pointer> Result array_lifecycle(std::size_t n) {
    Sum sum = 0;
    constexpr int passes = 32;
    const double ms = timed([&] {
        for (int pass = 0; pass < passes; ++pass) {
            Pointer pointer(new int[n]);
            for (std::size_t i = 0; i < n; ++i) pointer[i] = int(i % 97 + 1);
            observe_pointer(pointer.get());
            for (std::size_t i = 0; i < n; ++i) sum += pointer[i];
        }
    });
    if (sum != expected(n) * passes) throw std::runtime_error("array checksum");
    return {ms, sum / passes, n * passes};
}

// Same linked-list algorithm and node layout as Stack, with std::unique_ptr.
class StandardStack {
    struct Node { int value; std::unique_ptr<Node> next; explicit Node(int v): value(v) {} };
    std::unique_ptr<Node> head;
public:
    ~StandardStack() { while (head) pop(); }
    void push(const int& value) {
        auto current = std::unique_ptr<Node>(new Node(value));
        current->next = std::move(head);
        head = std::move(current);
    }
    void pop() { auto current = std::move(head); head = std::move(current->next); }
    int& top() { return head->value; }
};

template<class S> Result stack_lifecycle(std::size_t n) {
    Sum sum = 0;
    const double ms = timed([&] {
        S stack;
        for (std::size_t i = 0; i < n; ++i) stack.push(int(i % 97 + 1));
        observe_pointer(&stack);
        for (std::size_t i = 0; i < n; ++i) { sum += stack.top(); stack.pop(); }
    });
    return {ms, sum, n};
}

struct Case {
    std::string scenario, implementation;
    std::size_t n;
    std::function<Result()> run;
};

int main(int argc, char** argv) try {
    const std::string output = argc > 1 ? argv[1] : "benchmark.csv";
    const int repetitions = argc > 2 ? std::stoi(argv[2]) : 21;
    if (repetitions < 3) throw std::runtime_error("at least 3 repetitions required");
    constexpr int warmups = 3;
    constexpr unsigned seed = 20261001;
    std::vector<Case> cases;
    for (const std::size_t n : {1000u, 10000u, 100000u, 1000000u}) {
        for (const auto& scenario : {"lifecycle", "allocate", "read", "move"}) {
            cases.push_back({scenario, "UnqPtr", n, [=] { return scalar<UnqPtr<int>>(scenario, n); }});
            cases.push_back({scenario, "std::unique_ptr", n, [=] { return scalar<std::unique_ptr<int>>(scenario, n); }});
            cases.push_back({scenario, "ShrdPtr", n, [=] { return scalar<ShrdPtr<int>>(scenario, n); }});
            cases.push_back({scenario, "std::shared_ptr", n, [=] { return scalar<std::shared_ptr<int>>(scenario, n); }});
            cases.push_back({scenario, "std::make_shared", n, [=] { return scalar<std::shared_ptr<int>, true>(scenario, n); }});
        }
        cases.push_back({"copy", "ShrdPtr", n, [=] { return scalar<ShrdPtr<int>>("copy", n); }});
        cases.push_back({"copy", "std::shared_ptr", n, [=] { return scalar<std::shared_ptr<int>>("copy", n); }});
        cases.push_back({"copy", "std::make_shared", n, [=] { return scalar<std::shared_ptr<int>, true>("copy", n); }});
        cases.push_back({"array", "UnqPtr", n, [=] { return array_lifecycle<UnqPtr<int[]>>(n); }});
        cases.push_back({"array", "std::unique_ptr", n, [=] { return array_lifecycle<std::unique_ptr<int[]>>(n); }});
        cases.push_back({"array", "ShrdPtr", n, [=] { return array_lifecycle<ShrdPtr<int[]>>(n); }});
        cases.push_back({"array", "std::shared_ptr", n, [=] { return array_lifecycle<std::shared_ptr<int[]>>(n); }});
        cases.push_back({"stack", "Stack<UnqPtr>", n, [=] { return stack_lifecycle<Stack<int>>(n); }});
        cases.push_back({"stack", "Stack<std::unique_ptr>", n, [=] { return stack_lifecycle<StandardStack>(n); }});
    }
    std::ofstream csv(output);
    if (!csv) throw std::runtime_error("cannot open output");
    csv << "scenario,implementation,n,repetition,time_ms,operations,ns_per_operation,checksum\n";
    csv << std::setprecision(12);
    std::mt19937 random(seed);
    for (int repetition = -warmups; repetition < repetitions; ++repetition) {
        std::shuffle(cases.begin(), cases.end(), random);
        for (const auto& test : cases) {
            const auto result = test.run();
            if (result.checksum != expected(test.n)) throw std::runtime_error("checksum mismatch");
            if (result.ms <= 0) throw std::runtime_error("timer resolution too low");
            if (repetition >= 0)
                csv << test.scenario << ',' << test.implementation << ',' << test.n << ','
                    << repetition << ',' << result.ms << ',' << result.operations << ','
                    << result.ms * 1e6 / result.operations << ',' << result.checksum << '\n';
        }
        std::cerr << (repetition < 0 ? "Warmup " : "Repeat ")
                  << (repetition < 0 ? repetition + warmups + 1 : repetition + 1) << " done\n";
    }
    csv.flush();
    if (!csv) throw std::runtime_error("failed to write output");
    std::cout << "Saved " << cases.size() * repetitions << " validated samples to " << output << '\n';
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
