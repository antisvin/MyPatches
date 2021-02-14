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
// Wavetable is made in WaveEdit, uses reasonable bitdepth/SR
// #_64 is a special syntax that forces splitting - which we don't support yet
//wav2         = soundfile("Wavetable[url:WT.wav#_64]", 1);
//sample_sound = so.sound(wav2, 0);
// We can files in different formats in a single soundfile, because FAUST would transcode
// everything to floats. But only one part of soundfile can be played at any point of
// time. We don't want WT playback to interfere with percussion, so separate soundfiles
// must be used.

notes = (36, 40, 43, 47, 48, 47, 43, 40);
notes_per_bar = 4;
total_notes = 8;

base_freq = 440.0;
gain = 0.4;
gate_len = 0.1; // In seconds

process = kick , hat :> _, _
with {
    // Tempo
    tempo     = hslider("BPM[OWL:A]", 120, 60, 180, 0.1);
    wt_x      = hslider("WaveX[OWL:B]", 0, 0, 8, 0.001);
    wt_y      = hslider("WaveY[OWL:C]", 0, 0, 8, 0.001);    
    bar       = tempo : ba.tempo : *(notes_per_bar);
    
    //Phase is the place!
    phase_out = hbargraph("Phase>[OWL:F]", 0, 1);
    phasor     = (_ + 1.0 / bar : wrap) ~ _;
    wrap      = fmod(_, 1.0);
    trig      = (1.0 - _) <: _ > mem;
    //mkgate(t) = (t * gate_len) ~ * ba.pulse_countdown_loop(gate_len) > 0.01;
    mkgate    = en.ar(gate_len / 2, gate_len / 2, _) > 0;
    
    // Convert bar phase to triggers
    // Kick is triggered on first beat (1)
    kick_out  = hbargraph("Kick>[OWL:AA]", 0, 1);
    kick_trig = phasor : trig <: attach(_, mkgate : kick_out);
    // Hat is triggered on even beats (2, 4, 6, 8)
    hat_out  = hbargraph("Hat>[OWL:AB]", 0, 1);
    hat_trig  = phasor : *(4) : +(0.125) : wrap : trig <: attach(_, mkgate : hat_out);

    // Percussion samples playback
    kick      = kick_sound.play(gain, kick_trig) <: _, _;
    hat       = hat_sound.play(gain, hat_trig) : sp.panner(0.8);    

};

