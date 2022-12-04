import("stdfaust.lib");

MINUS60DB = 0.001;
MINUS4DB = 0.6309573;
MINUS3DB = 0.7079457;
MINUS2DB = 0.7943283;
MINUS1DB = 0.8912509;

make_channel(i) = leds : sp.spat(4, pan, distance)
with {
    j = i + 12;
    pan = hslider("Pan%i[OWL:%i]", i / 4, 0.0, 1.0, 0.001);
    distance = hslider("Distance%i[OWL:%j]", 1.0, 0.0, 1.0, 0.001);
    level = vbargraph("Level%i[OWL:%k]", 0.0, 1.0);
    rms = ba.slidingRMS(64); // supposed to be unstable with DC?
    rms_sqr = ba.if(rms < MINUS60DB, 0.0, ba.if(rms > MINUS4DB, 1.0, rms * rms));
    k = i + 8;
//    leds = _ <: attach(_, pan : vbargraph("Level%i[OWL:%k]", 0, 1)); // test
    leds = _ <: attach(_, rms_sqr : level);
};

panners = par(i, 4, make_channel(i));

process = panners :> _, _, _, _;

/*
  static constexpr float MINUS60DB = 0.001f;
  static constexpr float MINUS4DB = 0.6309573f;
  static constexpr float MINUS3DB = 0.7079457f;
  static constexpr float MINUS2DB = 0.7943283f;
  static constexpr float MINUS1DB = 0.8912509f;

        FloatArray samples = buffer.getSamples(i);
      samples.add(offsets[i]);
      float gain = getParameterValue(PatchParameterId(PARAMETER_A + i)) * 2;
      gains[i] = gain * gain;
      samples.multiply(gains[i]);
      float rms = samples.getRms();
#if 0
      // test input indicator:
      setParameterValue(PatchParameterId(PARAMETER_AA + i),
            getParameterValue(PatchParameterId(PARAMETER_A + i)));
      // red clip indicator:
      // setParameterValue(PatchParameterId(PARAMETER_AA + i),
      //     isButtonPressed(PatchButtonId(BUTTON_1 + i)) ? 1 : 0);
      // output indicator:
      setParameterValue(PatchParameterId(PARAMETER_BA + i),
            getParameterValue(PatchParameterId(PARAMETER_A + i)));
#else
      if (rms < MINUS60DB)
        rms = 0;
      else if (rms > MINUS1DB) // -1dB threshold
        rms = 1;         // set red led
      else
	rms = rms * rms;
      setParameterValue(PatchParameterId(PARAMETER_AA + i), rms);			
#endif
*/
