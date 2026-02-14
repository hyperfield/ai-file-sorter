#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/catch_test_case_info.hpp>

#include <iostream>

class TestNamePrinter : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testCaseStarting(Catch::TestCaseInfo const& info) override {
        std::cout << "[TEST] " << info.name << std::endl;
    }
};

CATCH_REGISTER_LISTENER(TestNamePrinter)
