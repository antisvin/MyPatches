#include "OpenWareLibrary.h"

#define P_TUNE PARAMETER_A
#define P_SHAPE PARAMETER_B
#define P_MIX PARAMETER_C
#define P_DETUNE PARAMETER_D

using O1 = AntialiasedRampOscillator;
using O2 = AntialiasedTriangleOscillator;
using O3 = AntialiasedSquareWaveOscillator;
using O4 = AntialiasedSquareWaveOscillator; // Narrow pulse

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
            oscillators[i]->morph(morph);
        }
    }
    void setPulseWidth(float pw) {
        for (int i = 0; i < num_oscillators; i++) {
            static_cast<O4*>(oscillators[i]->getOscillator(num_oscillators - 1))->setPulseWidth(pw);
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
            auto osc = OscillatorClass::create(4);
            osc->setOscillator(0, O1::create(sr));
            osc->setOscillator(1, O2::create(sr));
            osc->setOscillator(2, O3::create(sr));
            auto o4 = O4::create(sr);
            o4->setPulseWidth(0.01);
            osc->setOscillator(3, o4);
            oscillators[i] = osc;
        }
        return new SwarmOscillator(sr, oscillators, FloatArray::create(block_size));
    }
    static void destroy(SwarmOscillator* osc) {
        for (int i = 0; i < num_oscillators; i++) {
            O1::destroy(static_cast<O1*>(osc->oscillators[i]->getOscillator(0)));
            O2::destroy(static_cast<O2*>(osc->oscillators[i]->getOscillator(1)));
            O3::destroy(static_cast<O3*>(osc->oscillators[i]->getOscillator(2)));
            O4::destroy(static_cast<O4*>(osc->oscillators[i]->getOscillator(3)));
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
    float shape;
    //SmoothFloat shape = SmoothFloat(0.99);

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
        shape = getParameterValue(P_SHAPE);
        if (shape >= 0.8)
            osc->setPulseWidth(0.5 - (shape - 0.8) * 4.98 / 2);
        osc->setMorph(shape);
        osc->generate(left);
        left.multiply(0.7);
        left.copyTo(buffer.getSamples(1));
    }
};
