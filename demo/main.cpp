#include <lowl.h>

#include <iostream>
#include <thread>
#include <vector>
#include <string>


std::vector<std::string> music_paths = std::vector<std::string>();
Lowl::SampleRate sample_rate = 44100.0;
Lowl::Audio::AudioChannel channel = Lowl::Audio::AudioChannel::Stereo;
int device_index = -1;
int device_property_index = -1;

void print_audio_properties(Lowl::Audio::AudioDeviceProperties p_device_properties) {
    std::cout << "- Properties" << "\n";
    std::cout << "-- SampleRate:" << std::to_string(p_device_properties.sample_rate) << "\n";
    std::cout << "-- Channel:" << std::to_string(Lowl::Audio::get_channel_num(p_device_properties.channel)) << "\n";
    std::cout << "-- SampleFormat:" << Lowl::Audio::SampleFormatToString(p_device_properties.sample_format) << "\n";
    std::cout << "-- Exclusive:" << (p_device_properties.exclusive_mode ? "TRUE" : "FALSE") << "\n";
}

/**
 * example on how to use space
 */
void space(std::shared_ptr<Lowl::Audio::AudioDevice> device, Lowl::Audio::AudioDeviceProperties p_device_properties) {
    std::shared_ptr<Lowl::Audio::AudioSpace> space = std::make_shared<Lowl::Audio::AudioSpace>(sample_rate, channel);
    Lowl::Error error;

    for (std::string music_path: music_paths) {
        space->add_audio(music_path, error);
        if (error.has_error()) {
            std::cout << "Err: space->add_audio (" << music_path << ")\n";
            continue;
        }
    }

    std::map<Lowl::SpaceId, std::string> mapping = space->get_name_mapping();
    for (std::map<Lowl::SpaceId, std::string>::iterator it = mapping.begin(); it != mapping.end(); ++it) {
        Lowl::SpaceId space_id = it->first;
        std::string audio_name = it->second;
        std::cout << "Space Entry: " << space_id << "->" << audio_name << "\n";
    }

    device->start({}, space, error);
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
    for (std::shared_ptr<Lowl::Audio::AudioDriver> driver: drivers) {
        std::cout << "Driver: " + driver->get_name() + "\n";
        driver->initialize(error);
        if (error.has_error()) {
            std::cout << "Err: driver->initialize (" + driver->get_name() + ")\n";
            error = Lowl::Error();
        }

        std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>> devices = driver->get_devices();
        for (std::shared_ptr<Lowl::Audio::AudioDevice> device: devices) {
            std::cout << "+ Device[" + std::to_string(current_device_index++) + "]: " + device->get_name() + "\n";
            for (Lowl::Audio::AudioDeviceProperties device_properties: device->get_properties()) {
                print_audio_properties(device_properties);
            }
            all_devices.push_back(device);
        }
    }

    if (device_index <= -1) {
        std::cout << "Select Device:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        device_index = std::stoi(user_input);
    }
    if (device_index >= all_devices.size()) {
        std::cout << "selected device_index out of range\n";
        return -1;
    } else if (device_index < 0) {
        std::cout << "selected device_index out of range\n";
        return -1;
    }

    std::shared_ptr<Lowl::Audio::AudioDevice> device = all_devices[device_index];
    std::cout << "Selected Device:" << device_index << " Name:" << device->get_name() << "\n";

    std::vector<Lowl::Audio::AudioDeviceProperties> device_properties_list = device->get_properties();
    if (device_property_index <= -1) {
        std::cout << "Select Device Properties:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        device_index = std::stoi(user_input);
    }
    if (device_property_index >= device_properties_list.size()) {
        std::cout << "selected device_property_index out of range\n";
        return -1;
    } else if (device_property_index < 0) {
        std::cout << "selected device_property_index out of range\n";
        return -1;
    }

    Lowl::Audio::AudioDeviceProperties device_properties = device_properties_list[device_property_index];
    std::cout << "Selected Properties" << "\n";
    print_audio_properties(device_properties);

    space(device, device_properties);

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
int main(int argc, char **argv) {

    std::string music_prefix = "-m";
    std::string sample_rate_prefix = "-sr";
    std::string channel_prefix = "-ch";
    std::string device_index_prefix = "-dev";
    std::string device_property_index_prefix = "-dev-prop";

    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind(music_prefix, 0) == 0) {
            std::string val = arg.substr(music_prefix.length());
            music_paths.push_back(val);
        }
        if (arg.rfind(sample_rate_prefix, 0) == 0) {
            std::string val = arg.substr(sample_rate_prefix.length());
            sample_rate = std::stod(val);
        }
        if (arg.rfind(channel_prefix, 0) == 0) {
            std::string val = arg.substr(channel_prefix.length());
            channel = Lowl::Audio::get_channel(std::stoi(val));
        }
        if (arg.rfind(device_index_prefix, 0) == 0) {
            std::string val = arg.substr(device_index_prefix.length());
            device_index = std::stoi(val);
        }
        if (arg.rfind(device_property_index_prefix, 0) == 0) {
            std::string val = arg.substr(device_property_index_prefix.length());
            device_property_index = std::stoi(val);
        }
    }

    std::cout << "device_index:" << device_index << "\n";
    std::cout << "device_property_index:" << device_property_index << "\n";
    std::cout << "sample_rate:" << sample_rate << "\n";
    std::cout << "channel:" << Lowl::Audio::get_channel_num(channel) << "\n";

    Lowl::Logger::set_log_level(Lowl::Logger::Level::Debug);
    Lowl::Logger::register_std_out_log_receiver();

    run();

    std::cout << "Press any key to exit..\n";
    std::string user_input;
    std::getline(std::cin, user_input);

    std::cout << "Done\n";
    return 0;
}