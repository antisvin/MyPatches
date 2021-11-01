#ifndef __DATTORRO_REVERB_HPP__
#define __DATTORRO_REVERB_HPP__

#include "SignalProcessor.h"
#include "TriangleOscillator.h"
#include "SineOscillator.h"
#include "CircularBuffer.h"
#include "FractionalCircularBuffer.h"
#include "InterpolatingCircularBuffer.h"
#include "DryWetProcessor.h"

template <bool with_smear = true, size_t num_delays = 10>
class DattorroReverb : public MultiSignalProcessor {
    using LFO = SineOscillator;
    using DelayBuffer = InterpolatingCircularFloatBuffer<LINEAR_INTERPOLATION>;

public:
    DattorroReverb() = default;
    DattorroReverb(DelayBuffer** delays, LFO* lfo1, LFO* lfo2)
        : delays(delays)
        , lfo1(lfo1)
        , lfo2(lfo2)
        , lp(0.7f)
        , lp1_state(0)
        , lp2_state(0)
        , diffusion(0.625f)
        , amount(0)
        , reverb_time(0) {
        lfo1->setFrequency(0.517f);
        lfo2->setFrequency(0.293f);
        for (size_t i = 0; i < num_delays; i++) {
            delays[i]->setDelay((int)delays[i]->getSize() - 1);
        }
    }
    void process(AudioBuffer& input, AudioBuffer& output) {
        // This is the Griesinger topology described in the Dattorro paper
        // (4 AP diffusers on the input, then a loop of 2x 2AP+1Delay).
        // Modulation is applied in the loop of the first diffuser AP for additional
        // smearing; and to the two long delays for a slow shimmer/chorus effect.

        const float kap = diffusion;
        const float klp = lp;
        const float krt = reverb_time;

        size_t size = input.getSize();

        FloatArray left = input.getSamples(0);
        FloatArray right = input.getSamples(1);

        right.copyFrom(left);

        float* left_ptr = left.getData();
        float* right_ptr = right.getData();

        right.copyFrom(left);

        while (size--) {
            float acc = 0.f;

            // Smear AP1 inside the loop.
            if constexpr (with_smear) {
                delays[0]->delay(left_ptr, left_ptr, 1,
                    float(delays[0]->getSize() - 61) + lfo2->generate() * 30 + 10);
            }

            acc += *left_ptr;
            // c.Read(*left + *right, gain);

            // Diffuse through 4 allpasses.
            for (size_t i = with_smear ? 1 : 0; i < 4; i++) {
                processAPF(delays[i], acc, kap);
            }
            // Store a copy, since allpass output will be used by both delay branches in parallel
            float apout = acc;

            // Main reverb loop.
            // Modulate interpolated delay line
            acc += delays[9]->readAt(
                       float(delays[9]->getWriteIndex() + delays[9]->getSize() - 26) +
                       lfo1->generate() * 25) *
                krt;
            // Filter followed by two APFs
            processLPF(lp1_state, acc);
            processAPF(delays[4], acc, -kap);
            processAPF(delays[5], acc, kap);
            delays[6]->write(acc);
            // acc *= 2;

            *left_ptr += (acc - *left_ptr) * amount;
            *left_ptr++;

            acc = apout;
            if constexpr (with_smear) {
                acc += delays[6]->read() * krt;
            }
            else {
                acc += delays[6]->readAt(float(delays[6]->getWriteIndex() +
                                             delays[6]->getSize() - 21) +
                           lfo2->generate() * 20) *
                    krt;
            }
            processLPF(lp2_state, acc);
            processAPF(delays[7], acc, kap);
            processAPF(delays[8], acc, -kap);
            delays[9]->write(acc);
            *right_ptr += (acc - *right_ptr) * amount;
            *right_ptr++;
        }
    }

    void setAmount(float amount) {
        this->amount = amount;
    }

    void setTime(float reverb_time) {
        this->reverb_time = reverb_time;
    }

    void setDiffusion(float diffusion) {
        this->diffusion = diffusion;
    }

    void setLp(float lp) {
        this->lp = lp;
    }

    void clear() {
        for (size_t i = 0; i < num_delays; i++) {
            delays[i]->clear();
        }
    }

    static DattorroReverb* create(float sr, const size_t* delay_lengths) {
        DelayBuffer** delays = new DelayBuffer*[num_delays];
        for (size_t i = 0; i < num_delays; i++) {
            delays[i] = DelayBuffer::create(delay_lengths[i]);
        }
        LFO* lfo1 = LFO::create(sr);
        LFO* lfo2 = LFO::create(sr);
        
        return new DattorroReverb(delays, lfo1, lfo2);
    }

    static void destroy(DattorroReverb* reverb) {
        LFO::destroy(reverb->lfo1);
        LFO::destroy(reverb->lfo2);
        for (size_t i = 0; i < num_delays; i++) {
            DelayBuffer::destroy(reverb->delays[i]);
        }
        delete reverb->delays;
        delete reverb;
    }

protected:
    DelayBuffer** delays;
    LFO* lfo1;
    LFO* lfo2;
    float amount;
    float reverb_time;
    float diffusion;
    float lp;
    float lp1_state;
    float lp2_state;

    inline void processLPF(float& state, float& value) {
        state += lp * (value - state);
        value = state;
    }

    inline void processAPF(DelayBuffer* delay, float& acc, float kap) {
        float sample = delay->read();
        acc += sample * kap;
        delay->write(acc);
        acc *= -kap;
        acc += sample;
    }
};

// Rings, elements - has longer tails
const size_t rings_delays[] = {
    150,
    214,
    319,
    527,
    2182,
    2690,
    4501,
    2525,
    2197,
    6312,
};
// The nephologic classic
const size_t clouds_delays[] = {
    113,
    162,
    241,
    399,
    1653,
    2038,
    3411,
    1913,
    1663,
    4782,
};
// Jon Dattorro, Effect Design* 1:  Reverberator  and  Other  Filters
const size_t dattorro_delays[] = {
    142,
    107,
    379,
    277,
    4453,
    3720,
    908 + 40,
    4217,
    3163,
    672 + 50,
};
#endif
