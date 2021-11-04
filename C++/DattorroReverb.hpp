#ifndef __DATTORRO_REVERB_HPP__
#define __DATTORRO_REVERB_HPP__

#include "SignalProcessor.h"
#include "TriangleOscillator.h"
#include "SineOscillator.h"
#include "CircularBuffer.h"
#include "FractionalCircularBuffer.h"
#include "InterpolatingCircularBuffer.h"
#include "DryWetProcessor.h"

class bypass { };

template <bool with_smear = true, typename Processor = bypass>
class DattorroReverb : public MultiSignalProcessor {
private:
    using LFO = SineOscillator;
    using DelayBuffer = InterpolatingCircularFloatBuffer<LINEAR_INTERPOLATION>;
    static constexpr size_t num_delays = 10;
    Processor processors[2];

public:
    DattorroReverb() = default;
    DattorroReverb(DelayBuffer** delays, LFO* lfo1, LFO* lfo2)
        : delays(delays)
        , lfo1(lfo1)
        , lfo2(lfo2)
        , damping(0)
        , lp1_state(0)
        , lp2_state(0)
        , diffusion(0)
        , amount(0)
        , decay(0)
        , lfo_amount1(0)
        , lfo_amount2(0) {
        lfo1->setFrequency(0.5);
        lfo2->setFrequency(0.3);
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
        const float klp = damping;
        const float krt = decay;

        size_t size = input.getSize();

        float* left_in = input.getSamples(0).getData();
        float* right_in = input.getSamples(1).getData();

        float* left_out = output.getSamples(0).getData();
        float* right_out = output.getSamples(1).getData();

        size_t lfo1_read_offset, lfo1_write_offset, lfo2_read_offset;
        if constexpr (with_smear) {
            lfo1_read_offset =
                delays[0]->getWriteIndex() + delays[0]->getSize() - lfo_offset1;
            lfo1_write_offset = delays[0]->getWriteIndex() + 100; // Hardcoded for now
        }
        else {
            lfo1_read_offset =
                delays[6]->getWriteIndex() + delays[6]->getSize() - lfo_offset1;
        }
        lfo2_read_offset =
            delays[9]->getWriteIndex() + delays[9]->getSize() - lfo_offset2;

        while (size--) {
            // Smear AP1 inside the loop.
            if constexpr (with_smear) {
                // Interpolated read with an LFO
                float t = delays[0]->readAt(
                    fmodf(lfo1_read_offset++ - (lfo1->generate() + 1) * lfo_amount1,
                        delays[0]->getSize()));
                // Write back to buffer
                delays[0]->writeAt(lfo1_write_offset++, t);
            }

            float acc = (*left_in + *right_in) * 0.5;
            // c.Read(*left + *right, gain);

            // Diffuse through 4 allpasses.
            for (size_t i = 0; i < 4; i++) {
                processAPF(delays[i], acc, kap);
            }
            // Store a copy, since allpass output will be used by both delay branches in parallel
            float apout = acc;

            // Main reverb loop.
            // Modulate interpolated delay line
            acc += delays[9]->readAt(
                       (lfo2->generate() + 1) * lfo_amount2 + lfo2_read_offset++) *
                krt;
            // Filter followed by two APFs
            processLPF(lp1_state, acc);
            processAPF(delays[4], acc, -kap);
            processAPF(delays[5], acc, kap);
            if constexpr (!std::is_empty<Processor>::value)
                acc = processors[0].process(acc);

            delays[6]->write(acc);

            *left_out++ = *left_in + (acc - *left_in) * amount;
            *left_in++;

            acc = apout;
            if constexpr (with_smear) {
                acc += delays[6]->read() * krt;
            }
            else {
                acc += delays[6]->readAt((lfo1->generate() + 1) * lfo_amount1 +
                           lfo1_read_offset++) *
                    krt;
            }
            processLPF(lp2_state, acc);
            processAPF(delays[7], acc, kap);
            processAPF(delays[8], acc, -kap);
            if constexpr (!std::is_empty<Processor>::value)
                acc = processors[1].process(acc);

            delays[9]->write(acc);

            *right_out++ = *right_in + (acc - *right_in) * amount;
            *right_in++;
        }
    }

    Processor& getProcessor(size_t index) {
        return processors[index];
    }

    void setAmount(float amount) {
        this->amount = amount;
    }

    void setDecay(float decay) {
        this->decay = decay;
    }

    void setDiffusion(float diffusion) {
        this->diffusion = diffusion;
    }

    void setDamping(float damping) {
        this->damping = damping;
    }

    void clear() {
        for (size_t i = 0; i < num_delays; i++) {
            delays[i]->clear();
        }
    }

    void setModulation(size_t offset1, size_t amount1, size_t offset2, size_t amount2) {
        lfo_offset1 = offset1;
        lfo_amount1 = amount1 / 2;
        lfo_offset2 = offset2;
        lfo_amount2 = amount2 / 2;
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
        delete[] reverb->delays;
        delete reverb;
    }

protected:
    DelayBuffer** delays;
    LFO* lfo1;
    LFO* lfo2;
    float amount;
    float decay;
    float diffusion;
    float damping;
    float lp1_state, lp2_state;
    size_t lfo_offset1, lfo_offset2;
    size_t lfo_amount1, lfo_amount2;

    inline void processLPF(float& state, float& value) {
        state += damping * (value - state);
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
// Jon Dattorro, Effect Design 1:  Reverberator  and  Other  Filters
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
