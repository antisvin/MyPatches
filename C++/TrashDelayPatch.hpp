#ifndef __TRASH_DELAY_PATCH_HPP__
#define __TRASH_DELAY_PATCH_HPP__

/**
 * Trash delay is a tempo synced delay, looper and audio corruption utility.
 *
 * ``I looked, and behold, a white horse, and he who sat on it had a trash delay.``
 *
 * No PT2399 chips were hurt making this patch!
 *
 * Normal mode - this is an audio delay with nasty character.
 *
 * PARAM A - delay adjustment, set to max for starters, endless source of clicks and glitches if you change it
 * PARAM B - feedback, start from ~0.7 and adjust to taste
 * PARAM C - feedback bitcrusher level
 * PARAM D - feedback sample rate reduction
 * PARAM F - output sine LFO clocked by tempo
 * PARAM G - output ramp LFO synced by temo
 * BUTTON 1 - tap tempo / clock input
 * BUTTON 2 - FREEEEEZE (go to looper mode, see below)
 * BUTTON 3 - clock output (based on LFO1 phase)
 *
 * In frozen mode delay buffer is not written to and unit behavior changes:
 * PARAM B - dry/wet mix (will be applied to normal mode too)
 * BUTTON 1 - retrigger loop (tempo is still synced like in delay mode)
 * BUTTON 2 - exit to delay mode
 **/

#include "Patch.h"
#include "DelayProcessor.h"
#include "FeedbackProcessor.h"
#include "DcBlockingFilter.h"
#include "TapTempo.h"
#include "Bitcrusher.hpp"
#include "SampleRateReducer.hpp"
#include "SineOscillator.h"
#include "RampOscillator.h"
#include "SmoothValue.h"
#include "DryWetProcessor.h"

//#define FAST_FRAC
//#define USE_TRANSITION_LUT

// static constexpr float max_delay_seconds = 10.f; // OWL2+
static constexpr float max_delay_seconds = 2.7f; // Just enough for OWL1
static constexpr float max_feedback = 0.75f;

#ifdef FAST_FRAC
using Processor = FastFractionalDelayProcessor;
#else
using Processor = FractionalDelayProcessor<LINEAR_INTERPOLATION>;
#endif
using SawOscillator = RampOscillator<>;

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
        , freeze_index(0)
        , delay_size(0)
        , old_delay_size(0)
        , needs_crossfade(false)
        , state(DS_FADE_DELAY) {
    }
    void setDelay(float delay) {
        old_delay_size = delay_size;
        delay_size = delay;
        if (abs(delay_size - old_delay_size) > 4.f)
            needs_crossfade = true;
    }
    void setFeedback(float amount) {
        feedback_amount = amount;
    }
    void freeze(bool state) {
        frozen = state;
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
        case DS_FADE_LOOP:
            renderFadeLoop(input, output);
            break;
        case DS_LOOP:
            renderLoop(input, output);
            break;
        }
    }

    template <typename... Args>
    static FbDelay* create(size_t blocksize, Args&&... args) {
        return new FbDelay<Processor>(FloatArray::create(blocksize),
            FloatArray::create(blocksize), std::forward<Args>(args)...);
    }
    static void destroy(FbDelay* obj) {
        FloatArray::destroy(obj->feedback_buffer);
        FloatArray::destroy(obj->fade_buffer);
        Processor::destroy(obj);
    }

