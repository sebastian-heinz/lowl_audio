#include <lowl.h>

#include <iostream>
#include <thread>
#include <chrono>

void play(std::shared_ptr<Lowl::Audio::AudioDevice> device) {
    Lowl::Error error;
    std::shared_ptr<Lowl::Audio::AudioData> data = Lowl::Lib::create_data("audio_path", error);
    if (error.has_error()) {
        std::cout << "Err:  Lowl::create_stream\n";
        return;
    }

    device->start(data, error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return;
    }

    while (data->get_frames_remaining() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "==PLAYING==\n";
        std::cout << "frames remaining: \n" + std::to_string(data->get_frames_remaining()) + "\n";
    }
}

void mix(std::shared_ptr<Lowl::Audio::AudioDevice> device) {
    Lowl::Error error;
    std::shared_ptr<Lowl::Audio::AudioData> data_1 = Lowl::Lib::create_data("audio_path_1", error);
    if (error.has_error()) {
        std::cout << "Err: mix:audio_path_1 -> Lowl::create_stream\n";
        return;
    }

    std::shared_ptr<Lowl::Audio::AudioData> data_2 = Lowl::Lib::create_data("audio_path_2", error);
    if (error.has_error()) {
        std::cout << "Err: mix:audio_path_2 -> Lowl::create_stream\n";
        return;
    }

    std::shared_ptr<Lowl::Audio::AudioMixer> mixer = std::make_unique<Lowl::Audio::AudioMixer>(
            data_1->get_sample_rate(),
            data_1->get_channel());

    mixer->mix(data_1);
    mixer->mix(data_2);


    device->start(mixer, error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return;
    }

    while (mixer->get_frames_remaining() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "==PLAYING==\n";
        std::cout << "frames remaining: \n" + std::to_string(mixer->get_frames_remaining()) + "\n";
    }
}

/**
 * example on how to use space
 */
void space(std::shared_ptr<Lowl::Audio::AudioDevice> device) {
    std::shared_ptr<Lowl::Audio::AudioSpace> space = std::make_shared<Lowl::Audio::AudioSpace>(44100.0,
                                                                                               Lowl::Audio::AudioChannel::Stereo);
    Lowl::Error error;

    Lowl::SpaceId opus = space->add_audio("/Users/railgun/Downloads/sample1.opus", error);
    Lowl::SpaceId wav = space->add_audio("/Users/railgun/Downloads/audio/CantinaBand60.wav", error);
    Lowl::SpaceId wav2 = space->add_audio("/Users/railgun/Downloads/audio/StarWars60.wav", error);
    Lowl::SpaceId ogg = space->add_audio("/Users/railgun/Downloads/audio/OverThePeriod.ogg", error);
    if (error.has_error()) {
        std::cout << "Err: space->add_audio\n";
        return;
    }

    device->start(space, error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return;
    }

    std::vector<bool> status;
    while (true) {

        Lowl::SpaceId selected_id = Lowl::Audio::AudioSpace::InvalidSpaceId;
        std::cout << "Select Sound:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        try {
            selected_id = std::stoul(user_input);
        } catch (const std::exception &e) {
            continue;
        }

        if (selected_id <= Lowl::Audio::AudioSpace::InvalidSpaceId) {
            std::cout << "Stop Selecting SpaceId\n";
            break;
        } else {
            if (status.size() <= selected_id) {
                status.resize(selected_id + 1);
            }
            bool playing = status[selected_id];
            if (!playing) {
                space->play(selected_id);
            } else {
                space->stop(selected_id);
            }
            status[selected_id] = !playing;
        }

        std::cout << "frames remaining: \n" + std::to_string(space->get_frames_remaining()) + "\n";
    }

    device->stop(error);
    if (error.has_error()) {
        std::cout << "Err: driver->stop\n";
        return;
    }
}

/**
 * different examples to run
 */
int run() {
    Lowl::Error error;
    Lowl::Lib::initialize(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::initialize\n";
        return -1;
    }

    std::vector<std::shared_ptr<Lowl::Audio::AudioDriver>> drivers = Lowl::Lib::get_drivers(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::get_drivers\n";
        return -1;
    }

    std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>> all_devices = std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>>();
    int current_device_index = 0;
    for (std::shared_ptr<Lowl::Audio::AudioDriver> driver : drivers) {
        std::cout << "Driver: " + driver->get_name() + "\n";
        driver->initialize(error);
        if (error.has_error()) {
            std::cout << "Err: driver->initialize (" + driver->get_name() + ")\n";
            error = Lowl::Error();
        }

        std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>> devices = driver->get_devices();
        for (std::shared_ptr<Lowl::Audio::AudioDevice> device : devices) {
            std::cout << "Device[" + std::to_string(current_device_index++) + "]: " + device->get_name() + "\n";
            all_devices.push_back(device);
        }
    }

    int selected_index = 0;
    if (false) {
        std::cout << "Select Device:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        selected_index = std::stoi(user_input);
    }

    std::shared_ptr<Lowl::Audio::AudioDevice> device = all_devices[selected_index];

    space(device);

    device->stop(error);
    if (error.has_error()) {
        std::cout << "Err: device->stop\n";
        return -1;
    }
    return 0;
}

/**
 * example how to select a device
 */
int main() {
    Lowl::Logger::set_log_level(Lowl::Logger::Level::Debug);
    Lowl::Logger::register_std_out_log_receiver();

    run();

    std::cout << "Press any key to exit..\n";
    std::string user_input;
    std::getline(std::cin, user_input);

    std::cout << "Done\n";
    return 0;
}