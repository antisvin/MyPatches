#ifndef __POLYGONAL_LICH_PATCH_HPP__
#define __POLYGONAL_LICH_PATCH_HPP__

#include "PolygonalOscillator.hpp"
#include "MultiOscillator.hpp"
#include "SineOscillator.h"
#include "SquareOscillator.hpp"
#include "TriangleOscillator.hpp"
#include "VoltsPerOctave.h"
#include "DelayLine.hpp"
#include "FrequencyShifter.h"
#include "Quantizer.hpp"
#include "LockableValue.hpp"
#include "Resample.h"

#define BASE_FREQ 55.f
#define MAX_POLY 20.f
#define MAX_FM_AMOUNT 1.0f //0.1f
//#define QUANTIZER Quantizer12TET

#define OVERSAMPLE_FACTOR 4

#ifdef OVERSAMPLE_FACTOR
#include "Resample.h"
#endif

#define EXT_FM

using SmoothParam = LockableValue<SmoothValue<float>, float>;
using StiffParam = LockableValue<StiffValue<float>, float>;

class ModOscillator : public MultiOscillator<5> {
public:
    ModOscillator() = default;
    void addOscillators(float freq, float sr = 48000) {
        osc0.setSampleRate(sr / 2);
        osc0.setFrequency(freq);
        osc1.setSampleRate(sr / 2);
        osc1.setFrequency(freq);
        osc2.setSampleRate(sr);
        osc2.setFrequency(freq);
        osc3.setSampleRate(sr * 2);
        osc3.setFrequency(freq);
        osc4.setSampleRate(sr * 2);
        osc4.setFrequency(freq);
        addOscillator(&osc0);
        addOscillator(&osc1);
        addOscillator(&osc2);
        addOscillator(&osc3);
        addOscillator(&osc4);
    }

private:
    SquareOscillator osc0;
    SineOscillator osc1, osc2, osc3;
    TriangleOscillator osc4;
};

