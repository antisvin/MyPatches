#ifndef __QUADRATURE_PHASER_HPP__
#define __QUADRATURE_PHASER_HPP__

#include "SignalProcessor.h"
#include "Oscillators/QuadratureSineOscillator.hpp"

class QuadraturePhaser : public MultiSignalProcessor {
private:
    QuadratureSineOscillator oscillator;
    float sr, control;
    AudioBuffer* buffer1;
    AudioBuffer* buffer2;

    /*

    inline float crossfade(float a, float b, float fade) {
        return a * fade + b * (1 - fade);
    }
    */

public:
    QuadraturePhaser(float sr = 48000) : oscillator(sr), sr(sr) {
    }
    void setAmount(float amount) {
        oscillator.setFrequency(amount * sr);
    }
    void setControl(float control) {
        this->control = control;
    }
    /**
     * Current implementation allows in place rendering with the same
     * object as input and output buffer
     */
    void process(AudioBuffer& input, AudioBuffer& output) override {
        oscillator.generate(*buffer1);
        // Copy osc samples
        buffer2->getSamples(0).copyFrom(buffer1->getSamples(0));
        buffer2->getSamples(1).copyFrom(buffer1->getSamples(1));
        // Multiply osc x input in quadrature - rotates signal
        buffer2->getSamples(0).multiply(input.getSamples(0));
        buffer2->getSamples(1).multiply(input.getSamples(1));
        // Buffer1 is not needed for generating first output channel,
        // so we can reuse it as temporary buffer for second channel.
        // Quadrature multiplication for second output channel
        buffer1->getSamples(0).multiply(input.getSamples(1));
        buffer1->getSamples(1).multiply(input.getSamples(0));
        // Input buffer is unused after that, so we can start writing to output
        // Copy sidebands to output buffer's channels
        output.getSamples(0).copyFrom(buffer2->getSamples(0));
        output.getSamples(1).copyFrom(buffer2->getSamples(0));
        output.getSamples(0).subtract(buffer2->getSamples(1));
        output.getSamples(1).add(buffer2->getSamples(1));
        // Crossfade sidebands and store results in left channel
        output.getSamples(0).multiply(control);
        output.getSamples(1).multiply(1.0 - control);
        output.getSamples(0).add(output.getSamples(1));
        // Buffer2 is available, use it to generate sidebands for second channel
        output.getSamples(1).copyFrom(buffer1->getSamples(0));
        buffer2->getSamples(1).copyFrom(buffer1->getSamples(0));
        output.getSamples(1).subtract(buffer1->getSamples(1));
        buffer2->getSamples(1).add(buffer1->getSamples(1));
        // Crossfade sidebands and store results in right channel
        output.getSamples(1).multiply(control);
        buffer2->getSamples(1).multiply(1.0 - control);
        output.getSamples(1).add(buffer2->getSamples(1));

        /*

        for (size_t i = 0; i < input.getSize(); i++) {
            ComplexFloat sample = oscillator.generate();

            sample_i = oscillator.getX() * transformer.outputSample_1;
            float sample_q = oscillator.getY() * transformer.outputSample_2;
            float sample_USB = sample_i - sample_q;
            float sample_LSB = sample_i + sample_q;

            // crossfade the two sideband signals
            sample_i = crossfade(sample_USB, sample_LSB, control);

            *(out++) = clip(sample_I);
        }
        */
    }
    static QuadraturePhaser* create(float sr, int buffer_size) {
        auto phaser = new QuadraturePhaser(sr);
        phaser->buffer1 = AudioBuffer::create(2, buffer_size);
        phaser->buffer2 = AudioBuffer::create(2, buffer_size);
        return phaser;
    }
    static void destroy(QuadraturePhaser* phaser) {
        AudioBuffer::destroy(phaser->buffer1);
        AudioBuffer::destroy(phaser->buffer2);
        delete phaser;
    }
};

#endif
