#include "OpenWareLibrary.h"
#include "MonochromeScreenPatch.h"
#include "FFTProcessor.h"
#include "SmoothValue.h"

#define FFT_SIZE 512

class ConvolutionFilter : public ComplexSignalProcessor {
public:
    ConvolutionFilter(ComplexFloatArray kernel)
        : comb(0)
        , decay(0)
        , kernel(kernel) {
    }
    void process(ComplexFloatArray input, ComplexFloatArray output) {
        const int taps = 20;
        const size_t fft_size = kernel.getSize();

        kernel.clear();
        float amps[fft_size];
        //float t = (1.0 - decay);
        float t = 1.0 - decay;
        for (int j = 0; j < fft_size; j++) {
            amps[j] = t;
            t *= decay;
        }
#if 1
        // Build the kernel in Fourier space
        // Place taps at positions `comb * j`, with exponentially decreasing amplitude

        const float g = 0.9;
        //float g = decay * 2 - 1;

        ComplexFloat item(1.0);
        ComplexFloat incr;
        incr.setPolar(1.0, -2.0 * M_PI * comb);
        float norm = 0.0;

        const size_t mid = fft_size * (center * 0.5 + 0.25);

        for (int k = 0; k < mid; k++) {
            /*
            kernel[k].re = 0.5 *
                (sqrtf(1.0 + 2.0 * g * cosf(2.0 * M_PI * k * taps * comb / fft_size)) + g * g);
            kernel[k].im = 0.5 *
                (sqrtf(1.0 + 2.0 * g * sinf(2.0 * M_PI * k * taps * comb / fft_size)) + g * g);
            */

            float mag = sqrtf(1.0 + 2 * g * cosf(comb * -2.0 * M_PI * (mid - k) / fft_size * taps) + g * g) / 2;
            mag += (amps[(mid -k) * fft_size / mid] + 1.0 - mag) * (1.0 - center);
            norm += mag;
            kernel[k].setPolar(mag, item.getPhase());
            item *= incr;
        }

        for (int k = mid; k < fft_size; k++) {
            /*
            kernel[k].re = 0.5 *
                (sqrtf(1.0 + 2.0 * g * cosf(2.0 * M_PI * k * taps * comb / fft_size)) + g * g);
            kernel[k].im = 0.5 *
                (sqrtf(1.0 + 2.0 * g * sinf(2.0 * M_PI * k * taps * comb / fft_size)) + g * g);
            */

            float mag = sqrtf(1.0 + 2 * g * cosf(comb * -2.0 * M_PI * (k - mid) / fft_size * taps) + g * g) / 2;
            mag += (amps[(k - mid) * fft_size / (fft_size - mid)] + 1.0 - mag) * center;
            //mag *= 1.0 + (amps[(k - mid) * fft_size / (fft_size - mid)] - 1.0) * center;
            norm += mag;
            kernel[k].setPolar(mag, item.getPhase());
            item *= incr;
        }
        norm = float(fft_size) / norm * 0.5;
        for (int k = 0; k < fft_size; k++) {
            kernel[k].re *= norm;
            kernel[k].im *= norm;
        }

#else

        for (int j = 0; j < taps; j++) {
            ComplexFloat item(amps[j]);
            ComplexFloat incr;
            incr.im = sinf(-2.0 * M_PI * comb * j / fft_size * taps);
            incr.re = sqrtf(1.0 - incr.im * incr.im);
            //incr.re = cosf(-2.0 * M_PI * comb * j);
            //sinf(-2.0 * M_PI * comb * j);
            //incr.setPolar(1.0, -2.0 * M_PI * comb * j);
            for (int k = 0; k < fft_size; k++) {
                item *= incr;
                kernel[k] += item;
            }
            // kernel[k] *= amplitude;
        }

#endif
        // Convolve by kernel to filter in frequency domain
        input.complexByComplexMultiplication(kernel, output);
    }
    void setComb(float comb) {
        this->comb = comb;
    }
    void setDecay(float decay) {
        this->decay = decay;
    }
    void setCenter(float center) {
        this->center = center;
    }
    ComplexFloatArray getKernel() {
        return kernel;
    }
    ComplexFloat process(ComplexFloat input) {
        return ComplexFloat();
    }
    static ConvolutionFilter* create(size_t fft_size) {
        return new ConvolutionFilter(ComplexFloatArray::create(fft_size));
    }
    static void destroy(ConvolutionFilter* processor) {
        ComplexFloatArray::destroy(processor->kernel);
        delete processor;
    }

private:
    ComplexFloatArray kernel;
    float comb;
    float decay;
    float center;
};

using Processor = FFTProcessor<ConvolutionFilter, FFT_SIZE, 256>;

class ConvolutionFilterPatch : public MonochromeScreenPatch {
public:
    Processor* processor;
    SmoothFloat comb = SmoothFloat(0.99);
    ConvolutionFilterPatch() {
        registerParameter(PARAMETER_A, "Comb");
        setParameterValue(PARAMETER_A, 0.5);
        registerParameter(PARAMETER_B, "Decay");
        setParameterValue(PARAMETER_B, 0.5);
        registerParameter(PARAMETER_C, "Center");
        setParameterValue(PARAMETER_C, 0.5);
        processor = Processor::create(
            getBlockSize(), Window::HannWindow, Window::HannWindow, FFT_SIZE);
    }
    ~ConvolutionFilterPatch() {
        Processor::destroy(processor);
    }
    void processScreen(MonochromeScreenBuffer& screen) {
        ComplexFloatArray kernel =
            processor->getFrequencyDomainProcessor()->getKernel();
        uint16_t step = kernel.getSize() / screen.getWidth();
        uint16_t height = screen.getHeight();
        const uint16_t offset = 16;
        height -= offset;
        for (size_t i = 0; i < kernel.getSize() / step; i++) {
            screen.setPixel(i,
                height -
                    std::min<uint16_t>(
                        kernel[i * step].getMagnitude() * height, height - 1),
                WHITE);
        }
    }
    void processAudio(AudioBuffer& buffer) {
        auto fft_processor = processor->getFrequencyDomainProcessor();
        comb = getParameterValue(PARAMETER_A);
        fft_processor->setComb(comb);
        fft_processor->setDecay(getParameterValue(PARAMETER_B));
        fft_processor->setCenter(getParameterValue(PARAMETER_C));
        processor->process(buffer.getSamples(0), buffer.getSamples(0));
    }
};
