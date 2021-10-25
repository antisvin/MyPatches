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
#include "daisysp.h"

static constexpr size_t FLANGER_BUFFER_SIZE = 2048;

using Delay = InterpolatingCircularFloatBuffer<HERMITE_INTERPOLATION>;
// using FBProcessor = FeedbackProcessor<TZFlanger>;
// using MixProcessor = DryWetSignalProcessor<FBProcessor>;

template <typename LFO>
class StereoTZFlanger : public MultiSignalProcessor {
public:
    StereoTZFlanger() = default;
    StereoTZFlanger(Delay** delays, LFO* lfo)
        : delays(delays)
        , lfo(lfo) {
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
    float getModValue() const {
        return last_mod_value;
    }
    void process(AudioBuffer& input, AudioBuffer& output) {
        for (int ch = 0; ch < 2; ch++) {
            auto in = input.getSamples(ch);
            // First channel writes and reads without interpolation
            delays[ch]->delay(in.getData(), output.getSamples(ch).getData(),
                in.getSize(), (int)fixed_delay);
            // Second channel uses interpolated read from fractional delay line
        }
        float delay_pos = delays[0]->getWriteIndex() + delays[0]->getSize();
        float lfo_sample;
        for (size_t i = 0; i < input.getSize(); i++) {
            lfo_sample = lfo->generate();
            for (int ch = 0; ch < 2; ch++) {
                output.getSamples(ch)[i] += delays[ch]->readAt(
                    delay_pos - fixed_delay * (1.0 + lfo_sample * depth));
            }
            delay_pos += 1;
        }
        output.multiply(0.5);
        last_mod_value = lfo_sample * depth;
    }
    static StereoTZFlanger* create(float sr, size_t buffer_size) {
        Delay** delays = new Delay*[2];
        delays[0] = Delay::create(buffer_size);
        delays[1] = Delay::create(buffer_size);
        auto lfo = LFO::create(sr);
        auto flanger = new StereoTZFlanger(delays, lfo);
        flanger->setDelay(buffer_size / 2);
        return flanger;
    }
    static void destroy(StereoTZFlanger* flanger) {
        Delay::destroy(flanger->delays[0]);
        Delay::destroy(flanger->delays[1]);
        delete[] flanger->delays;
        SineOscillator::destroy(flanger->lfo);
        delete flanger;
    }

private:
    LFO* lfo;
    Delay** delays;
    float fixed_delay;
    float depth;
    float last_mod_value;
};

// class ReverbProcessor : public

class FlangeVerbPatch : public MonochromeScreenPatch {
private:
    StereoTZFlanger<SineOscillator>* flanger;
    SmoothFloat rate;
    SmoothFloat depth;
    SmoothFloat reverb_fb;
    SmoothFloat reverb_lpf;
    SmoothFloat reverb_mix;
    daisysp::ReverbSc* reverb;

public:
    FlangeVerbPatch() {
        registerParameter(PARAMETER_A, "Rate"); // modulation rate
        setParameterValue(PARAMETER_A, 0.4);
        registerParameter(PARAMETER_B, "Depth"); // modulation depth
        setParameterValue(PARAMETER_B, 0.35);
        registerParameter(PARAMETER_C, "Reverb FB");
        setParameterValue(PARAMETER_C, 0.9);
        registerParameter(PARAMETER_D, "Reverb LPF");
        setParameterValue(PARAMETER_D, 0.7);
        registerParameter(PARAMETER_E, "Reverb Mix");
        setParameterValue(PARAMETER_E, 0.5);
        rate = SmoothValue(0.95, getParameterValue(PARAMETER_A) * 0.5f);
        depth = SmoothValue(0.95, getParameterValue(PARAMETER_B));
        reverb_fb = SmoothValue(0.95, getParameterValue(PARAMETER_C));
        reverb_lpf = SmoothValue(0.95, getParameterValue(PARAMETER_D));
        reverb_mix = SmoothValue(0.95, getParameterValue(PARAMETER_E));
        flanger = StereoTZFlanger<SineOscillator>::create(
            getSampleRate(), FLANGER_BUFFER_SIZE);
        reverb = new daisysp::ReverbSc();
        reverb->Init(getSampleRate());
    }

    ~FlangeVerbPatch() {
        StereoTZFlanger<SineOscillator>::destroy(flanger);
        delete reverb;
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
        reverb_fb = getParameterValue(PARAMETER_C);
        reverb_lpf = getParameterValue(PARAMETER_D);
        reverb_mix = getParameterValue(PARAMETER_E);

        float reverb_freq = powf(8, reverb_lpf) * getSampleRate() / 16;
        // float reverb_freq = 50.0 + 20000 * getParameterValue(PARAMETER_C);

        flanger->setModRate(rate);
        flanger->setModDepth(depth);
        flanger->process(buffer, buffer);

        reverb->SetFeedback(reverb_fb);
        reverb->SetLpFreq(reverb_freq);

        auto left = buffer.getSamples(0);
        auto right = buffer.getSamples(1);
        for (auto i = 0; i < buffer.getSize(); i++) {
            float left_sample = left[i];
            float right_sample = right[i];
            reverb->Process(left_sample, right_sample, &left_sample, &right_sample);
            left[i] += (left_sample - left[i]) * reverb_mix;
            right[i] += (right_sample - right[i]) * reverb_mix;
        }
    }
};