protected:
    DelayState state;

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
    size_t freeze_index;

    SmoothFloat smooth_delay;
    float stepped_delay;

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
    inline void fadeIn(FloatArray input, FloatArray output) {
#ifdef USE_TRANSITION_LUT
        size_t size = input.getSize();
        for (size_t i = 0; i < size; i++) {
            output[i] = input[i] * fade_in_lut[i];
        }
#else
        input.scale(0.f, 1.f, output);
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
        size_t buffer_size = Processor::buffer.getSize();
        size_t size = input.getSize();

        Processor::setDelay(delay_size);
        Processor::buffer.setDelay(delay_size - size);
        Processor::buffer.delay(output.getData(), size, delay_size);

        fadeIn(output);

        output.add(fade_buffer);
        feedback_buffer.copyFrom(output);
        state = DS_DELAY;
    }

    void renderDelay(FloatArray input, FloatArray output) {
        // Feedback processing
        crusher.process(feedback_buffer, feedback_buffer);
        reducer.process(feedback_buffer, feedback_buffer);
        dc_fb.process(feedback_buffer, feedback_buffer);
        feedback_buffer.multiply(feedback_amount);
        input.add(feedback_buffer);
        // Delay processing
        size_t size = input.getSize();
        if (needs_crossfade) {
//            Processor::smooth(input, fade_buffer, old_delay_size);
            Processor::process(input, fade_buffer);
            fadeOut(fade_buffer);
            needs_crossfade = false;
            renderFadeDelay(input, output);

/*
            size_t buffer_size = Processor::buffer.getSize();

            //Processor::buffer.read(output.getData(), size);
            Processor::setDelay(delay_size);
            //Processor::buffer.read(output.getData(), size);
            Processor::buffer.delay(output.getData(), size, delay_size);

            fadeIn(output);
            feedback_buffer.copyFrom(output);
            output.add(fade_buffer);

            needs_crossfade = false;
*/
        }
        else if (frozen) {
            fadeOut(input);
            Processor::process(input, fade_buffer);
            fadeOut(fade_buffer);


            //fadeOut(output, fade_buffer);

            // Store faded out buffer
            size_t buffer_size = Processor::buffer.getSize();
            //Processor::buffer.write(fade_buffer.getData(), size);
            /// XXX 1
            //Processor::buffer.moveWriteHead(buffer_size - size);
            //Processor::buffer.write(fade_buffer.getData(), size);

            /// XXX2
//            freeze_index = Processor::buffer.getReadIndex();
                
            freeze_index =
                fmodf(Processor::buffer.getReadIndex() + buffer_size - delay_size,
                    buffer_size);
            Processor::buffer.setReadIndex(freeze_index);
//            freeze_index =
//                fmodf(Processor::buffer.getWriteIndex() + buffer_size - delay_size,
//                    buffer_size);
//            Processor::buffer.setReadIndex(freeze_index);
            renderFadeLoop(input, output);
#if 0            
            feedback_buffer.scale(1.0, 0.0, fade_buffer);
            output.copyFrom(fade_buffer);
            size_t buffer_size = Processor::buffer.getSize();
            freeze_index =
                (Processor::buffer.getWriteIndex() + buffer_size - delay_size) %
                buffer_size;
            Processor::buffer.setReadIndex(freeze_index);
#endif
        }
        else {

            //Processor::process(input, output);
            Processor::smooth(input, output, delay_size);

            // Store feedback
            feedback_buffer.copyFrom(output);
        }
    }

    void renderFadeLoop(FloatArray input, FloatArray output) {
        // Feedback and input will fade out when freeze mode is entered
        // input.scale(1.0, 0.0, input);
        // fade_buffer.add(input);

        // Frozen delay won't write to buffer, input is discarded
        Processor::buffer.read(output.getData(), output.getSize());

#if 0
        Processor::setDelay(delay_size);
        Processor::buffer.setDelay(delay_size - size);
        Processor::buffer.delay(output.getData(), size, delay_size);
#endif


        // output.scale(0.0, 1.0, output);
        fadeIn(output);
        output.add(fade_buffer);

        // Looper FX
        crusher.process(output, output);
        reducer.process(output, output);
        // dc_fb.process(output, output);

        state = DS_LOOP;
    }

    void renderLoop(FloatArray input, FloatArray output) {
        size_t size = input.getSize();
        size_t buffer_size = Processor::buffer.getSize();

        // Triggered manually or reached frozen buffer end
        if (triggered ||
            fmodf(Processor::buffer.getReadIndex() + buffer_size + size - freeze_index,
                buffer_size) > delay_size) {
            triggered = false;
            // Read buffer tail
            Processor::buffer.read(fade_buffer.getData(), size);
            //Processor::setDelay(buffer_size + delay_size);

            // Prepare fade out buffer
            fadeOut(fade_buffer);

            if (frozen) {
                // Rewind loop - no need to change state as it will return to current one
                renderFadeLoop(input, output);
            }
            else {
                // Exiting looper mode - only done when looper reaches end.
                // Alternatives would require jumping to different place in
                // buffer or dropping part of buffer. Update write index
                //Processor::buffer.setWriteIndex(freeze_index + delay_size);
                ///Processor::buffer.setWriteIndex(Processor::buffer.getReadIndex());
                ///Processor::buffer.setReadIndex(Processor::buffer.getReadIndex() + buffer_size - size);
                ///Processor::buffer.read()
                // Unfreeze delay
                fadeOut(feedback_buffer, fade_buffer);
                renderFadeDelay(input, output);
            }
        }
        else {
            // Normal looper processing
            // Frozen delay doesn't write to buffer, input is discarded
            Processor::buffer.read(output.getData(), size);
            // feedback_buffer.multiply(feedback_amount);
            // output
            // Processor::buffer.setWriteIndex(
            //    Processor::buffer.getReadIndex() + delay_size);
            // Looper FX
            crusher.process(output, output);
            reducer.process(output, output);
            // dc.process(output, output);
            feedback_buffer.copyFrom(output);
        }
    }
};

