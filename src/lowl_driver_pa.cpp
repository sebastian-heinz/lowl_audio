#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_driver_pa.h"
#include "lowl_device_pa.h"
#include "lowl_logger.h"

void Lowl::PaDriver::create_devices(Error &error) {
    devices.clear();

    PaDeviceIndex default_device_index = get_default_output_device_index();
    if (default_device_index == paNoDevice) {
        Logger::log(Logger::Level::Warn, "Lowl::PaDriver::create_devices: default_device_index == paNoDevice");
    }

    PaHostApiIndex api_count = Pa_GetHostApiCount();
    for (PaHostApiIndex api_index = 0; api_index < api_count; api_index++) {
        const PaHostApiInfo *api_info = Pa_GetHostApiInfo(api_index);
        for (PaDeviceIndex api_device_index = 0; api_device_index < api_info->deviceCount; api_device_index++) {
            PaDeviceIndex device_index = Pa_HostApiDeviceIndexToDeviceIndex(api_index, api_device_index);
            const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
            if (device_info->maxOutputChannels <= 0) {
                // only output
                continue;
            }
            std::string name = "[" + std::string(api_info->name) + "] " + std::string(device_info->name);
            std::shared_ptr<PaDevice> device = std::make_shared<PaDevice>();
            device->set_name(name);
            device->set_device_index(device_index);
            devices.push_back(device);

            if (device_index == default_device_index) {
                if (default_device) {
                    Logger::log(Logger::Level::Warn,
                                "Lowl::PaDriver::create_devices: default_device already assigned");
                    continue;
                }
                default_device = device;
                Logger::log(Logger::Level::Debug,
                            "Lowl::PaDriver::create_devices: default_device assigned: " + name);
            }
        }
    }
}

void Lowl::PaDriver::initialize(Error &error) {
    create_devices(error);
}

Lowl::PaDriver::PaDriver() : Driver() {
    name = std::string("Port Audio");
}

Lowl::PaDriver::~PaDriver() {
}

PaHostApiIndex Lowl::PaDriver::get_default_host_api_index() {
#ifdef LOWL_PA_DEFAULT_DRIVER_PRIORITY
    PaHostApiIndex api_count = Pa_GetHostApiCount();
    std::string default_driver_priority_setting = std::string(LOWL_PA_DEFAULT_DRIVER_PRIORITY);
    std::string delimiter = ",";
    size_t start = 0U;
    size_t end = default_driver_priority_setting.find(delimiter);
    std::vector<std::string> default_driver_priority;
    if (end != std::string::npos) {
        while (end != std::string::npos) {
            std::string default_driver = default_driver_priority_setting.substr(start, end - start);
            default_driver_priority.push_back(default_driver);
            start = end + delimiter.length();
            end = default_driver_priority_setting.find(delimiter, start);
        }
        default_driver_priority.push_back(default_driver_priority_setting.substr(start, end - start));
    } else {
        default_driver_priority.push_back(default_driver_priority_setting);
    }

    for (std::string default_driver : default_driver_priority) {
        for (PaHostApiIndex api_index = 0; api_index < api_count; api_index++) {
            const PaHostApiInfo *api_info = Pa_GetHostApiInfo(api_index);
            std::string current_driver;
            switch (api_info->type) {
                case paDirectSound:
                    current_driver = "DSOUND";
                    break;
                case paMME:
                    current_driver = "WMME";
                    break;
                case paASIO:
                    current_driver = "ASIO";
                    break;
                case paCoreAudio:
                    current_driver = "COREAUDIO";
                    break;
                case paOSS:
                    current_driver = "OSS";
                    break;
                case paALSA:
                    current_driver = "ALSA";
                    break;
                case paBeOS:
                    current_driver = "OSS";
                    break;
                case paWDMKS:
                    current_driver = "WDMKS";
                    break;
                case paJACK:
                    current_driver = "JACK";
                    break;
                case paWASAPI:
                    current_driver = "WASAPI";
                    break;
                case paAudioScienceHPI:
                case paInDevelopment:
                case paSoundManager:
                case paAL:
                default:
                    current_driver = "";
                    break;
            }
            if (default_driver == current_driver) {
                Logger::log(Logger::Level::Debug,
                            "Lowl::PaDriver::get_default_host_api_index: default host device: " + current_driver);
                return api_index;
            }
        }
    }
#endif
    return Pa_GetDefaultHostApi();
}

PaDeviceIndex Lowl::PaDriver::get_default_output_device_index() {
    PaHostApiIndex default_host_api = get_default_host_api_index();
    if (default_host_api < 0) {
        Logger::log(Logger::Level::Warn,
                    "Lowl::PaDriver::get_default_output_device_index: default_host_api < 0");
        return paNoDevice;
    }
    const PaHostApiInfo *api_info = Pa_GetHostApiInfo(default_host_api);
    if (!api_info) {
        Logger::log(Logger::Level::Warn,
                    "Lowl::PaDriver::get_default_output_device_index: !api_info");
        return paNoDevice;
    }
    if (api_info->defaultOutputDevice == paNoDevice) {
        Logger::log(Logger::Level::Warn,
                    "Lowl::PaDriver::get_default_output_device_index: api_info->defaultOutputDevice == paNoDevice");
        return paNoDevice;
    }
    return api_info->defaultOutputDevice;
}

#endif