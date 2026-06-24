#include "MatchingEngine.hpp"
#include "FastIO.hpp"
#include <cassert>
#include <iostream>
#include <memory>

void test_memory_pool_and_intrusive_list() {
    auto engine = std::make_unique<MatchingEngine>(100); 
    
    engine->addOrder(1, Side::BUY, 10, 1000000); 
    assert(engine->isOrderActive(1));

    engine->addOrder(2, Side::SELL, 10, 1000000);
    // After trade, 1 should no longer be active (filled)
    assert(!engine->isOrderActive(1));
    assert(!engine->isOrderActive(2));

    std::cout << "test_memory_pool_and_intrusive_list passed.\n";
}

void test_parser_negative_errors() {
    auto engine = std::make_unique<MatchingEngine>(100); 

    auto m1 = FastIO::parseLine("BADMESSAGE");
    assert(m1.error_code == -3); 

    auto m2 = FastIO::parseLine("99,123,0,9,100");
    assert(m2.error_code == -2); 

    auto m3 = FastIO::parseLine("0,ABC,0,9,100");
    assert(m3.error_code == -3); 

    std::cout << "test_parser_negative_errors passed.\n";
}

int main() {
    test_memory_pool_and_intrusive_list();
    test_parser_negative_errors();
    std::cout << "All tests passed successfully.\n";
    return 0;
}