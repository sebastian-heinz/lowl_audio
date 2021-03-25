#ifndef LOWL_AUDIO_MIXER_EVENT_H
#define LOWL_AUDIO_MIXER_EVENT_H

namespace Lowl {
    struct AudioMixerEvent {
        static const uint8_t AudioStreamType = 0;
        static const uint8_t AudioDataType = 1;

        // todo potentially timing to run at specific time
        uint8_t type;
        std::shared_ptr<void> ptr;
    };
}

#endif //LOWL_AUDIO_MIXER_EVENT_H
