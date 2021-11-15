#ifndef __TRASH_DELAY_PATCH_HPP__
#define __TRASH_DELAY_PATCH_HPP__

/**
 * Trash delay is a tempo synced delay and an audio corruption utility.
 * 
 * FX chain contains a delay with tap tempo, bitcrusher, sample rate reducer and saturator. Decimation
 * can be performed before writing audio to delay's feedback buffer or after reading it. Saturator
 * will add some harmonics and soft clip audio in case of excessive feedback levels.
 *
 * No PT2399 chips were hurt making this patch!
 *
 * PARAM A - delay adjustment, set to max for starters, endless source of clicks and glitches if you change it
 * PARAM B - feedback amount
 * PARAM C - bitcrusher level
 * PARAM D - sample rate reduction
 * PARAM F - output sine LFO clocked by tempo
 * PARAM G - output saw LFO clocked and synced by tempo
 * BUTTON 1 - tap tempo / clock input
 * BUTTON 2 - switch corruption location (inside/outside of delay's FB loop)
 * BUTTON 3 - clock output (based on LFO phase)
 * 
 **/

#include "Patch.h"
#include "DelayProcessor.h"
//#include "FeedbackProcessor.h"
#include "DcBlockingFilter.h"
#include "TapTempo.h"
#include "Bitcrusher.hpp"
#include "SampleRateReducer.hpp"
#include "SineOscillator.h"
#include "SmoothValue.h"
#include "DryWetProcessor.h"
#include "Nonlinearity.hpp"

//#define CIRCULAR
#define USE_TRANSITION_LUT
#define DECIMATE_PRE

static constexpr float max_delay_seconds = 10.f; // OWL2+
//static constexpr float max_delay_seconds = 2.7f; // Just enough for OWL1
static constexpr float max_feedback = 1.2f; // Yes, we can! Excessive signal would be softclipped 

#if defined CIRCULAR
using Processor = CrossFadingDelayProcessor;
#else
using Processor = FractionalDelayProcessor<HERMITE_INTERPOLATION>;
#endif

class SawOscillator : public OscillatorTemplate<SawOscillator> {
public:
  static constexpr float begin_phase = 0;
  static constexpr float end_phase = 1;
  float getSample(){
    return phase;
  }
};

using Saturator = AntialiasedCubicSaturator;

/**
 * This template was easier to rewrite than inherit from. Really.
 **/

enum DelayState {
    DS_FADE_DELAY,
    DS_DELAY,
    DS_FADE_LOOP,
    DS_LOOP,
};

#ifdef USE_TRANSITION_LUT
static constexpr float fade_in_lut[64] = { 0.0, 0.125, 0.1767766952966369,
    0.21650635094610965, 0.25, 0.2795084971874737, 0.30618621784789724,
    0.33071891388307384, 0.3535533905932738, 0.375, 0.39528470752104744,
    0.414578098794425, 0.4330127018922193, 0.45069390943299864,
    0.46770717334674267, 0.4841229182759271, 0.5, 0.5153882032022076,
    0.5303300858899106, 0.5448623679425842, 0.5590169943749475, 0.57282196186948,
    0.5863019699779287, 0.5994789404140899, 0.6123724356957945, 0.625,
    0.6373774391990981, 0.649519052838329, 0.6614378277661477, 0.673145600891813,
    0.6846531968814576, 0.6959705453537527, 0.7071067811865476,
    0.7180703308172536, 0.7288689868556626, 0.739509972887452, 0.75,
    0.7603453162872774, 0.770551750371122, 0.7806247497997998,
    0.7905694150420949, 0.8003905296791061, 0.8100925873009825, 0.81967981553775,
    0.82915619758885, 0.8385254915624212, 0.8477912478906585, 0.8569568250501305,
    0.8660254037844386, 0.875, 0.8838834764831844, 0.8926785535678563,
    0.9013878188659973, 0.9100137361600648, 0.9185586535436918, 0.9270248108869579,
    0.9354143466934853, 0.9437293044088437, 0.9519716382329886, 0.960143218483576,
    0.9682458365518543, 0.9762812094883317, 0.9842509842514764, 0.9921567416492215

};

