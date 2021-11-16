#include "OpenWareLibrary.h"
#include "MonochromeScreenPatch.h"
#include "FFTProcessor.h"
#include "SmoothValue.h"
#include "WaveBank2D.hpp"
#include "daisysp.h"

#define FFT_SIZE 256
#define WAVE_SIZE 256
#define P_ROTATE PARAMETER_B
#define P_MORPH_X PARAMETER_C
#define P_MORPH_Y PARAMETER_D

#define P_COMP_RATIO PARAMETER_AA
#define P_COMP_THRESH PARAMETER_AB
#define P_COMP_ATTACK PARAMETER_AC
#define P_COMP_RELEASE PARAMETER_AD
#define P_COMP_MAKEUP PARAMETER_AE

using WaveBank = WaveBank2D<8, 8, 256>;
// using WaveFactory = WaveBank2DFactory<8, 8, 256>;

template <size_t X, size_t Y, size_t SIZE>
class ConvolutionFilter : public ComplexSignalProcessor {
public:
    ConvolutionFilter(ComplexFloatArray kernel)
        : kernel(kernel) {
    }
    void process(ComplexFloatArray input, ComplexFloatArray output) {
        // Convolve by kernel to filter in frequency domain
        input.complexByComplexMultiplication(kernel, output);
    }
    ComplexFloatArray getKernel() {
        return kernel;
    }
    ComplexFloat process(ComplexFloat input) {
        return ComplexFloat();
    }
    static ConvolutionFilter* create(ComplexFloatArray kernel) {
        return new ConvolutionFilter(kernel);
    }
    static void destroy(ConvolutionFilter* processor) {
        //ComplexFloatArray::destroy(processor->kernel);
        delete processor;
    }

private:
    ComplexFloatArray kernel;
};

using Processor = FFTProcessor<ConvolutionFilter<8, 8, 256>, FFT_SIZE, 256>;

class ConvolutionWavFilterPatch : public MonochromeScreenPatch {
public:
    static constexpr uint32_t X = 8;
    static constexpr uint32_t Y = 8;
    Processor* processors[2];
    WaveBank* fft_bank;
    FloatArray tmp, samples;
    SmoothFloat morph_x = SmoothFloat(0.98);
    SmoothFloat morph_y = SmoothFloat(0.98);
    SmoothInt rotation = SmoothInt(0.98);
    ComplexFloatArray kernel;
    daisysp::Compressor* compressor;
    bool automakeup = false;

