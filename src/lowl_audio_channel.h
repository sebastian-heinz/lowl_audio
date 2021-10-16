#ifndef LOWL_AUDIO_CHANNEL
#define LOWL_AUDIO_CHANNEL

namespace Lowl {
    enum class AudioChannel {
        None = 0,
        Mono = 1,
        Stereo = 2,
    };

    inline size_t get_channel_num(AudioChannel channel) {
        switch (channel) {
            case AudioChannel::None:
                return 0;
            case AudioChannel::Mono:
                return 1;
            case AudioChannel::Stereo:
                return 2;
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
            default:
                return AudioChannel::None;
        }
    }

}

#endif