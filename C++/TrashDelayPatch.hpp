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

// static constexpr float max_delay_seconds = 10.f; // OWL2+
static constexpr float max_delay_seconds = 2.7f; // Just enough for OWL1
static constexpr float max_feedback = 0.75f;

//using Processor = FastFractionalDelayProcessor;
using Processor = FractionalDelayProcessor<HERMITE_INTERPOLATION>;
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

static constexpr float fade_in_lut[64] = { 0.12403473458920845,
    0.17541160386140583, 0.21483446221182986, 0.2480694691784169,
    0.2773500981126146, 0.3038218101251, 0.3281650616569468, 0.35082320772281167,
    0.3721042037676254, 0.3922322702763681, 0.41137667560372115, 0.4296689244236597,
    0.4472135954999579, 0.4640954808922571, 0.4803844614152614, 0.4961389383568338,
    0.5114083119567588, 0.5262348115842176, 0.5406548735632486, 0.5547001962252291,
    0.5683985600588051, 0.5817744738827396, 0.5948496901286525, 0.6076436202502,
    0.6201736729460423, 0.6324555320336759, 0.6445033866354896, 0.6563301233138936,
    0.6679474875720741, 0.6793662204867574, 0.6905961749988752, 0.7016464154456233,
    0.7125253031944253, 0.723240570679579, 0.7337993857053428, 0.7442084075352507,
    0.7544738360147217, 0.7646014547562571, 0.7745966692414834, 0.7844645405527362,
    0.7942098153317112, 0.8038369524685004, 0.8133501469468494, 0.8227533512074423,
    0.8320502943378437, 0.8412444993533733, 0.8503392987960294, 0.8593378488473195,
    0.8682431421244592, 0.8770580193070292, 0.8857851797221404, 0.8944271909999159,
    0.9029864978971809, 0.9114654303753, 0.9198662110077999, 0.9281909617845142,
    0.936441710371274, 0.9446203958774616, 0.9527288741779099, 0.9607689228305228,
    0.9687422456265332, 0.9766504768063925, 0.9844951849708404, 0.9922778767136676

};

