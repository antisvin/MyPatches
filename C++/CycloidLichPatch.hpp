#ifndef __CYCLOID_OSCILLATOR_PATCH_HPP__
#define __CYCLOID_OSCILLATOR_PATCH_HPP__

#include "SineOscillator.h"
#include "VoltsPerOctave.h"
#include "Quantizer.hpp"
#include "LockableValue.hpp"
#include "CycloidOscillator.hpp"
#include "MultiProcessor.hpp"
#include "Wavefolder.hpp"
#include "ChebyshevPolynomial.hpp"
#include "DcBlockingFilter.h"

#define BASE_FREQ 55.f
#define MAX_FM_AMOUNT 0.1f
#define MAX_RATIO 20
#define CHEB_ORDER 6
//#define OVERSAMPLE_FACTOR 2

#ifdef OVERSAMPLE_FACTOR
#include "Resample.h"
#endif

//#define QUANTIZER Quantizer12TET
// Uncomment to quantize

#define P_TUNE PARAMETER_A
#define P_HARMONICS PARAMETER_B
#define P_MULT PARAMETER_C
#define P_DIV PARAMETER_D
#define P_FEEDBACK PARAMETER_AA
#define P_FB_DIR PARAMETER_AB
#define P_FB_MAG PARAMETER_AC
#define P_FOLD PARAMETER_AD
#define P_DIR PARAMETER_AE
#define P_MAG PARAMETER_AF
#define P_CHAOS_X PARAMETER_F
#define P_CHAOS_LFO PARAMETER_G

using SmoothParam = LockableValue<SmoothValue<float>, float>;
using StiffParam = LockableValue<StiffValue<float>, float>;
using RatioQuantizer = Quantizer<MAX_RATIO, true>;

#ifdef OVERSAMPLE_FACTOR
// UpSampler* upsampler_pitch;
UpSampler* upsampler_fm;
DownSampler* downsampler_left;
DownSampler* downsampler_right;
AudioBuffer* oversample_buf;
#endif

using StereoWavefolder = MultiProcessor<Wavefolder<HardClipper>, 2>;
using StereoCheb = MultiProcessor<MorphingChebyshevPolynomial<false>, 2>;
using StereoDc = MultiProcessor<DcBlockingFilter, 2>;

class CycloidLichPatch : public Patch {
private:
    CycloidOscillator osc;
    SineOscillator mod_lfo;
    DcBlockingFilter dc_fm;

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
#ifdef OVERSAMPLE_FACTOR
    // UpSampler* upsampler_pitch;
    UpSampler* upsampler_fm;
    DownSampler* downsampler_left;
    DownSampler* downsampler_right;
    AudioBuffer* oversample_buf;
#endif
    bool isModeA = false, isModeB = false;
    float fm_ratio;
    float fm_amount;
    VoltsPerOctave hz;
    // QuadraturePhaser* phaser;
    StereoWavefolder wavefolder;
    StereoCheb cheb;
    StereoDc dc;
    float lfo_sample;
    SmoothParam p_feedback = SmoothParam(0.01);
    StiffParam p_divisor = StiffParam(1.f / float(MAX_RATIO + 1));
    StiffParam p_multiplier = StiffParam(1.f / float(MAX_RATIO));
    SmoothParam p_ratio = SmoothParam(0.01);
    SmoothParam p_harmonics = SmoothParam(0.01);
    SmoothParam p_fb_direction = SmoothParam(0.01);
    SmoothParam p_fb_magnitude = SmoothParam(0.01);
    SmoothParam p_fold = SmoothParam(0.01);
    SmoothParam p_direction = SmoothParam(0.01);
    SmoothParam p_magnitude = SmoothParam(0.01);

public:
    CycloidLichPatch()
        : mod_lfo(0.75, getSampleRate() / getBlockSize())
#ifdef OVERSAMPLE_FACTOR
        , osc(BASE_FREQ, getSampleRate() * OVERSAMPLE_FACTOR)
#else
        , osc(BASE_FREQ, getSampleRate())
#endif
    {
        registerParameter(P_TUNE, "Tune");
        setParameterValue(P_TUNE, 0.5f);
        registerParameter(P_HARMONICS, "Harmonics");
        registerParameter(P_MULT, "Multiplier");
        registerParameter(P_DIV, "Divisor");
        registerParameter(P_FEEDBACK, "Feedback");
        setParameterValue(P_FEEDBACK, 0.f);
        registerParameter(P_FB_MAG, "Feedback Magnitude");
        setParameterValue(P_FB_MAG, 0.f);
        registerParameter(P_FB_DIR, "Feedback Phase");
        setParameterValue(P_FB_DIR, 0.f);
        registerParameter(P_FOLD, "Chebyshaper");
        setParameterValue(P_FOLD, 0.f);
        registerParameter(P_DIR, "Fold Direction");
        setParameterValue(P_DIR, 0.f);
        registerParameter(P_MAG, "Fold Magntitude");
        setParameterValue(P_MAG, 0.f);
        registerParameter(P_CHAOS_X, "ChaosX>");
        registerParameter(P_CHAOS_LFO, "ChaoLFO>");

        p_feedback.rawValue().lambda = 0.96;
        p_multiplier.rawValue().delta = (1.f / float(MAX_RATIO + 1));
        p_divisor.rawValue().delta = (1.f / float(MAX_RATIO + 2));
        p_harmonics.rawValue().lambda = 0.96;
        p_fb_direction.rawValue().lambda = 0.96;
        p_fb_magnitude.rawValue().lambda = 0.96;
        p_fold.rawValue().lambda = 0.96;
        p_direction.rawValue().lambda = 0.96;
        p_magnitude.rawValue().lambda = 0.96;

#ifdef OVERSAMPLE_FACTOR
        // upsampler_pitch = UpSampler::create(1, OVERSAMPLE_FACTOR);
        upsampler_fm = UpSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_left = DownSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_right = DownSampler::create(1, OVERSAMPLE_FACTOR);
        oversample_buf = AudioBuffer::create(2, getBlockSize() * OVERSAMPLE_FACTOR);
#endif

        setButton(BUTTON_A, false, 0);
        setButton(BUTTON_B, false, 0);
        cheb.get(0).setMaxOrder(CHEB_ORDER);
        cheb.get(1).setMaxOrder(CHEB_ORDER);
        cheb.get(0).setOffset(1);
        cheb.get(1).setOffset(1);
        //        lockAll();
    }

