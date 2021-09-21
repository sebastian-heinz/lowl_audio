#include <doctest/doctest.h>

#include <lowl.h>

#include <iostream>

TEST_CASE("Driver") {

    Lowl::Error error;
    Lowl::Lib::initialize(error);
    REQUIRE_FALSE_MESSAGE(
            error.has_error(),
            "Driver initialized"
    );

    std::vector<std::shared_ptr<Lowl::AudioDriver>> drivers = Lowl::Lib::get_drivers(error);
    REQUIRE_FALSE_MESSAGE(
            error.has_error(),
            "List of driver retrieved"
    );

    std::vector<std::shared_ptr<Lowl::AudioDevice>> all_devices = std::vector<std::shared_ptr<Lowl::AudioDevice>>();
    for (std::shared_ptr<Lowl::AudioDriver> driver : drivers) {
        driver->initialize(error);
        std::string message = "Driver Initialized: " + driver->get_name();
        REQUIRE_FALSE_MESSAGE(
                error.has_error(),
                message
        );
    }
}



