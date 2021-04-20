#ifndef __POLYGONAL_MAGUS_PATCH_HPP__
#define __POLYGONAL_MAGUS_PATCH_HPP__

#include "MonochromeScreenPatch.h"
#include "PolygonalOscillator.hpp"
#include "MultiOscillator.hpp"
#include "SquareOscillator.hpp"
#include "TriangleOscillator.hpp"
#include "SineOscillator.h"
#include "QuadratureSineOscillator.hpp"
#include "VoltsPerOctave.h"
#include "DelayLine.hpp"
#include "Resample.h"
#include "Quantizer.hpp"


#define BASE_FREQ 55.f
#define MAX_POLY 20.f
#define PREVIEW_FREQ 2.0f
#define SCREEN_WIDTH 128 // Hardcoded to make initialization easier
#define SCREEN_BUF_OVERLAP (SCREEN_WIDTH / 8)
#define SCREEN_PREVIEW_BUF_SIZE (SCREEN_WIDTH / 2 + SCREEN_BUF_OVERLAP)
#define MAX_FM_AMOUNT 0.05f
#define FM_AMOUNT_MULT (1.f / MAX_FM_AMOUNT)
#define SCREEN_OFFSET(x) ((x)*SCREEN_WIDTH / 4)
#define MAX_DELAY (64 * 1024)
#define MAX_FEEDBACK 0.7
#define QUANTIZER Quantizer24TET

//#define OVERSAMPLE_FACTOR 2

