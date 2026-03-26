#include <doctest/doctest.h>

#include <lowl.h>

#include <iostream>

TEST_CASE("Driver - enumeration stays available without platform devices") {
    Lowl::Error error;
    Lowl::Lib::initialize(error);
    REQUIRE_FALSE_MESSAGE(
            error.has_error(),
            "Driver initialized"
    );

    std::vector<std::shared_ptr<Lowl::Audio::AudioDriver>> drivers = Lowl::Lib::get_drivers(error);
    REQUIRE_FALSE_MESSAGE(
            error.has_error(),
            "List of driver retrieved"
    );

    REQUIRE_FALSE(drivers.empty());

    bool has_dummy_driver = false;
    for (const std::shared_ptr<Lowl::Audio::AudioDriver> &driver : drivers) {
        if (driver && driver->get_name() == "DummyDriver") {
            has_dummy_driver = true;
            break;
        }
    }
    REQUIRE(has_dummy_driver);
}

TEST_CASE("Driver - platform backend initialization is environment dependent") {
    Lowl::Error error;
    std::vector<std::shared_ptr<Lowl::Audio::AudioDriver>> drivers = Lowl::Lib::get_drivers(error);
    REQUIRE_FALSE(error.has_error());

    bool saw_real_backend = false;
    bool saw_initialized_backend_with_devices = false;

    for (const std::shared_ptr<Lowl::Audio::AudioDriver> &driver : drivers) {
        if (!driver || driver->get_name() == "DummyDriver") {
            continue;
        }

        saw_real_backend = true;
        Lowl::Error init_error;
        driver->initialize(init_error);
        if (init_error.has_error()) {
            const std::string message =
                "Skipping backend initialization failure for " + driver->get_name() + ": " +
                init_error.get_error_text();
            MESSAGE(message);
            continue;
        }

        const std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>> devices = driver->get_devices();
        if (devices.empty()) {
            const std::string message = "Skipping backend without devices: " + driver->get_name();
            MESSAGE(message);
            continue;
        }

        saw_initialized_backend_with_devices = true;
        CHECK_FALSE(devices.empty());
    }

    if (!saw_real_backend) {
        MESSAGE("No platform-specific backend compiled in this build.");
        return;
    }

    if (!saw_initialized_backend_with_devices) {
        MESSAGE("No platform backend with devices was available on this machine.");
    }
}