using Delay = FbDelay<>;

using MixDelay = DryWetSignalProcessor<Delay>;

class TrashDelayPatch : public Patch {
public:
    MixDelay* delay1;
    MixDelay* delay2;
    size_t max_delay_samples;
    AdjustableTapTempo* tempo;
    SineOscillator* lfo1;
    SawOscillator* lfo2;
    bool frozen;
    float delay_samples;
    //    SmoothFloat delay_samples;

    TrashDelayPatch() {
        max_delay_samples = max_delay_seconds * getSampleRate();
        registerParameter(PARAMETER_A, "Delay");
        setParameterValue(PARAMETER_A, 0.f);
        registerParameter(PARAMETER_B, "Feedback");
        setParameterValue(PARAMETER_B, 0.5f);
        registerParameter(PARAMETER_C, "Crush");
        setParameterValue(PARAMETER_C, 0.f);
        registerParameter(PARAMETER_D, "Decimate");
        setParameterValue(PARAMETER_D, 0.f);
        registerParameter(PARAMETER_F, "LFO Sine>");
        registerParameter(PARAMETER_G, "LFO Ramp>");
        setButton(BUTTON_A, 0);
        setButton(BUTTON_B, 0);
        setButton(BUTTON_C, 0);

        // delay_samples.lambda = 0.96;

        lfo1 = SineOscillator::create(getSampleRate() / getBlockSize());
        lfo2 = SawOscillator::create(getSampleRate() / getBlockSize());
        tempo = AdjustableTapTempo::create(getSampleRate(), max_delay_samples);
        tempo->setSamples(max_delay_samples);
#ifdef FAST_FRAC
        delay1 = MixDelay::create(getBlockSize(), FloatArray::create(getBlockSize()),
            FloatArray::create(getBlockSize()), new float[max_delay_samples + 1],
            new float[max_delay_samples + 1], max_delay_samples);
        delay2 = MixDelay::create(getBlockSize(), FloatArray::create(getBlockSize()),
            FloatArray::create(getBlockSize()), new float[max_delay_samples + 1],
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
                lfo1->reset();
                if (frozen) {
                    delay1->trigger();
                    delay2->trigger();
                }
            }
            break;
        case BUTTON_B:
            if (value)
                frozen = !frozen;
            setButton(BUTTON_B, frozen, 0);
            delay1->freeze(frozen);
            delay2->freeze(frozen);
        }
    }

    void processAudio(AudioBuffer& buffer) {
        // Tap tempo
        tempo->clock(buffer.getSize());
        tempo->adjust((1.f - getParameterValue(PARAMETER_A)) * 4096);
        delay_samples = min(tempo->getSamples(), max_delay_samples);
        // if (delay_samples > max_delay_samples) {
        //            delay_samples.reset(max_delay_samples);
        //}

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
        delay1->process(left, left);
        delay1->setDelay(delay_samples);
        FloatArray right = buffer.getSamples(1);
        delay2->setFeedback(fb);
        delay2->process(right, right);
        delay2->setDelay(delay_samples);

        // Tempo synced LFO
        lfo1->setFrequency(tempo->getFrequency());
        float lfoFreq = getSampleRate() / max_delay_samples;
        lfo2->setFrequency(lfoFreq);
        setParameterValue(PARAMETER_F, lfo1->generate() * 0.5 + 0.5);
        setParameterValue(PARAMETER_G, lfo2->generate() * 0.5 + 0.5);
        setButton(BUTTON_C, lfo1->getPhase() < M_PI);
    }
};
#endif
