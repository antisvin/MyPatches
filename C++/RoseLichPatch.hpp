#ifndef __ROSE_OSCILLATOR_PATCH_HPP__
#define __ROSE_OSCILLATOR_PATCH_HPP__

#include "MultiOscillator.hpp"
#include "SineOscillator.h"
#include "SquareOscillator.hpp"
#include "TriangleOscillator.hpp"
#include "VoltsPerOctave.h"
#include "Quantizer.hpp"
#include "LockableValue.hpp"
#include "RoseOscillator.hpp"
#include "Resample.h"
#include "MultiProcessor.hpp"
#include "Wavefolder.hpp"


#define BASE_FREQ 55.f
#define MAX_FM_AMOUNT 0.1f
#define EXT_FM
#define MAX_RATIO 16

//#define QUANTIZER Quantizer12TET
// Uncomment to quantize

#define P_TUNE      PARAMETER_A
#define P_HARMONICS PARAMETER_B
#define P_MULT      PARAMETER_C
#define P_DIV       PARAMETER_D
#define P_FEEDBACK1 PARAMETER_AA
#define P_FB2_DIR   PARAMETER_AB
#define P_FB2_MAG   PARAMETER_AC
#define P_FOLD      PARAMETER_AD
#define P_DIR       PARAMETER_AE
#define P_MAG       PARAMETER_AF
#define P_CHAOS_X   PARAMETER_F
#define P_CHAOS_LFO PARAMETER_G

using SmoothParam = LockableValue<SmoothValue<float>, float>;
using StiffParam = LockableValue<StiffValue<float>, float>;
using RatioQuantizer = Quantizer<MAX_RATIO, true>;

class ModOscillator : public MultiOscillator {
public:
    ModOscillator(float freq, float sr = 48000)
        : MultiOscillator(3, freq, sr)
        , osc0(freq, sr)
        , osc1(freq, sr)
        , osc2(freq, sr) {
        addOscillator(&osc0);
        addOscillator(&osc1);
        addOscillator(&osc2);
    }

private:
    SquareOscillator osc0;
    SineOscillator osc1;
    TriangleOscillator osc2;
};


using StereoWavefolder = MultiProcessor<Wavefolder<>, 2>;

