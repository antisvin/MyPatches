#ifndef __MULTI_PROCESSOR_HPP__
#define __MULTI_PROCESSOR_HPP__

#include "SignalProcessor.h"

/**
 * Template class to convert SignalProcessor multi-channel version
 */
template<typename Processor, size_t num_channels>
class MultiProcessor : public MultiSignalProcessor {
public:
    void process(AudioBuffer& input, AudioBuffer& output) {
        for (size_t i = 0; i < num_channels; i++) {
            FloatArray array = input.getSamples(i);
            processors[i].process(array, array);
        }
    }
    Processor& get(size_t index) {
        return processors[index];
    }
protected:
    Processor processors[num_channels];
};

#endif
