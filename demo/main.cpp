#include <iostream>
#include <string>

#include <lowl.h>

int main() {

    LowlError error;
    Lowl::initialize(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::initialize\n";
        return -1;
    }

    std::unique_ptr<LowlAudioStream> stream = Lowl::create_stream("/Users/railgun/Downloads/CantinaBand60.wav", error);
    if (error.has_error()) {
        std::cout << "Err:  Lowl::create_stream\n";
        return -1;
    }

    std::vector<LowlDriver *> drivers = Lowl::get_drivers(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::get_drivers\n";
        return -1;
    }

    std::vector<LowlDevice *> all_devices = std::vector<LowlDevice *>();
    int current_device_index = 0;
    for (LowlDriver *driver : drivers) {
        std::cout << "Driver: " + driver->get_name() + "\n";
        driver->initialize(error);
        if (error.has_error()) {
            std::cout << "Err: driver->initialize (" + driver->get_name() + ")\n";
            error = LowlError();
        }
        std::vector<LowlDevice *> devices = driver->get_devices();
        for (LowlDevice *device : devices) {
            std::cout << "Device[" + std::to_string(current_device_index++) + "]: " + device->get_name() + "\n";
            all_devices.push_back(device);
        }
    }

    std::cout << "Select Device:\n";
    std::string user_input;
    std::getline(std::cin, user_input);
    int selected_index = std::stoi(user_input);

    LowlDevice *device = all_devices[selected_index];
    device->set_stream(std::move(stream), error);
    if (error.has_error()) {
        std::cout << "Err: driver->set_stream\n";
        return -1;
    }

    device->start(error);
    if (error.has_error()) {
        std::cout << "Err: driver->start\n";
        return -1;
    }

    // wait for duration

    device->stop(error);
    if (error.has_error()) {
        std::cout << "Err: driver->stop\n";
        return -1;
    }

    std::cout << "Done\n";
    return 0;
}