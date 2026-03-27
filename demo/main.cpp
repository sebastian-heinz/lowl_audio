#include <exception>
#include <iostream>
#include <lowl.h>
#include <string>
#include <thread>
#include <vector>

namespace {
    struct DemoConfig {
        std::vector<std::string> music_paths{};
        int device_index = -1;
        int device_property_index = -1;
        bool print_all_device_properties = false;
        bool show_help = false;
    };

    bool try_parse_int(const std::string &p_text, int &p_value) {
        size_t parsed_size = 0;
        try {
            const int parsed_value = std::stoi(p_text, &parsed_size);
            if (parsed_size != p_text.size()) {
                return false;
            }
            p_value = parsed_value;
            return true;
        } catch (const std::exception &) {
            return false;
        }
    }

    bool parse_prefixed_int_arg(const std::string &p_arg, const std::string &p_prefix, int &p_value) {
        if (p_arg.rfind(p_prefix, 0) != 0) {
            return false;
        }
        const std::string value_text = p_arg.substr(p_prefix.length());
        return try_parse_int(value_text, p_value);
    }

    void print_usage(const char *p_program_name) {
        const char *program_name = p_program_name != nullptr ? p_program_name : "lowl_audio_app";
        std::cout << "Usage: " << program_name << " [options]\n";
        std::cout << "Options:\n";
        std::cout << "  -m<path>             Add an audio file to the demo playlist\n";
        std::cout << "  -dev<index>          Select the device index\n";
        std::cout << "  -dev-prop<index>     Select the device properties index\n";
        std::cout << "  --all-properties     Print every property set for each device\n";
        std::cout << "  --help, -h           Show this help text\n";
    }
} // namespace

void print_audio_properties(const Lowl::Audio::AudioDeviceProperties &p_device_properties) {
    std::cout << "-- SampleRate:" << std::to_string(p_device_properties.sample_rate) << "\n";
    std::cout << "-- Channel:" << std::to_string(p_device_properties.channel_layout.channel_count) << "\n";
    std::cout << "-- ChannelLayout:" << p_device_properties.channel_layout.to_string() << "\n";
    std::cout << "-- SampleFormat:" << Lowl::Audio::sample_format_to_string(p_device_properties.sample_format) << "\n";
    std::cout << "-- Exclusive:" << (p_device_properties.exclusive_mode ? "TRUE" : "FALSE") << "\n";
}

/**
 * example on how to use space
 */
