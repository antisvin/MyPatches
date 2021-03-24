#ifndef __POLYGONAL_VCO_PATCH_HPP__
#define __POLYGONAL_VCO_PATCH_HPP__

#include "MonochromeScreenPatch.h"
#include "Oscillators/PolygonalOscillator.hpp"
#include "Oscillators/MultiOscillator.hpp"
#include "SineOscillator.h"
#include "VoltsPerOctave.h"
#include "DelayLine.hpp"
#include "Resample.h"
#include <algorithm>

#define BASE_FREQ 55.f
#define MAX_POLY 20.f
#define PREVIEW_FREQ 2.0f
#define SCREEN_WIDTH 128 // Hardcoded to make initialization easier
#define SCREEN_BUF_OVERLAP (SCREEN_WIDTH / 8)
#define SCREEN_PREVIEW_BUF_SIZE (SCREEN_WIDTH / 2 + SCREEN_BUF_OVERLAP)
#define MAX_FM_AMOUNT 0.2f
#define FM_AMOUNT_MULT (1.f / MAX_FM_AMOUNT)
#define SCREEN_OFFSET(x) ((x)*SCREEN_WIDTH / 4 - int(SCREEN_WIDTH / 16))

//#define OVERSAMPLE_FACTOR 4

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

class PolygonalVCOPatch : public MonochromeScreenPatch {
private:
    PolygonalOscillator osc;
    PolygonalOscillator osc_preview;
    ModOscillator mod;
    ModOscillator mod_preview;
    PolygonalOscillator::NPolyQuant quant;
#ifdef OVERSAMPLE_FACTOR
    UpSampler* upsampler_pitch;
    UpSampler* upsampler_fm;
    DownSampler* downsampler_left;
    DownSampler* downsampler_right;
    AudioBuffer* oversample_buf;
#endif
    FloatArray preview;
    float fm_ratio;
    float fm_amount;
    VoltsPerOctave hz;
    DelayLine<Point, 128> preview_buf;

public:
    PolygonalVCOPatch()
        : hz(true)
#ifdef OVERSAMPLE_FACTOR
        , osc(BASE_FREQ, getSampleRate() * OVERSAMPLE_FACTOR)
        , mod(BASE_FREQ, getSampleRate() * OVERSAMPLE_FACTOR)
#else
        , osc(BASE_FREQ, getSampleRate())
        , mod(BASE_FREQ, getSampleRate())
#endif
        , osc_preview(1.0f, float(SCREEN_WIDTH / 2))
        , mod_preview(PREVIEW_FREQ, float(SCREEN_WIDTH / 2)) {
        registerParameter(PARAMETER_A, "Tune");
        registerParameter(PARAMETER_B, "Quantize");
        registerParameter(PARAMETER_C, "NPoly");
        registerParameter(PARAMETER_D, "Teeth");
        registerParameter(PARAMETER_E, "FM Amount");
        setParameterValue(PARAMETER_E, 0.0);
        registerParameter(PARAMETER_F, "FM Mod Shape");
        setParameterValue(PARAMETER_F, 0.0);
        registerParameter(PARAMETER_G, "Ext FM Amount");
        setParameterValue(PARAMETER_G, 0.0);
        preview = FloatArray::create(SCREEN_PREVIEW_BUF_SIZE);
        preview_buf = DelayLine<Point, 128>::create();
#ifdef OVERSAMPLE_FACTOR
        upsampler_pitch = UpSampler::create(1, OVERSAMPLE_FACTOR);
        upsampler_fm = UpSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_left = DownSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_right = DownSampler::create(1, OVERSAMPLE_FACTOR);
        oversample_buf = AudioBuffer::create(2, getBlockSize() * OVERSAMPLE_FACTOR);
#endif
    }

    ~PolygonalVCOPatch() {
        FloatArray::destroy(preview);
        DelayLine<Point, 128>::destroy(preview_buf);
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
        for (int i = 0; i < SCREEN_WIDTH / 2; i++) {
            // Mod VCO preview
            float mod_sample = mod_preview.generate() * fm_amount;
            float mod_new = (mod_sample + 1.0) * height_offset;
            screen.drawVerticalLine(SCREEN_WIDTH / 2 + i, std::min(mod_prev, mod_new),
                std::max(1.0, abs(mod_new - mod_prev)), WHITE);
            mod_prev = mod_new;

            Point p = preview_buf.read(float(i) * 64.0 / 50.0);
            screen.setPixel(SCREEN_OFFSET(p.x + 1.0), SCREEN_OFFSET(p.y + 1), WHITE);
        }
        FloatArray::copy(preview, preview + SCREEN_WIDTH / 2, SCREEN_BUF_OVERLAP);
    }
    void processAudio(AudioBuffer& buffer) {
        hz.setTune(-3.0 + getParameterValue(PARAMETER_A) * 4.0);
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

        fm_amount = getParameterValue(PARAMETER_E);
        fm_ratio = getParameterValue(PARAMETER_F);
        float ext_fm_amt = getParameterValue(PARAMETER_G);
    
        osc.setParams(
            updateQuant(getParameterValue(PARAMETER_B)),
            getParameterValue(PARAMETER_C),
            getParameterValue(PARAMETER_D));

        // Carrier / modulator frequency
        float freq = hz.getFrequency(left[0]);
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
        preview_buf.write(Point(audio_buf.getSamples(0)[0], audio_buf.getSamples(1)[0]));

#ifdef OVERSAMPLE_FACTOR
        downsampler_left->process(audio_buf.getSamples(0), left);
        downsampler_right->process(audio_buf.getSamples(1), right);
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