struct Point {
    float x, y;
    Point() = default;
    Point(float x, float y)
        : x(x)
        , y(y) {};
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
        : MultiOscillator(5, freq, sr)
        , osc0(freq, sr / 2)
        , osc1(freq, sr / 2)
        , osc2(freq, sr)
        , osc3(freq, sr * 2)
        , osc4(freq, sr * 2) {
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

class PolygonalMagusPatch : public MonochromeScreenPatch {
private:
    PolygonalOscillator osc;
    ModOscillator mod;
    ModOscillator mod_preview;
    FloatArray lfo_preview;
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
    FloatArray preview;
    FloatArray env_copy;
    float fm_ratio;
    float fm_amount;
    VoltsPerOctave hz;
    DelayLine<Point, 128> preview_buf;
    DelayLine<Point, 128> lfo_preview_buf;
    DelayLine<float, MAX_DELAY> delayBufferL, delayBufferR;
    StereoBiquadFilter* highpass;
    StereoBiquadFilter* lowpass;
    int delayL, delayR;    
    inline float crossfade(float a, float b, float fade) {
        return a * fade + b * (1 - fade);
    }

public:
    PolygonalMagusPatch()
        : hz(true)
        , mod_lfo(BASE_FREQ, getSampleRate() / getBlockSize())
#ifdef OVERSAMPLE_FACTOR
        , osc(BASE_FREQ, getSampleRate() * OVERSAMPLE_FACTOR)
        , mod(BASE_FREQ, getSampleRate() * OVERSAMPLE_FACTOR)
#else
        , osc(BASE_FREQ, getSampleRate())
        , mod(BASE_FREQ, getSampleRate())
#endif
        , mod_preview(PREVIEW_FREQ, float(SCREEN_WIDTH * 3 / 8)) {
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
        registerParameter(PARAMETER_AC, "Ext FM Amt");
        setParameterValue(PARAMETER_AC, 0.0);
        //registerParameter(PARAMETER_AD, "Ext Env");
        //setParameterValue(PARAMETER_AD, 1.0);
        // Delay
        registerParameter(PARAMETER_E, "Ping");
        setParameterValue(PARAMETER_E, 0.5);
        registerParameter(PARAMETER_F, "Pong");
        setParameterValue(PARAMETER_F, 0.5);
        registerParameter(PARAMETER_AE, "Feedback");
        setParameterValue(PARAMETER_AE, 0.5);
        registerParameter(PARAMETER_AF, "Dry / wet");
        setParameterValue(PARAMETER_AF, 0.5);
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
        preview = FloatArray::create(SCREEN_PREVIEW_BUF_SIZE);
        preview_buf = DelayLine<Point, 128>::create();
        lfo_preview_buf = DelayLine<Point, 128>::create();
        env_copy = FloatArray::create(getBlockSize());
        delayBufferL = DelayLine<float, MAX_DELAY>::create();
        delayBufferR = DelayLine<float, MAX_DELAY>::create();        
#ifdef OVERSAMPLE_FACTOR
        upsampler_pitch = UpSampler::create(1, OVERSAMPLE_FACTOR);
        upsampler_fm = UpSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_left = DownSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_right = DownSampler::create(1, OVERSAMPLE_FACTOR);
        oversample_buf = AudioBuffer::create(2, getBlockSize() * OVERSAMPLE_FACTOR);
#endif
        highpass = StereoBiquadFilter::create(1);
        highpass->setHighPass(40 / (getSampleRate() / 2),
            FilterStage::BUTTERWORTH_Q); // dc filter
        lowpass = StereoBiquadFilter::create(1);
        lowpass->setLowPass(8000 / (getSampleRate() / 2), FilterStage::BUTTERWORTH_Q);
    }

    ~PolygonalMagusPatch() {
        FloatArray::destroy(preview);
        DelayLine<Point, 128>::destroy(preview_buf);
        DelayLine<Point, 128>::destroy(lfo_preview_buf);
        FloatArray::destroy(env_copy);
        DelayLine<float, MAX_DELAY>::destroy(delayBufferL);
        DelayLine<float, MAX_DELAY>::destroy(delayBufferR);
        StereoBiquadFilter::destroy(highpass);
        StereoBiquadFilter::destroy(lowpass);
#ifdef OVERSAMPLE_FACTOR
        UpSampler::destroy(upsampler_pitch);
        UpSampler::destroy(upsampler_fm);
        DownSampler::destroy(downsampler_left);
        DownSampler::destroy(downsampler_right);
        AudioBuffer::destroy(oversample_buf);
#endif
    }

    void processScreen(MonochromeScreenBuffer& screen) {
        // New data gets added after previous cycle's tail
        FloatArray wave_array(
            preview.getData() + SCREEN_BUF_OVERLAP, SCREEN_PREVIEW_BUF_SIZE);
        float height_offset = screen.getHeight() * 0.3;
        float mod_prev = height_offset;
        mod_preview.reset();
        mod_preview.setMorph(fm_ratio);
        // Render oscillator, modulator and chaotic LFO
        for (int i = 0; i < SCREEN_WIDTH * 3 / 8; i++) {
            // Mod VCO preview
            float mod_sample = mod_preview.generate() * fm_amount;
            float mod_new = (mod_sample + 1.0) * height_offset;
            screen.drawHorizontalLine(SCREEN_WIDTH * 3 / 8 + min(mod_prev, mod_new) - 3,
                i, max(1.0, abs(mod_new - mod_prev)), WHITE);
            mod_prev = mod_new;

            Point p = preview_buf.read(float(i) * 64.0 / 50);
            screen.setPixel((p.x + 0.95) * SCREEN_WIDTH / 4,
                (p.y + 0.75) * SCREEN_WIDTH / 4, WHITE);

            p = lfo_preview_buf.read(float(i) * 64.0 / 50);
            screen.setPixel(SCREEN_WIDTH * 3 / 8 + (p.x + 1) * SCREEN_WIDTH / 4,
                (p.y + 0.25) * SCREEN_WIDTH / 4, WHITE);
        }
        FloatArray::copy(preview, preview + SCREEN_WIDTH / 2, SCREEN_BUF_OVERLAP);
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
        Point lfo_pt(crossfade(attr_x * 0.5, lfo.re, chaos) * 0.5 + 0.5,
            crossfade(attr_y * 0.5, lfo.im, chaos) * 0.5 + 0.5);
        lfo_preview_buf.write(lfo_pt);
        setParameterValue(PARAMETER_H, lfo_pt.x);
        setParameterValue(PARAMETER_AH, lfo_pt.y);
    }

    void processAudio(AudioBuffer& buffer) {
        // Attractor/lfo CV output
        mod_lfo.setFrequency(0.01 + getParameterValue(PARAMETER_G) * 4.99);
        processAttractor(getParameterValue(PARAMETER_AG));

        float tune = -2.0 + getParameterValue(PARAMETER_A) * 4.0;
        #ifndef QUANTIZER
        hz.setTune(tune);
        #endif
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);
        env_copy.copyFrom(right);

        fm_amount = getParameterValue(PARAMETER_C);
        fm_ratio = getParameterValue(PARAMETER_D);
        float ext_fm_amt = getParameterValue(PARAMETER_AC);

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
        mod.generate(left);

        // Use mod + ext input for FM
        right.multiply(ext_fm_amt * MAX_FM_AMOUNT);
        left.multiply(fm_amount * MAX_FM_AMOUNT);
        right.add(left);

#ifdef OVERSAMPLE_FACTOR
        upsampler_pitch->process(left, oversample_buf->getSamples(0));
        upsampler_fm->process(right, oversample_buf->getSamples(1));
        AudioBuffer& audio_buf = *oversample_buf;
#else
        AudioBuffer& audio_buf = buffer;
#endif

        // Generate quadrature output
        osc.generate(audio_buf, audio_buf.getSamples(1));
        preview_buf.write(
            Point(audio_buf.getSamples(0)[0], audio_buf.getSamples(1)[0]));

#ifdef OVERSAMPLE_FACTOR
        downsampler_left->process(audio_buf.getSamples(0), left);
        downsampler_right->process(audio_buf.getSamples(1), right);
#endif
        float env = getParameterValue(PARAMETER_AD);
        if (env >= 0.5) {
            env_copy.multiply(env * 2.0 - 1.0);
            left.multiply(env_copy);
            right.multiply(env_copy);
        }
        else {
            left.multiply(1.0 - 2.0 * env);
            right.multiply(1.0 - 2.0 * env);
        }

        float ping = 0.01 + 0.99 * getParameterValue(PARAMETER_E);
        float pong = 0.01 + 0.99 * getParameterValue(PARAMETER_E);
        float feedback = getParameterValue(PARAMETER_AE);
        int newDelayL = ping * (delayBufferL.getSize() - 1);
        int newDelayR = pong * (delayBufferR.getSize() - 1);
        float wet = getParameterValue(PARAMETER_AF);
        float dry = 1.0 - wet;
        highpass->process(buffer, buffer);        
        auto size = buffer.getSize();
        left.multiply(0.5);
        right.multiply(0.5);
        for (auto n = 0; n < size; n++) {
            float x1 = n / (float)size;
            float x0 = 1 - x1;
            float ldly =
                delayBufferL.read(delayL) * x0 + delayBufferL.read(newDelayL) * x1;
            float rdly =
                delayBufferR.read(delayR) * x0 + delayBufferR.read(newDelayR) * x1;
            delayBufferL.write(feedback * rdly + left[n]);
            delayBufferR.write(feedback * ldly + right[n]);
            left[n] = ldly * wet + left[n] * dry;
            right[n] = rdly * wet + right[n] * dry;
        }
        lowpass->process(buffer, buffer);
        delayL = newDelayL;
        delayR = newDelayR;        
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
