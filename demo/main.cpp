#include <lowl.h>

#include <iostream>
#include <thread>
#include <chrono>


std::shared_ptr<Lowl::AudioStream> play(const std::string &audio_path) {
    Lowl::Error error;
    std::shared_ptr<Lowl::AudioStream> stream = Lowl::Lib::create_stream(audio_path, error);
    if (error.has_error()) {
        std::cout << "Err:  Lowl::create_stream\n";
        return nullptr;
    }
    return stream;
}

std::shared_ptr<Lowl::AudioStream> mix(const std::string &audio_path_1, const std::string &audio_path_2) {

    Lowl::Error error;
    std::shared_ptr<Lowl::AudioStream> stream_1 = Lowl::Lib::create_stream(audio_path_1, error);
    if (error.has_error()) {
        std::cout << "Err: mix:audio_path_1 -> Lowl::create_stream\n";
        return nullptr;
    }

    std::shared_ptr<Lowl::AudioStream> stream_2 = Lowl::Lib::create_stream(audio_path_2, error);
    if (error.has_error()) {
        std::cout << "Err: mix:audio_path_2 -> Lowl::create_stream\n";
        return nullptr;
    }

    Lowl::AudioMixer *mixer = new Lowl::AudioMixer(stream_1->get_sample_rate(), stream_1->get_channel());

    mixer->mix_stream(stream_1);
    mixer->mix_stream(stream_2);
    mixer->mix_all();

#ifdef LOWL_PROFILING
    std::cout << "LOWL_PROFILING: mix_frame_count:" + std::to_string(mixer->mix_frame_count) + "\n";
    std::cout << "LOWL_PROFILING: mix_total_duration:" + std::to_string(mixer->mix_total_duration) + "\n";
    std::cout << "LOWL_PROFILING: mix_avg_duration:" + std::to_string(mixer->mix_avg_duration) + "\n";
    std::cout << "LOWL_PROFILING: mix_max_duration:" + std::to_string(mixer->mix_max_duration) + "\n";
    std::cout << "LOWL_PROFILING: mix_min_duration:" + std::to_string(mixer->mix_min_duration) + "\n";
#endif
    return mixer->get_out_stream();

}

int main() {
    std::string audio_root = "/Users/railgun/audio/";

    // mono
    // std::string audio_file = "PCM_FLOAT_32_1CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_32_1CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_24_1CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_16_1CH_SR44100_Juanitos_Exotica.wav";

    // stereo
    std::string audio_file = "PCM_FLOAT_32_2CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_32_2CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_24_2CH_SR44100_Juanitos_Exotica.wav";
    // std::string audio_file = "PCM_INT_16_2CH_SR44100_Juanitos_Exotica.wav";

    // mp3
    //std::string audio_file = "MP3_CBITRATE128_SR44100_Juanitos_Exotica.mp3";

    // flac
    //std::string audio_file = "FLAC_2CH_SR44100_Juanitos_Exotica.flac";

    ///

    std::shared_ptr<Lowl::AudioStream> stream = play(audio_root + audio_file);


    //std::shared_ptr<Lowl::AudioStream> stream = mix(
    //        "/Users/railgun/Downloads/StarWars60.wav",
    //        "/Users/railgun/Downloads/CantinaBand60.wav"
    //);

    ///

    if (!stream) {
        std::cout << "Err: failed to create stream\n";
        return -1;
    }

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

    std::cout << "Select Device:\n";
    std::string user_input;
    std::getline(std::cin, user_input);
    int selected_index = std::stoi(user_input);


    Lowl::Device *device = all_devices[selected_index];

    device->set_stream(stream, error);
    if (error.has_error()) {
        std::cout << "Err: device->set_stream\n";
        return -1;
    }
    stream->set_output_sample_rate(22000.0);
    device->start(error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return -1;
    }

    bool set = false;
    while (device->is_playing()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "==PLAYING==\n";
        std::cout << "frames_played: \n" + std::to_string(stream->get_num_frame_queued()) + "\n";
#ifdef LOWL_PROFILING
        std::cout << "LOWL_PROFILING: produce_count:" + std::to_string(stream->produce_count) + "\n";
        std::cout << "LOWL_PROFILING: produce_total_duration:" + std::to_string(stream->produce_total_duration) + "\n";
        std::cout << "LOWL_PROFILING: produce_max_duration:" + std::to_string(stream->produce_max_duration) + "\n";
        std::cout << "LOWL_PROFILING: produce_min_duration:" + std::to_string(stream->produce_min_duration) + "\n";
        std::cout << "LOWL_PROFILING: produce_avg_duration:" + std::to_string(stream->produce_avg_duration) + "\n";
#endif
        // if (!set && stream->get_num_frame_queued() < 9000000) {
        //     stream->set_output_sample_rate(22000.0);
        //     set = true;
        // }
    }

    device->stop(error);
    if (error.has_error()) {
        std::cout << "Err: driver->stop\n";
        return -1;
    }

    std::cout << "Done\n";
    return 0;
}