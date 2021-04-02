#include <lowl.h>


#include <iostream>
#include <thread>
#include <chrono>


void play(Lowl::Device *device) {
    Lowl::Error error;
    std::shared_ptr<Lowl::AudioData> data = Lowl::Lib::create_data("audio_path", error);
    if (error.has_error()) {
        std::cout << "Err:  Lowl::create_stream\n";
        return;
    }

    std::shared_ptr<Lowl::AudioStream> stream = data->to_stream();
    device->start_stream(stream, error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return;
    }

    while (device->is_playing()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "==PLAYING==\n";
        std::cout << "frames remaining: \n" + std::to_string(stream->get_num_frame_queued()) + "\n";
#ifdef LOWL_PROFILING
        // std::cout << "LOWL_PROFILING: produce_count:" + std::to_string(stream->produce_count) + "\n";
        // std::cout << "LOWL_PROFILING: produce_total_duration:" + std::to_string(stream->produce_total_duration) + "\n";
        // std::cout << "LOWL_PROFILING: produce_max_duration:" + std::to_string(stream->produce_max_duration) + "\n";
        // std::cout << "LOWL_PROFILING: produce_min_duration:" + std::to_string(stream->produce_min_duration) + "\n";
        // std::cout << "LOWL_PROFILING: produce_avg_duration:" + std::to_string(stream->produce_avg_duration) + "\n";
        std::cout << "==\n";
        std::cout << "LOWL_PROFILING: callback_count:" + std::to_string(device->callback_count) + "\n";
        std::cout
                << "LOWL_PROFILING: callback_total_duration:" + std::to_string(device->callback_total_duration) + "\n";
        std::cout << "LOWL_PROFILING: callback_max_duration:" + std::to_string(device->callback_max_duration) + "\n";
        std::cout << "LOWL_PROFILING: callback_min_duration:" + std::to_string(device->callback_min_duration) + "\n";
        std::cout << "LOWL_PROFILING: callback_avg_duration:" + std::to_string(device->callback_avg_duration) + "\n";
        std::cout << "LOWL_PROFILING: time_request_ms:" + std::to_string(device->time_request_ms) + "\n";
#endif
    }
}

void node(Lowl::Device *device) {
    // create stream
    Lowl::Error error;
    std::shared_ptr<Lowl::AudioData> data = Lowl::Lib::create_data("audio_path", error);
    if (error.has_error()) {
        std::cout << "Err:  Lowl::create_stream\n";
        return;
    }
    std::shared_ptr<Lowl::AudioStream> stream = data->to_stream();

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
    device->start_stream(out->get_stream(), error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return;
    }

    while (device->is_playing()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "==PLAYING==\n";
        std::cout << "frames remaining: \n" + std::to_string(stream->get_num_frame_queued()) + "\n";
    }
}

void mix(Lowl::Device *device) {
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

    mixer->mix_stream(data_1->to_stream());
    mixer->mix_stream(data_2->to_stream());
    // mixer->mix_all();

//#ifdef LOWL_PROFILING
//    std::cout << "LOWL_PROFILING: mix_frame_count:" + std::to_string(mixer->mix_frame_count) + "\n";
//    std::cout << "LOWL_PROFILING: mix_total_duration:" + std::to_string(mixer->mix_total_duration) + "\n";
//    std::cout << "LOWL_PROFILING: mix_avg_duration:" + std::to_string(mixer->mix_avg_duration) + "\n";
//    std::cout << "LOWL_PROFILING: mix_max_duration:" + std::to_string(mixer->mix_max_duration) + "\n";
//    std::cout << "LOWL_PROFILING: mix_min_duration:" + std::to_string(mixer->mix_min_duration) + "\n";
//#endif

    device->start_mixer(mixer, error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return;
    }

    while (device->is_playing()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "==PLAYING==\n";
        std::cout << "frames remaining: \n" + std::to_string(mixer->get_out_stream()->get_num_frame_queued()) + "\n";
    }
}

/**
 * example on how to use space
 */
void space(Lowl::Device *device) {

    std::shared_ptr<Lowl::Space> space = std::make_shared<Lowl::Space>();
    Lowl::Error error;

    space->add_audio("/Users/railgun/Downloads/CantinaBand60.wav", error);
    space->add_audio("/Users/railgun/Downloads/StarWars60.wav", error);

    space->load();

    std::shared_ptr<Lowl::AudioStream> out_stream = space->get_mixer()->get_out_stream();
    device->start_mixer(space->get_mixer(), error);
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
        } else {
            space->play(selected_id);
        }

        std::cout << "frames remaining: \n" + std::to_string(out_stream->get_num_frame_queued()) + "\n";
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
void run(Lowl::Device *device) {
    // play(device);
    space(device);
    // mix(device);
    // node(device);
}

/**
 * example how to select a device
 */
int main() {
    Lowl::Error error;
    Lowl::Lib::initialize(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::initialize\n";
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

    int selected_index = 0;
    if (false) {
        std::cout << "Select Device:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        selected_index = std::stoi(user_input);
    }

    Lowl::Device *device = all_devices[selected_index];
    run(device);

    device->stop(error);
    if (error.has_error()) {
        std::cout << "Err: driver->stop\n";
        return -1;
    }

    std::cout << "Done\n";
    return 0;
}