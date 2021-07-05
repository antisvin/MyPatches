#ifndef __WAVE_LOADER_HPP__
#define __WAV_LOADER_HPP__

#include "WavFile.h"
#include "OpenWareLibrary.h"


class WavLoader {
public:
    static FloatArray load(const char* name) {
        Resource* resource = Resource::load(name);
        if(resource == NULL) {
            error(CONFIGURATION_ERROR_STATUS, "Missing Resource");
            return FloatArray();
        }

        WavFile wav(resource->getData(), resource->getSize());
        if (!wav.isValid()) {
            error(CONFIGURATION_ERROR_STATUS, "Invalid wav");
            return FloatArray();
        }

        FloatArray array = wav.createFloatArray(0);
        Resource::destroy(resource);
        return array;
    }
};

#endif