class RoseLichPatch : public Patch {
private:
    RoseOscillator osc;
    ModOscillator mod;
    SineOscillator mod_lfo;
    float attr_x = 0.0, attr_y = 0.0;
    const float attr_a = -0.827;
    const float attr_b = -1.637;
    const float attr_c = 1.659;
    const float attr_d = -0.943;
    ComplexFloat cmp_offset;
    ComplexFloat sample;
#ifdef QUANTIZER
    QUANTIZER quantizer;
#endif
    bool isModeA = false, isModeB = false;
    float fm_ratio;
    float fm_amount;
    VoltsPerOctave hz;
    // QuadraturePhaser* phaser;
    StereoWavefolder wavefolder;
    float lfo_sample;
    SmoothParam p_feedback = SmoothParam(0.01);
    StiffParam p_divisor = StiffParam(1.f / float(MAX_RATIO + 1));
    StiffParam p_multiplier = StiffParam(1.f / float(MAX_RATIO));
    SmoothParam p_ratio = SmoothParam(0.01);
    SmoothParam p_harmonics = SmoothParam(0.01);
    SmoothParam p_fb_direction = SmoothParam(0.01);
    SmoothParam p_fb_magnitude = SmoothParam(0.01);
//    StiffParam p_fm_divisor = StiffParam(1.f / float(MAX_FM_RATIO + 1));
//    StiffParam p_fm_multiplier = StiffParam(1.f / float(MAX_FM_RATIO));
//    SmoothParam p_fm_ratio = SmoothParam(0.01);
    SmoothParam p_fold = SmoothParam(0.01);
    SmoothParam p_direction = SmoothParam(0.01);
    SmoothParam p_magnitude = SmoothParam(0.01);
#ifndef EXT_FM
    FloatArray env_copy;
#endif
    inline float crossfade(float a, float b, float fade) {
        return a * fade + b * (1 - fade);
    }

public:
    RoseLichPatch()
        : mod_lfo(0.75, getSampleRate() / getBlockSize())
        , osc(BASE_FREQ, getSampleRate())
        , mod(BASE_FREQ, getSampleRate())
    {
        registerParameter(P_TUNE, "Tune");
        registerParameter(P_HARMONICS, "Harmonics");
        registerParameter(P_MULT, "Multiplier");
        registerParameter(P_DIV, "Divisor");
        registerParameter(P_FEEDBACK1, "Feedback1");
        setParameterValue(P_FEEDBACK1, 0.f);
        registerParameter(P_FB2_MAG, "Feedbaack2 Magnitude");
        registerParameter(P_FB2_DIR, "Feedbaack2 Phase");        
//        setParameterValue(P_FM_MULT, 0.5);
//        registerParameter(P_FM_DIV, "FM Divisor");
//        setParameterValue(P_FM_DIV, 0.5);
        registerParameter(P_FOLD, "Fold");
        registerParameter(P_DIR, "Direction");
        registerParameter(P_MAG, "Offset");
        registerParameter(P_CHAOS_X, "ChaosX>");
        registerParameter(P_CHAOS_LFO, "ChaoLFO>");

        p_feedback.rawValue().lambda = 0.9;
        p_multiplier.rawValue().delta = (1.f / float(MAX_RATIO + 1));
        p_divisor.rawValue().delta = (1.f / float(MAX_RATIO + 2));
        p_harmonics.rawValue().lambda = 0.9;
        p_fb_direction.rawValue().lambda = 0.9;
        p_fb_magnitude.rawValue().lambda = 0.9;
        p_fold.rawValue().lambda = 0.9;
        p_direction.rawValue().lambda = 0.9;
        p_magnitude.rawValue().lambda = 0.9;

        setButton(BUTTON_A, false, 0);
        setButton(BUTTON_B, false, 0);
//        lockAll();
#ifndef EXT_FM
        env_copy = FloatArray::create(getBlockSize());
#endif
    }

    ~RoseLichPatch() {
#ifndef EXT_FM
        FloatArray::destroy(env_copy);
#endif
    }