static constexpr float fade_out_lut[64] = { 0.9921567416492215, 0.9842509842514764,
    0.9762812094883317, 0.9682458365518543, 0.960143218483576, 0.9519716382329886,
    0.9437293044088437, 0.9354143466934853, 0.9270248108869579, 0.9185586535436918,
    0.9100137361600648, 0.9013878188659973, 0.8926785535678563, 0.8838834764831844,
    0.875, 0.8660254037844386, 0.8569568250501305, 0.8477912478906585,
    0.8385254915624212, 0.82915619758885, 0.81967981553775, 0.8100925873009825,
    0.8003905296791061, 0.7905694150420949, 0.7806247497997998, 0.770551750371122,
    0.7603453162872774, 0.75, 0.739509972887452, 0.7288689868556626,
    0.7180703308172536, 0.7071067811865476, 0.6959705453537527, 0.6846531968814576,
    0.673145600891813, 0.6614378277661477, 0.649519052838329, 0.6373774391990981,
    0.625, 0.6123724356957945, 0.5994789404140899, 0.5863019699779287,
    0.57282196186948, 0.5590169943749475, 0.5448623679425842, 0.5303300858899106,
    0.5153882032022076, 0.5, 0.4841229182759271, 0.46770717334674267,
    0.45069390943299864, 0.4330127018922193, 0.414578098794425, 0.39528470752104744,
    0.375, 0.3535533905932738, 0.33071891388307384, 0.30618621784789724,
    0.2795084971874737, 0.25, 0.21650635094610965, 0.1767766952966369, 0.125, 0.0 };
#endif

template <class Processor = Processor>
class FbDelay : public Processor {
public:
    Bitcrusher crusher;
    SmoothSampleRateReducer<> reducer;

    template <typename... Args>
    FbDelay(FloatArray feedback_buffer, FloatArray fade_buffer, Args&&... args)
        : Processor(std::forward<Args>(args)...)
        , feedback_buffer(feedback_buffer)
        , fade_buffer(fade_buffer)
        , feedback_amount(0)
        , triggered(false)
        , frozen(false)
        , decimate_outside(false)
        , delay_size(0)
        , old_delay_size(0)
        , needs_crossfade(false)
        , state(DS_FADE_DELAY) {
    }
    void setDelay(float delay) {
        old_delay_size = delay_size;
        delay_size = delay;
        if (abs(delay_size - old_delay_size) >= 64.f)
            needs_crossfade = true;
    }
    void setFeedback(float amount) {
        feedback_amount = amount;
    }
    void setDecimationLocation(bool outside) {
        decimate_outside = outside;
    }
    void trigger() {
        triggered = true;
    }
    float getFeedback() {
        return feedback_amount;
    }
    void process(FloatArray input, FloatArray output) {
        dc.process(input, input);
        switch (state) {
        case DS_FADE_DELAY:
            renderFadeDelay(input, output);
            break;
        case DS_DELAY:
            renderDelay(input, output);
            break;
        }
    }

    template <typename... Args>
    static FbDelay* create(size_t blocksize, Args&&... args) {
#ifdef CIRCULAR
        return new FbDelay<Processor>(FloatArray::create(blocksize),
            FloatArray::create(blocksize), std::forward<Args>(args)...);
#else
        return new FbDelay<Processor>(FloatArray::create(blocksize),
            FloatArray::create(blocksize), std::forward<Args>(args)...);
#endif
    }
    static void destroy(FbDelay* obj) {
        FloatArray::destroy(obj->feedback_buffer);
        FloatArray::destroy(obj->fade_buffer);
        Processor::destroy(obj);
    }

protected:
    DelayState state;
    bool decimate_outside;

    template <DelayState state>
    void render(FloatArray input, FloatArray output);

    FloatArray feedback_buffer;
    FloatArray fade_buffer;
    float feedback_amount;
    DcBlockingFilter dc, dc_fb;
    bool frozen;
    bool needs_crossfade;
    float old_delay_size;
    bool triggered;
    float delay_size;
    Saturator saturator;

