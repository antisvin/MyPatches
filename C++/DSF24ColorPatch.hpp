#ifndef __DSF_2_4_PATCH_HPP__
#define __DSF_2_4_PATCH_HPP__

#include "DiscreteSummationOscillator.hpp"
#include "StereoMorphingOscillator.hpp"
#include "SineOscillator.h"
#include "QuadratureSineOscillator.h"
#include "VoltsPerOctave.h"
#include "DelayLine.hpp"
#include "Quantizer.hpp"
#include "LockableValue.hpp"
#include "MultiProcessor.hpp"
#include "Wavefolder.hpp"
#include "Contour.hpp"
#include "ColourScreenPatch.h"
#include "ColorUtils.hpp"
#include "CircularBuffer.h"

/**
 * Morphing DSF oscillator
 *
 * This patch outputs stereo signal in quadrature mode, so it's recommended
 * to preview it with a scope in XY plane (Lissajous mode).
 *
 * The patch includes 2 oscillators based on cutting edge music research
 * described by James Moorer in "The Synthesis of Complex Audio Spectra by
 * Means of Discrete Summation Formulas" in 1975. Paper is available online
 * at https://ccrma.stanford.edu/files/papers/stanm5.pdf . Third oscillator
 * is used as a modulator for phase modulation. Results are processed by
 * a VCA with a wavefolder.
 *
 * This synthesis methods describes a computationally cheap way to generate
 * complex waveforms with some control over partials distribution. Graphics
 * for all 4 formulas can be viewed here:
 * https://www.desmos.com/calculator/ahurvpm3qg
 *
 * The main difference between oscillators is that the first one (DSF2)
 *generates one-sided spectrum, while the second (DSF4) generates spectrum with
 *equal number of partials on both sides.
 *
 * Param A sets tuning, buttons choose submenu
 *
 * Main menu:
 * - param B morphs output of both oscillators
 * - param C - alpha parameter for DSF, this sets slope for amplitude of
 *generated partials
 * - param D - beta parameter for DSF, sets distance between partials. This
 *allows createing harmonic or inharmonic waveforms
 *
 * Submenu 1 controls phase modulation
 * - param B sets PM amount
 * - param C/D set ratio for modulator. C is used as multiplier with integer
 *values in [1..5] range, D is the divider in [0..5] range. If D is set to 0
 *then C is used as unquantized values in [0.2..5] range with exponential growth
 *
 * Submenu 2 controls oscillator feedback and VCA/wavefolder
 * - param B sets gain for wavefolder in [0..4] range. Using lower values allow
 *using it as a simple end of chain VCA
 * - param C sets direction (polar phase) for primary oscillator's feedback.
 *This determines direction where complex waveform is stretched.
 * - param D adds feedback to complex oscillator. This stretches complex
 *oscillator shape towards one of the corners.
 *
 * Parameters F and G can be used as outputs for modulation
 *
 * Audio input 1 is V/Oct pitch CV, input 2 is used as a source for external PM.
 *Pitch CV can be quantized if you rebuild patch with quantizer enabled.
 **/

//#define OVERSAMPLE_FACTOR 2

#ifdef OVERSAMPLE_FACTOR
#include "Resample.h"
#endif

#define BASE_FREQ 55.f
#define BASE_LFO_FREQ 0.125f
//#define QUANTIZER Quantizer12TET
//#define QUANTIZER Quantizer<19>
#define EXT_FM

#define MAX_RATIO 5
#define FM_AMOUNT_MULT (1.f / MAX_FM_AMOUNT)
#define MAX_STEPS 16
#define NUM_PIXELS 8192 // 6 bytes per pixel
#define SCREEN_SMOOTH 0.9f

#define P_TUNE PARAMETER_A
#define P_MORPH PARAMETER_B
#define P_DSF_A PARAMETER_C
#define P_DSF_B PARAMETER_D
#define P_LFO_X PARAMETER_F
#define P_LFO_Y PARAMETER_G
#define P_CNT_OUT PARAMETER_H
#define P_FM_AMT PARAMETER_AA
#define P_FM_MULT PARAMETER_AB
#define P_FM_DIV PARAMETER_AC
#define P_FOLD PARAMETER_AD
#define P_FB_ANGLE PARAMETER_AE
#define P_FB_MAG PARAMETER_AF
#define P_LFO_FREQ PARAMETER_BA
#define P_CHAOS PARAMETER_BB
#define P_CNT_TRIG PARAMETER_BC
#define P_CNT_SHP PARAMETER_BD

