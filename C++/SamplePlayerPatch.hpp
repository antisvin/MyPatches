#include "OpenWareLibrary.h"
#include "SamplePlayer.hpp"
#include "WavLoader.hpp"
#include "MonochromeScreenPatch.h"

#define GRID_SIZE 16
#define P_INDEX PARAMETER_A
#define P_TEMPO PARAMETER_B


using Player = SamplePlayer<COSINE_INTERPOLATION, CROSSFADE_PARABOLIC, 64, GRID_SIZE>;
//using Player = SamplePlayer<LINEAR_INTERPOLATION, CROSSFADE_LINEAR, 64>;

const char* player_states[] = {
    "None",
    "Fade in",
    "Fade out",
    "Crossfade",
    "Playing"
};

class SamplePlayerPatch : public MonochromeScreenPatch {
public:
    Player* player;
    FloatArray sample_buf;
    AdjustableTapTempo* tempo;
    SamplePlayerPatch() {
        registerParameter(P_INDEX, "Loop point");
        registerParameter(P_TEMPO, "Tempo");
        sample_buf = WavLoader::load("breaks/jungle2.wav");
        player = Player::create(getSampleRate(), sample_buf, 4.0);
        tempo = AdjustableTapTempo::create(getSampleRate(), 1 << 22);
        player->setLooping(true);
        // player->trigger();
    }
    ~SamplePlayerPatch() {
        Player::destroy(player);
        FloatArray::destroy(sample_buf);
        AdjustableTapTempo::destroy(tempo);
    }
    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        bool set = value != 0;
        switch (bid) {
        case BUTTON_A:
            tempo->trigger(set, samples);
            break;
        case BUTTON_B:
            if (value)
                player->trigger();
            break;
        }
    }
    void processScreen(MonochromeScreenBuffer& screen) override {
        screen.print(1, 10, "State=");
        screen.print(player_states[(int)player->getState()]);
        screen.print(1, 20, "Pos=");
        screen.print(player->getPosition());
        screen.print(1, 30, "BPM=");
        screen.print(tempo->getBeatsPerMinute());
    }
    void processAudio(AudioBuffer& buffer) {
        tempo->clock(buffer.getSize());
        tempo->adjust(getParameterValue(P_TEMPO) * 4096);
        player->setDuration(tempo->getPeriodInSamples());
        player->setLoopPoint(getParameterValue(P_INDEX) * GRID_SIZE);
        //player->setBPM(tempo->getBeatsPerMinute());
        FloatArray left = buffer.getSamples(0);
        player->generate(left);
        buffer.getSamples(1).copyFrom(left);
    }
};
