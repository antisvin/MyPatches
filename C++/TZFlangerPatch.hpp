/**
 *
 * This patch works as a digital emulation of tape-real based flanging units. This means that there
 * are 2 delay lines in used. First one has fixed duration. Second is being slowly modulated with
 * a sine LFO.
 * 
 * Feedback can be positive or negative
 * 
 * Flanger and circular buffer from OwlPatches repo were used as starting points.
 * CircularBuffer was converted more generic based on additions from code in Mutable Instruments repo
 * 
 */

////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 
 
 LICENSE:
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 */

/* created by the OWL team 2013 */

////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __TZFlangerPatch_hpp__
#define __TZFlangerPatch_hpp__

#include "DelayLine.hpp"

#ifdef OWL_MAGUS
#include "ScreenBuffer.h"
#endif

constexpr unsigned int FLANGER_BUFFER_SIZE = 2048;

using DryDelay = DelayLine<float, FLANGER_BUFFER_SIZE>;
using ModDelay = DelayLine<float, FLANGER_BUFFER_SIZE / 2>;

class TZFlangerPatch : public Patch {
private:
    DryDelay dryDelayBuffers[2];
    ModDelay modDelayBuffers[2];
    float rate, depth, mix, feedback, phase, phase2, mod2, mirror;
#ifdef OWL_MAGUS
    int8_t pos[2];
#endif
public:
  TZFlangerPatch(){
    for (int ch = 0; ch < 2; ch++) {
      dryDelayBuffers[ch].create();
      modDelayBuffers[ch].create();
    }
    registerParameter(PARAMETER_A, "Rate");     // modulation rate
    registerParameter(PARAMETER_B, "Depth");    // modulation depth
    registerParameter(PARAMETER_C, "Feedback"); // amount of output signal returned as input
    registerParameter(PARAMETER_D, "Mirror");   // this option toggles using inverse modulator for second channel
    phase = 0;
  }

  ~TZFlangerPatch(){
    for (int ch = 0; ch < 2; ch++) {
      DryDelay::destroy(dryDelayBuffers[ch]);
      ModDelay::destroy(modDelayBuffers[ch]);
    }
  }

  float modulate(float rate) {
    phase += rate;
    if (phase >= 1.0f) {
        phase -= 1.0f;
    }
    float phase2 = phase + 0.25;
    if (phase2 >= 1.0f) {
      phase2 -= 1.0f;
    }

    mod2 = sinf(phase2*(2*M_PI));    // sine function between -1..1
    return sinf(phase*(2*M_PI));     // sine function between -1..1
  };

  float getMod(float mod, float mirror_mode){
    float new_mod1, new_mod2;
    if (mirror_mode < 1.0f) {
      new_mod1 = -mod;
      new_mod2 = mod2;
    }
    else {
      new_mod1 = mod2;
      new_mod2 = mod;
      mirror_mode -= 1.0f;
    }
    return new_mod1 + (new_mod2 - new_mod1) * mirror_mode;
  }

  void processAudio(AudioBuffer &buffer){
    int size = buffer.getSize();
    float delay;
      
    rate     = getParameterValue(PARAMETER_A) * 0.000005f; // flanger needs slow rate
    depth    = getParameterValue(PARAMETER_B) * 0.5f;
    feedback = getParameterValue(PARAMETER_C) * 0.9f - 0.45f;
    mirror   = getParameterValue(PARAMETER_D) * 2;

    for (int i = 0 ; i < size; i++) {
      float mod = modulate(rate);
      for (int ch = 0; ch<buffer.getChannels(); ++ch) {
          float* buf = buffer.getSamples(ch);

          if (ch) {
            mod = getMod(mod, mirror);
#ifdef OWL_MAGUS
            pos[1] = mod * 24;
#endif
          }
#ifdef OWL_MAGUS
          else {
            pos[0] = mod * 24;
          }
#endif

          mod -= 2 * ch * mod * mirror; // modulation function for channel 2
          delay = (0.5f + depth * mod) * (FLANGER_BUFFER_SIZE - 1);
          float modSample = modDelayBuffers[ch].interpolate(delay);
          float drySample = dryDelayBuffers[ch].interpolate(FLANGER_BUFFER_SIZE / 2 - 1);
          buf[i] += feedback * modSample;
          dryDelayBuffers[ch].write(buf[i]); // update dry delay buffer
          modDelayBuffers[ch].write(buf[i]); // update modulated delay buffer
          buf[i] = (drySample - modSample) * 0.5f;
        }
    }
  }

#ifdef OWL_MAGUS
  void processScreen(ScreenBuffer& screen){
    screen.drawRectangle(28, 10, 8, 8, WHITE);
    screen.drawRectangle(28 + pos[0], 30, 8, 8, WHITE);
    screen.drawRectangle(28 + 64, 10, 8, 8, WHITE);
    screen.drawRectangle(28 + 64 + pos[1], 30, 8, 8, WHITE);
  }
#endif
    
};


#endif /* __FlangerPatch_hpp__ */

////////////////////////////////////////////////////////////////////////////////////////////////////