    ~CycloidLichPatch() {
#ifdef OVERSAMPLE_FACTOR
        // UpSampler::destroy(upsampler_pitch);
        UpSampler::destroy(upsampler_fm);
        DownSampler::destroy(downsampler_left);
        DownSampler::destroy(downsampler_right);
        AudioBuffer::destroy(oversample_buf);
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
        dc_fm.process(right, right);
        right.multiply(MAX_FM_AMOUNT);

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
            int divisor = (MAX_RATIO / 2 + 1) * p_divisor.getValue();
            if (divisor == 0) {
                osc.setRatio((p_ratio.getValue() - 0.5f) * MAX_RATIO);
            }
            else {
                int multiplier = MAX_RATIO * p_multiplier.getValue() + 1;
                osc.setRatio(float(multiplier - MAX_RATIO / 2) / float(divisor));
            }
        }

        p_feedback.update(getParameterValue(P_FEEDBACK));
        p_fb_direction.update(getParameterValue(P_FB_DIR));
        p_fb_magnitude.update(getParameterValue(P_FB_MAG));

        p_fold.update(getParameterValue(P_FOLD));
        p_direction.update(getParameterValue(P_DIR));
        p_magnitude.update(getParameterValue(P_MAG));

        osc.setFrequency(freq);
        osc.setHarmonics(p_harmonics.getValue());
        //osc.setFeedback1(p_feedback.getValue());
        float fb = p_feedback;
        float fb_scale = 2.f - (2.f * fb - 1.f);
        osc.setFeedback(
            p_fb_magnitude.getValue(), p_fb_direction.getValue() * M_PI * 4.f,
            fb, 1.f - fb);

#ifdef OVERSAMPLE_FACTOR
        upsampler_fm->process(right, oversample_buf->getSamples(1));
        AudioBuffer& audio_buf = *oversample_buf;
#else
        AudioBuffer& audio_buf = buffer;
#endif
        osc.generate(audio_buf, audio_buf.getSamples(1));

        // Chebyshev wavefolder
        float fold = p_fold.getValue();
        cheb.get(0).setMorph(fold);
        cheb.get(1).setMorph(fold);
        cheb.process(audio_buf, audio_buf);

#ifdef OVERSAMPLE_FACTOR
        downsampler_left->process(oversample_buf->getSamples(0), left);
        downsampler_right->process(oversample_buf->getSamples(1), right);
#endif

        // Complex wavefolder - oversampling is not necessary, algo suppresses aliasing
        float offset_phase = p_direction.getValue() * M_PI;
        float offset_mag = p_magnitude.getValue() * 3;
        cmp_offset.setPolar(offset_mag, offset_phase);

        left.multiply(1.f + abs(cmp_offset.re));
        right.multiply(1.f + abs(cmp_offset.im));
        wavefolder.process(buffer, buffer);

        dc.process(buffer, buffer);
    }
};
#endif