    //float stepped_delay;

    inline void fadeIn(FloatArray data) {
#ifdef USE_TRANSITION_LUT
        size_t size = data.getSize();
        for (size_t i = 0; i < size; i++) {
            data[i] *= fade_in_lut[i];
        }
#else
        data.scale(0.f, 1.f, data);
#endif
    }
    inline void fadeOut(FloatArray data) {
#ifdef USE_TRANSITION_LUT
        size_t size = data.getSize();
        for (size_t i = 0; i < size; i++) {
            data[i] *= fade_out_lut[i];
        }
#else
        data.scale(1.f, 0.f, data);
#endif
    }
    inline void fadeOut(FloatArray input, FloatArray output) {
#ifdef USE_TRANSITION_LUT
        size_t size = input.getSize();
        for (size_t i = 0; i < size; i++) {
            output[i] = input[i] * fade_out_lut[i];
        }
#else
        input.scale(1.f, 0.f, output);
#endif
    }

    void renderFadeDelay(FloatArray input, FloatArray output) {
        // Refetch previous buffer to be used as feedback
        #ifdef CIRCULAR
        size_t buffer_size = this->ringbuffer->getSize();
        #else
        size_t buffer_size = Processor::buffer.getSize();
        #endif
        size_t size = input.getSize();

        Processor::buffer.setDelay(delay_size);
        Processor::buffer.delay(output.getData(), size, delay_size);
        Processor::setDelay(delay_size);

        fadeIn(output);

        output.add(fade_buffer);
        feedback_buffer.copyFrom(output);
        state = DS_DELAY;
    }

    void renderDelay(FloatArray input, FloatArray output) {
#ifdef DECIMATE_PRE
        if (decimate_outside) {
            crusher.process(input, input);
            reducer.process(input, input);
        }
        else 
#endif
        {
            // Feedback processing        
            crusher.process(feedback_buffer, feedback_buffer);
            reducer.process(feedback_buffer, feedback_buffer);
        }
        dc_fb.process(feedback_buffer, feedback_buffer);
        feedback_buffer.multiply(feedback_amount);
        input.add(feedback_buffer);
        
        saturator.process(input, input);

        // Delay processing
        size_t size = input.getSize();
        if (triggered) {
            triggered = false;
        }
        if (needs_crossfade) {
            Processor::process(input, fade_buffer);
            fadeOut(fade_buffer);
            needs_crossfade = false;
            renderFadeDelay(input, output);
        }
        else {
            Processor::setDelay(delay_size);
            Processor::process(input, output); // XXX was - Processor::smooth
//            setDelay(delay_size);

            // Store feedback
            feedback_buffer.copyFrom(output);
        }
#ifndef DECIMATE_PRE
        if (decimate_outside) {
            crusher.process(output, output);
            reducer.process(output, output);
        }
#endif
    }
};

using Delay = FbDelay<>;

using MixDelay = DryWetSignalProcessor<Delay>;

class TrashDelayPatch : public Patch {
public:
    MixDelay* delay1;
    MixDelay* delay2;
    size_t max_delay_samples;
    SmoothFloat smooth_delay;
    AdjustableTapTempo* tempo;
    SineOscillator* lfo1;
    SawOscillator* lfo2;
    bool frozen;
    float delay_samples;
    bool decimate_outside = false;
    
    //    SmoothFloat delay_samples;

