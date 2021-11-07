#include "SignalProcessor.h"
#include "Window.h"

template <class FrequencyDomainProcessor, size_t fft_size>
class FFTProcessor : public SignalProcessor {
public:
    FFTProcessor(FastFourierTransform* fft, FrequencyDomainProcessor* processor,
        FloatArray timedomain,
        ComplexFloatArray freqdomain, FloatArray overlap, Window in_win, Window out_win)
        : fft(fft)
        , index(0)
        , timedomain(timedomain)
        , freqdomain(freqdomain)
        , overlap(overlap)
        , in_win(in_win)
        , out_win(out_win), processor(processor) {
        fft->init(fft_size);
    }
    FrequencyDomainProcessor* getBinsProcessor() {
        return processor;
    }
    float process(float input) {
        return 0.0;
    }
    void process(FloatArray input, FloatArray output) {
        size_t blocksize = input.getSize();
        in_win.apply(input);
        timedomain.copyFrom(input);
        fft->fft(timedomain, freqdomain);
        ComplexFloatArray bins = freqdomain.subArray(0, fft_size / 2);
        processor->process(bins, bins);
        fft->ifft(freqdomain, timedomain);
        out_win.apply(timedomain);
        output.copyFrom(timedomain.subArray(0, blocksize)); // use first half of output
        //output.copyFrom(timedomain); // use first half of output
        output.add(overlap);
        overlap.copyFrom(timedomain.subArray(blocksize, blocksize)); 
    }
    template <typename... Args>
    static FFTProcessor* create(size_t block_size,
        Window::WindowType in_win = Window::HannWindow,
        Window::WindowType out_win = Window::TriangularWindow, Args&&... args) {
        FrequencyDomainProcessor* processor = FrequencyDomainProcessor::create(std::forward<Args>(args)...);
        return new FFTProcessor(
            FastFourierTransform::create(fft_size), processor,
            FloatArray::create(fft_size), 
            ComplexFloatArray::create(fft_size),
            FloatArray::create(block_size),
            Window::create(in_win, block_size), Window::create(out_win, block_size * 2)
        );
    }
    static void destroy(FFTProcessor* fft) {
        FloatArray::destroy(fft->timedomain);
        FloatArray::destroy(fft->overlap);
        ComplexFloatArray::destroy(fft->freqdomain);
        //Window::destroy(fft->in_win);
        //Window::destroy(fft->out_win);
        FrequencyDomainProcessor::destroy(fft->processor);
        delete fft;
    }

protected:
    size_t index;
    FastFourierTransform* fft;
    FloatArray timedomain, overlap;
    ComplexFloatArray freqdomain;
    Window in_win;
    Window out_win;
    FrequencyDomainProcessor* processor;
};
