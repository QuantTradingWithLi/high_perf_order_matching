#include <iostream>
#include <string>
#include <thread>
#include <pthread.h>
#include <sched.h>
#include <atomic>
#include <memory>
#include "MatchingEngine.hpp"
#include "FastIO.hpp"
#include "SPSCQueue.hpp"

// SPSC queue for lock-free communication between reader and engine
SPSCQueue<std::string, 4096> inputQueue;
std::atomic<bool> doneReading{false};

void setCoreAffinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void readerThread() {
    // Pin Reader to Core 0
    setCoreAffinity(0);
    
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line[0] == '#') continue;
        while (!inputQueue.push(line)) {
            std::this_thread::yield(); 
        }
    }
    doneReading.store(true, std::memory_order_release);
}

int main() {
    // Fast I/O
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    // Pin Engine Thread to Core 1
    setCoreAffinity(1);

    std::thread reader(readerThread);
    auto engine = std::make_unique<MatchingEngine>(1000000);

    std::string line;
    while (true) {
        if (inputQueue.pop(line)) {
            ParsedMessage msg = FastIO::parseLine(line);
            
            if (msg.error_code == 0) {
                engine->addOrder(msg.id, msg.side, msg.quantity, msg.price);
            } else if (msg.error_code == 1) {
                engine->cancelOrder(msg.id);
            }
			else {
                std::cerr << "Error " << msg.error_code << " processing input: " << line << std::endl;
            }
        } else if (doneReading.load(std::memory_order_acquire)) {
            break;
        }
    }

    reader.join();
    return 0;
}