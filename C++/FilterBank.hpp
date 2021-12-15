#ifndef __FILTERBANK_HPP__
#define __FILTERBANK_HPP__

#include "SignalProcessor.h"
#include "ComplexFloatArray.h"
#include "BiquadFilter.h"

enum LRFType {
    LRF_LOWPASS,
    LRF_BANDPASS,
    LRF_HIPASS,
};

/**
 * This class implements a Linkwitz-Riley crossover filter
 *
 * Effectively this should be just 12dB Butterworth (squared) filter.
 * This is a cascaded version of TPT SVF (StatevaViableFilter in OWL library)
 *
 * SVF class itself is not used, because we need access to multiple outputs.
 * Beside that, we need to compute 2 cascaded stages.
 **/

static const float k = 1. / FilterStage::BUTTERWORTH_Q;

class LinkwitzRileySVF : public MultiSignalProcessor {
public:
    LinkwitzRileySVF() = default;
    LinkwitzRileySVF(float sr) {
        setSampleRate(sr);
    }
    void setSampleRate(float sr) {
        pioversr = M_PI / sr;
    }
    /**
     * Set crossover frequency.
     * This would be used for both filter stages, Q is
     * not intended to be changed
     **/
    void setFrequency(float frequency) {
        const float w = tanf(pioversr * frequency);
        const float g = w;
        m_a1 = 1. / (1. + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
    }
    /**
     * It's not recommended to use per sample processing,
     * but at least it illustrates how this is computed.
     **/
    ComplexFloat process(float input) {
        ComplexFloat output;
        render1(&input, &output.re, &output.im, 1);
        render2(&output.re, &output.im, &output.re, &output.im, 1);
        return output;
    }
    void process(AudioBuffer& input, AudioBuffer& output) {
        process(input.getSamples(0), output.getSamples(0), output.getSamples(1));
    }
    void process(FloatArray input, FloatArray left_out, FloatArray right_out) {
        size_t size = input.getSize();
        render1(input.getData(), left_out.getData(), right_out.getData(), size);
        render2(left_out.getData(), right_out.getData(), left_out.getData(),
            right_out.getData(), size);
    }
    /*
        void compensate(FloatArray band) {
            float v0;
            float v1;
            float v2;
            float v3;
            float* data = band.getData();
            size_t size = band.getSize();
            while (size--) {
                v0 = *data;
                v3 = v0 - mIc6eq;
                v1 = m_a1 * mIc5eq + m_a2 * v3;
                v2 = mIc6eq + m_a2 * mIc5eq + m_a3 * v3;
                mIc5eq = 2. * v1 - mIc5eq;
                mIc6eq = 2. * v2 - mIc6eq;
                *data++ = v0 - 2 * k * v1; // AP
            }
        }
    */
    /**
     * Applies allpass filter - this should be used on previous band
     * in order to correct phase
     */
    void compensate(FloatArray data) {
        float v0;
        float v1;
        float v2;
        float v3;
        // First pass
        size_t size = data.getSize();
        float* input = data.getData();
        float* output = input;
        while (size--) {
            v0 = *input++;
            v3 = v0 - mIc6eq;
            v1 = m_a1 * mIc5eq + m_a2 * v3;
            v2 = mIc6eq + m_a2 * mIc5eq + m_a3 * v3;
            mIc5eq = 2. * v1 - mIc5eq;
            mIc6eq = 2. * v2 - mIc6eq;
            *output++ = v0 - 2 * k * v1; // AP
        }
#if 0
        // Second pass
        size = data.getSize();
        input = data.getData();
        output = input;
        while (size--) {
            v0 = *input++;
            v3 = v0 - mIc8eq;
            v1 = m_a1 * mIc7eq + m_a2 * v3;
            v2 = mIc8eq + m_a2 * mIc7eq + m_a3 * v3;
            mIc7eq = 2. * v1 - mIc7eq;
            mIc8eq = 2. * v2 - mIc8eq;
//            *output++ = 2 * k * v1 - v0; // -AP
            *output++ = v0 - 2 * k * v1; // AP
        }
#endif
    }
    static LinkwitzRileySVF* create(float sr) {
        return new LinkwitzRileySVF(sr);
    }
    static void destroy(LinkwitzRileySVF* svf) {
        delete svf;
    }

protected:
    float pioversr;
    float m_a1 = 0.;
    float m_a2 = 0.;
    float m_a3 = 0.;
    float mIc1eq = 0.f; // SVF1
    float mIc2eq = 0.f;
    float mIc3eq = 0.f; // SVF2
    float mIc4eq = 0.f;
    float mIc5eq = 0.f; // APF - two stages
    float mIc6eq = 0.f;
    float mIc7eq = 0.f;
    float mIc8eq = 0.f;
    /**
     * Generate LP and AP outputs from SVF1
     **/
    void render1(float* input, float* output_lp, float* output_ap, size_t size) {
        float v0;
        float v1;
        float v2;
        float v3;
        while (size--) {
            v0 = *input++;
            v3 = v0 - mIc2eq;
            v1 = m_a1 * mIc1eq + m_a2 * v3;
            v2 = mIc2eq + m_a2 * mIc1eq + m_a3 * v3;
            mIc1eq = 2. * v1 - mIc1eq;
            mIc2eq = 2. * v2 - mIc2eq;
            *output_lp++ = v2; // LP1
            *output_ap++ = v0 - 2 * k * v1; // AP1
        }
    }
    /**
     * Second pass of LP and HP.
     *
     * To avoid using third SVF for chained HP, our final HP output is generated
     * by subtracting LP output from SVF1 AP. It's inverted to correct phase.
     *
     * See https://www.kvraudio.com/forum/viewtopic.php?f=33&t=500121 for details
     */
    void render2(float* input_lp, float* input_ap, float* output_lp,
        float* output_hp, size_t size) {
        float v0;
        float v1;
        float v2;
        float v3;
        while (size--) {
            v0 = *input_lp++;
            v3 = v0 - mIc4eq;
            v1 = m_a1 * mIc3eq + m_a2 * v3;
            v2 = mIc4eq + m_a2 * mIc3eq + m_a3 * v3;
            mIc3eq = 2. * v1 - mIc3eq;
            mIc4eq = 2. * v2 - mIc4eq;
            *output_lp++ = v2; // LP2
            *output_hp++ = *input_ap++ - v2; // HP2 = AP1 - LP2
            //*output_hp++ = v2 - *input_ap++; // -HP2 = LP2 - AP1
        }
    }
};

class LinkwitzRileyBiquad : public MultiSignalProcessor {
public:
    LinkwitzRileyBiquad() {};
    LinkwitzRileyBiquad(BiquadFilter* lpf, BiquadFilter* hpf)
        : lpf(lpf)
        , hpf(hpf) {
    }
    /**
     * Set crossover frequency.
     * This would be used for both filter stages, Q is
     * not intended to be changed
     **/
    void setFrequency(float frequency) {
        lpf->setLowPass(frequency, FilterStage::BUTTERWORTH_Q);
        hpf->setHighPass(frequency, FilterStage::BUTTERWORTH_Q);
    }

