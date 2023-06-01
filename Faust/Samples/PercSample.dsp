import("stdfaust.lib");

// Simple syntax for loading a single file
//wav1 = soundfile("test[url:test.wav]",2);
// Full syntax for multiple files
//wav1 = soundfile("test[url:{'test1.wav';'test2.wav'}]",2);
// We support both!

wav1         = soundfile("Percussion[url:{'KICK.wav';'HAT.wav'}]", 1);
// Percussion samples from KastleDrum are LoFi beyond ridiculous
kick_sound   = so.sound(wav1, 0);
hat_sound    = so.sound(wav1, 1);

process = kick , hat :> _, _
with {
    // Kick sample
    kick_gain = hslider("Kick Gain[OWL:A]", 0.5, 0.0, 1.0, 0.001);
    kick_trig_cv = hslider("Kick Trig by CV[OWL:C]", 0.0, 0.0, 1.0, 0.001) > 0.1;
    kick_trig_but = button("Kick Trig Gate[OWL:B1]");
    kick_trig = kick_trig_cv | kick_trig_but;

    // Hat sample
    hat_gain = hslider("Hat Gain[OWL:B]", 0.5, 0.0, 1.0, 0.001);
    hat_trig_cv = hslider("Hat Trig[OWL:D]", 0.0, 0.0, 1.0, 0.001) > 0.1;
    hat_trig_but = button("Hat Trig Gate[OWL:B2]");
    hat_trig = hat_trig_cv | hat_trig_but;

    // Percussion samples playback
    kick      = kick_sound.play(kick_gain, kick_trig) <: _, _;
    hat       = hat_sound.play(hat_gain, hat_trig) : sp.panner(0.8);    
};

