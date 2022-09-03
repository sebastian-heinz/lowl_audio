#ifndef LOWL_AUDIO_CHANNEL
#define LOWL_AUDIO_CHANNEL

namespace Lowl::Audio {
    enum class AudioChannel {
        None = 0,
        Mono = 1,
        Stereo = 2,
        Quadraphonic  = 4,
    };

    inline size_t get_channel_num(AudioChannel channel) {
        switch (channel) {
            case AudioChannel::None:
                return 0;
            case AudioChannel::Mono:
                return 1;
            case AudioChannel::Stereo:
                return 2;
            case AudioChannel::Quadraphonic:
                return 4;
            default:
                return 0;
        }
    }

    inline AudioChannel get_channel(int channel) {
        switch (channel) {
            case 1:
                return AudioChannel::Mono;
            case 2:
                return AudioChannel::Stereo;
            case 4:
                return AudioChannel::Quadraphonic;
            default:
                return AudioChannel::None;
        }
    }

}

#endif