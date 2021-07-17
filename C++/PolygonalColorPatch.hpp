#ifndef __POLYGONAL_MAGUS_PATCH_HPP__
#define __POLYGONAL_MAGUS_PATCH_HPP__

#include "PolygonalOscillator.hpp"
#include "MultiOscillator.hpp"
#include "SquareOscillator.hpp"
#include "TriangleOscillator.hpp"
#include "SineOscillator.h"
#include "QuadratureSineOscillator.h"
#include "VoltsPerOctave.h"
#include "Resample.h"
#include "Quantizer.hpp"
#include "ColourScreenPatch.h"
#include "ColorUtils.hpp"
#include "CircularBuffer.h"


#define BASE_FREQ 55.f
#define MAX_POLY 20.f
#define PREVIEW_FREQ 2.0f
#define FM_AMOUNT_MULT (1.f / MAX_FM_AMOUNT)
//#define QUANTIZER Quantizer24TET
#define NUM_PIXELS 8192 // 6 bytes per pixel
#define SCREEN_SMOOTH 0.9f


//#define OVERSAMPLE_FACTOR 2

using ColourBuffer = CircularBuffer<Pixel>;

class ModOscillator : public MultiOscillator<4> {
public:
    ModOscillator() {
        addOscillator(&osc0);
        addOscillator(&osc1);
        addOscillator(&osc2);
        addOscillator(&osc3);
        addOscillator(&osc4);
    }
    void setSampleRate(float sr) {
        osc0.setSampleRate(sr / 2);
        osc1.setSampleRate(sr / 2);
        osc2.setSampleRate(sr);
        osc3.setSampleRate(sr * 2);
        osc4.setSampleRate(sr * 2);
    }
    void setFrequency(float freq) {
        osc0.setFrequency(freq);
        osc1.setFrequency(freq);
        osc2.setFrequency(freq);
        osc3.setFrequency(freq);
        osc4.setFrequency(freq);
    }

private:
    SquareOscillator osc0;
    SineOscillator osc1, osc2, osc3;
    TriangleOscillator osc4;
};

class PolygonalColorPatch : public ColourScreenPatch {
private:
    PolygonalOscillator osc;
    ModOscillator mod;
    ColourBuffer* pixels;
    uint16_t screen_radius = 0;
    uint16_t screen_center_x = 0, screen_center_y = 0;
    PolygonalOscillator::NPolyQuant quant;
    float attr_x, attr_y;
    const float attr_a = -0.827;
    const float attr_b = -1.637;
    const float attr_c = 1.659;
    const float attr_d = -0.943;
    QuadratureSineOscillator mod_lfo;
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
    float fm_ratio;
    float fm_amount;
    VoltsPerOctave hz;
//    StereoBiquadFilter* highpass;
//    StereoBiquadFilter* lowpass;
    int delayL, delayR;    
    inline float crossfade(float a, float b, float fade) {
        return a * fade + b * (1 - fade);
    }

public:
    PolygonalColorPatch()
        : hz(true) {
        mod_lfo.setSampleRate(getSampleRate() / getBlockSize());
        mod_lfo.setFrequency(0.75);
#ifdef OVERSAMPLE_FACTOR
        osc.setSampleRate(getSampleRate() * OVERSAMPLE_FACTOR);
        mod.setSampleRate(getSampleRate() * OVERSAMPLE_FACTOR);
#else
        osc.setSampleRate(getSampleRate());
        mod.setSampleRate(getSampleRate());
#endif
        osc.setFrequency(BASE_FREQ);
        // Polygonal VCO
        registerParameter(PARAMETER_A, "Tune");
        registerParameter(PARAMETER_B, "Quantize");
        registerParameter(PARAMETER_AA, "NPoly");
        setParameterValue(PARAMETER_AA, 0.15);
        registerParameter(PARAMETER_AB, "Teeth");
        // FM / env
        registerParameter(PARAMETER_C, "FM Amount");
        setParameterValue(PARAMETER_C, 0.0);
        registerParameter(PARAMETER_D, "FM Mod Shape");
        setParameterValue(PARAMETER_D, 0.0);
        //registerParameter(PARAMETER_AC, "Ext FM Amt");
        //setParameterValue(PARAMETER_AC, 0.0);
        //registerParameter(PARAMETER_AD, "Ext Env");
        //setParameterValue(PARAMETER_AD, 1.0);
        // Chaotic modulator
        registerParameter(PARAMETER_G, "LFO freq");
        setParameterValue(PARAMETER_G, 0.1);
        registerParameter(PARAMETER_H, "ModX>");
        registerParameter(PARAMETER_AG, "Chaos Amt");
        registerParameter(PARAMETER_AH, "ModY>");
        // Separate modulator outputs
        registerParameter(PARAMETER_BA, "LfoX>");
        registerParameter(PARAMETER_BB, "LfoY>");
        registerParameter(PARAMETER_BC, "AttrX>");
        registerParameter(PARAMETER_BD, "AttrY>");
#ifdef OVERSAMPLE_FACTOR
        upsampler_pitch = UpSampler::create(1, OVERSAMPLE_FACTOR);
        upsampler_fm = UpSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_left = DownSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_right = DownSampler::create(1, OVERSAMPLE_FACTOR);
        oversample_buf = AudioBuffer::create(2, getBlockSize() * OVERSAMPLE_FACTOR);
#endif
//        highpass = StereoBiquadFilter::create(1);
//        highpass->setHighPass(40 / (getSampleRate() / 2),
//            FilterStage::BUTTERWORTH_Q); // dc filter
//        lowpass = StereoBiquadFilter::create(1);
//        lowpass->setLowPass(8000 / (getSampleRate() / 2), FilterStage::BUTTERWORTH_Q);
        pixels = ColourBuffer::create(NUM_PIXELS);
    }

