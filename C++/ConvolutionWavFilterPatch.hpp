#include "OpenWareLibrary.h"
#include "MonochromeScreenPatch.h"
#include "FFTProcessor.h"
#include "SmoothValue.h"
#include "WaveBank2D.hpp"

#define FFT_SIZE 256
#define WAVE_SIZE 256
#define P_MORPH_X PARAMETER_C
#define P_MORPH_Y PARAMETER_D

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
    ComplexFloatArray kernel;

    ConvolutionWavFilterPatch() {
        registerParameter(PARAMETER_C, "X");
        setParameterValue(PARAMETER_C, 0.5);
        registerParameter(PARAMETER_D, "Y");
        setParameterValue(PARAMETER_D, 0.5);

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
        morph_x = getParameterValue(PARAMETER_C);
        morph_y = getParameterValue(PARAMETER_D);
        updateKernel();
        processors[0]->process(buffer.getSamples(0), buffer.getSamples(0));
        processors[1]->process(buffer.getSamples(1), buffer.getSamples(1));
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