    TrashDelayPatch() {
        max_delay_samples = max_delay_seconds * getSampleRate();
        registerParameter(PARAMETER_A, "Delay");
        setParameterValue(PARAMETER_A, 1.f);
        registerParameter(PARAMETER_B, "Feedback");
        setParameterValue(PARAMETER_B, 0.5f);
        registerParameter(PARAMETER_C, "Crush");
        setParameterValue(PARAMETER_C, 0.f);
        registerParameter(PARAMETER_D, "Decimate");
        setParameterValue(PARAMETER_D, 0.f);
        registerParameter(PARAMETER_F, "LFO Sine>");
        registerParameter(PARAMETER_G, "LFO Tri>");
        setButton(BUTTON_A, 0);
        setButton(BUTTON_B, 0);
        setButton(BUTTON_C, 0);

        // delay_samples.lambda = 0.96;

        lfo1 = SineOscillator::create(getSampleRate() / getBlockSize());
        lfo2 = SawOscillator::create(getSampleRate() / getBlockSize());
        tempo = AdjustableTapTempo::create(getSampleRate(), max_delay_samples);
        tempo->setPeriodInSamples(max_delay_samples);
#if defined CIRCULAR
        delay1 = MixDelay::create(getBlockSize(),
            FloatArray::create(getBlockSize()), FloatArray::create(getBlockSize()),
            new float[max_delay_samples + 1], max_delay_samples);
        delay2 = MixDelay::create(getBlockSize(),
            FloatArray::create(getBlockSize()), FloatArray::create(getBlockSize()),
            new float[max_delay_samples + 1], max_delay_samples);
#else
        delay1 = MixDelay::create(getBlockSize(),
            FloatArray::create(getBlockSize()), FloatArray::create(getBlockSize()),
            new float[max_delay_samples + 1], max_delay_samples);
        delay2 = MixDelay::create(getBlockSize(),
            FloatArray::create(getBlockSize()), FloatArray::create(getBlockSize()),
            new float[max_delay_samples + 1], max_delay_samples);
#endif
        delay1->setMix(0.5);
        delay1->setFeedback(0.5);
        delay2->setMix(0.5);
        delay2->setFeedback(0.5);
    }

    ~TrashDelayPatch() {
        SineOscillator::destroy(lfo1);
        SawOscillator::destroy(lfo2);
        MixDelay::destroy(delay1);
        MixDelay::destroy(delay2);
        AdjustableTapTempo::destroy(tempo);
    }

    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) {
        bool set = value != 0;
        static uint32_t counter = 0;
        switch (bid) {
        case BUTTON_A:
            tempo->trigger(set, samples);
            if (value) {
                lfo2->reset();
                if (frozen) {
                    delay1->trigger();
                    delay2->trigger();
                }
            }
            break;
        case BUTTON_B:
            if (value) {
                decimate_outside = !decimate_outside;
            }
            setButton(BUTTON_B, decimate_outside, 0);
            delay1->setDecimationLocation(decimate_outside);
            delay2->setDecimationLocation(decimate_outside);
            break;
        }
    }

    void processAudio(AudioBuffer& buffer) {
        // Tap tempo
        tempo->clock(buffer.getSize());
        tempo->adjust((1.f - getParameterValue(PARAMETER_A)) * 4096);
        delay_samples = tempo->getPeriodInSamples();
        
        if (delay_samples > max_delay_samples) {
            tempo->resetAdjustment(max_delay_samples);
            delay_samples = max_delay_samples - 1;
        }

        // Use quadratic crush factor
        float crush = 1.f - getParameterValue(PARAMETER_C);
        crush = 1.f - crush * crush;
        delay1->crusher.setCrush(crush);
        delay2->crusher.setCrush(crush);

        // Linear SR reduction seems OK
        float reduce = getParameterValue(PARAMETER_D);
        delay1->reducer.setFactor(reduce);
        delay2->reducer.setFactor(reduce);

        // Negative feedback doesn't seem to make things interesting here
        float fb = getParameterValue(PARAMETER_B);
        FloatArray left = buffer.getSamples(0);

        delay1->setFeedback(fb);
        delay1->setDelay(delay_samples);
        delay1->process(left, left);
        FloatArray right = buffer.getSamples(1);
        delay2->setFeedback(fb);
        delay2->setDelay(delay_samples);
        delay2->process(right, right);

        // Tempo synced LFO
        float freq = tempo->getFrequency();
        lfo1->setFrequency(freq);
        lfo2->setFrequency(freq);
        setParameterValue(PARAMETER_F, lfo1->generate() * 0.5 + 0.5);
        setParameterValue(PARAMETER_G, lfo2->generate());
        setButton(BUTTON_C, lfo2->getPhase() < M_PI);
        setButton(BUTTON_D, 1);
    }
};
#endif
