#ifndef __POLYGONAL_LICH_PATCH_HPP__
#define __POLYGONAL_LICH_PATCH_HPP__

#include "Oscillators/PolygonalOscillator.hpp"
#include "Oscillators/MultiOscillator.hpp"
#include "SineOscillator.h"
#include "VoltsPerOctave.h"
#include "DelayLine.hpp"
#include "QuadraturePhaser.hpp"
//#include <algorithm>


#define BASE_FREQ 55.f
#define MAX_POLY 20.f
#define MAX_FM_AMOUNT 0.1f
#define FM_AMOUNT_MULT (1.f / MAX_FM_AMOUNT)

// Oversampling ends up sounding fairly disappointing
#define OVERSAMPLE_FACTOR 2

#ifdef OVERSAMPLE_FACTOR
#include "Resample.h"
#endif

//#define EXT_FM

struct Point {
    Point() = default;
    Point(float x, float y) : x(x), y(y) {};
    float x, y;

    friend Point operator+(Point lhs, const Point& rhs) {
        return Point(lhs.x + rhs.x, lhs.y + rhs.y);
    }

    friend Point operator-(Point lhs, const Point& rhs) {
        return Point(lhs.x - rhs.x, lhs.y - rhs.y);
    }

    Point operator*(float val) {
        return Point(x * val, y * val);
    }    
};


class ModOscillator : public MultiOscillator {
public:
    ModOscillator(float freq, float sr = 48000)
        : MultiOscillator(3, freq, sr)
        , osc1(freq, sr / 2)
        , osc2(freq, sr)
        , osc3(freq, sr / 3) {
        addOscillator(&osc1);
        addOscillator(&osc2);
        addOscillator(&osc3);
    }
private:
    SineOscillator osc1, osc2, osc3;
};

class PolygonalLichPatch : public Patch {
private:
    PolygonalOscillator osc;
    ModOscillator mod;
    QuadratureSineOscillator mod_lfo;
    PolygonalOscillator::NPolyQuant quant;
    float attr_x = 0.0, attr_y = 0.0;
//    float attr_a = 1.641, attr_b = 1.902;
//    float attr_c = 0.316, attr_d = 1.525;
    float attr_a = -0.827, attr_b = -1.637;
    float attr_c = 1.659, attr_d = -0.943;
//    float attr_a = 2.75, attr_b = 0.2;

    inline float crossfade(float a, float b, float fade) {
        return a * fade + b * (1 - fade);
    }    
#ifdef OVERSAMPLE_FACTOR
    UpSampler* upsampler_pitch;
    UpSampler* upsampler_fm;
    DownSampler* downsampler_left;
    DownSampler* downsampler_right;
    AudioBuffer* oversample_buf;
#endif
    bool isModeA = false, isModeB = false;
    float fm_ratio;
    float fm_amount;
    VoltsPerOctave hz;
    QuadraturePhaser* phaser;
#ifndef EXT_FM
    FloatArray env_copy;
#endif
public:
    PolygonalLichPatch()
        : mod_lfo(1.0, getSampleRate() / getBlockSize())
#ifdef OVERSAMPLE_FACTOR
        , osc(BASE_FREQ, getSampleRate() * OVERSAMPLE_FACTOR)
        , mod(BASE_FREQ, getSampleRate() * OVERSAMPLE_FACTOR)
#else
        , osc(BASE_FREQ, getSampleRate())
        , mod(BASE_FREQ, getSampleRate())
#endif
    {
        registerParameter(PARAMETER_A, "Tune");
        registerParameter(PARAMETER_B, "Teeth");
        registerParameter(PARAMETER_C, "NPoly");
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
        setParameterValue(PARAMETER_AC, 1.0);
#endif
        registerParameter(PARAMETER_AD, "Rotation");
        setParameterValue(PARAMETER_AD, 0.0);
        registerParameter(PARAMETER_AE, "Shift");
        setParameterValue(PARAMETER_AE, 0.0);
        registerParameter(PARAMETER_AF, "Chaos");
        setParameterValue(PARAMETER_AF, 0.0);
        registerParameter(PARAMETER_F, "ChaosX>");
        registerParameter(PARAMETER_G, "ChaosY>");
#ifdef OVERSAMPLE_FACTOR
        upsampler_pitch = UpSampler::create(1, OVERSAMPLE_FACTOR);
        upsampler_fm = UpSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_left = DownSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_right = DownSampler::create(1, OVERSAMPLE_FACTOR);
        oversample_buf = AudioBuffer::create(2, getBlockSize() * OVERSAMPLE_FACTOR);
        phaser = QuadraturePhaser::create(getSampleRate(), getBlockSize() * OVERSAMPLE_FACTOR);
#else
        phaser = QuadraturePhaser::create(getSampleRate(), getBlockSize());
#endif
        setButton(BUTTON_A, false, 0);
        setButton(BUTTON_B, false, 0);
#ifndef EXT_FM
        env_copy = FloatArray::create(getBlockSize());
#endif
    }

