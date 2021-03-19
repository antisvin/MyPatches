#ifndef __POLYGONAL_VCO_PATCH_HPP__
#define __POLYGONAL_VCO_PATCH_HPP__

#include "MonochromeScreenPatch.h"
#include "Oscillators/PolygonalOscillator.hpp"
#include "Oscillators/MultiOscillator.hpp"
#include "SineOscillator.h"
#include "VoltsPerOctave.h"
#include "DelayLine.hpp"
#include <algorithm>

#define BASE_FREQ 55.f
#define MAX_POLY 20.f
#define PREVIEW_FREQ 2.0f
#define SCREEN_WIDTH 128 // Hardcoded to make initialization easier
#define SCREEN_BUF_OVERLAP (SCREEN_WIDTH / 8)
#define SCREEN_PREVIEW_BUF_SIZE (SCREEN_WIDTH / 2 + SCREEN_BUF_OVERLAP)
#define MAX_FM_AMOUNT 0.2f
#define FM_AMOUNT_MULT (1.f / MAX_FM_AMOUNT)
#define SCREEN_OFFSET(x) ((x) * SCREEN_WIDTH / 4 - int(SCREEN_WIDTH / 16))
// #define OVERSAMPLE_PREVIEW


struct Point {
    Point() = default;
    Point(float x, float y) : x(x), y(y) {};
    float x, y;

    friend Point operator+(Point lhs, const Point& rhs) {
        return Point(lhs.x + rhs.x, lhs.y + rhs.y);
    }

    friend Point operator-(Point lhs, const Point& rhs) {
        return Point(lhs.x - rhs.x, lhs.y - rhs.y);
    }

    Point operator*(float val) {
        return Point(x * val, y * val);
    }    
};


class ModOscillator : public MultiOscillator {
public:
    ModOscillator(float freq, float sr = 48000)
        : MultiOscillator(3, freq, sr)
        , osc1(freq, sr / 2)
        , osc2(freq, sr)
        , osc3(freq, sr / 3) {
        addOscillator(&osc1);
        addOscillator(&osc2);
        addOscillator(&osc3);
    }
private:
    SineOscillator osc1, osc2, osc3;
};

class PolygonalVCOPatch : public MonochromeScreenPatch {
private:
    PolygonalOscillator2D osc;
    PolygonalOscillator2D osc_preview;
    ModOscillator mod;
    ModOscillator mod_preview;
    PolygonalOscillator2D::NPolyQuant quant;
    FloatArray preview;
    float fm_ratio;
    float fm_amount;
    VoltsPerOctave hz;
    DelayLine<Point, 64> preview_buf;
public:
    PolygonalVCOPatch()
        : hz(true)
        , osc(BASE_FREQ, getSampleRate())
        , osc_preview(1.0f, float(SCREEN_WIDTH / 2))
        , mod(BASE_FREQ, getSampleRate())
        , mod_preview(PREVIEW_FREQ, float(SCREEN_WIDTH / 2)) {
        registerParameter(PARAMETER_A, "Tune");
        registerParameter(PARAMETER_B, "Quantize");
        registerParameter(PARAMETER_C, "NPoly");
        registerParameter(PARAMETER_D, "Teeth");
        registerParameter(PARAMETER_E, "FM Amount");
        setParameterValue(PARAMETER_E, 0.0);
        registerParameter(PARAMETER_F, "FM Mod Shape");
        setParameterValue(PARAMETER_F, 0.0);
        registerParameter(PARAMETER_G, "Ext FM Amount");
        setParameterValue(PARAMETER_G, 0.0);
        preview = FloatArray::create(SCREEN_PREVIEW_BUF_SIZE);
        preview_buf = DelayLine<Point, 64>::create();
    }

    ~PolygonalVCOPatch(){
        FloatArray::destroy(preview);        
        DelayLine<Point, 64>::destroy(preview_buf);
    }