    void lockAll() {
        p_feedback.setLock(true);
        p_ratio.setLock(true);
        p_multiplier.setLock(true);
        p_divisor.setLock(true);
        p_harmonics.setLock(true);
        p_fb_direction.setLock(true);
        p_fb_magnitude.setLock(true);
        p_fold.setLock(true);
        p_direction.setLock(true);
        p_magnitude.setLock(true);
    }

    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        switch (bid) {
        case BUTTON_A:
            if (value)
                isModeA = !isModeA;
            setButton(BUTTON_A, isModeA, 0);
            if (isModeA) {
                setButton(BUTTON_B, false, 0);
                isModeB = false;
            }
            lockAll();
            break;
        case BUTTON_B:
            if (value)
                isModeB = !isModeB;
            setButton(BUTTON_B, isModeB, 0);
            if (isModeB) {
                setButton(BUTTON_A, false, 0);
                isModeA = false;
            }
            lockAll();
            break;
        default:
            break;
        }
    }

    void processAttractor(float chaos) {
        float old_x = attr_x, old_y = attr_y;
        attr_x = sin(attr_a * old_y) - cos(attr_b * old_x);
        attr_y = sin(attr_c * old_x) - cos(attr_d * old_y);
        lfo_sample = mod_lfo.generate(attr_y * chaos * 0.02);
    }

    void processAudio(AudioBuffer& buffer) {
        // CV output
        processAttractor(1.0);
        setParameterValue(P_CHAOS_X, attr_x * 0.5 + 0.5);
        setParameterValue(P_CHAOS_LFO, lfo_sample * 0.5 + 0.5);
        setButton(BUTTON_C, lfo_sample >= 0.0);

        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

        // Tuning
        float tune = -4.0 + getParameterValue(P_TUNE) * 4.0;
#ifndef QUANTIZER
        hz.setTune(tune);
#endif
// Carrier / modulator frequency
#ifdef QUANTIZER
        float volts = quantizer.process(tune + hz.sampleToVolts(left[0]));
        float freq = hz.voltsToHertz(volts);
#else
        float freq = hz.getFrequency(left[0]);
#endif

        // Update params in FM settings
        if (isModeA) {
            setParameterValue(PARAMETER_AA, getParameterValue(PARAMETER_B));
            setParameterValue(PARAMETER_AB, getParameterValue(PARAMETER_C));
            setParameterValue(PARAMETER_AC, getParameterValue(PARAMETER_D));
        }
        else if (isModeB) {
            setParameterValue(PARAMETER_AD, getParameterValue(PARAMETER_B));
            setParameterValue(PARAMETER_AE, getParameterValue(PARAMETER_C));
            setParameterValue(PARAMETER_AF, getParameterValue(PARAMETER_D));
        }
        else {
            p_harmonics.update(getParameterValue(P_HARMONICS));
            p_multiplier.update(getParameterValue(P_MULT));
            p_divisor.update(getParameterValue(P_DIV));
            p_ratio.update(getParameterValue(P_MULT));
            int divisor = (MAX_RATIO + 1) * p_divisor.getValue();
            if (divisor == 0) {
                osc.setRatio(p_ratio.getValue() * MAX_RATIO);
            }
            else {                
                int multiplier = MAX_RATIO * p_multiplier.getValue() + 1;
                osc.setRatio(float(multiplier) / float(divisor));
            }
        }

#ifndef EXT_FM
        env_copy.copyFrom(right);
#endif
        p_feedback.update(getParameterValue(P_FEEDBACK1));
        p_fb_direction.update(getParameterValue(P_FB2_DIR));
        p_fb_magnitude.update(getParameterValue(P_FB2_MAG));

        p_fold.update(getParameterValue(P_FOLD) * 2.f);
        p_direction.update(getParameterValue(P_DIR));
        p_magnitude.update(getParameterValue(P_MAG));

        osc.setFrequency(freq);
        osc.setHarmonics(p_harmonics.getValue());
        osc.setFeedback1(p_feedback.getValue());
        osc.setFeedback2(p_fb_magnitude.getValue(), p_fb_direction.getValue() * M_PI * 4.f);

        // Generate modulator
        //mod.generate(left);

        // Use mod + ext input for FM
        left.multiply(MAX_FM_AMOUNT);
#ifdef EXT_FM
//        right.multiply(ext_fm_amt * MAX_FM_AMOUNT);
        right.add(left);
#else
        right.copyFrom(left);
#endif

        osc.generate(buffer, right);

        
        //biquad->setLowPass(ap_coef * getSampleRate(), 1.0);
        //biquad->setBandPass(freq * (0.5 + ap_coef * 4.f), 1.0);
        //biquad->process(buffer, buffer);

        float fold = p_fold.getValue() + 1.0;
        float offset_phase = p_direction.getValue() * M_PI;
        float offset_mag = p_magnitude.getValue();
        cmp_offset.setPolar(offset_mag, offset_phase);

        buffer.getSamples(0).multiply(fold + abs(cmp_offset.re));
        buffer.getSamples(1).multiply(fold + abs(cmp_offset.im));

        wavefolder.process(buffer, buffer);

#ifndef EXT_FM
        if (ext_env_amt >= 0.5) {
            left.multiply(env_copy);
            right.multiply(env_copy);
        }
        else {
            left.multiply(1.0 - ext_env_amt * 2);
            right.multiply(1.0 - ext_env_amt * 2);
        }
#endif
    }
};
#endif
