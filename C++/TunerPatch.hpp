#include "OpenWareLibrary.h"
#include "MonochromeScreenPatch.h"

#define USE_FFT
#define FFT_SIZE 4096
#define MIN_FREQ 50
#define MAX_FREQ 1000
#define TUNING 440.f
#define ENV_THRESHOLD 0.1 // Amplitude detection threshold
#define ENV_ATTACK 10
#define ENV_RELEASE 20
#define RESOLUTION_MAX (65535)

const char* note_names[12] = {
    "C ",
    "C#",
    "D ",
    "D#",
    "E ",
    "F ",
    "F#",
    "G ",
    "G+",
    "A ",
    "A#",
    "B ",
};

class Tuning {
public:
    Tuning() = default;
    void setBase(float base) {
        this->base = base;
    }
    void setFrequency(float freq) {
        this->freq = freq;
        float _cents = 12.0 * log2f(freq / base) + 69;
        midi_note = _cents;
        _cents -= (int)_cents;
        cents = _cents * 100;
        if (cents > 50) {
            midi_note++;
            cents -= 100;
        }
    }
    int getOctave() const {
        return midi_note / 12 - 1;
    }
    uint8_t getMidiNote() const {
        return midi_note;
    }
    const char* getNoteName() const {
        return note_names[getMidiNote() % 12];
    }
    float getCents() const {
        return cents;
    }

private:
    float base, freq;
    int midi_note;
    float cents;
    SmoothFloat cents_smooth = 0;
};

#ifdef USE_FFT
using Detector = FourierPitchDetector;
#else
using Detector = ZeroCrossingPitchDetector;
#endif

/**
 * @brief Utility to extract envelope from audio
 * 
 * https://www.musicdsp.org/en/latest/Analysis/136-envelope-follower-with-different-attack-and-release.html
 * 
 */

class AmplitudeFollower : public SignalProcessor {
public:
    AmplitudeFollower()
        : threshold(0) {
    }
    AmplitudeFollower(float threshold, float mul)
        : threshold(threshold)
        , mul(mul)
        , last_env(0) {
    }
    using SignalProcessor::process;
    float process(float input) {
        float env_in = fabs(input);
        float env_out = last_env;
        if (env_out < env_in)
            env_out = env_in + attack * (env_out - env_in);
        else
            env_out = env_in + release * (env_out - env_in);

        if (env_out >= threshold)
            is_threshold = true;
        last_env = env_out;
        return env_out;
    }
    bool checkThreshold() {
        bool result = is_threshold;
        is_threshold = false;
        return result;
    }
    void setAttack(float attack_ms) {
        attack = expf(mul * attack_ms);
    }
    void setRelease(float release_ms) {
        release = expf(mul * release_ms);
    }
    static AmplitudeFollower* create(float sr, float threshold) {
        return new AmplitudeFollower(threshold, -1000.0 / sr);
    }
    static void destroy(AmplitudeFollower* amp) {
        delete amp;
    }

private:
    float rev_sr;
    bool is_threshold;
    float threshold;
    float attack, release, mul;
    float last_env;
};

class TuningEstimator {
public:
    /**
     * @brief Get the Estimate object
     *
     * Outputs value in [-1..1] range suitable for LED PWM
     * or display as a scale.
     *
     * For example, chromatic scale would give 0.6 for C+30 cents
     * or -0.6 for C + 70 cents.
     *
     * @param cents
     * @return float
     */
    float getEstimate(float cents) {
        float min_delta = 10000;
        float min_delta_abs = min_delta;
        if (num_notes) {
            // Match a tuning
            for (int i = 0; i < num_notes; i++) {
                float delta = cents - min_delta;
                float delta_abs = std::abs(delta);
                if (delta_abs < min_delta_abs) {
                    min_delta = delta;
                    min_delta_abs = delta_abs;
                }
            }
            if (min_delta < -1)
                min_delta = -1;
            else if (min_delta > 1)
                min_delta = 1;
        }
        else {
            // Chromatic
            min_delta = cents - (int)cents;
            if (min_delta > 0.5)
                min_delta -= 1;
            min_delta *= 2;
        }
        return min_delta;
    }

private:
    uint8_t* notes;
    uint8_t num_notes;
};

/**
 * @brief PWM LED control, borrowed from libDaisy:
 *  https://github.com/electro-smith/libDaisy/blob/master/src/hid/led.cpp
 *
 */
