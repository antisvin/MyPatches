#ifndef __POLYGONAL_OSCILLATOR_HPP__
#define __POLYGONAL_OSCILLATOR_HPP__

/**
 * Ported from https://github.com/philippesalembier/SckitamVCV/blob/master/src/PolygonalVCO.cpp
 *
 * Original code by Philippe Salembier is distributed under terms of GPL v3 Licence
 */

#include <cmath>
#include "ComplexOscillator.h"

class PolygonalOscillator : public ComplexOscillator {
public:
    //    static constexpr float begin_phase = 0.f;
    //    static constexpr float end_phase = 1.0;
    enum NPolyQuant {
        NONE,
        Q025,
        Q033,
        Q050,
        Q100,
        NUM_NPOLYQUANT,
    };
    PolygonalOscillator()
        : mul(0.0)
        , nfreq(0.0)
        , phase(0)
        , nPolyQuant(NONE)
        , feedback() {
    }
    void setSampleRate(float sr) override {
        mul = M_PI * 2 / sr;
    }
    void reset() {
        phase = 0.f;
    }

    void setFrequency(float freq) override {
        nfreq = mul * freq;
    }
    float getFrequency() override {
        return nfreq / mul;
    }
    void setPhase(float phase) override {
        this->phase = phase / (M_PI * 2.0);
    }
    float getPhase() override {
        return phase * 2.0 * M_PI;
    }
    void setFeedback(float angle, float magnitude) {
        feedback.setPolar(magnitude, angle);
    }
    void setParams(NPolyQuant quant, float nPoly, float teeth) {
        nPolyQuant = quant;
        nPoly *= 20.0;
        switch (quant) {
        case NONE:
            nPoly = clamp(nPoly, 2.1f, 20.f);
            break;
        case Q025:
            nPoly = round(4.f * nPoly) / 4.f;
            nPoly = clamp(nPoly, 2.25f, 20.f);
            break;
        case Q033:
            nPoly = round(3.f * nPoly) / 3.f;
            nPoly = clamp(nPoly, 2.33333f, 20.f);
            break;
        case Q050:
            nPoly = round(2.f * nPoly) / 2.f;
            nPoly = clamp(nPoly, 2.5f, 20.f);
            break;
        case Q100:
            nPoly = round(nPoly);
            nPoly = clamp(nPoly, 3.f, 20.f);
            break;
        default:
            break;
        }
        this->nPoly = nPoly;

        // Adapt the teeth value to nPoly
        if (nPoly < 5.f) {
            teeth = teeth * (-0.03f * nPoly * nPoly + 0.4f * nPoly - 0.66f);
        }
        else {
            teeth = teeth * (-0.0019f * nPoly * nPoly + 0.07f * nPoly + 0.2875f);
        }
        this->teeth = teeth;
        gain = 0.5f - 0.25f * teeth;
    }
    ComplexFloat generate() {
        ComplexFloat sample;
        render<false>(1, nullptr, &sample);
        return sample;
    }
    ComplexFloat generate(float fm) {
        ComplexFloat sample;
        render<true>(1, &fm, &sample);
        return sample;
    }
    void generate(ComplexFloatArray output) {
        render<false>(output.getSize(), nullptr, output.getData());
    }
    void generate(ComplexFloatArray output, FloatArray fm) override {
        render<true>(output.getSize(), fm.getData(), output.getData());
    }
    // using ComplexOscillator::generate;

    static PolygonalOscillator* create(float sr) {
        auto osc = new PolygonalOscillator();
        osc->setSampleRate(sr);
        return osc;
    }