    void process(AudioBuffer& input, AudioBuffer& output) {
        process(input.getSamples(0), output.getSamples(0), output.getSamples(1));
    }
    void process(FloatArray input, FloatArray left_out, FloatArray right_out) {
        size_t size = input.getSize();
        lpf->process(input, left_out);
        hpf->process(input, right_out);
        right_out.multiply(-1.f);
    }
    void compensate(FloatArray array) {
        // Not implemented because why even bother with biquads
    }

    static LinkwitzRileyBiquad* create(float sr) {
        BiquadFilter* lpf = BiquadFilter::create(sr, 2);
        BiquadFilter* hpf = BiquadFilter::create(sr, 2);
        return new LinkwitzRileyBiquad(lpf, hpf);
    }
    static void destroy(LinkwitzRileyBiquad* lrf) {
        BiquadFilter::destroy(lrf->lpf);
        BiquadFilter::destroy(lrf->hpf);
        delete lrf;
    }

protected:
    BiquadFilter* lpf;
    BiquadFilter* hpf;
};

/**
 * N bands requires N - 1 crossover filters
 **/
template <typename LinkwitzRileyFilter, typename ChannelProcessor>
class CrossoverFilterBank : public SignalProcessor {
public:
    CrossoverFilterBank() = default;
    CrossoverFilterBank(LinkwitzRileyFilter** filters, size_t bands,
        ChannelProcessor& processor, FloatArray tmp1, FloatArray tmp2)
        : filters(filters)
        , bands(bands)
        , processor(processor)
        , tmp1(tmp1)
        , tmp2(tmp2) {
    }
    ~CrossoverFilterBank() = default;
    /**
     * Per-sample processing would perform poorly and is not even tested yet
     **/
    float process(float input) {
        return 0.f;
    }

    void process(FloatArray input, FloatArray output) {
        // First pass is handled specially in order to avoid zeroing out output buffer
        filters[0]->process(input, output, tmp2);
        processor.process(0, output);
        filters[1]->compensate(output);

        for (size_t i = 1; i < bands - 2; i++) {
            filters[i]->process(tmp2, tmp1, tmp2);
            processor.process(i, tmp1);
            output.add(tmp1);
            filters[i + 1]->compensate(output);
        }
        // Last band is HPF results from previous band, we don't need to
        // compensate for last 2 bands.
        filters[bands - 2]->process(tmp2, tmp1, tmp2);
        processor.process(bands - 2, tmp1);
        output.add(tmp1);
        output.add(tmp2);
    }

    static CrossoverFilterBank* create(float sr, float* frequencies,
        size_t bands, ChannelProcessor& processor, size_t block_size) {
        FloatArray tmp1 = FloatArray::create(block_size);
        FloatArray tmp2 = FloatArray::create(block_size);
        LinkwitzRileyFilter** filters = new LinkwitzRileyFilter*[bands];
        for (size_t i = 0; i < bands; i++) {
            filters[i] = LinkwitzRileyFilter::create(sr);
            filters[i]->setFrequency(frequencies[i]);
        }
        return new CrossoverFilterBank(filters, bands, processor, tmp1, tmp2);
    }
    static void destroy(CrossoverFilterBank* bank) {
        for (size_t i = 0; i < bank->bands; i++) {
            LinkwitzRileyFilter::destroy(bank->filters[i]);
        }
        delete[] bank->filters;
        FloatArray::destroy(bank->tmp1);
        FloatArray::destroy(bank->tmp2);
        delete bank;
    }

private:
    LinkwitzRileyFilter** filters = nullptr;
    FloatArray tmp1, tmp2;
    ChannelProcessor& processor;
    size_t bands;
};

#endif
