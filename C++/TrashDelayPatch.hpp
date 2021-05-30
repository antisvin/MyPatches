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

static constexpr float max_delay_seconds = 10.f; // OWL2+
//static constexpr float max_delay_seconds = 2.7f; // Just enough for OWL1
static constexpr float max_feedback = 0.75f;

using Processor = FractionalDelayProcessor<HERMITE_INTERPOLATION>;
using SawOscillator = RampOscillator<>;

/**
 * This template was easier to rewrite than inherit from. Really.
 **/

template <class Processor = Processor>
class FbDelay : public Processor {
protected:
    FloatArray feedback_buffer;
    float feedback_amount;
    DcBlockingFilter dc;
    bool frozen;
    bool crossfade;
    size_t delay_size;

public:
    Bitcrusher crusher;
    SmoothSampleRateReducer<> reducer;

    template <typename... Args>
    FbDelay(FloatArray buffer, Args&&... args)
        : Processor(std::forward<Args>(args)...)
        , feedback_buffer(buffer)
        , feedback_amount(0)
        , crossfade(false)
        , frozen(false)
        , delay_size(0) {
    }
    void setDelay(size_t delay) {
        Processor::setDelay(delay);
        delay_size = delay;
    }
    void setFeedback(float amount) {
        feedback_amount = amount;
    }
    void freeze(bool state) {
        frozen = state;
        crossfade = true;
    }
    void trigger() {
        crossfade = true;
    }
    float getFeedback() {
        return feedback_amount;
    }
    void process(FloatArray input, FloatArray output) {
        if (frozen) {
            if (crossfade) {
                // Feedback and input - fade out when freeze mode is enter
                output.ramp(1.0, 0.0);
//                input.multiply(output);
                feedback_buffer.multiply(output);
//                feedback_buffer.add(input);
            }

            size_t size = output.getSize();
            if (Processor::buffer.getReadCapacity() < size) {
                // Reached delay buffer end
                Processor::buffer.read(feedback_buffer.getData(), size);
                //Processor::buffer.moveReadHead(Processor::buffer.getSize() - delay_size);
                Processor::buffer.setReadIndex(
                    Processor::buffer.getWriteIndex() + Processor::buffer.getSize() - delay_size);
                output.ramp(1.0, 0.0);
                feedback_buffer.multiply(output);
                crossfade = true;
            }

            // Frozen delay doesn't write to buffer, input is discarded
            Processor::buffer.read(output.getData(), size);

            if (crossfade) {
                // Fade in buffer output and mix channels we're fading out
                output.add(feedback_buffer);
                feedback_buffer.ramp(0.0, 1.0);
                input.multiply(feedback_buffer);
                crossfade = false;
            }

            // Loopback FX in feedback line
            crusher.process(output, output);
            reducer.process(output, output);
            dc.process(output, output);
            // Feedback is not heard, but we prepare buffer for exiting this mode or retriggering
            feedback_buffer.copyFrom(output);
        }
        else {
            if (crossfade) {
                // Fade in feedback buffer
                output.ramp(1.0, 0.0);
                feedback_buffer.multiply(output);
                output.ramp(0.0, 0.1);
                input.multiply(output);
                input.add(feedback_buffer);
                crossfade = false;
            }
            // Loopback FX in feedback line
            crusher.process(feedback_buffer, feedback_buffer);
            reducer.process(feedback_buffer, feedback_buffer);
            dc.process(feedback_buffer, feedback_buffer);
            // Apply feedback amount
            feedback_buffer.multiply(feedback_amount); // Mix
            input.add(feedback_buffer);
            // Normal processing
            Processor::process(input, output);
            // Store feedback
            feedback_buffer.copyFrom(output);
        }
    }
    template <typename... Args>
    static FbDelay* create(size_t blocksize, Args&&... args) {
        return new FbDelay<Processor>(
            FloatArray::create(blocksize), std::forward<Args>(args)...);
    }
    static void destroy(FbDelay* obj) {
        FloatArray::destroy(obj->feedback_buffer);
        Processor::destroy(obj);
    }
};

using Delay = FbDelay<>;

class TrashDelayPatch : public Patch {
public:
    Delay* delay1;
    Delay* delay2;
    size_t max_delay_samples;
    AdjustableTapTempo* tempo;
    SineOscillator* lfo1;
    SawOscillator* lfo2;
    static constexpr int tempo_limit = 1 << 17;
    SmoothFloat time;
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
        tempo = AdjustableTapTempo::create(getSampleRate(), tempo_limit);
        tempo->setSamples(max_delay_samples);
        delay1 = Delay::create(getBlockSize(), new float[max_delay_samples + 1],
            max_delay_samples);
        delay1->setFeedback(0.5);
        delay2 = Delay::create(getBlockSize(), new float[max_delay_samples + 1],
            max_delay_samples);
        delay2->setFeedback(0.5);
        time.reset(1.f);
    }

    ~TrashDelayPatch() {
        SineOscillator::destroy(lfo1);
        SawOscillator::destroy(lfo2);
        Delay::destroy(delay1);
        Delay::destroy(delay2);
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
        while (period > max_delay_samples)
            period *= 0.5f;
        time.update(period);
        float time_value = time.getValue();

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
        delay1->setFeedback(fb);
        delay1->setDelay(time_value);
        FloatArray left = buffer.getSamples(0);
        delay1->process(left, left);
        FloatArray right = buffer.getSamples(1);
        delay2->setDelay(time_value);
        delay2->setFeedback(fb);
        delay2->process(right, right);

        // Tempo synced LFO
        lfo1->setFrequency(tempo->getFrequency());
        float lfoFreq = getSampleRate() / tempo_limit;
        lfo2->setFrequency(lfoFreq);
        setParameterValue(PARAMETER_F, lfo1->generate() * 0.5 + 0.5);
        setParameterValue(PARAMETER_G, lfo2->generate() * 0.5 + 0.5);
        setButton(BUTTON_C, lfo1->getPhase() < M_PI);
    }
};

#endif
