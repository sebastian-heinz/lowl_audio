#ifndef LOWL_CHANNEL
#define LOWL_CHANNEL

namespace Lowl {
    enum class Channel {
        None = 0,
        Mono = 1,
        Stereo = 2,
    };

    inline size_t get_channel_num(Channel channel) {
        switch (channel) {
            case Channel::None:
                return 0;
            case Channel::Mono:
                return 1;
            case Channel::Stereo:
                return 2;
        }
    }

    inline Channel get_channel(int channel) {
        switch (channel) {
            case 1:
                return Channel::Mono;
            case 2:
                return Channel::Stereo;
            default:
                return Channel::None;
        }
    }

}

#endif