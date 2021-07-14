#ifndef __CONTOUR_HPP__
#define __CONTOUR_HPP__

#include "Envelope.h"

// Contour with a single parameter to control shape
// Borrowed from MI Elements more or less

class Contour : public Envelope {
public:
    using Envelope::gate;
    using Envelope::process;
    using Envelope::trigger;
    using SignalGenerator::generate;

    Contour()
        : trig(kGate), gateTime(-1) {
    }

    void setSampleRate(float sampleRate) {
        sr = sampleRate;
    }
    void setRate(float r) {
        incr = r / sr;
    }

    void setShape(float shape) {
        if (shape < 0.4f) {
            a = 0.15f + 0.75f * shape;
            s = 0;
        }
        else if (shape < 0.6f) {
            a = 0.45f;
            s = shape - 0.4f;
        }
        else {
            a = (1.f - shape) * 0.75 + 0.15;
            s = 1.f;
        }
        d = 1.8f * a;
        d_div = 1.f / d;
    }

    void trigger(bool state, int delay) {
        gate(state, delay);
        trig = kTrigger;
    }

    void gate(bool state) {
        gate(state, 0);
    }

    void gate(bool state, int delay) {
        if (gateState != state) {
            gateTime = delay;
            gateState = state;
        }
        trig = kGate;
    }

    float generate() override {
        if (gateTime == 0) {
            x = 0.f;
            if (trig == kTrigger) {
                gateState = false;
            }
        }
        if (gateTime >= 0) {
            gateTime--;
            return 0.f;
        }
        x += incr;
        return getLevel();
    }

protected:
    float a, d, s, d_div;
    float x;
    bool trig, gateState;
    int gateTime;
    enum EnvelopeTrigger { kGate, kTrigger };
    float sr, incr;
    float getLevel() {
        if (x < a) {
            return (x * x * x * x) / (a * a * a * a);
        }
        else if (x < a + d) {
            return 1.f + getE((x - a) * d_div) * (s - 1.f);
        }
        else {
            float t = a + d + 1.f;
            if (x < t + d) {
                return getE((x - t) * d_div) * (-s) + s;
            }
            return 0.f;
        }
    }
    inline float getE(float x) {
        return (1.f - powf(M_E, -4 * x)) * e_div;
    }
    static constexpr float e_div = 1.f / (1.f - M_E * M_E * M_E * M_E);
};

#endif
