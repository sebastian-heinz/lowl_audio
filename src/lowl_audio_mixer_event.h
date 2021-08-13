#ifndef LOWL_AUDIO_MIXER_EVENT_H
#define LOWL_AUDIO_MIXER_EVENT_H

namespace Lowl {
    struct AudioMixerEvent {
        static const uint8_t MixAudioStream = 0;
        static const uint8_t MixAudioData = 1;
        static const uint8_t MixAudioMixer = 2;
        static const uint8_t RemoveAudioStream = 3;
        static const uint8_t RemoveAudioData = 4;
        static const uint8_t RemoveAudioMixer = 5;

        uint8_t type;
        std::shared_ptr<void> ptr;
    };
}

#endif //LOWL_AUDIO_MIXER_EVENT_H
