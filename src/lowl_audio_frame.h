#ifndef LOWL_AUDIO_FRAME_H
#define LOWL_AUDIO_FRAME_H

#include "lowl_typedef.h"

namespace Lowl {
    struct AudioFrame {
        float left;
        float right;

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