    ~PolygonalColorPatch() {
        ColourBuffer::destroy(pixels);
//        StereoBiquadFilter::destroy(highpass);
//        StereoBiquadFilter::destroy(lowpass);
#ifdef OVERSAMPLE_FACTOR
        UpSampler::destroy(upsampler_pitch);
        UpSampler::destroy(upsampler_fm);
        DownSampler::destroy(downsampler_left);
        DownSampler::destroy(downsampler_right);
        AudioBuffer::destroy(oversample_buf);
#endif
    }

    void processScreen(ColourScreenBuffer& screen) {
        // This zooms in visualization when patch starts
        screen_radius = (float)screen_radius * SCREEN_SMOOTH +
            float(std::min(screen.getWidth(), screen.getHeight())) *
                (1.f - SCREEN_SMOOTH);
        screen_center_x = screen.getWidth() / 2;
        screen_center_y = screen.getHeight() / 2;

        //uint16_t hue_16_bit = RGB888toRGB565(HSBtoRGB(hue_step * hue, 1.f, 1.f));
        uint16_t hue_16_bit = CYAN;

        for (size_t i = 0; i < NUM_PIXELS; i++) {
            Pixel px = pixels->readAt(i);
//            uint16_t hue_16_bit = RGB888toRGB565(HSBtoRGB(i * 360.f * hue_step, 1.f, 1.f));
            
            screen.setPixel(px.x, px.y, hue_16_bit);//
            //hue_16bit);
        }
    }

    void processAttractor(float chaos) {
        float old_x = attr_x, old_y = attr_y;
        attr_x = sin(attr_a * old_y) - cos(attr_b * old_x);
        attr_y = sin(attr_c * old_x) - cos(attr_d * old_y);
        auto lfo = mod_lfo.generate();
        setParameterValue(PARAMETER_BA, lfo.re * 0.5 + 0.5);
        setParameterValue(PARAMETER_BB, lfo.im * 0.5 + 0.5);
        setParameterValue(PARAMETER_BC, attr_x * 0.5 + 0.5);
        setParameterValue(PARAMETER_BD, attr_y * 0.5 + 0.5);
        ComplexFloat lfo_pt(crossfade(attr_x * 0.5, lfo.re, chaos) * 0.5 + 0.5,
            crossfade(attr_y * 0.5, lfo.im, chaos) * 0.5 + 0.5);
        setParameterValue(PARAMETER_H, lfo_pt.re);
        setParameterValue(PARAMETER_AH, lfo_pt.im);
    }

    void processAudio(AudioBuffer& buffer) {
        // Attractor/lfo CV output
        mod_lfo.setFrequency(0.01 + getParameterValue(PARAMETER_G) * 4.99);
        processAttractor(getParameterValue(PARAMETER_AG));

        float tune = -4.0 + getParameterValue(PARAMETER_A) * 4.0;
        #ifndef QUANTIZER
        hz.setTune(tune);
        #endif
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

        fm_amount = getParameterValue(PARAMETER_C);
        fm_ratio = getParameterValue(PARAMETER_D);

        osc.setParams(updateQuant(getParameterValue(PARAMETER_B)),
            getParameterValue(PARAMETER_AA), getParameterValue(PARAMETER_AB));

        // Carrier / modulator frequency
        #ifdef QUANTIZER
        float volts = quantizer.process(tune + hz.sampleToVolts(left[0]));
        float freq = hz.voltsToHertz(volts);
        #else
        float freq = hz.getFrequency(left[0]);
        #endif
        
        osc.setFrequency(freq);
        mod.setFrequency(freq);

        // Generate modulator
        mod.setMorph(fm_ratio);
        mod.generate(right);
        right.multiply(fm_amount);

#ifdef OVERSAMPLE_FACTOR
        upsampler_pitch->process(left, oversample_buf->getSamples(0));
        upsampler_fm->process(right, oversample_buf->getSamples(1));
        AudioBuffer& audio_buf = *oversample_buf;
#else
        AudioBuffer& audio_buf = buffer;
#endif

        // Generate quadrature output
        osc.generate(audio_buf, audio_buf.getSamples(1));

#ifdef OVERSAMPLE_FACTOR
        downsampler_left->process(audio_buf.getSamples(0), left);
        downsampler_right->process(audio_buf.getSamples(1), right);
#endif

        for (size_t i = 0; i < buffer.getSize(); i += 4) {
            Pixel new_px(
                screen_center_x + int16_t(left[i] * screen_radius),
                screen_center_y + int16_t(right[i] * screen_radius),
                HSBto565(1.f, 1.f, 1.f));
            pixels->write(new_px);
            //prev_px = new_px;
        }

//        lowpass->process(buffer, buffer);

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
