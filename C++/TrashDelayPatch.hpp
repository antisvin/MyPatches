#ifndef __TRASH_DELAY_PATCH_HPP__
#define __TRASH_DELAY_PATCH_HPP__

/**
 * I looked, and behold, a white horse, and he who sat on it had a trash delay.
 * 
 * No PT2399 chips were hurt making this patch!
 * 
 * PARAM A - delay adjustment, set to max for starters, endless source of clicks and glitches later
 * PARAM B - feedback, start from ~0.7 and adjust to taste
 * PARAM C - feedback bitcrusher level
 * PARAM D - feedback sample rate reduction
 * PARAM F - output sine LFO clocked by tempo
 * PARAM G - output ramp LFO synced by temo
 * BUTTON 1 - tap tempo
 * BUTTON 2 - effect bypass, some peace finally
 * BUTTON 3 - clock output (based on LFO1 phase)
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

static constexpr float max_delay_seconds = 5.f;
static constexpr float max_feedback = 0.7f;


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

public:
    Bitcrusher crusher;
    SmoothSampleRateReducer<> reducer;

    template <typename... Args>
    FbDelay(FloatArray buffer, Args&&... args)
        : Processor(std::forward<Args>(args)...)
        , feedback_buffer(buffer)
        , feedback_amount(0) {
    }
    void setFeedback(float amount) {
        feedback_amount = amount;
    }
    float getFeedback() {
        return feedback_amount;
    }
    void process(FloatArray input, FloatArray output) {
        crusher.process(feedback_buffer, feedback_buffer);
        reducer.process(feedback_buffer, feedback_buffer);
        dc.process(feedback_buffer, feedback_buffer);
        feedback_buffer.multiply(feedback_amount);
        input.add(feedback_buffer);
        Processor::process(input, output);
        feedback_buffer.copyFrom(output);
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
            if (value)
                lfo1->reset();
            break;
        }
    }

    void processAudio(AudioBuffer& buffer) {

        tempo->clock(buffer.getSize());
        tempo->adjust((1.f - getParameterValue(PARAMETER_A)) * 4096);
        float period = tempo->getSamples();
        while (period >= max_delay_samples)
            period *= 0.5f;
        time.update(period);
        float time_value = time.getValue();
        delay1->setDelay(time_value);
        delay2->setDelay(time_value);

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
        delay2->setFeedback(fb);
        FloatArray left = buffer.getSamples(0);
        delay1->process(left, left);
        FloatArray right = buffer.getSamples(1);
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