void space(std::shared_ptr<Lowl::Audio::AudioDevice> device,
           Lowl::Audio::AudioDeviceProperties p_device_properties,
           const DemoConfig &p_config) {
    std::shared_ptr<Lowl::Audio::AudioSpace> audio_space =
        std::make_shared<Lowl::Audio::AudioSpace>(p_device_properties.sample_rate, p_device_properties.channel_layout);
    Lowl::Error error;
    struct PlaybackEntry {
        Lowl::AudioAssetHandle asset_handle = Lowl::Audio::AudioSpace::InvalidAudioAssetHandle;
        Lowl::AudioPlaybackHandle playback_handle = Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle;
    };
    std::vector<PlaybackEntry> playback_entries;

    for (const std::string &music_path : p_config.music_paths) {
        const Lowl::AudioAssetHandle asset_handle = audio_space->add_audio(music_path, error);
        if (error.has_error() || !asset_handle.is_valid()) {
            std::cout << "Err: space->add_audio (" << music_path << ")\n";
            error.clear();
            continue;
        }
        const Lowl::AudioPlaybackHandle playback_handle = audio_space->create_playback(asset_handle);
        if (!playback_handle.is_valid()) {
            std::cout << "Err: space->create_playback (" << music_path << ")\n";
            continue;
        }
        playback_entries.push_back({asset_handle, playback_handle});
    }

    std::map<Lowl::AudioAssetId, std::string> mapping = audio_space->get_name_mapping();
    for (const PlaybackEntry &entry : playback_entries) {
        const std::string audio_name =
            mapping.count(entry.asset_handle.id) > 0 ? mapping.at(entry.asset_handle.id) : std::string();
        std::cout << "Playback Entry: " << entry.playback_handle.id << "->" << audio_name << "\n";
    }

    device->start(p_device_properties, audio_space, error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return;
    }

    std::vector<bool> status;
    while (true) {
        int selected_value = 0;
        std::cout << "Select Sound:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        if (!try_parse_int(user_input, selected_value) || selected_value < 0) {
            std::cout << "invalid playback id\n";
            continue;
        }
        const Lowl::AudioPlaybackId selected_id = static_cast<Lowl::AudioPlaybackId>(selected_value);

        if (selected_id == 0) {
            std::cout << "Stop Selecting PlaybackId\n";
            break;
        } else {
            Lowl::AudioPlaybackHandle selected_handle = Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle;
            for (const PlaybackEntry &entry : playback_entries) {
                if (entry.playback_handle.id == selected_id) {
                    selected_handle = entry.playback_handle;
                    break;
                }
            }
            if (!selected_handle.is_valid()) {
                std::cout << "selected playback handle not found\n";
                continue;
            }
            if (status.size() <= selected_id) {
                status.resize(selected_id + 1);
            }
            bool playing = status[selected_id];
            if (!playing) {
                audio_space->play(selected_handle);
            } else {
                audio_space->stop(selected_handle);
            }
            status[selected_id] = !playing;
        }

        std::cout << "frames remaining: \n" + std::to_string(audio_space->get_frames_remaining()) + "\n";
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
int run(const DemoConfig &p_config) {
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

    int device_index = p_config.device_index;
    int device_property_index = p_config.device_property_index;
    std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>> all_devices =
        std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>>();
    int current_device_index = 0;
    for (const std::shared_ptr<Lowl::Audio::AudioDriver> &driver : drivers) {
        std::cout << "Driver: " + driver->get_name() + "\n";
        driver->initialize(error);
        if (error.has_error()) {
            std::cout << "Err: driver->initialize (" + driver->get_name() + ")\n";
            error = Lowl::Error();
        }

        const std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>> &devices = driver->get_devices();
        for (const std::shared_ptr<Lowl::Audio::AudioDevice> &device : devices) {
            std::cout << "+ Device[" + std::to_string(current_device_index++) + "]: " + device->get_name() + "\n";
            if (p_config.print_all_device_properties) {
                int index = 0;
                for (const Lowl::Audio::AudioDeviceProperties &device_properties : device->get_properties_list()) {
                    std::cout << "- Properties[" << index++ << "]\n";
                    print_audio_properties(device_properties);
                }
            }
            all_devices.push_back(device);
        }
    }

    while (device_index <= -1) {
        std::cout << "Select Device:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        if (!try_parse_int(user_input, device_index)) {
            std::cout << "invalid device index\n";
            device_index = -1;
        }
    }
    if (device_index < 0 || static_cast<size_t>(device_index) >= all_devices.size()) {
        std::cout << "selected device_index out of range\n";
        return -1;
    }

    std::shared_ptr<Lowl::Audio::AudioDevice> device = all_devices[device_index];
    std::cout << "Selected Device:" << device_index << " Name:" << device->get_name() << "\n";

    std::vector<Lowl::Audio::AudioDeviceProperties> device_properties_list = device->get_properties_list();
    if (device_properties_list.empty()) {
        std::cout << "device_properties_list is empty, no valid configurations available\n";
        return -1;
    }
    while (device_property_index <= -1) {
        int index = 0;
        for (const Lowl::Audio::AudioDeviceProperties &device_properties : device_properties_list) {
            std::cout << "- Properties[" << index++ << "]\n";
            print_audio_properties(device_properties);
        }
        std::cout << "Select Device Properties:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        if (!try_parse_int(user_input, device_property_index)) {
            std::cout << "invalid device properties index\n";
            device_property_index = -1;
        }
    }
    if (device_property_index < 0 || static_cast<size_t>(device_property_index) >= device_properties_list.size()) {
        std::cout << "selected device_property_index out of range\n";
        return -1;
    }

    Lowl::Audio::AudioDeviceProperties device_properties = device_properties_list[device_property_index];
    std::cout << "Selected Properties:" << "\n";
    print_audio_properties(device_properties);

    space(device, device_properties, p_config);

    device->stop(error);
    if (error.has_error()) {
        std::cout << "Err: device->stop\n";
        return -1;
    }
    return 0;
}

/**
 * example how to select a device
 *
 * -dev14
 * -ch2
 * --sr44100.0
 * -sr48000.0
 * -mC:\\Users\\railgun\\Downloads\\CantinaBand60.wav
 * -mC:\\Users\\railgun\\Downloads\\StarWars60.wav
 *
 *
 */
int main(int argc, char **argv) {
    const std::string music_prefix = "-m";
    const std::string device_index_prefix = "-dev";
    const std::string device_property_index_prefix = "-dev-prop";
    DemoConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config.show_help = true;
            continue;
        }
        if (arg == "--all-properties") {
            config.print_all_device_properties = true;
            continue;
        }
        if (arg.rfind(music_prefix, 0) == 0) {
            std::string val = arg.substr(music_prefix.length());
            config.music_paths.push_back(val);
            continue;
        }
        if (arg.rfind(device_property_index_prefix, 0) == 0) {
            if (!parse_prefixed_int_arg(arg, device_property_index_prefix, config.device_property_index)) {
                std::cout << "invalid value for " << device_property_index_prefix << "\n";
                print_usage(argv[0]);
                return -1;
            }
            continue;
        }
        if (arg.rfind(device_index_prefix, 0) == 0) {
            if (!parse_prefixed_int_arg(arg, device_index_prefix, config.device_index)) {
                std::cout << "invalid value for " << device_index_prefix << "\n";
                print_usage(argv[0]);
                return -1;
            }
            continue;
        }
    }

    if (config.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    std::cout << "device_index:" << config.device_index << "\n";
    std::cout << "device_property_index:" << config.device_property_index << "\n";

    Lowl::Logger::set_log_level(Lowl::Logger::Level::Debug);
    Lowl::Logger::register_std_out_log_receiver();

    run(config);

    std::cout << "Press any key to exit..\n";
    std::string user_input;
    std::getline(std::cin, user_input);

    std::cout << "Done\n";
    return 0;
}