    void processScreen(MonochromeScreenBuffer& screen){
        osc_preview.setParams(
            quant,
            getParameterValue(PARAMETER_C),
            getParameterValue(PARAMETER_D));
        // New data gets added after previous cycle's tail
        FloatArray wave_array(preview.getData() + SCREEN_BUF_OVERLAP, SCREEN_PREVIEW_BUF_SIZE);
        float height_offset = screen.getHeight() * 0.3;
        float mod_prev = height_offset;
        mod_preview.reset();
        mod_preview.setMorph(fm_ratio);
        for (int i = 0; i < SCREEN_WIDTH / 2; i++) {
            // Mod VCO preview
            float mod_sample = mod_preview.generate() * fm_amount * FM_AMOUNT_MULT;
            float mod_new = (mod_sample + 1.0) * height_offset;            
            screen.drawVerticalLine(
                SCREEN_WIDTH / 2 + i,
                std::min(mod_prev, mod_new), std::max(1.0, abs(mod_new - mod_prev)),
                WHITE);
            mod_prev = mod_new;

            Point p = preview_buf.read(float(i) * 64.0 / 50.0);
            screen.setPixel(SCREEN_OFFSET(p.x + 1.0), SCREEN_OFFSET(p.y + 1), WHITE);
#ifdef OVERSAMPLE_PREVIEW
            p = preview_buf.interpolate((float(i) + 0.5)* 64.0 / 50.0);
            screen.setPixel(SCREEN_OFFSET(p.x + 1.0), SCREEN_OFFSET(p.y + 1), WHITE);
#endif
        }
        FloatArray::copy(preview, preview + SCREEN_WIDTH / 2, SCREEN_BUF_OVERLAP);
    }    
    void processAudio(AudioBuffer& buffer) {
        float ext_fm_amt = getParameterValue(PARAMETER_G) * MAX_FM_AMOUNT;
        hz.setTune(-3.0 + getParameterValue(PARAMETER_A) * 2.0);
        auto left = buffer.getSamples(LEFT_CHANNEL);
        auto right = buffer.getSamples(RIGHT_CHANNEL);
        hz.getFrequency(left);
        right.multiply(ext_fm_amt);
    
        // FM with dead zone
        fm_amount = getParameterValue(PARAMETER_E) * MAX_FM_AMOUNT;
        fm_ratio = getParameterValue(PARAMETER_F);
        osc.setParams(updateQuant(getParameterValue(PARAMETER_B)),
            getParameterValue(PARAMETER_C),
            getParameterValue(PARAMETER_D));

        for (int i = 0; i < buffer.getSize(); i++){
            float freq = left[i];
            osc.setFrequency(freq);
            mod.setMorph(fm_ratio);
            left[i] = osc.generate(mod.generate() * fm_amount + right[i]);
            right[i] = osc.getY();
            // Output phase
//            right[i] = osc.getPhase() / (M_PI * 2) + 0.5f;
        }
        //debugMessage("X, Y", osc.getX(), osc.getY());
        preview_buf.write(Point(osc.getX(), osc.getY()));
    }

    PolygonalOscillator2D::NPolyQuant updateQuant(float val) {
        /**
         * Quantize with dead zones at
         * 0.15 - 0.25
         * 0.35 - 0.45
         * 0.55 - 0.65
         * 0.75 - 0.85
         */
        if (val < 0.15f) {
            quant = PolygonalOscillator2D::NPolyQuant::NONE;
        }
        else if (val > 0.25) {
            if (val < 0.35) {
                quant = PolygonalOscillator2D::NPolyQuant::Q025;
            }
            else if (val > 0.45) {
                if (val < 0.55) {
                    quant = PolygonalOscillator2D::NPolyQuant::Q033;
                }
                else if (val > 0.65) {
                    if (val < 0.75) {
                        quant = PolygonalOscillator2D::NPolyQuant::Q050;
                    }
                    else if (val > 0.85) {
                        quant = PolygonalOscillator2D::NPolyQuant::Q100;
                    }
                }
            }
        }
        return quant;
    }
};

#endif