    ConvolutionWavFilterPatch() {
        registerParameter(P_ROTATE, "Rotate");
        setParameterValue(P_ROTATE, 0.0);
        registerParameter(P_MORPH_X, "X");
        setParameterValue(P_MORPH_X, 0.5);
        registerParameter(P_MORPH_Y, "Y");
        setParameterValue(P_MORPH_Y, 0.5);

        registerParameter(P_COMP_RATIO, "Comp ratio");
        setParameterValue(P_COMP_RATIO, 2.0 / 40);
        registerParameter(P_COMP_THRESH, "Comp thresh");
        setParameterValue(P_COMP_THRESH, -12.0 / -80);
        registerParameter(P_COMP_ATTACK, "Comp attack");
        setParameterValue(P_COMP_ATTACK, 0.1);
        registerParameter(P_COMP_RELEASE, "Comp release");
        setParameterValue(P_COMP_RELEASE, 0.1);
        registerParameter(P_COMP_MAKEUP, "Comp makeup");
        setParameterValue(P_COMP_MAKEUP, 0.0);
        compressor = new daisysp::Compressor();
        compressor->Init(getSampleRate());
        compressor->AutoMakeup(false);

        Resource* resource = getResource("filter.wav");
        WavFile wav(resource->getData(), resource->getSize());
        if (!wav.isValid())
            error(CONFIGURATION_ERROR_STATUS, "Invalid wav");

        samples = wav.createFloatArray(0);
        fft_bank = WaveBank::create(samples);
        kernel = ComplexFloatArray::create(FFT_SIZE);
        processors[0] = Processor::create(
            getBlockSize(), Window::HannWindow, Window::HannWindow, kernel);
        processors[1] = Processor::create(
            getBlockSize(), Window::HannWindow, Window::HannWindow, kernel);
        Resource::destroy(resource);
        // FloatArray::destroy(samples);
        tmp = FloatArray::create(256);
    }
    ~ConvolutionWavFilterPatch() {
        Processor::destroy(processors[0]);
        Processor::destroy(processors[1]);
        ComplexFloatArray::destroy(kernel);
        WaveBank::destroy(fft_bank);
        FloatArray::destroy(samples);
        FloatArray::destroy(tmp);
        delete compressor;
    }
    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        switch (bid) {
        case BUTTON_A:
            if (value)
                automakeup = !automakeup;
            setButton(BUTTON_A, automakeup, 0);
            compressor->AutoMakeup(automakeup);
            break;
        default:
            break;
        }
    }
    void processScreen(MonochromeScreenBuffer& screen) {
        uint16_t step = kernel.getSize() / screen.getWidth();
        uint16_t height = screen.getHeight();
        const uint16_t offset = 18;
        height -= offset;

        int y_prev = height -
            std::min<uint16_t>(kernel[0].getMagnitude() * height, height - 1);
        int y_next;
        for (size_t i = 1; i < kernel.getSize() / step; i++) {
            y_next = height -
                std::min<uint16_t>(
                    kernel[i * step].getMagnitude() * height, height - 1);
            drawVerticalLine(screen, i, y_prev, y_next, WHITE);
            y_prev = y_next;
        }
    }
    void processAudio(AudioBuffer& buffer) {
        auto fft_processor = processors[0]->getFrequencyDomainProcessor();
        morph_x = getParameterValue(P_MORPH_X);
        morph_y = getParameterValue(P_MORPH_Y);
        updateKernel();
        FloatArray left = buffer.getSamples(0);
        FloatArray right = buffer.getSamples(1);
        processors[0]->process(left, left);
        processors[1]->process(right, right);

        compressor->SetRatio(1.0 + getParameterValue(P_COMP_RATIO) * 39);
        compressor->SetAttack(0.001 + getParameterValue(P_COMP_ATTACK) * 9.999);
        compressor->SetRelease(0.001 + getParameterValue(P_COMP_RELEASE) * 9.999);
        compressor->SetThreshold(getParameterValue(P_COMP_THRESH) * -80);
        if (!automakeup)
            compressor->SetMakeup(getParameterValue(P_COMP_MAKEUP) * 80);
        compressor->ProcessBlock(left.getData(), left.getData(), getBlockSize());
        compressor->ProcessBlock(right.getData(), right.getData(), getBlockSize());

    }
    void updateKernel() {
        float xf = morph_x * (X - 1);
        size_t xi = xf;
        xf -= xi;
        float yf = (morph_y * (Y - 1));
        size_t yi = yf;
        yf -= yi;
        FloatArray wave00 = fft_bank->getWave(xi, yi);
        FloatArray wave01 = fft_bank->getWave(xi, yi + 1);
        FloatArray wave10 = fft_bank->getWave(xi + 1, yi);
        FloatArray wave11 = fft_bank->getWave(xi + 1, yi + 1);
        size_t len = wave00.getSize();

        for (size_t i = 0; i < len; i++) {
            float s1 = wave00[i];
            s1 += (wave10[i] - s1) * xf;
            float s2 = wave01[i];
            s2 += (wave11[i] - s2) * xf;
            s1 += (s2 - s1) * yf;
            tmp[i] = s1;
        }


        for (size_t i = 0; i < len; i++) {
            float mag = tmp[i] * 0.5 + 0.5;
            float y_coord = sinf(M_PI * 2 * i / (len - 1));
            float x_coord = sqrtf(1.0 - y_coord * y_coord);
            kernel[i].re = mag * x_coord;
            kernel[i].im = mag * y_coord;
            // using sqrtf saves ~1% CPU on OWL2
            // kernel[i].setPolar(tmp[i] * 0.5 + 0.5, M_PI * 2 * i / (len - 1));
        }
        rotation = int(getParameterValue(P_ROTATE) * 512) % 256;
        std::rotate(&kernel[0], &kernel[rotation], &kernel[255]);
    }

private:
    uint16_t drawVerticalLine(MonochromeScreenBuffer& screen, uint16_t x,
        uint16_t y, uint16_t to, uint16_t c) {
        if (y > to)
            drawVerticalLine(screen, x, to, y, c);
        else
            do {
                screen.setPixel(x, y++, c);
            } while (y < to);
        return to;
    }
};
