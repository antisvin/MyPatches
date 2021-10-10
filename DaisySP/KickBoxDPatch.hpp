#ifndef __KickBoxDPatch_hpp__
#define __KickBoxDPatch_hpp__

#include "MonochromeScreenPatch.h"
#include "Sequence.h"
#include "MonochromeAudioDisplay.hpp"

/*
#undef min
#undef max
#undef abs
#undef sin
#undef cos
#undef exp
#undef sqrt
#undef pow
#undef log
#undef log10
*/

#include "daisysp.h"
using namespace daisysp;

#define KILL_REMAINING_PERFORMANCE_WITH_REVERB

//HiHat<RingModNoise> hat;
//ModalVoice voice;
//AnalogBassDrum kick;

class KickBoxDPatch : public MonochromeScreenPatch {
private:
  ModalVoice* voice;
  AnalogBassDrum kick;
  HiHat<> hat;
#ifdef KILL_REMAINING_PERFORMANCE_WITH_REVERB
  ReverbSc* reverb;
#endif
  Sequence<uint32_t> seq[5];
  uint8_t midinote;
  float mult;
public:
  KickBoxDPatch(){
    // Physical modeling voice
    registerParameter(PARAMETER_A, "Base Note");
    setParameterValue(PARAMETER_A, 0.3);
    registerParameter(PARAMETER_B, "Damping");
    setParameterValue(PARAMETER_B, 0.8);
    registerParameter(PARAMETER_C, "Structure");
    setParameterValue(PARAMETER_C, 0.45);
    registerParameter(PARAMETER_D, "Brightness");
    setParameterValue(PARAMETER_D, 0.5);
    registerParameter(PARAMETER_E, "Hat Tone");
    setParameterValue(PARAMETER_E, 0.8);
    registerParameter(PARAMETER_F, "Hat Decay");
    setParameterValue(PARAMETER_F, 0.5);
    registerParameter(PARAMETER_G, "Kick Tone");
    setParameterValue(PARAMETER_G, 0.4);
    registerParameter(PARAMETER_H, "Kick Decay");
    setParameterValue(PARAMETER_H, 0.25);
    registerParameter(PARAMETER_AA, "Root");
    setParameterValue(PARAMETER_AA, 0.126);
    registerParameter(PARAMETER_AB, "Hat Noise");
    setParameterValue(PARAMETER_AB, 0.5);
    registerParameter(PARAMETER_AC, "3rds");
    registerParameter(PARAMETER_AD, "5ths");
    registerParameter(PARAMETER_AE, "Hat Beats");
    setParameterValue(PARAMETER_AE, 0.51);
    registerParameter(PARAMETER_AF, "LFO1>");
    registerParameter(PARAMETER_AG, "Kick Beats");
    setParameterValue(PARAMETER_AG, 0.26);
//    registerParameter(PARAMETER_AH, "LFO2>");
    registerParameter(PARAMETER_BB, "Tempo");
    setParameterValue(PARAMETER_BB, 0.4);
    registerParameter(PARAMETER_BC, "LFO1");
    setParameterValue(PARAMETER_BC, 0.2);
    registerParameter(PARAMETER_BD, "LFO2");
    setParameterValue(PARAMETER_BD, 0.4);

#ifdef KILL_REMAINING_PERFORMANCE_WITH_REVERB    
    reverb = new ReverbSc();
    reverb->Init(getSampleRate());
#endif

    //hat = new HiHat<RingModNoise>();
    hat.Init(getSampleRate());
    //kick = new AnalogBassDrum();
    kick.Init(getSampleRate());
    kick.SetAttackFmAmount(0.0);
    kick.SetSelfFmAmount(0.0);
    voice = new ModalVoice();
    voice->Init(getSampleRate());
    midinote = 69;
    mult = getBlockSize() / getSampleRate();
  }

  ~KickBoxDPatch(){
//    delete hat;
//    delete kick;
    delete voice;
#ifdef KILL_REMAINING_PERFORMANCE_WITH_REVERB    
    delete reverb;
#endif
  }

  void noteOn(uint8_t note, uint16_t velocity, uint16_t samples){
    midinote = note;
    setParameterValue(PARAMETER_A, (note-20)/80.0);    
    //voice->SetAccent(velocity / 4095.0);
    //(4095.0f*2));
    voice->Trig();
    //voice->SetSustain(true);
  }

  void noteOff(uint8_t note, uint16_t samples){
    //voice->SetSustain(false);
  }

  void setParameters(float structure, float brightness, float damping, float pb){
    float freq = 440.0f*exp2f((midinote-69 + pb*2)/12.0);
    voice->SetFreq(freq);
    voice->SetStructure(structure);
    voice->SetBrightness(brightness);
    voice->SetDamping(damping);
  }