using SmoothParam = LockableValue<SmoothValue<float>, float>;
using StiffParam = LockableValue<StiffValue<float>, float>;
using MorphBase =
    StereoMorphingOscillator<DiscreteSummationOscillator<DSF2>, DiscreteSummationOscillator<DSF4>>;
using StereoWavefolder = MultiProcessor<Wavefolder<HardClipper>, 2>;
using ColourBuffer = CircularBuffer<Pixel>;


class DsfMorph : public MorphBase {
public:
    static DsfMorph* create(float freq, float sr) {
        DsfMorph* dsf = new DsfMorph();
        dsf->setSampleRate(sr);
        dsf->setFrequency(freq);
        return dsf;
    }

    static void destroy(DsfMorph* osc) {
        delete osc;
    }
};

class DSF24ColorPatch : public ColourScreenPatch {
private:
    SineOscillator* mod;
    QuadratureSineOscillator mod_lfo;
    DsfMorph* dsf;
    StereoWavefolder wavefolder;
    ComplexFloat attr;
    const float attr_a = -0.827;
    const float attr_b = -1.637;
    const float attr_c = 1.659;
    const float attr_d = -0.943;
    Contour contour;
    ColourBuffer* pixels;
    uint16_t screen_radius = 0;
    uint16_t screen_center_x = 0, screen_center_y = 0;
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
    bool isModeA = false;
    bool isModeB = false;
    bool fm_rational;
    float fm_ratio;
    int fm_multiplier, fm_divisor;
    float fm_amount;
    VoltsPerOctave hz;
    ComplexFloat lfo_sample;
    SmoothParam p_dsf_a = SmoothParam(0.01);
    SmoothParam p_dsf_b = SmoothParam(0.01);
    SmoothParam p_dsf_morph = SmoothParam(0.01);
    SmoothParam p_fm_amount = SmoothParam(0.01);
    StiffParam p_fm_mult = StiffParam(1.f / float(MAX_RATIO));
    StiffParam p_fm_div = StiffParam(1.f / float(MAX_RATIO + 1));
    SmoothParam p_fm_ratio = SmoothParam(0.01);
    SmoothParam p_ext_amount = SmoothParam(0.01);
    SmoothParam p_fold = SmoothParam(0.01);
    SmoothParam p_fb_angle = SmoothParam(0.01);
    SmoothParam p_fb_mag = SmoothParam(0.01);
#ifndef EXT_FM
    FloatArray env_copy;
#endif
    inline float crossfade(float a, float b, float fade) {
        return a * fade + b * (1 - fade);
    }

public:
    DSF24ColorPatch() {
        mod_lfo.setSampleRate(getSampleRate() / getBlockSize());
        mod = SineOscillator::create(getSampleRate());
        dsf = DsfMorph::create(BASE_FREQ, getSampleRate());

        registerParameter(P_TUNE, "Tune");
        registerParameter(P_MORPH, "Morph");
        registerParameter(P_DSF_A, "DSF alpha");
        setParameterValue(P_DSF_A, 0.0);
        registerParameter(P_DSF_B, "DSF beta");
        setParameterValue(P_DSF_B, 0.0);
        registerParameter(P_FM_AMT, "FM Amount");
        setParameterValue(P_FM_AMT, 0.0);
        registerParameter(P_FM_MULT, "FM Multiplier");
        setParameterValue(P_FM_MULT, 0.25);
        registerParameter(P_FM_DIV, "FM Divider");
        setParameterValue(P_FM_DIV, 0.25);
        registerParameter(P_FOLD, "Wavefolder");
        setParameterValue(P_FOLD, 0.2);
        registerParameter(P_FB_ANGLE, "FB Angle");
        setParameterValue(P_FB_ANGLE, 0.0);
        registerParameter(P_FB_MAG, "FB Amount");
        setParameterValue(P_FB_MAG, 0.0);
        registerParameter(P_LFO_FREQ, "LFO Freq");
        setParameterValue(P_LFO_FREQ, 0.5);
        registerParameter(P_LFO_X, "LfoX>");
        registerParameter(P_LFO_Y, "LFOY>");
        registerParameter(P_CHAOS, "LFO Chaos");
        setParameterValue(P_CHAOS, 0.22);
        //registerParameter(P_ATTR_X, "Attractor>");
        //registerParameter(P_ATTR_Y, "Attractor>");
        registerParameter(P_CNT_SHP, "Contour shp");
        setParameterValue(P_CNT_SHP, 0.5);
        registerParameter(P_CNT_TRIG, "Contour trig");
        setParameterValue(P_CNT_TRIG, 0.0);
        registerParameter(P_CNT_OUT, "Contour>");
        setParameterValue(P_CNT_OUT, 0.0);

        p_dsf_a.rawValue().lambda = 0.98;
        p_dsf_b.rawValue().lambda = 0.98;
        p_dsf_morph.rawValue().lambda = 0.98;
        p_fm_amount.rawValue().lambda = 0.98;
        p_fm_mult.rawValue().delta = (1.f / float(MAX_RATIO + 1));
        p_fm_div.rawValue().delta = (1.f / float(MAX_RATIO + 2));
        p_fm_ratio.rawValue().lambda = 0.98;
        p_fold.rawValue().lambda = 0.98;
        p_fb_angle.rawValue().lambda = 0.98;
        p_fb_mag.rawValue().lambda = 0.98;

        pixels = ColourBuffer::create(NUM_PIXELS);

#ifdef OVERSAMPLE_FACTOR
        upsampler_pitch = UpSampler::create(1, OVERSAMPLE_FACTOR);
        upsampler_fm = UpSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_left = DownSampler::create(1, OVERSAMPLE_FACTOR);
        downsampler_right = DownSampler::create(1, OVERSAMPLE_FACTOR);
        oversample_buf = AudioBuffer::create(2, getBlockSize() * OVERSAMPLE_FACTOR);
#endif
        setButton(BUTTON_A, false, 0);
        setButton(BUTTON_B, false, 0);
#ifndef EXT_FM
        env_copy = FloatArray::create(getBlockSize());
#endif
    }

