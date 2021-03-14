#include <lowl.h>

#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::string audio_root = "/Users/railgun/dev/lowl_audio/demo/audio/";

    // mono
    // std::string audio_file = "PCM_FLOAT_32_1CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_32_1CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_24_1CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_16_1CH_SR44100_Juanitos_Exotica.wav";

    // stereo
    // std::string audio_file = "PCM_FLOAT_32_2CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_32_2CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_24_2CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_16_2CH_SR44100_Juanitos_Exotica.wav";

   std::string audio_file = "MP3_CBITRATE128_SR44100_Juanitos_Exotica.mp3";


    Lowl::Error error;
    Lowl::Lib::initialize(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::initialize\n";
        return -1;
    }
    std::string audio_path = audio_root + audio_file;
    std::unique_ptr<Lowl::AudioStream> stream = Lowl::Lib::create_stream(audio_path, error);

    if (error.has_error()) {
        std::cout << "Err:  Lowl::create_stream\n";
        return -1;
    }

    std::vector<Lowl::Driver *> drivers = Lowl::Lib::get_drivers(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::get_drivers\n";
        return -1;
    }

    std::vector<Lowl::Device *> all_devices = std::vector<Lowl::Device *>();
    int current_device_index = 0;
    for (Lowl::Driver *driver : drivers) {
        std::cout << "Driver: " + driver->get_name() + "\n";
        driver->initialize(error);
        if (error.has_error()) {
            std::cout << "Err: driver->initialize (" + driver->get_name() + ")\n";
            error = Lowl::Error();
        }
        std::vector<Lowl::Device *> devices = driver->get_devices();
        for (Lowl::Device *device : devices) {
            std::cout << "Device[" + std::to_string(current_device_index++) + "]: " + device->get_name() + "\n";
            all_devices.push_back(device);
        }
    }

    std::cout << "Select Device:\n";
    // std::string user_input;
    // std::getline(std::cin, user_input);
    // int selected_index = std::stoi(user_input);
    int selected_index = 1;

    Lowl::Device *device = all_devices[selected_index];
    device->set_stream(std::move(stream), error);
    if (error.has_error()) {
        std::cout << "Err: device->set_stream\n";
        return -1;
    }

    device->start(error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return -1;
    }

    while (device->is_playing()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "frames_played: \n" + std::to_string(device->frames_played()) + "\n";
    }

    device->stop(error);
    if (error.has_error()) {
        std::cout << "Err: driver->stop\n";
        return -1;
    }

    std::cout << "Done\n";
    return 0;
}