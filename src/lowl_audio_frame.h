#ifndef LOWL_AUDIO_FRAME_H
#define LOWL_AUDIO_FRAME_H

#include "lowl_typedef.h"

namespace Lowl {
    struct AudioFrame {
        float left;
        float right;

        // Mono
        // 0: M: mono

        // Stereo
        // 0: L: left
        // 1: R: right

        // Quad
        // 0: L: left
        // 1: R: right
        // 2: SL: surround left
        // 3: SR: surround right

        // 5.1
        // 0: L: left
        // 1: R: right
        // 2: C: center
        // 3: LFE: subwoofer
        // 4: SL: surround left
        // 5: SR: surround right

        _INLINE_ const float &operator[](int idx) const { return idx == 0 ? left : right; }

        _INLINE_ float &operator[](int idx) { return idx == 0 ? left : right; }

        _INLINE_ AudioFrame operator+(const AudioFrame &p_frame) const {
            return AudioFrame(left + p_frame.left, right + p_frame.right);
        }

        _INLINE_ AudioFrame operator-(const AudioFrame &p_frame) const {
            return AudioFrame(left - p_frame.left, right - p_frame.right);
        }

        _INLINE_ AudioFrame operator*(const AudioFrame &p_frame) const {
            return AudioFrame(left * p_frame.left, right * p_frame.right);
        }

        _INLINE_ AudioFrame operator/(const AudioFrame &p_frame) const {
            return AudioFrame(left / p_frame.left, right / p_frame.right);
        }

        _INLINE_ AudioFrame operator+(float p_sample) const { return AudioFrame(left + p_sample, right + p_sample); }

        _INLINE_ AudioFrame operator-(float p_sample) const { return AudioFrame(left - p_sample, right - p_sample); }

        _INLINE_ AudioFrame operator*(float p_sample) const { return AudioFrame(left * p_sample, right * p_sample); }

        _INLINE_ AudioFrame operator/(float p_sample) const { return AudioFrame(left / p_sample, right / p_sample); }

        _INLINE_ void operator+=(const AudioFrame &p_frame) {
            left += p_frame.left;
            right += p_frame.right;
        }

        _INLINE_ void operator-=(const AudioFrame &p_frame) {
            left -= p_frame.left;
            right -= p_frame.right;
        }

        _INLINE_ void operator*=(const AudioFrame &p_frame) {
            left *= p_frame.left;
            right *= p_frame.right;
        }

        _INLINE_ void operator/=(const AudioFrame &p_frame) {
            left /= p_frame.left;
            right /= p_frame.right;
        }

        _INLINE_ void operator+=(float p_sample) {
            left += p_sample;
            right += p_sample;
        }

        _INLINE_ void operator-=(float p_sample) {
            left -= p_sample;
            right -= p_sample;
        }

        _INLINE_ void operator*=(float p_sample) {
            left *= p_sample;
            right *= p_sample;
        }

        _INLINE_ void operator/=(float p_sample) {
            left /= p_sample;
            right /= p_sample;
        }

        _INLINE_ AudioFrame(float p_l, float p_r) {
            left = p_l;
            right = p_r;
        }

        _INLINE_ AudioFrame(const AudioFrame &p_frame) {
            left = p_frame.left;
            right = p_frame.right;
        }

        _INLINE_ AudioFrame() {
            left = 0.0;
            right = 0.0;
        }
    };
}

#endif //LOWL_AUDIO_FRAME_H