    ~DSF24ColorPatch() {
        ColourBuffer::destroy(pixels);
        SineOscillator::destroy(mod);
        DsfMorph::destroy(dsf);
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

    void lockAll() {
        p_dsf_a.setLock(true);
        p_dsf_b.setLock(true);
        p_dsf_morph.setLock(true);
        p_fm_amount.setLock(true);
        p_fm_div.setLock(true);
        p_fm_mult.setLock(true);
        p_fm_ratio.setLock(true);
        p_fold.setLock(true);
        p_fb_angle.setLock(true);
        p_fb_mag.setLock(true);
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

    void processCv() {
        float chaos = getParameterValue(P_CHAOS);
        float lfo_f = getParameterValue(P_LFO_FREQ) + 1.f;
        mod_lfo.setFrequency(BASE_LFO_FREQ * lfo_f * lfo_f);
        float last_lfo_re = lfo_sample.re;

        float old_x = attr.re, old_y = attr.im;
        attr.re = sin(attr_a * old_y) - cos(attr_b * old_x);
        attr.im = sin(attr_c * old_x) - cos(attr_d * old_y);
        lfo_sample = mod_lfo.generate();
        lfo_sample.re = crossfade(attr.re / 2, lfo_sample.re, chaos);
        lfo_sample.im = crossfade(attr.im / 2, lfo_sample.im, chaos);
        setParameterValue(P_LFO_X, lfo_sample.re * 0.5 + 0.5);
        setParameterValue(P_LFO_Y, lfo_sample.im * 0.5 + 0.5);
//        setParameterValue(P_ATTR_X, attr.re * 0.5 + 0.5);
//        setParameterValue(P_ATTR_Y, attr.im * 0.5 + 0.5);

        contour.setShape(getParameterValue(P_CNT_SHP));
        contour.gate(getParameterValue(P_CNT_TRIG) > 0.05, 0);
        setParameterValue(P_CNT_OUT, contour.generate());
    }

    void processScreen(ColourScreenBuffer& screen) {
        // This zooms in visualization when patch starts
        screen_radius = (float)screen_radius * SCREEN_SMOOTH +
            float(std::min(screen.getWidth(), screen.getHeight())) * 0.5f *
                (1.f - SCREEN_SMOOTH);
        screen_center_x = screen.getWidth() / 2;
        screen_center_y = screen.getHeight() / 2;

        //uint16_t hue_16_bit = RGB888toRGB565(HSBtoRGB(hue_step * hue, 1.f, 1.f));
        uint16_t hue_16_bit = YELLOW;

        for (size_t i = 0; i < NUM_PIXELS; i++) {
            Pixel px = pixels->readAt(i);
//            uint16_t hue_16_bit = RGB888toRGB565(HSBtoRGB(i * 360.f * hue_step, 1.f, 1.f));
            
            screen.setPixel(px.x, px.y, hue_16_bit);//
            //hue_16bit);
        }
    }

    void processAudio(AudioBuffer& buffer) {

        // CV output
        processCv();
        // attr_x * 0.5 + 0.5);
        setButton(BUTTON_C, lfo_sample.re * lfo_sample.im >= 0.0);

        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

        // Tuning
        float tune = -2.0 + getParameterValue(P_TUNE) * 4.0;
        // Carrier / modulator frequency with optional quantizing
#ifdef QUANTIZER
        float volts = quantizer.process(tune + hz.sampleToVolts(left[0]));
        float freq = hz.voltsToHertz(volts);
#else
        hz.setTune(tune);
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
            p_dsf_morph.update(getParameterValue(P_MORPH));
            p_dsf_a.update(getParameterValue(P_DSF_A) * 0.9f + 0.05f);
            p_dsf_b.update(getParameterValue(P_DSF_B) * 2.f - 1.f);
            dsf->osc_a.setA(p_dsf_a);
            dsf->osc_a.setB(p_dsf_b);
            dsf->osc_b.setA(p_dsf_a);
            dsf->osc_b.setB(p_dsf_b);
            dsf->setMorph(p_dsf_morph);
        }
        dsf->setFrequency(freq);

        p_fm_amount.update(getParameterValue(P_FM_AMT));
        p_fm_mult.update(getParameterValue(P_FM_MULT));
        float fm_mult = p_fm_mult.getValue();
        p_fm_div.update(getParameterValue(P_FM_DIV));
        float fm_div = p_fm_div.getValue();

        p_fm_ratio.update(getParameterValue(P_FM_MULT));
        fm_divisor = (MAX_RATIO + 1) * p_fm_div.getValue();
        if (fm_divisor == 0) {
            fm_rational = false;
            fm_ratio = 0.2f * powf(25.0, p_fm_ratio.getValue());
            mod->setFrequency(freq * fm_ratio);
        }
        else {
            fm_rational = true;
            fm_multiplier = MAX_RATIO * p_fm_mult.getValue() + 1;
            mod->setFrequency(freq * float(fm_multiplier) / float(fm_divisor));
        }
        p_fb_angle.update(getParameterValue(P_FB_ANGLE) * 2 * M_PI);
        p_fb_mag.update(getParameterValue(P_FB_MAG));
        dsf->osc_a.setFeedback(p_fb_mag, p_fb_angle);
        dsf->osc_b.setFeedback(p_fb_mag, p_fb_angle);

        // Generate modulator
        mod->generate(left);

        // Use mod + ext input for FM
        left.multiply(p_fm_amount.getValue());
#ifdef EXT_FM
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
        dsf->generate(audio_buf, audio_buf.getSamples(1));

        p_fold.update(getParameterValue(P_FOLD));
        float gain = p_fold.getValue() * 3.f;
        audio_buf.getSamples(0).multiply(gain);
        audio_buf.getSamples(1).multiply(gain);
        wavefolder.process(audio_buf, audio_buf);

#ifdef OVERSAMPLE_FACTOR
        downsampler_left->process(oversample_buf->getSamples(0), left);
        downsampler_right->process(oversample_buf->getSamples(1), right);
#endif

        for (size_t i = 0; i < buffer.getSize(); i += 4) {
            Pixel new_px(
                screen_center_x + int16_t(left[i] * screen_radius),
                screen_center_y + int16_t(right[i] * screen_radius),
                HSBto565(1.f, 1.f, 1.f));
            pixels->write(new_px);
            //prev_px = new_px;
        }
    }
};

#endif