class PolygonalLichPatch : public Patch {
private:
    PolygonalOscillator osc;
    ModOscillator mod;
    SineOscillator mod_lfo;
    PolygonalOscillator::NPolyQuant quant;
    float attr_x = 0.0, attr_y = 0.0;
    const float attr_a = -0.827;
    const float attr_b = -1.637;
    const float attr_c = 1.659;
    const float attr_d = -0.943;
#ifdef QUANTIZER
    QUANTIZER quantizer;
#endif
#ifdef OVERSAMPLE_FACTOR
    UpSampler* upsampler_pitch;
    UpSampler* upsampler_fm;
    DownSampler* downsampler_left;
    DownSampler* downsampler_right;
    AudioBuffer* oversample_buf;
#endif
    ComplexFloatArray tmp;
    bool isModeA = false, isModeB = false;
    float fm_ratio;
    float fm_amount;
    VoltsPerOctave hz;
    ComplexRotator* shifter;
    float lfo_sample;
    SmoothParam p_teeth = SmoothParam(0.01);
    SmoothParam p_npoly = SmoothParam(0.01);
    StiffParam p_quantize = StiffParam(0.2);
    SmoothParam p_fm_amount = SmoothParam(0.01);
    SmoothParam p_fm_shape = SmoothParam(0.01);
    SmoothParam p_ext_amount = SmoothParam(0.01);
    SmoothParam p_rotation = SmoothParam(0.01);
    SmoothParam p_shift = SmoothParam(0.01);
    SmoothParam p_chaos = SmoothParam(0.01, 0.5);
#ifndef EXT_FM
    FloatArray env_copy;
#endif
    inline float crossfade(float a, float b, float fade) {
        return a * fade + b * (1 - fade);
    }

public:
    PolygonalLichPatch() {
        mod_lfo.setSampleRate(getSampleRate() / getBlockSize());
        mod_lfo.setFrequency(0.75);
#ifdef OVERSAMPLE_FACTOR
        osc.setSampleRate(getSampleRate() * OVERSAMPLE_FACTOR);
        osc.setFrequency(BASE_FREQ);
        mod.addOscillators(BASE_FREQ, getSampleRate() * OVERSAMPLE_FACTOR);
        tmp = ComplexFloatArray::create(getBlockSize() * OVERSAMPLE_FACTOR);
#else
        osc.setSampleRate(getSampleRate());
        osc.setFrequency(BASE_FREQ);
        mod.addOscillators(BASE_FREQ, getSampleRate());
        tmp = ComplexFloatArray::create(getBlockSize());
#endif
        registerParameter(PARAMETER_A, "Tune");
        registerParameter(PARAMETER_B, "NPoly");
        registerParameter(PARAMETER_C, "Teeth");
        registerParameter(PARAMETER_D, "Quantize");
        registerParameter(PARAMETER_AA, "FM Amount");
        setParameterValue(PARAMETER_AA, 0.0);
        registerParameter(PARAMETER_AB, "FM Mod Shape");
        setParameterValue(PARAMETER_AB, 0.0);
#ifdef EXT_FM
        registerParameter(PARAMETER_AC, "Ext FM Amount");
        setParameterValue(PARAMETER_AC, 0.0);
#else
        registerParameter(PARAMETER_AC, "Ext Env Amount");
        setParameterValue(PARAMETER_AC, 0.0);
#endif
        registerParameter(PARAMETER_AD, "Rotation");
        setParameterValue(PARAMETER_AD, 0.0);
        registerParameter(PARAMETER_AE, "Shift");
        setParameterValue(PARAMETER_AE, 0.0);
        registerParameter(PARAMETER_AF, "Chaos");
        setParameterValue(PARAMETER_AF, 0.5);
        registerParameter(PARAMETER_F, "ChaosX>");
        registerParameter(PARAMETER_G, "ChaoLFO>");

        p_npoly.rawValue().lambda = 0.9;
        p_teeth.rawValue().lambda = 0.9;
        p_quantize.rawValue().delta = 0.1;
        p_fm_amount.rawValue().lambda = 0.9;
        p_fm_shape.rawValue().lambda = 0.9;
        p_ext_amount.rawValue().lambda = 0.9;
        p_rotation.rawValue().lambda = 0.9;
        p_shift.rawValue().lambda = 0.9;
        p_chaos.rawValue().lambda = 0.9;

#ifdef OVERSAMPLE_FACTOR
        upsampler_pitch = UpSampler::create(1, OVERSAMPLE_FACTOR);
        upsampler_fm = UpSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_left = DownSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_right = DownSampler::create(1, OVERSAMPLE_FACTOR);
        oversample_buf = AudioBuffer::create(2, getBlockSize() * OVERSAMPLE_FACTOR);
        shifter = ComplexRotator::create(
            getSampleRate() * OVERSAMPLE_FACTOR, getBlockSize() * OVERSAMPLE_FACTOR);
#else
        shifter = ComplexRotator::create(getSampleRate(), getBlockSize());
#endif
        setButton(BUTTON_A, false, 0);
        setButton(BUTTON_B, false, 0);
#ifndef EXT_FM
        env_copy = FloatArray::create(getBlockSize());
#endif
    }

    ~PolygonalLichPatch() {
        ComplexRotator::destroy(shifter);
#ifdef OVERSAMPLE_FACTOR
        UpSampler::destroy(upsampler_pitch);
        UpSampler::destroy(upsampler_fm);
        DownSampler::destroy(downsampler_left);
        DownSampler::destroy(downsampler_right);
        AudioBuffer::destroy(oversample_buf);
#endif
        ComplexFloatArray::destroy(tmp);
#ifndef EXT_FM
        FloatArray::destroy(env_copy);
#endif
    }