static constexpr float fade_out_lut[64] = { 0.9922778767136676, 0.9844951849708404,
    0.9766504768063925, 0.9687422456265332, 0.9607689228305228, 0.9527288741779099,
    0.9446203958774616, 0.936441710371274, 0.9281909617845142, 0.9198662110077999,
    0.9114654303753, 0.9029864978971809, 0.8944271909999159, 0.8857851797221404,
    0.8770580193070292, 0.8682431421244592, 0.8593378488473195, 0.8503392987960294,
    0.8412444993533733, 0.8320502943378437, 0.8227533512074423, 0.8133501469468494,
    0.8038369524685004, 0.7942098153317112, 0.7844645405527362, 0.7745966692414834,
    0.7646014547562571, 0.7544738360147217, 0.7442084075352507, 0.7337993857053428,
    0.723240570679579, 0.7125253031944253, 0.7016464154456233, 0.6905961749988752,
    0.6793662204867574, 0.6679474875720741, 0.6563301233138936, 0.6445033866354896,
    0.6324555320336759, 0.6201736729460423, 0.6076436202502, 0.5948496901286525,
    0.5817744738827396, 0.5683985600588051, 0.5547001962252291, 0.5406548735632486,
    0.5262348115842176, 0.5114083119567588, 0.4961389383568338, 0.4803844614152614,
    0.4640954808922571, 0.4472135954999579, 0.4296689244236597, 0.41137667560372115,
    0.3922322702763681, 0.3721042037676254, 0.35082320772281167,
    0.3281650616569468, 0.3038218101251, 0.2773500981126146, 0.2480694691784169,
    0.21483446221182986, 0.17541160386140583, 0.12403473458920845 };

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
        , state(DS_FADE_DELAY) {
    }
    void setDelay(size_t delay) {
        delay_size = delay;
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
//        return new FbDelay<Processor>(FloatArray::create(blocksize),
//            FloatArray::create(blocksize), std::forward<Args>(args)...);
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
    bool triggered;
    size_t delay_size;
    size_t freeze_index;

    SmoothFloat smooth_delay;
    float stepped_delay;

    inline void fadeIn(FloatArray data) {
        size_t size = data.getSize();
        for (size_t i = 0; i < size; i++) {
            data[i] *= fade_in_lut[i];
        }
        //        data.scale(0.f, 1.f, data);
    }
    inline void fadeIn(FloatArray input, FloatArray output) {
        size_t size = input.getSize();
        for (size_t i = 0; i < size; i++) {
            output[i] = input[i] * fade_in_lut[i];
        }
        //        input.scale(0.f, 1.f, output);
    }
    inline void fadeOut(FloatArray data) {
        size_t size = data.getSize();
        for (size_t i = 0; i < size; i++) {
            data[i] *= fade_out_lut[i];
        }
        //        data.scale(1.f, 0.f, data);
    }
    inline void fadeOut(FloatArray input, FloatArray output) {
        size_t size = input.getSize();
        for (size_t i = 0; i < size; i++) {
            output[i] = input[i] * fade_out_lut[i];
        }
        //        input.scale(1.f, 0.f, output);
    }

    void renderFadeDelay(FloatArray input, FloatArray output) {
        // Fade in feedback buffer - conains last buffer from looper
        fadeIn(input);
        // input.scale(0.0, 1.0, input);
        // Delay processing - feedback buffer is empty at the moment

        // Refetch previous buffer to be used as feedback
        size_t buffer_size = Processor::buffer.getSize();
        size_t size = input.getSize();
        Processor::buffer.moveReadHead(buffer_size - size);
        Processor::buffer.read(feedback_buffer.getData(), size);
        // fadeOut(feedback_buffer);

        // Resume rendering with old feedback
        renderDelay(input, output);
        fadeIn(output);
        feedback_buffer.copyFrom(output);
        output.add(fade_buffer);
        // Store feedback
        // feedback_buffer.scale(0.0, 1.0, feedback_buffer);
        // fadeIn(feedback_buffer);
        state = DS_DELAY;
    }

    void renderDelay(FloatArray input, FloatArray output) {
        // Delay feedback FX
        crusher.process(feedback_buffer, feedback_buffer);
        reducer.process(feedback_buffer, feedback_buffer);
        dc_fb.process(feedback_buffer, feedback_buffer);
        // Apply feedback amount
        feedback_buffer.multiply(feedback_amount);
        input.add(feedback_buffer);
        // Delay processing
        size_t size = input.getSize();
        //Processor::setDelay(delay_size);
        #if 0
        for (size_t i = 0; i < size; i++) {
            //smooth_delay.update(stepped_delay);
            //delay_size = smooth_delay.getValue();
            //Processor::setDelay(delay_size);
            output[i] = Processor::process(input[i]);
        }
        #endif
        Processor::smooth(input, output, delay_size);

        if (frozen) {
            fadeOut(output, fade_buffer);

            // Store faded out buffer
            size_t buffer_size = Processor::buffer.getSize();
            Processor::buffer.moveWriteHead(buffer_size - size);
            Processor::buffer.write(fade_buffer.getData(), size);

            // feedback_buffer.scale(1.0, 0.0, fade_buffer);
            // output.copyFrom(fade_buffer);
            freeze_index =
                (Processor::buffer.getWriteIndex() + buffer_size - delay_size) %
                buffer_size;
            Processor::buffer.setReadIndex(freeze_index);
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

        // output.scale(0.0, 1.0, output);
        fadeIn(output);
        output.add(fade_buffer);

        // Looper FX
        crusher.process(output, output);
        reducer.process(output, output);
        // dc_fb.process(output, output);

        // Feedback is not heard, but we prepare buffer for exiting this
        // mode or retriggering feedback_buffer.copyFrom(output);
        state = DS_LOOP;
    }

    void renderLoop(FloatArray input, FloatArray output) {
        size_t size = input.getSize();
        size_t buffer_size = Processor::buffer.getSize();

        // Triggered manually or reached frozen buffer end
        if (triggered ||
            ((Processor::buffer.getReadIndex() + buffer_size + size * 2 - freeze_index) % buffer_size >
                delay_size)) {
            triggered = false;
            // Read buffer tail
            Processor::buffer.read(fade_buffer.getData(), size);
            Processor::buffer.setReadIndex(freeze_index);
            // Prepare fade out buffer
            // fade_buffer.scale(1.0, 0.0, fade_buffer);
            fadeOut(fade_buffer);

            if (frozen) {
                // Rewind loop - no need to change state as it will return to current one
                renderFadeLoop(input, output);
            }
            else {
                // Exiting looper mode - only done when looper reaches end.
                // Alternatives would require jumping to different place in
                // buffer or dropping part of buffer. Update write index
                Processor::buffer.setWriteIndex(freeze_index + delay_size);
                // Unfreeze delay
                state = DS_FADE_DELAY;
                // feedback_buffer.copyFrom(fade_buffer); // XXX?
                renderFadeDelay(input, output);
            }
        }
        else {
            // Normal looper processing
            // Frozen delay doesn't write to buffer, input is discarded
            Processor::buffer.read(output.getData(), size);
            //feedback_buffer.multiply(feedback_amount);
            //output
                // Processor::buffer.setWriteIndex(
                //    Processor::buffer.getReadIndex() + delay_size);
                // Looper FX
                crusher.process(output, output);
            reducer.process(output, output);
            // dc.process(output, output);
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

        lfo1 = SineOscillator::create(getSampleRate() / getBlockSize());
        lfo2 = SawOscillator::create(getSampleRate() / getBlockSize());
        tempo = AdjustableTapTempo::create(getSampleRate(), max_delay_samples);
        tempo->setSamples(max_delay_samples);
        delay1 = MixDelay::create(getBlockSize(),
            FloatArray::create(getBlockSize()), FloatArray::create(getBlockSize()),
            new float[max_delay_samples + 4], max_delay_samples);
        delay1->setMix(0.5);
        delay1->setFeedback(0.5);
        delay2 = MixDelay::create(getBlockSize(),
            FloatArray::create(getBlockSize()), FloatArray::create(getBlockSize()),
            new float[max_delay_samples + 4], max_delay_samples);
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
        float period = tempo->getSamples();
        if (period > max_delay_samples) {
            period = max_delay_samples;
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
        delay1->process(left, left);
        delay1->setDelay(period);
        FloatArray right = buffer.getSamples(1);
        delay2->setFeedback(fb);
        delay2->process(right, right);
        delay2->setDelay(period);

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
