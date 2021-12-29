#include "OpenWareLibrary.h"
#include "MultiOscillator.hpp"

#define P_TUNE PARAMETER_A
#define P_SHAPE PARAMETER_B
#define P_MIX PARAMETER_C
#define P_DETUNE PARAMETER_D

using O1 = AntialisedRampOscillator;
using O2 = AntialiasedTriangleOscillator;
using O3 = SineOscillator;
using O4 = AntialiasedSquareWaveOscillator;
using O5 = AntialiasedSquareWaveOscillator; // Narrow pulse

class MorphingOscillator : public MultiOscillator<5> {
public:
    using MultiOscillator<5>::MultiOscillator;
    // using MultiOscillator<5>::generate;

    void setPulseWidth(float pw) {
        static_cast<O5*>(oscillators[4])->setPulseWidth(pw);
    }

    static MorphingOscillator* create(float sr) {
        auto osc = new MorphingOscillator(sr);
        osc->addOscillator(O1::create(sr));
        osc->addOscillator(O2::create(sr));
        osc->addOscillator(O3::create(sr));
        osc->addOscillator(O4::create(sr));
        auto o5 = O5::create(sr);
        o5->setPulseWidth(0.01);
        osc->addOscillator(o5);
        return osc;
    }
    static void destroy(MorphingOscillator* osc) {
        MultiOscillator::destroy(osc);
    }
};

template <size_t num_oscillators, typename OscillatorClass>
class SwarmOscillator : public OscillatorTemplate<SwarmOscillator<num_oscillators, OscillatorClass>> {
public:
    SwarmOscillator() = default;
    SwarmOscillator(float sr, OscillatorClass** oscillators, FloatArray tmp) : oscillators(oscillators), tmp(tmp) {
        this->setSampleRate(sr);
        setMix(0);
        setDetune(0);
    }

    static constexpr float begin_phase = 0;
    static constexpr float end_phase = 2 * M_PI;
    void setFrequency(float freq) {
        for (int i = 0; i < num_oscillators; i++) {
            oscillators[i]->setFrequency(freq * freq_ratios[i]);
        }
    }
    void setMix(float mix) {
        float norm = 1.0f / (num_oscillators - 1);
        float amp = mix * norm;
        for (int n = 0; n < num_oscillators; n++) {
            amplitudes[n] = amp;
        }
        amplitudes[num_oscillators / 2] = 1.0 - mix;
    }
    void setDetune(float detune) {
        for (int i = 0; i < num_oscillators; i++) {
            float spread = (i - (int)(num_oscillators / 2)) / (float)num_oscillators;
            float freq_ratio = 1 + detune * spread;
            freq_ratios[i] = 1.0 / freq_ratio;
        }
    }
    void setMorph(float morph) {
        for (int i = 0; i < num_oscillators; i++) {
            oscillators[i]->setMorph(morph);
        }
    }
    void setPulseWidth(float pw) {
        for (int i = 0; i < num_oscillators; i++) {
            oscillators[i]->setPulseWidth(pw);
        }
    }
    float getSample() {
        float t = 0;
        for (int i = 0; i < num_oscillators; i++) {
            t += oscillators[i]->generate() * amplitudes[i];
        }
        return t;
    }
    void generate(FloatArray output) {
        oscillators[0]->generate(output);
        output.multiply(amplitudes[0]);
        for (int i = 1; i < num_oscillators; i++) {
            oscillators[i]->generate(tmp);
            tmp.multiply(amplitudes[i]);
            output.add(tmp);
        }
    }
    static SwarmOscillator* create(float sr, size_t block_size) {
        auto oscillators = new OscillatorClass*[num_oscillators];
        for (int i = 0; i < num_oscillators; i++) {
            oscillators[i] = OscillatorClass::create(sr);
        }
        return new SwarmOscillator(sr, oscillators, FloatArray::create(block_size));
    }
    static void destroy(SwarmOscillator* osc) {
        for (int i = 0; i < num_oscillators; i++) {
            OscillatorClass::destroy(osc->oscillators[i]);
        }
        delete[] osc->oscillators;
        FloatArray::destroy(osc->tmp);
        delete osc;
    }

private:
    OscillatorClass** oscillators;
    float freq_ratios[num_oscillators];
    float amplitudes[num_oscillators];
    FloatArray tmp;
};

using SwarmOsc = SwarmOscillator<7, MorphingOscillator>;

class SwarmOscillatorPatch : public Patch {
public:
    SwarmOsc* osc;
    VoltsPerOctave hz;

    SwarmOscillatorPatch() {
        registerParameter(P_TUNE, "Tune");
        registerParameter(P_SHAPE, "Shape");
        registerParameter(P_DETUNE, "Detune");
        registerParameter(P_MIX, "Mix");

        osc = SwarmOsc::create(getSampleRate(), getBlockSize());
    }
    ~SwarmOscillatorPatch() {
        SwarmOsc::destroy(osc);
    }

    void processAudio(AudioBuffer& buffer) {
        auto left = buffer.getSamples(0);
        // Tuning
        float tune = -2.0 + getParameterValue(P_TUNE) * 4.0;
#ifdef QUANTIZER
        float volts = quantizer.process(tune + hz.sampleToVolts(left[0]));
        float freq = hz.voltsToHertz(volts);
#else
        // hz.setTune(tune);
        // float freq = hz.getFrequency(left[0]);
        float freq = hz.voltsToHertz(tune * 4 - 2);
#endif
        //        osc->setFrequency(100);
        osc->setDetune(getParameterValue(P_DETUNE));
        osc->setMix(getParameterValue(P_MIX));
        osc->setFrequency(freq);
        float shape = getParameterValue(P_SHAPE);
        if (shape >= 0.8)
            osc->setPulseWidth(0.5 - (shape - 0.8) * 4.98 / 2);
        osc->setMorph(shape);
        osc->generate(left);
        left.copyTo(buffer.getSamples(1));
    }
};