    ~PolygonalLichPatch(){
        QuadraturePhaser::destroy(phaser);
#ifdef OVERSAMPLE_FACTOR
        UpSampler::destroy(upsampler_pitch);
        UpSampler::destroy(upsampler_fm);
        DownSampler::destroy(downsampler_left);
        DownSampler::destroy(downsampler_right);
        AudioBuffer::destroy(oversample_buf);
#endif
#ifndef EXT_FM
        FloatArray::destroy(env_copy);
#endif
    }

    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        switch(bid){
        case BUTTON_A:
            if(value)
                isModeA = !isModeA;
            setButton(BUTTON_A, isModeA, 0);
            if (isModeA) {
                setButton(BUTTON_B, false, 0);
                isModeB = false;
            }
            break;
        case BUTTON_B:
            if(value)
                isModeB = !isModeB;
            setButton(BUTTON_B, isModeB, 0);        
            if (isModeB) {
                setButton(BUTTON_A, false, 0);
                isModeA = false;
            }
            break;
        default:
            break;
        }
    }

    void processAttractor(float chaos) {
        //float t = attr_y;
        //attr_y = -attr_b * attr_x + attr_a * attr_y - attr_y * attr_y * attr_y;
        //attr_x = t;
        //float lfo = 1.0 / getSampleRate();
        float old_x = attr_x * chaos, old_y = attr_y * chaos;
        attr_x = sin(attr_a * old_y) - cos(attr_b * old_x);
        attr_y = sin(attr_c * old_x) - cos(attr_d * old_y);
    }

    void processAudio(AudioBuffer& buffer) {
        processAttractor(getParameterValue(PARAMETER_AF));
        setParameterValue(PARAMETER_F, attr_x * 0.5 + 0.5);
        setParameterValue(PARAMETER_G, attr_y * 0.5 + 0.5);
        //setParameterValue(PARAMETER_G, atan2f(attr_y, attr_x) / M_PI / 2.0);

        hz.setTune(-3.0 + getParameterValue(PARAMETER_A) * 4.0);
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

#ifndef EXT_FM
        env_copy.copyFrom(right);
        float ext_env_amt = getParameterValue(PARAMETER_AC);
        env_copy.multiply(ext_env_amt);
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
            osc.setParams(
                updateQuant(getParameterValue(PARAMETER_D)),
                getParameterValue(PARAMETER_C),
                getParameterValue(PARAMETER_B));
        }

#ifdef EXT_FM
        float ext_fm_amt = getParameterValue(PARAMETER_AC);
#endif
        fm_amount = getParameterValue(PARAMETER_AA);
        fm_ratio = getParameterValue(PARAMETER_AB) * MAX_FM_AMOUNT;        
        phaser->setAmount(getParameterValue(PARAMETER_AD) * 0.0005);
        phaser->setControl(getParameterValue(PARAMETER_AE));

        // Carrier / modulator frequency
        float freq = hz.getFrequency(left[0]);
        osc.setFrequency(freq);
        mod.setFrequency(freq);

        // Generate modulator
        mod.setMorph(fm_ratio);
        mod.generate(left);

        // Use mod + ext input for FM
        left.multiply(fm_amount * MAX_FM_AMOUNT);
#ifdef EXT_FM
        right.multiply(ext_fm_amt * MAX_FM_AMOUNT);
        left.add(right);
#endif

#ifdef OVERSAMPLE_FACTOR
//        upsampler_pitch->process(left, oversample_buf->getSamples(0));
        upsampler_fm->process(left, oversample_buf->getSamples(1));
        AudioBuffer& audio_buf = *oversample_buf;
#else
        AudioBuffer& audio_buf = buffer;
#endif
        // Generate quadrature output
        osc.generate(audio_buf, audio_buf.getSamples(1));
        phaser->process(audio_buf, audio_buf);

#ifdef OVERSAMPLE_FACTOR
        downsampler_left->process(oversample_buf->getSamples(0), left);
        downsampler_right->process(oversample_buf->getSamples(1), right);
#endif
#ifndef EXT_FM
        left.multiply(env_copy);
        right.multiply(env_copy);
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
