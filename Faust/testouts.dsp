import("stdfaust.lib");

// An example of global metadata usage (see also
// https://faust.grame.fr/doc/manual/index.html#global-metadata ). There doesn't 
// seem to be a standard label intended for storing patch description, so
// we just use a custom label "message"
declare message "Patch D > A to\nmodulate LFOs";

// declare options "[midi:on]";
//Uncommenting the line above makes freq disappear from UI, but it becomes controllabel
// by MIDI.

// Currently using vbargraph or hbargraph for outputs makes no difference.
// But we could use different UI widgets once rendering to screen would be available
// from Faust.

// LFO1 is a sine wave
sineFreq = hslider("freq[OWL:A]", 1, -10, 10, 0.01);
sineOut = vbargraph("sine>[OWL:B]", -1.0, 1.0);

// LFO2 is a square, you can also change LFO output gain
sqrFreq = hslider("freq2[OWL:C]:", 0.5, 0, 10, 0.01);
sqrOut = vbargraph("square>[OWL:D]", 0.0, 1.0);
sqrOutGain = hslider("square gain[OWL:E]", 1, -1, 1, 0.01);

// We use "attach" to bypass audio - this way we won't send LFOs to outputs.
// See https://faust.grame.fr/doc/manual/index.html#attach-primitive
process = _ <: (attach(_, sineWave: sineOut) : attach(_, sqrWave: sqrOut))
with {
  sineWave = os.oscs(sineFreq);
  sqrWave = os.lf_squarewave(sqrFreq) * sqrOutGain;
};
