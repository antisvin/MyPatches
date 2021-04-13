#ifndef __POLYGONAL_DAISY_PATCH_HPP__
#define __POLYGONAL_DAISY_PATCH_HPP__

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
#include <algorithm>
#include "Envelope.h"
#include "daisysp.h"

//using namespace daisysp;


#define BASE_FREQ 55.f
#define MAX_POLY 20.f
#define PREVIEW_FREQ 2.0f
#define SCREEN_WIDTH 128 // Hardcoded to make initialization easier
#define SCREEN_BUF_OVERLAP (SCREEN_WIDTH / 8)
#define SCREEN_PREVIEW_BUF_SIZE (SCREEN_WIDTH / 2 + SCREEN_BUF_OVERLAP)
#define MAX_FM_AMOUNT 0.2f
#define FM_AMOUNT_MULT (1.f / MAX_FM_AMOUNT)
#define SCREEN_OFFSET(x) ((x)*SCREEN_WIDTH / 4 )

//#define OVERSAMPLE_FACTOR 4

#define P_TUNE       PARAMETER_A
#define P_NPOLY      PARAMETER_B
#define P_FM_AMOUNT  PARAMETER_C
#define P_FM_SHAPE   PARAMETER_D
#define P_QUANTIZE   PARAMETER_E
#define P_CVOUT_X    PARAMETER_F
#define P_CVOUT_Y    PARAMETER_G
#define P_CHAOS      PARAMETER_H
#define P_LFO_FREQ   PARAMETER_AA
#define P_TEETH      PARAMETER_AB
#define P_ENV_AMOUNT PARAMETER_AC
#define P_EXT_FM     PARAMETER_AD
#define P_ENV_A      PARAMETER_AE
#define P_ENV_D      PARAMETER_AF
#define P_ENV_S      PARAMETER_AG
#define P_ENV_R      PARAMETER_AH
#define P_REV_FB     PARAMETER_BA
#define P_REV_LP     PARAMETER_BB
#define P_REV_MIX    PARAMETER_BC

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

class PolygonalDaisyPatch : public MonochromeScreenPatch {
private:
    PolygonalOscillator osc;
    ModOscillator mod;
    ModOscillator mod_preview;
    FloatArray lfo_preview;
    FloatArray env_buf;
    PolygonalOscillator::NPolyQuant quant;
    float attr_x, attr_y;
    const float attr_a = -0.827;
    const float attr_b = -1.637;
    const float attr_c = 1.659;
    const float attr_d = -0.943;    
    QuadratureSineOscillator mod_lfo;
    AdsrEnvelope adsr;
    daisysp::ReverbSc* reverb;
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
    float half_sr;
    VoltsPerOctave hz;
    DelayLine<Point, 128> preview_buf;
    DelayLine<Point, 128> lfo_preview_buf;

    inline float crossfade(float a, float b, float fade) {
        return a * fade + b * (1 - fade);
    }

public:
    PolygonalDaisyPatch()
        : hz(true)
        , adsr(getSampleRate())
        , half_sr(getSampleRate() / 2)
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
        registerParameter(P_TUNE, "Tune");
        registerParameter(P_QUANTIZE, "Quantize");
        registerParameter(P_NPOLY, "NPoly");
        registerParameter(P_TEETH, "Teeth");
        // FM / env
        registerParameter(P_FM_AMOUNT, "FM Amount");
        setParameterValue(P_FM_AMOUNT, 0.0);
        registerParameter(P_FM_SHAPE, "FM Mod Shape");
        setParameterValue(P_FM_SHAPE, 0.0);
        registerParameter(P_EXT_FM, "Ext FM Amt");
        setParameterValue(P_EXT_FM, 0.0);
        // Chaotic modulator
        registerParameter(P_LFO_FREQ, "LFO freq");
        setParameterValue(P_LFO_FREQ, 0.5);
        registerParameter(P_CVOUT_X, "ModX>");
        registerParameter(P_CVOUT_Y, "ModY>");
        registerParameter(P_CHAOS, "Chaos Amt");
        setParameterValue(P_CHAOS, 0.1);
        // ADSR envelope
        registerParameter(P_ENV_A, "Env A");
        setParameterValue(P_ENV_A, 0.1);
        registerParameter(P_ENV_D, "Env D");
        setParameterValue(P_ENV_D, 0.2);
        registerParameter(P_ENV_S, "Env S");
        setParameterValue(P_ENV_S, 0.5);
        registerParameter(P_ENV_R, "Env R");
        setParameterValue(P_ENV_R, 0.2);
        registerParameter(P_ENV_AMOUNT, "Env Amt");
        setParameterValue(P_ENV_AMOUNT, 1.0);
        // Reverb
        registerParameter(P_REV_FB, "Reverb FB");
        setParameterValue(P_REV_FB, 0.95);
        registerParameter(P_REV_LP, "Reverb Filter");
        setParameterValue(P_REV_LP, 0.8);
        registerParameter(P_REV_MIX, "Reverb Mix");
        setParameterValue(P_REV_MIX, 0.6);