  void processAudio(AudioBuffer& buffer){
    FloatArray left = buffer.getSamples(LEFT_CHANNEL);
    FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

    // synth voice
    //float shape = getParameterValue(PARAMETER_BA)*2;
    float structure = getParameterValue(PARAMETER_C);
    float brightness = getParameterValue(PARAMETER_D);
    float damping = getParameterValue(PARAMETER_B);
    float pitchbend = 0;//getParameterValue(PARAMETER_G); // MIDI Pitchbend
    setParameters(structure, brightness, damping, pitchbend);
    for (int i = 0; i < getBlockSize(); i++){
      left[i] = voice->Process();
    }
    display.update(left, 2, 0.0, 3.0, 0.0);
    
    // hat + kick
    float tone = 200 * exp2f(getParameterValue(PARAMETER_E)*4);
    float decay = getParameterValue(PARAMETER_F);
    float accent = 0.9;//getParameterValue(PARAMETER_BA);
    hat.SetFreq(tone);
    hat.SetTone(getParameterValue(PARAMETER_E)*0.3 + 0.5);
    hat.SetDecay(decay);
    hat.SetAccent(accent);
    hat.SetNoisiness(getParameterValue(PARAMETER_AB));
    tone = 50*exp2f(getParameterValue(PARAMETER_G));
    decay = getParameterValue(PARAMETER_H);
    kick.SetTone(getParameterValue(PARAMETER_G)*0.2 + 0.5);
    kick.SetFreq(tone);
    kick.SetDecay(decay);
    kick.SetAccent(accent);
#ifdef KILL_REMAINING_PERFORMANCE_WITH_REVERB
    float out1, out2;
    for (int i = 0; i < getBlockSize(); i++){
      float tmp = kick.Process() * 2.4 + hat.Process() * 0.2;
      reverb->Process(left[i], left[i], &out1, &out2);
      left[i] = out1 * 0.25f + left[i] * 0.5 + tmp;
      right[i] = out2 * 0.25f + left[i] * 0.5 + tmp;
    }
    left.multiply(0.15);
    right.multiply(0.15);
#else
    for (int i = 0; i < getBlockSize(); i++){
      right[i] = kick.Process() + hat.Process() * 0.8;
      //left[i] = hat.Process() * 0.8;
    }
    //left.add(right);
    //left.multiply(0.15);
    //right.copyFrom(left);
#endif
  
    int steps = 16;
    int fills;
    // steps = getParameterValue(PARAMETER_D)*31;
    fills = getParameterValue(PARAMETER_AA)*steps;
    seq[0].calculate(steps, fills);    
    fills = getParameterValue(PARAMETER_AC)*steps;
    seq[1].calculate(steps, fills);
    fills = getParameterValue(PARAMETER_AD)*steps;
    seq[2].calculate(steps, fills);
    fills = getParameterValue(PARAMETER_AE)*steps;
    seq[3].calculate(steps, fills);
    fills = getParameterValue(PARAMETER_AG)*steps;
    seq[4].calculate(steps, fills);

    uint8_t basenote = getParameterValue(PARAMETER_A)*80+20;
    float tempo = getParameterValue(PARAMETER_BB)*16 + 0.5;
    static float lfo0 = 0;
    if(lfo0 > 1.0){
      lfo0 = 0;
      int note = basenote;
      if(seq[1].next())
        note += 5;
      if(seq[2].next())
        note += 7;
      if(seq[0].next() || note != basenote)
      	noteOn(note, 2047, 0);
      else
      	noteOff(note, 0);
      if(seq[3].next())
      	hat.Trig();
      if(seq[4].next())
      	kick.Trig();
    }
    lfo0 += tempo * mult;

    static float lfo1 = 0;
    if(lfo1 > 1.0){
      lfo1 = 0;
    }else{
    }
    // lfo1 += 2 * getBlockSize() / getSampleRate();
    tempo = getParameterValue(PARAMETER_BC)*4+0.01;
    lfo1 += tempo * mult;
    setParameterValue(PARAMETER_AF, lfo1*0.4);

    static float lfo2 = 0;
    if(lfo2 > 1.0){
      lfo2 = 0;
    }else{
    }
    // lfo2 += 1 * mult;
    tempo = getParameterValue(PARAMETER_BD)*4+0.01;
    lfo2 += tempo * getBlockSize() / getSampleRate();
    setParameterValue(PARAMETER_AH, lfo2*0.4);
  }

  MonochromeAudioDisplay display;

  void processScreen(MonochromeScreenBuffer& screen){
    display.draw(screen, WHITE);
   }
};

#endif   // __KickBoxDPatch_hpp__
