/**
 *
 * This patch works as a digital emulation of tape-real based flanging units. A
 * delay line with 2 read heads are used. First one has fixed duration. Second
 * is being slowly modulated with a sine LFO.
 *
 * Feedback can be positive or negative
 */

#include "SignalProcessor.h"
#include "InterpolatingCircularBuffer.h"
#include "SineOscillator.h"
#include "MonochromeScreenPatch.h"
#include "SmoothValue.h"

static constexpr size_t FLANGER_BUFFER_SIZE = 2048;

using Delay = InterpolatingCircularFloatBuffer<HERMITE_INTERPOLATION>;
// using FBProcessor = FeedbackProcessor<TZFlanger>;
// using MixProcessor = DryWetSignalProcessor<FBProcessor>;

template <typename LFO>
class StereoTZFlanger : public MultiSignalProcessor {
public:
    StereoTZFlanger() = default;
    StereoTZFlanger(Delay** delays, LFO* lfo, AudioBuffer* fb_buffer)
        : delays(delays)
        , lfo(lfo)
        , old_fb_amount(0)
        , new_fb_amount(0)
        , fb_buffer(fb_buffer) {
    }
    void setDelay(float delay) {
        fixed_delay = delay;
    }
    void setModDepth(float depth) {
        this->depth = depth;
    }
    void setModRate(float rate) {
        lfo->setFrequency(rate);
    }
    void setMix(float mix) {
        this->mix = mix;
    }
    void setFeedback(float fb_amount) {
        old_fb_amount = new_fb_amount;
        new_fb_amount = fb_amount;
    }
    float getModValue() const {
        return last_mod_value;
    }
    void process(AudioBuffer& input, AudioBuffer& output) {
        for (int ch = 0; ch < 2; ch++) {
            FloatArray fb_samples = fb_buffer->getSamples(ch);
            fb_samples.scale(old_fb_amount, new_fb_amount);
            FloatArray in = input.getSamples(ch);
            in.add(fb_samples);
            FloatArray out = output.getSamples(ch);
            Delay* delay = delays[ch];
            // First channel writes and reads without interpolation
            delay->delay(in.getData(), out.getData(), in.getSize(), (int)fixed_delay);
            // Second channel uses interpolated read from fractional delay line
        }
        output.multiply(1.0 - mix);
        float delay_pos = delays[0]->getWriteIndex() + delays[0]->getSize();
        float lfo_sample;
        for (size_t i = 0; i < input.getSize(); i++) {
            lfo_sample = lfo->generate();
            for (int ch = 0; ch < 2; ch++) {
                output.getSamples(ch)[i] +=
                    delays[ch]->readAt(
                        delay_pos - fixed_delay * (1.0 + lfo_sample * depth)) *
                    mix;
            }
            delay_pos += 1;            
        }
        last_mod_value = lfo_sample * depth;        
        fb_buffer->getSamples(0).copyFrom(output.getSamples(0));
        fb_buffer->getSamples(1).copyFrom(output.getSamples(1));
        #if 0
        float fb_incr = (new_fb_amount - old_fb_amount) * sample_incr;
        for (int ch = 0; ch < 2; ch++) {
            float fb = old_fb_amount;
            float prev_sample = fb_samples[ch];
            auto samples = output.getSamples(ch);
            for (size_t i = 0; i < output.getSize(); i++) {
                samples[i] += fb * prev_sample;
                prev_sample = samples[i];
                fb += fb_incr;
            }
            fb_samples[ch] = prev_sample;
            // fb_buffer->getSamples(ch).scale(old_fb_amount, new_fb_amount);
            // output.getSamples(ch).scale(1.0 - old_fb_amount, 1.0 - new_fb_amount);
        }
        #endif
        // output.add(*fb_buffer);
        // fb_buffer->copyFrom(output);
        //
    }
    static StereoTZFlanger* create(float sr, size_t buffer_size, size_t block_size) {
        Delay** delays = new Delay*[2];
        delays[0] = Delay::create(buffer_size);
        delays[1] = Delay::create(buffer_size);
        auto lfo = LFO::create(sr);
        auto flanger =
            new StereoTZFlanger(delays, lfo, AudioBuffer::create(2, block_size));
        flanger->setDelay(buffer_size / 2);
        return flanger;
    }
    static void destroy(StereoTZFlanger* flanger) {
        Delay::destroy(flanger->delays[0]);
        Delay::destroy(flanger->delays[1]);
        delete[] flanger->delays;
        SineOscillator::destroy(flanger->lfo);
        AudioBuffer::destroy(flanger->fb_buffer);
        delete flanger;
    }

private:
    LFO* lfo;
    Delay** delays;
    AudioBuffer* fb_buffer;
    float fixed_delay;
    float depth;
    float last_mod_value;
    float mix;
    float old_fb_amount, new_fb_amount;
    float fb_samples[2];
    float sample_incr;
};

class TZFlangerPatch : public MonochromeScreenPatch {
private:
    StereoTZFlanger<SineOscillator>* flanger;
    SmoothFloat rate;
    SmoothFloat depth;
    SmoothFloat feedback;
    SmoothFloat mix;

public:
    TZFlangerPatch() {
        registerParameter(PARAMETER_A, "Rate"); // modulation rate
        registerParameter(PARAMETER_B, "Depth"); // modulation depth
        registerParameter(PARAMETER_C,
            "Feedback"); // amount of output signal returned as input
        registerParameter(PARAMETER_D,
            "Mix"); // this option toggles using inverse modulator for second channel
        rate = SmoothValue(0.95, getParameterValue(PARAMETER_A) * 0.5f);
        depth = SmoothValue(0.95, getParameterValue(PARAMETER_B));
        feedback = SmoothValue(0.95, getParameterValue(PARAMETER_C) * 2 - 1.0);
        mix = SmoothValue(0.95, getParameterValue(PARAMETER_D));
        flanger = StereoTZFlanger<SineOscillator>::create(
            getSampleRate(), FLANGER_BUFFER_SIZE, getBlockSize());
    }

    ~TZFlangerPatch() {
        StereoTZFlanger<SineOscillator>::destroy(flanger);
    }

    void processScreen(MonochromeScreenBuffer& screen) {
        auto width = screen.getWidth();
        auto height = screen.getHeight();
        auto square_side = height / 8;
        screen.drawRectangle(width / 2 - square_side / 2,
            height / 2 - square_side * 2, square_side, square_side, WHITE);
        auto center_width = width / 2;
        float mod = flanger->getModValue();
        screen.drawRectangle((center_width - square_side / 2) * (1.0 + mod),
            height / 2, square_side, square_side, WHITE);
    }

    void processAudio(AudioBuffer& buffer) {
        rate = getParameterValue(PARAMETER_A) * 0.5f; // flanger needs slow rate
        depth = getParameterValue(PARAMETER_B);
        feedback = getParameterValue(PARAMETER_C) * 2 - 1.0;
        mix = getParameterValue(PARAMETER_D);

        flanger->setModRate(rate);
        flanger->setModDepth(depth);
        flanger->setMix(mix);
        flanger->setFeedback(feedback);
        flanger->process(buffer, buffer);
    }
};
