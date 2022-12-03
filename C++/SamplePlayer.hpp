#ifndef __SAMPLE_PLAYER_HPP__
#define __SAMPLE_PLAYER_HPP__

#include "OpenWareLibrary.h"
#include "Crossfader.hpp"

enum SamplePlayerState {
    SP_NONE,
    SP_FADE_IN,
    SP_FADE_OUT,
    SP_CROSSFADE,
    SP_PLAY,
};

template <InterpolationMethod im>
struct SamplePlayerInterpolation {
    static float interpolate(float index, FloatArray data);
};

template <>
float SamplePlayerInterpolation<LINEAR_INTERPOLATION>::interpolate(
    float index, FloatArray data) {
    size_t idx = (int)index;
    return Interpolator::linear(data[idx], data[idx + 1], index - idx);
}

template <>
float SamplePlayerInterpolation<COSINE_INTERPOLATION>::interpolate(
    float index, FloatArray data) {
    size_t idx = (int)index;
    return Interpolator::cosine(data[idx], data[idx + 1], index - idx);
}

template <InterpolationMethod im, CrossfadeShape cf, size_t fade_size, size_t grid_size = 1>
class SamplePlayer : public SignalGenerator {
public:
    using Crossfade = Crossfader<cf>;

    SamplePlayer() = default;
    SamplePlayer(float sr, FloatArray buffer)
        : sr(sr)
        , buffer(buffer)
        , rate(1.0)
        , pos(0)
        , loop_index(0)
        , state(SP_NONE) {
        setLooping(false);
    }
    void trigger() {
        switch (state) {
        case SP_NONE:
            state = SP_FADE_IN;
            pos = loop_points[loop_index];
            break;
        default:
            state = SP_CROSSFADE;
            break;
        }
        transition = 0;
    }
    void setDuration(size_t samples) {
        //if (is_looping)
        //    samples += fade_size;
        rate = float(length) / float(samples);
        //fade_start = length - rate * fade_size;
    }
    void setDuration(float seconds) {
        setDuration(size_t(seconds * sr));
    }
    void setBPM(float bpm) {
        setDuration(float(length) / sr * 60 / bpm);
    }
    void setLooping(bool looping) {
        is_looping = looping;
    }
    void setLoopPoint(size_t index) {
        loop_index = min(index, grid_size - 1);
    }
    SamplePlayerState getState() const {
        return state;
    }
    float getPosition() const {
        return pos;
    }
    float getLoopPosition() const {
        return (float)pos / (loop_points[loop_index + 1] - loop_points[loop_index]);
    }
    size_t getStepDuration() const {
        return length / grid_size;
    }
    using SignalGenerator::generate;
    float generate() override {
        float sample;
        switch (state) {
        case SP_NONE:
            sample = 0;
            break;
        case SP_FADE_IN:
            sample = Crossfader<cf>::crossfade(0,
                SamplePlayerInterpolation<im>::interpolate(pos, buffer), transition);
            transition += transition_step;
            if (transition >= 1.0) {
                state = SP_PLAY;
            }
            pos += rate;
            break;
        case SP_FADE_OUT:
            sample = Crossfader<cf>::crossfade(
                SamplePlayerInterpolation<im>::interpolate(pos, buffer), 0, transition);
            transition += transition_step;
            if (transition >= 1.0) {
                state = SP_NONE;
            }
            else {
                pos += rate;
            }
            break;
        case SP_CROSSFADE:
            sample = Crossfader<cf>::crossfade(
                SamplePlayerInterpolation<im>::interpolate(pos, buffer),
                SamplePlayerInterpolation<im>::interpolate(
                    loop_points[loop_index] + transition * rate, buffer),
                transition);
            transition += transition_step;
            if (transition >= 1.0) {
                state = SP_PLAY;
                debugMessage("I", int(loop_index));
                pos = loop_points[loop_index] + transition * rate;
            }
            pos += rate;
            break;
        case SP_PLAY:
            sample = SamplePlayerInterpolation<im>::interpolate(pos, buffer);
            // debugMessage("P", (float)pos);
            pos += rate;
            if (pos >= end) {
                transition = (pos - end) / rate;
                state = is_looping ? SP_CROSSFADE : SP_FADE_OUT;
            }
            break;
        }
        return sample;
    }
    static SamplePlayer* create(float sr, FloatArray buf, float max_rate) {
        auto sampler = new SamplePlayer(sr, buf);
        sampler->start = sampler->findZeroCrossing(0, true);
        sampler->end = sampler->findZeroCrossing(buf.getSize() - fade_size * max_rate, false);
        sampler->length = sampler->end - sampler->start;
        sampler->transition_step = 1.0 / fade_size;
        sampler->setDuration(sampler->length);
        sampler->setupGrid();
        return sampler;
    }
    static void destroy(SamplePlayer* sampler) {
        delete sampler;
    }

protected:
    SamplePlayerState state;
    size_t start, end, length;
    float sr, rate, pos, transition, transition_step;
    FloatArray buffer;
    bool is_looping;
    size_t fade_start, output_length;
    size_t loop_points[grid_size];
    size_t loop_index;

    void setupGrid() {
        size_t grid_step = length / grid_size;
        size_t idx = 0;
        for (int i = 0; i < grid_size; i++) {
            loop_points[i] = findZeroCrossing(idx, true);
            idx += grid_step;
        }
    }

    size_t findZeroCrossing(size_t index, bool forward) {
        size_t len = buffer.getSize() - 1;
        index = min(index, len);
        if (forward) {
            if (buffer[index] > 0)
                while (index < len && buffer[index] > 0)
                    index++;
            else
                while (index < len && buffer[index] < 0)
                    index++;
        }
        else {
            if (buffer[index] > 0)
                while (index > 0 && buffer[index] > 0)
                    index--;
            else
                while (index > 0 && buffer[index] < 0)
                    index--;
        }
        return index;
    }
    void updateOutputLength() {
        if (is_looping) {
            output_length = length - fade_size * rate;
        }
        else {
            output_length = length;
        }
    }
};

#endif
