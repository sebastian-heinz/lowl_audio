#include <lowl.h>


#include <iostream>
#include <thread>
#include <chrono>


void play(std::shared_ptr<Lowl::Device> device) {
    Lowl::Error error;
    std::shared_ptr<Lowl::AudioData> data = Lowl::Lib::create_data("audio_path", error);
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

void node(std::shared_ptr<Lowl::Device> device) {
    // create stream
    Lowl::Error error;
    std::shared_ptr<Lowl::AudioData> data = Lowl::Lib::create_data("audio_path", error);
    if (error.has_error()) {
        std::cout << "Err:  Lowl::create_stream\n";
        return;
    }

    std::shared_ptr<Lowl::AudioStream> stream = Lowl::AudioUtil::to_stream(data);

    // create nodes
    std::shared_ptr<Lowl::NodeInStream> in = std::make_shared<Lowl::NodeInStream>(stream);

    Lowl::SampleRate out_sample_rate = 22100;
    std::shared_ptr<Lowl::NodeReSampler> sampler = std::make_shared<Lowl::NodeReSampler>(
            stream->get_sample_rate(),
            out_sample_rate,
            stream->get_channel(),
            32,
            8
    );

    std::shared_ptr<Lowl::NodeOutStream> out = std::make_shared<Lowl::NodeOutStream>(
            out_sample_rate, stream->get_channel()
    );

    in->connect(sampler)->connect(out);


    // play
    device->start(out->get_stream(), error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return;
    }

    while (out->get_stream()->get_frames_remaining() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "==PLAYING==\n";
        std::cout << "frames remaining: \n" + std::to_string(out->get_stream()->get_frames_remaining()) + "\n";
    }
}

void mix(std::shared_ptr<Lowl::Device> device) {
    Lowl::Error error;
    std::shared_ptr<Lowl::AudioData> data_1 = Lowl::Lib::create_data("audio_path_1", error);
    if (error.has_error()) {
        std::cout << "Err: mix:audio_path_1 -> Lowl::create_stream\n";
        return;
    }

    std::shared_ptr<Lowl::AudioData> data_2 = Lowl::Lib::create_data("audio_path_2", error);
    if (error.has_error()) {
        std::cout << "Err: mix:audio_path_2 -> Lowl::create_stream\n";
        return;
    }

    std::shared_ptr<Lowl::AudioMixer> mixer = std::make_unique<Lowl::AudioMixer>(data_1->get_sample_rate(),
                                                                                 data_1->get_channel());

    mixer->mix_data(data_1);
    mixer->mix_data(data_2);


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
void space(std::shared_ptr<Lowl::Device> device) {

    std::shared_ptr<Lowl::Space> space = std::make_shared<Lowl::Space>();
    Lowl::Error error;

    space->add_audio("/Users/railgun/Downloads/CantinaBand60.wav", error);
    space->add_audio("/Users/railgun/Downloads/StarWars60.wav", error);

    space->load();

    device->start(space->get_mixer(), error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return;
    }

    while (true) {

        Lowl::SpaceId selected_id = Lowl::Space::InvalidSpaceId;
        std::cout << "Select Sound:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        try {
            selected_id = std::stoul(user_input);
        } catch (const std::exception &e) {
            continue;
        }

        if (selected_id <= Lowl::Space::InvalidSpaceId) {
            std::cout << "Stop Selecting SpaceId\n";
            break;
        } else if (selected_id == 3) {
            space->stop(1);
        } else {
            space->play(selected_id);
        }

        std::cout << "frames remaining: \n" + std::to_string(space->get_mixer()->get_frames_remaining()) + "\n";
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
void run(std::shared_ptr<Lowl::Device> device) {
    // play(device);
    space(device);
    // mix(device);
    // node(device);
}

/**
 * example how to select a device
 */
int main() {
    Lowl::Logger::set_log_level(Lowl::Logger::Level::Debug);
    Lowl::Logger::register_std_out_log_receiver();
    Lowl::Profiler::register_std_out_profiling_receiver(10 * 1000);
    Lowl::Profiler::start();

    Lowl::Error error;
    Lowl::Lib::initialize(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::initialize\n";
        return -1;
    }

    std::vector<std::shared_ptr<Lowl::Driver>> drivers = Lowl::Lib::get_drivers(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::get_drivers\n";
        return -1;
    }

    std::vector<std::shared_ptr<Lowl::Device>> all_devices = std::vector<std::shared_ptr<Lowl::Device>>();
    int current_device_index = 0;
    for (std::shared_ptr<Lowl::Driver> driver : drivers) {
        std::cout << "Driver: " + driver->get_name() + "\n";
        driver->initialize(error);
        if (error.has_error()) {
            std::cout << "Err: driver->initialize (" + driver->get_name() + ")\n";
            error = Lowl::Error();
        }

        std::vector<std::shared_ptr<Lowl::Device>> devices = driver->get_devices();
        for (std::shared_ptr<Lowl::Device> device : devices) {
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

    std::shared_ptr<Lowl::Device> device = all_devices[selected_index];
    run(device);

    device->stop(error);
    if (error.has_error()) {
        std::cout << "Err: driver->stop\n";
        return -1;
    }

    std::cout << "Done\n";
    return 0;
}