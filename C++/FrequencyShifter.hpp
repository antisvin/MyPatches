#ifndef __FREQUENCY_SHIFTER_HPP__
#define __FREQUENCY_SHIFTER_HPP__

#include "SignalProcessor.h"
#include "Oscillators/QuadratureSineOscillator.hpp"

class FrequencyShifter : public MultiSignalProcessor {
private:
    QuadratureSineOscillator oscillator;
    AudioBuffer* tmp;
    float control = 1.0;
    float sr;

    inline float crossfade(float a, float b, float fade) {
        return a * fade + b * (1 - fade);
    }

public:
    FrequencyShifter(float sr = 48000) : sr(sr), oscillator(sr) {}
    void setAmount(float amount) {
        oscillator.setFrequency(amount * sr);
    }
    void setControl(float control) {
        this->control = control;
    }
    void process(AudioBuffer& input, AudioBuffer& output) override {
        oscillator.generate(*tmp);

        tmp->getSamples(0).multiply(input.getSamples(1));
        tmp->getSamples(1).multiply(input.getSamples(0));
        output.getSamples(0).copyFrom(tmp->getSamples(0));
        output.getSamples(0).add(tmp->getSamples(1));
        output.getSamples(1).copyFrom(tmp->getSamples(0));
        output.getSamples(1).subtract(tmp->getSamples(1));

#if 0
        for (int i = 0; i < input.getSize(); i++) {
            float sample_i = input[0]
            

            sample_i = oscillator.getX() * transformer.outputSample_1;
            float sample_q = oscillator.getY() * transformer.outputSample_2;
            float sample_USB = sample_i - sample_q;
            float sample_LSB = sample_i + sample_q;

            // crossfade the two sideband signals
            sample_i = crossfade(sample_USB, sample_LSB, control);

            *(out++) = clip(sample_I);
        }
#endif
    }

    static FrequencyShifter* create(float sr, int bs) {
        FrequencyShifter* shifter = new FrequencyShifter(sr);
        AudioBuffer* tmp = AudioBuffer::create(2, bs);
        shifter->tmp = tmp;
        return shifter;
    }

    static void destroy(FrequencyShifter* shifter){
        AudioBuffer::destroy(shifter->tmp);
        delete shifter;
    }
};

#endif
