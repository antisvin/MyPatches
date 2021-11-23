#ifndef __DATTORRO_REVERB_HPP__
#define __DATTORRO_REVERB_HPP__

#include "OpenWareLibrary.h"

class bypass { };

template <typename Processor = bypass>
class DattorroStereoReverb : public MultiSignalProcessor {
private:
    using LFO = SineOscillator;
    using DelayBuffer = InterpolatingCircularFloatBuffer<LINEAR_INTERPOLATION>;
    static constexpr size_t num_delays = 14;
    Processor** processors;

public:
    DattorroStereoReverb() = default;
    DattorroStereoReverb(FloatArray tmp, DelayBuffer** delays, LFO* lfo1,
        LFO* lfo2, Processor** processors)
        : tmp(tmp)
        , delays(delays)
        , lfo1(lfo1)
        , lfo2(lfo2)
        , damping(0)
        , lp1_state(0)
        , lp2_state(0)
        , hp1_state(0)
        , hp2_state(0)
        , hpf_amount(0.05)
        , diffusion(0)
        , amount(0)
        , decay(0)
        , lfo_amount1(0)
        , lfo_amount2(0)
        , processors(processors) {
        lfo1->setFrequency(0.5);
        lfo2->setFrequency(0.3);
        for (size_t i = 0; i < num_delays; i++) {
            delays[i]->setDelay((int)delays[i]->getSize());
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
        lfo1_read_offset =
            delays[10]->getWriteIndex() + delays[10]->getSize() - lfo_offset1;
        lfo2_read_offset =
            delays[13]->getWriteIndex() + delays[13]->getSize() - lfo_offset2;

        while (size--) {
            // Left channel
            float acc = *left_in;

            // Diffuse through 4 allpasses.
            for (size_t i = 0; i < 4; i++) {
                processAPF(delays[i], acc, kap);
            }

            // Main reverb loop.
            // Modulate interpolated delay line
            acc += delays[13]->readAt(
                       (lfo2->generate() + 1) * lfo_amount2 + lfo2_read_offset++) *
                krt;
            // Filter followed by two APFs
            processLPF(lp1_state, acc);
            processAPF(delays[8], acc, -kap);
            processAPF(delays[9], acc, kap);
            if constexpr (!std::is_empty<Processor>::value)
                acc = processors[0]->process(acc);

            processHPF(hp1_state, acc);
            delays[10]->write(acc);

            *left_out++ = *left_in + (acc - *left_in) * amount;
            *left_in++;

            // Right channel
            acc = *right_in;

            // Diffuse through 4 allpasses.
            for (size_t i = 4; i < 8; i++) {
                processAPF(delays[i], acc, kap);
            }

            acc += delays[10]->readAt(
                       (lfo1->generate() + 1) * lfo_amount1 + lfo1_read_offset++) *
                krt;
            processLPF(lp2_state, acc);
            processAPF(delays[11], acc, kap);
            processAPF(delays[12], acc, -kap);
            if constexpr (!std::is_empty<Processor>::value)
                acc = processors[1]->process(acc);

            processHPF(hp2_state, acc);
            delays[13]->write(acc);

            *right_out++ = *right_in + (acc - *right_in) * amount;
            *right_in++;
        }
    }

    Processor& getProcessor(size_t index) {
        return *processors[index];
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

    template <typename... Args>
    static DattorroStereoReverb* create(size_t block_size, float sr,
        const size_t* delay_lengths, Args&&... args) {
        DelayBuffer** delays = new DelayBuffer*[num_delays];
        for (size_t i = 0; i < num_delays; i++) {
            delays[i] = DelayBuffer::create(delay_lengths[i]);
        }
        LFO* lfo1 = LFO::create(sr);
        LFO* lfo2 = LFO::create(sr);

        FloatArray tmp = FloatArray::create(block_size);

        if constexpr (std::is_empty<Processor>::value) {
            return new DattorroStereoReverb(tmp, delays, lfo1, lfo2, nullptr);
        }
        else {
            Processor** processors = new Processor*[2];
            processors[0] = Processor::create(std::forward<Args>(args)...);
            processors[1] = Processor::create(std::forward<Args>(args)...);
            return new DattorroStereoReverb(tmp, delays, lfo1, lfo2, processors);
        }
    }

    static void destroy(DattorroStereoReverb* reverb) {
        LFO::destroy(reverb->lfo1);
        LFO::destroy(reverb->lfo2);
        for (size_t i = 0; i < num_delays; i++) {
            DelayBuffer::destroy(reverb->delays[i]);
        }
        delete[] reverb->delays;
        if constexpr (!std::is_empty<Processor>::value) {
            Processor::destroy(reverb->processors[0]);
            Processor::destroy(reverb->processors[1]);
            delete[] reverb->processors;
        }
        FloatArray::destroy(reverb->tmp);
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
    float hp1_state, hp2_state, hpf_amount;
    size_t lfo_offset1, lfo_offset2;
    size_t lfo_amount1, lfo_amount2;
    FloatArray tmp;

    inline void processLPF(float& state, float& value) {
        state += damping * (value - state);
        value = state;
    }

    inline void processHPF(float& state, float& value) {
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

// Rings, elements - has longer tails. Second diffuser APF chain delays are improvised.
const size_t rings_delays[] = {
    150,
    214,
    319,
    527,
    126,
    191,
    344,
    569,
    2182,
    2690,
    4501,
    2525,
    2197,
    6312,
};
// Tank delays from nephologic classic, diffuser values replaced with
// stereo diffuser from the same module
const size_t clouds_delays[] = {
    126,
    180,
    269,
    444,
    151,
    205,
    245,
    405,
    1653,
    2038,
    3411,
    1913,
    1663,
    4782,
};
#endif