    void lockAll() {
        p_teeth.setLock(true);
        p_npoly.setLock(true);
        p_quantize.setLock(true);
        p_fm_amount.setLock(true);
        p_fm_shape.setLock(true);
        p_ext_amount.setLock(true);
        p_rotation.setLock(true);
        p_shift.setLock(true);
        p_chaos.setLock(true);
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
        processAttractor(getParameterValue(PARAMETER_AF));
        setParameterValue(PARAMETER_F, attr_x * 0.5 + 0.5);
        setParameterValue(PARAMETER_G, lfo_sample * 0.5 + 0.5);
        setButton(BUTTON_C, lfo_sample >= 0.0);

        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

        // Tuning
        float tune = -4.0 + getParameterValue(PARAMETER_A) * 4.0;
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
            p_npoly.update(getParameterValue(PARAMETER_B));
            p_teeth.update(getParameterValue(PARAMETER_C));
            p_quantize.update(getParameterValue(PARAMETER_D));
            osc.setParams(updateQuant(p_quantize.getValue()),
                p_npoly.getValue(), p_teeth.getValue());
        }

#ifdef EXT_FM
        p_ext_amount.update(getParameterValue(PARAMETER_AC));
        float ext_fm_amt = p_ext_amount.getValue();
#else
        env_copy.copyFrom(right);
        float ext_env_amt =
            p_ext_amount.update(getParameterValue(PARAMETER_AC)).getValue();
        env_copy.multiply(ext_env_amt);
#endif
        p_fm_amount.update(getParameterValue(PARAMETER_AA));
        fm_amount = p_fm_amount.getValue();
        p_fm_shape.update(getParameterValue(PARAMETER_AB));
        fm_ratio = p_fm_shape.getValue();
        p_rotation.update(getParameterValue(PARAMETER_AD));
        shifter->setFrequency(p_rotation.getValue() * 10);
        p_shift.update(getParameterValue(PARAMETER_AE));
        shifter->setAmount(p_shift.getValue());

        osc.setFrequency(freq);
#ifdef OVERSAMPLE_FACTOR
        mod.setFrequency(freq / OVERSAMPLE_FACTOR);
#else
        mod.setFrequency(freq);
#endif

        // Generate modulator
        mod.setMorph(fm_ratio);
        mod.generate(left);

        // Use mod + ext input for FM
        left.multiply(fm_amount * MAX_FM_AMOUNT);
#ifdef EXT_FM
        right.multiply(ext_fm_amt * MAX_FM_AMOUNT);
        right.add(left);
#else
        right.copyFrom(left);
#endif

#ifdef OVERSAMPLE_FACTOR
        upsampler_fm->process(left, oversample_buf->getSamples(1));
        AudioBuffer& audio_buf = *oversample_buf;
#else
        AudioBuffer& audio_buf = buffer;
#endif
        // Generate quadrature output
        // return;
        // osc.generate(audio_buf);
        osc.generate(tmp, audio_buf.getSamples(1));
        tmp.copyTo(audio_buf.getSamples(0), audio_buf.getSamples(1));
        shifter->process(audio_buf, audio_buf);

#ifdef OVERSAMPLE_FACTOR
        downsampler_left->process(oversample_buf->getSamples(0), left);
        downsampler_right->process(oversample_buf->getSamples(1), right);
#endif
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

    PolygonalOscillator::NPolyQuant updateQuant(float val) {
        /**
         * Quantize with dead zones at
         * 0.15 - 0.25
         * 0.35 - 0.45
         * 0.55 - 0.65
         * 0.75 - 0.85
         */
        if (val < 0.15f) {
            quant = PolygonalOscillator::NPolyQuant::NONE;
        }
        else if (val > 0.25) {
            if (val < 0.35) {
                quant = PolygonalOscillator::NPolyQuant::Q025;
            }
            else if (val > 0.45) {
                if (val < 0.55) {
                    quant = PolygonalOscillator::NPolyQuant::Q033;
                }
                else if (val > 0.65) {
                    if (val < 0.75) {
                        quant = PolygonalOscillator::NPolyQuant::Q050;
                    }
                    else if (val > 0.85) {
                        quant = PolygonalOscillator::NPolyQuant::Q100;
                    }
                }
            }
        }
        return quant;
    }
};

#endif