    static void destroy(PolygonalOscillator* poly) {
        delete poly;
    }

protected:
    template <bool with_fm>
    void render(size_t size, float* fm, ComplexFloat* out) {
        float _last_p = lastP;
        float _last_x = lastX;
        float _last_y = lastY;
        while (size--) {
            phase += nfreq;
            if (phase >= 1.0f)
                phase -= 1.f;
            else if (phase < 0.0f)
                phase += 1.f;

            if constexpr (with_fm) // Actually PM
                phase += nfreq * *fm++;

            modPhase += nfreq * nPoly;
            // Increment phase and check if there is a discontinuity, if
            // necessary compute data for PolyBlep and PolyBlam
            correction = 0.f;
            float phi = phase * 2.f * M_PI;
            if (modPhase >= 1.0f) {
                fracDelay = (modPhase - 1.f) / (modPhase - modPhasePrev);
                tmp1 = 2.f * M_PI * nfreq;
                PhiTr = phi - fracDelay * tmp1;
                if (teeth > 0.f) {
                    tmp2 = cos(M_PI / nPoly) *
                        (1.f / cos(-1.f * M_PI / nPoly + teeth) -
                            1.f / cos(M_PI / nPoly + teeth));
                    fx = cos(phi) * tmp2;
                    fy = sin(phi) * tmp2;
                }
                tmp2 = cos(M_PI / nPoly) *
                    (tan(teeth - (M_PI / nPoly)) / cos(teeth - (M_PI / nPoly)) -
                        tan(teeth + (M_PI / nPoly)) / cos(teeth + (M_PI / nPoly)));
                // tmp2 = -2.f * std::tan(M_PI/NPoly);
                fxprime = tmp2 * cos(PhiTr) * tmp1;
                fyprime = tmp2 * sin(PhiTr) * tmp1;
                correction = 1.0;
                modPhase -= floor(modPhase);
            }
            else if (modPhase < 0.0f) {
                fracDelay = modPhase / (modPhase - modPhasePrev);
                tmp1 = 2.f * M_PI * nfreq;
                PhiTr = phi - fracDelay * tmp1;
                if (teeth > 0.f) {
                    tmp2 = cos(M_PI / nPoly) *
                        (1.f / cos(-M_PI / nPoly + teeth) -
                            1.f / cos(M_PI / nPoly + teeth));
                    fx = -cos(phi) * tmp2;
                    fy = -sin(phi) * tmp2;
                }
                tmp2 = -1.f * cos(M_PI / nPoly) *
                    (tan(teeth - (M_PI / nPoly)) / cos(teeth - (M_PI / nPoly)) -
                        tan(teeth + (M_PI / nPoly)) / cos(teeth + (M_PI / nPoly)));
                // tmp2 = 2.f * std::tan(M_PI/NPoly);
                fxprime = tmp2 * cos(PhiTr) * tmp1;
                fyprime = tmp2 * sin(PhiTr) * tmp1;
                correction = 1.0;
                modPhase -= floor(modPhase);
            }

            // Create Polygon
            P = cos(M_PI / nPoly) / cos((2 * modPhase - 1) * M_PI / nPoly + teeth);
            x = cos(phi + _last_x * feedback.re) * P;
            y = sin(phi + _last_y * feedback.im) * P;
            _last_p = P;
            _last_x = x;
            _last_y = y;

            xout0 = 0.f;
            xout1 = x;
            yout0 = 0.f;
            yout1 = y;
            if (correction == 1.f) {
                d1 = fracDelay;
                d2 = d1 * d1;
                d3 = d2 * d1;
                d4 = d3 * d1;
                d5 = d4 * d1;

                // PolyBlep correction (if teeth is not zero)
                if (teeth > 0.f) {
                    h3 = 0.03871f * d4 + 0.00617f * d3 + 0.00737f * d2 + 0.00029f * d1;
                    h2 = -0.11656f * d4 + 0.14955f * d3 + 0.22663f * d2 +
                        0.18783f * d1 + 0.05254f;
                    h1 = 0.11656f * d4 - 0.31670f * d3 + 0.02409f * d2 +
                        0.62351f * d1 - 0.5f;
                    h0 = -0.03871f * d4 + 0.16102f * d3 - 0.25816f * d2 +
                        0.18839f * d1 - 0.05254f;

                    xout0 = xout0 + fx * h0;
                    xout1 = xout1 + fx * h1;
                    xout2 = xout2 + fx * h2;
                    xout3 = xout3 + fx * h3;

                    yout0 = yout0 + fy * h0;
                    yout1 = yout1 + fy * h1;
                    yout2 = yout2 + fy * h2;
                    yout3 = yout3 + fy * h3;
                }

                // PolyBlam Correction
                h0 = -0.008333 * d5 + 0.041667 * d4 - 0.083333 * d3 +
                    0.083333 * d2 - 0.041667 * d1 + 0.008333;
                h1 = 0.025 * d5 - 0.083333 * d4 + 0.333333 * d2 - 0.5 * d1 + 0.233333;
                h2 = -0.025 * d5 + 0.041667 * d4 + 0.083333 * d3 +
                    0.083333 * d2 + 0.041667 * d1 + 0.008333;
                h3 = 0.008333 * d5;

                xout0 = xout0 + fxprime * h0;
                xout1 = xout1 + fxprime * h1;
                xout2 = xout2 + fxprime * h2;
                xout3 = xout3 + fxprime * h3;

                yout0 = yout0 + fyprime * h0;
                yout1 = yout1 + fyprime * h1;
                yout2 = yout2 + fyprime * h2;
                yout3 = yout3 + fyprime * h3;
            }
            // Output
            xout = gain * xout3;
            yout = clamp(gain * yout3, -1.f, 1.f);

            // Keep values for the next iteration
            modPhasePrev = modPhase;
            xout3 = xout2;
            xout2 = xout1;
            xout1 = xout0;
            yout3 = yout2;
            yout2 = yout1;
            yout1 = yout0;

            // Set output values
            out->re = xout;
            out->im = yout;
            out++;
        }
        lastP = _last_p;
        lastX = _last_x;
        lastY = _last_y;
    }

    float nPoly = 0.f, P = 0.f;
    float phase = 0.f, nfreq = 0.f, mul = 0.f;
    float modPhase = 0.f, modPhasePrev = 0.f;
    NPolyQuant nPolyQuant = NONE;

    float x, y, tmp1, tmp2;
    float teeth = 0.f, fracDelay, PhiTr, fxprime, fyprime, fx, fy,
          correction = 0.f, gain = 1.f;
    float xout0, xout1, xout2, xout3, xout;
    float yout0, yout1, yout2, yout3, yout;

    float h0, h1, h2, h3, d1, d2, d3, d4, d5;
    float lastP = 0.f, lastX = 0.f, lastY = 0.f;
    ComplexFloat feedback {};
};

#endif