        env_buf = FloatArray::create(getBlockSize());
        preview = FloatArray::create(SCREEN_PREVIEW_BUF_SIZE);
        preview_buf = DelayLine<Point, 128>::create();
        lfo_preview_buf = DelayLine<Point, 128>::create();
        reverb = new daisysp::ReverbSc();
        reverb->Init(getSampleRate());        
#ifdef OVERSAMPLE_FACTOR
        upsampler_pitch = UpSampler::create(1, OVERSAMPLE_FACTOR);
        upsampler_fm = UpSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_left = DownSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_right = DownSampler::create(1, OVERSAMPLE_FACTOR);
        oversample_buf = AudioBuffer::create(2, getBlockSize() * OVERSAMPLE_FACTOR);
#endif
    }

    ~PolygonalDaisyPatch() {
        FloatArray::destroy(env_buf);
        FloatArray::destroy(preview);
        DelayLine<Point, 128>::destroy(preview_buf);
        DelayLine<Point, 128>::destroy(lfo_preview_buf);
        delete reverb;
#ifdef OVERSAMPLE_FACTOR
        UpSampler::destroy(upsampler_pitch);
        UpSampler::destroy(upsampler_fm);
        DownSampler::destroy(downsampler_left);
        DownSampler::destroy(downsampler_right);
        //AudioBuffer::destroy(oversample_buf);
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
            screen.drawHorizontalLine(SCREEN_WIDTH * 3 / 8 + std::min(mod_prev, mod_new) - 3, i, 
                std::max(1.0, abs(mod_new - mod_prev)), WHITE);
            mod_prev = mod_new;

            Point p = preview_buf.read(float(i) * 64.0 / 50);
            screen.setPixel(
                (p.x + 0.95) * SCREEN_WIDTH / 4,
                (p.y + 0.75) * SCREEN_WIDTH / 4 ,
                WHITE);

            p = lfo_preview_buf.read(float(i) * 64.0 / 50);
            screen.setPixel(
                SCREEN_WIDTH * 3 / 8 + (p.x + 1) * SCREEN_WIDTH / 4,
                (p.y + 0.25) * SCREEN_WIDTH / 4 ,
                WHITE);
        }
        FloatArray::copy(preview, preview + SCREEN_WIDTH / 2, SCREEN_BUF_OVERLAP);
    }

    void processAttractor(float chaos) {
        float old_x = attr_x, old_y = attr_y;
        attr_x = sin(attr_a * old_y) - cos(attr_b * old_x);
        attr_y = sin(attr_c * old_x) - cos(attr_d * old_y);
        auto lfo = mod_lfo.generate();
        Point lfo_pt(
            crossfade(attr_x * 0.5, lfo.re, chaos) * 0.5 + 0.5,
            crossfade(attr_y * 0.5, lfo.im, chaos) * 0.5 + 0.5);
        lfo_preview_buf.write(lfo_pt);
        setParameterValue(P_CVOUT_X, lfo_pt.x);
        setParameterValue(P_CVOUT_Y, lfo_pt.y);
    }

    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples=0) override {
        adsr.trigger(samples);
    }

    void processAudio(AudioBuffer& buffer) {
        // Process ADSR envelope
        adsr.setAttack(getParameterValue(P_ENV_A));
        adsr.setDecay(getParameterValue(P_ENV_D));
        adsr.setSustain(getParameterValue(P_ENV_S));
        adsr.setRelease(getParameterValue(P_ENV_R));        

        // Attractor/lfo CV output
        mod_lfo.setFrequency(0.01 + getParameterValue(P_LFO_FREQ) * 4.99);
        processAttractor(getParameterValue(P_CHAOS));

        hz.setTune(-3.0 + getParameterValue(P_TUNE) * 4.0);
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

        fm_amount = getParameterValue(P_FM_AMOUNT);
        fm_ratio = getParameterValue(P_FM_SHAPE);
        float ext_fm_amt = getParameterValue(P_EXT_FM);
    
        osc.setParams(
            updateQuant(getParameterValue(P_QUANTIZE)),
            getParameterValue(P_NPOLY),
            getParameterValue(P_TEETH));

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
        float env = 1.0 - getParameterValue(P_ENV_AMOUNT);
        adsr.generate(env_buf);
        //env_buf.add(env);

        left.multiply(env_buf);
        left.multiply(0.35);
        right.multiply(env_buf);
        right.multiply(0.35);
        float wet = getParameterValue(P_REV_MIX);
        float dry = 1.0 - dry;

        reverb->SetFeedback(getParameterValue(P_REV_FB));
        reverb->SetLpFreq(getParameterValue(P_REV_LP) * half_sr);

        for (int i = 0; i < getBlockSize(); i++){
            float out1, out2;
            reverb->Process(left[i], left[i], &out1, &out2);
            left[i] = out1 * dry + left[i] * wet;
            right[i] = out2 * dry + left[i] * wet;
        }

        FloatArray in3 = buffer.getSamples(2);
        left.add(in3);
        right.add(in3);
        FloatArray in4 = buffer.getSamples(3);
        left.add(in4);
        right.add(in4);
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