class PwmLed {
public:
    PwmLed() = default;
    PwmLed(bool invert, float samplerate)
        : invert(invert)
        , samplerate(samplerate) {
        bright = 0.0f;
        pwm_cnt = 0;
        set(bright);
        invert = invert;
        samplerate = samplerate;
        if (invert) {
            on = false;
            off = true;
        }
        else {
            on = true;
            off = false;
        }
    }
    void set(float val) {
        //bright = val * val * val; // Curve ends up too steep
        bright = val;
        pwm_thresh = bright * static_cast<float>(RESOLUTION_MAX);
    }
    bool get() {
        pwm += 120.f / samplerate;
        if (pwm > 1.f)
            pwm -= 1.f;
        return bright > pwm ? on : off;
    }

private:
    size_t pwm_cnt, pwm_thresh;
    float bright;
    float pwm;
    float samplerate;
    bool invert, on, off;
};

class TunerPatch : public MonochromeScreenPatch {
public:
    Detector* detector;
    AmplitudeFollower* follower;
    float frequency;
    Tuning tuning;
    TuningEstimator estimator;
    float estimate;
    PwmLed leds[2];
    bool buttons[2];
    TunerPatch()
        : frequency(0) {
        leds[0] = PwmLed(false, getBlockRate());
        leds[1] = PwmLed(false, getBlockRate());
        tuning.setBase(TUNING);
        follower = AmplitudeFollower::create(getSampleRate(), ENV_THRESHOLD);
        follower->setAttack(ENV_ATTACK);
        follower->setRelease(ENV_RELEASE);
#ifdef USE_FFT
        detector = new Detector(FFT_SIZE, getSampleRate());
        detector->setMinFrequency(MIN_FREQ);
        detector->setMaxFrequency(MAX_FREQ);
#else
        detector = new Detector(getSampleRate(), getBlockSize());
        detector->setLowPassCutoff(MAX_FREQ);
        detector->setHighPassCutoff(MIN_FREQ);
#endif
    }
    ~TunerPatch() {
        delete detector;
        AmplitudeFollower::destroy(follower);
    }
    void processScreen(MonochromeScreenBuffer& screen) override {
        screen.setTextSize(2);
        screen.setCursor(20, 20);
        if (frequency > 0.0)
            tuning.setFrequency(frequency);
        screen.print(tuning.getNoteName());
        screen.print(tuning.getOctave());
        int cents = tuning.getCents();
        screen.print(" ");
        if (cents > 0)
            screen.print("+");
        if (cents == 0)
            screen.print(" ");
        screen.print(cents);

        int width = screen.getWidth();
        screen.drawHorizontalLine(1, 32, width - 1, WHITE);
        for (int i = width / 8; i < width; i += width / 8) {
            screen.drawVerticalLine(i, 30, 4, WHITE);
        }
        screen.drawRectangle(width / 2 + width / 2 * estimate - 1, 28, 2, 8, WHITE);
    }
    void processAudio(AudioBuffer& buf) override {
        FloatArray left = buf.getSamples(0);
        FloatArray right = buf.getSamples(1);
        follower->process(left, right);
        right.copyFrom(left);
#ifdef USE_FFT
        if (detector->process(left)) {
            detector->computeFrequency();
            if (follower->checkThreshold())
                frequency = detector->getFrequency();
        }
#else
        detector->process(left);
        if (follower->checkThreshold())
            frequency = detector->getFrequency();
#endif

        if (frequency > 0.0) {
            estimate = estimator.getEstimate(tuning.getCents() / 100);
            if (estimate > 0) {
                leds[0].set(estimate);
                leds[1].set(0);
            }
            else {
                leds[0].set(0);
                leds[1].set(-estimate);
            }
        }

        debugMessage(tuning.getNoteName(), estimate);

        // Above note
        bool button = leds[0].get();
        setButton(GREEN_BUTTON, button, 0);
        setButton(BUTTON_2, button, 0);
        // below note
        button = leds[1].get();
        setButton(RED_BUTTON, button, 0);
        setButton(BUTTON_1, button, 0);
        //setParameterValue(PARAMETER_F, estimate < 0 ? -estimate : 0);
        //setParameterValue(PARAMETER_G, estimate > 0 ? estimate : 0);
    }
};
