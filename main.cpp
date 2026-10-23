#include <atomic>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

#include "FastIO.hpp"
#include "MatchingEngine.hpp"
#include "SPSCQueue.hpp"

namespace {

constexpr size_t INPUT_QUEUE_CAPACITY = 4096;

struct Pipeline {
    SPSCQueue<ParsedMessage, INPUT_QUEUE_CAPACITY> inputQueue;
    std::atomic<bool> doneReading{false};
};

void spinPause() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#else
    std::this_thread::yield();
#endif
}

#if defined(__linux__)
bool setCoreAffinity(int core_id) noexcept {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),
                                  &cpuset) == 0;
}
#endif

void readerThread(Pipeline& pipeline) {
#if defined(__linux__)
    if (!setCoreAffinity(0)) {
        std::fprintf(stderr, "warning: could not pin reader to core 0\n");
    }
#endif

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line[0] == '#') continue;

        const ParsedMessage msg = FastIO::parseLine(line);
        if (msg.error_code == 0 || msg.error_code == 1) {
            while (!pipeline.inputQueue.push(msg)) spinPause();
        } else {
            std::fprintf(stderr, "Error %d processing input: %s\n",
                         msg.error_code, line.c_str());
        }
    }

    pipeline.doneReading.store(true, std::memory_order_release);
}

} // namespace

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Pipeline pipeline;

#if defined(__linux__)
    if (!setCoreAffinity(1)) {
        std::fprintf(stderr, "warning: could not pin engine to core 1\n");
    }
#endif

    std::thread reader(readerThread, std::ref(pipeline));
    auto engine = std::make_unique<MatchingEngine>(1000000);

    auto apply = [&](const ParsedMessage& msg) {
        if (msg.error_code == 0) {
            engine->addOrder(msg.id, msg.side, msg.quantity, msg.price);
        } else if (msg.error_code == 1) {
            engine->cancelOrder(msg.id);
        }
    };

    ParsedMessage msg;
    while (true) {
        if (pipeline.inputQueue.pop(msg)) {
            apply(msg);
        } else if (pipeline.doneReading.load(std::memory_order_acquire)) {
            // The producer only sets done after its final push. Drain once
            // more after observing done so a message that became visible
            // between the failed pop and the done check cannot be lost.
            while (pipeline.inputQueue.pop(msg)) apply(msg);
            break;
        } else {
            spinPause();
        }
    }

    reader.join();
    FastIO::flush();
    return 0;
}
