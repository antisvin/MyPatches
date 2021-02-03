#ifndef __WAV_PARSER_HPP__
#define __WAV_PARSER_HPP__

#include "Resource.h"

/**
 * Recommended reading:
 * https://bleepsandpops.com/post/37792760450/adding-cue-points-to-wav-files-in-c
 * https://web.archive.org/web/20141226210234/http://www.sonicspot.com/guide/wavefiles.html#list
 */

static constexpr size_t sample_name_len  = 12;
static constexpr size_t max_samples      = 32;
static constexpr uint32_t headerRiffType = 0x57415645; // "WAVE"
static constexpr uint32_t adtlTypeId     = 0x6164746C; // "adtl"

/**
 * This is common header for all chunk objects
 */
struct BaseChunk {
    enum chunk_id : uint32_t {
        CI_HEADER  = 0x52494646, // "RIFF"
        CI_FORMAT  = 0x666D7420, // "fmt "
        CI_DATA    = 0x64617461, // "data"
        CI_CUE     = 0x63756520, // "cue "
        CI_LIST    = 0x4c495354, // "LIST" - Note that "list" (0x6C696E74) is also used, seems to be less common
        CI_LABEL   = 0x6C61626C, // "labl"
    };

	chunk_id chunkId;
	uint32_t dataSize;
};

/**
 * Wave header is top level chunk
 */
struct WaveHeader : public BaseChunk {
	char riffType[4];

    bool isValid() const {
        return (uint32_t)chunkId == headerChunkId && (uint32_t)riffType == headerRiffType;
    }
};

/**
 * Format description chunk
 */
struct FormatChunk : public BaseChunk {
    enum comp_code : uint16_t {
        CC_UNKNOWN      = 0x0000,
        CC_PCM          = 0x0001,
        CC_MS_ADPCM     = 0x0002,
        CC_IEEE_FLOAT   = 0x0003,
        CC_ALAW         = 0x0006,
        CC_ULAW         = 0x0007,
        CC_IMA_ADPCM    = 0x0011,
        CC_ITU_G723     = 0x0016,
        CC_GSM          = 0x0031,
        CC_ITU_G721     = 0x0040,
        CC_MPEG         = 0x0050,
        CC_EXPERIMENTAL = 0xFFFF,
    };
    enum sample_format : uint16_t {
        SF_UNKNOWN      = 0,
        SF_UINT8        = 8,
        SF_INT16        = 16,
        SF_INT24        = 24,
        SF_INT32        = 32,
    };
	comp_code compressionCode;
	uint16_t numberOfChannels;
	uint32_t sampleRate;
	uint32_t averageBytesPerSecond;
	uint16_t blockAlign;
	sample_format significantBitsPerSample;
};

/**
 * Cue chunk contains a list of cue points
 */
struct CueChunk : public BaseChunk {
	uint32_t cuePointsCount;
//	CuePoint *cuePoints;
};

struct CuePoint {
	uint32_t cuePointId;
	uint32_t playOrderPosition;
	chunk_id dataChunkId;
	uint32_t chunkStart;
	uint32_t blockStart;
	uint32_t frameOffset;
};

struct AssociatedList : public BaseChunk {
    uint32_t typeId;
    // Followed by variable number of bytes as list items
};


struct Label : public BaseChunk {
    uint32_t cuePointId;
    // Followed by variable number of bytes as label text
}


class Sample {
public:
    Sample(const Resource& resource, uint32_t sample_id)
        : resource(resource)
        , sample_id(sample_id) {}

    uin32_t getId() const {
        return sampleId;
    }

    void setName(const char* new_name) {
        strcpy(name, new_name, sample_name_len - 1);
    }
    
    void setOffset(uint32_t offset){
        startOffset = offset;
    }

    void setFormat(sample_format new_format){
        sample_format = format;
    }

    void setFloatSamples(){
        isFloat = true;
        sample_format = SF_INT32;
    }
    
    void setChannels(uint32_t num_channels){
        channels = num_channels;
    }

    template<typename Data>
    const Data* getData() {}

    const MemoryBuffer& loadData(){
        if (!isValidSample())
            return MemoryBuffer();
        MemoryBuffer buffer()
    }

    template<typename Data>
    void loadData(Data** dst) {

    }

private:
    uint32_t sampleId;
	uint32_t startOffset; // in bytes
	uint32_t size; // in bytes
    bool isFloat;
    uint32_t channels;
    sample_format format;
    char name[sample_name_len];

    bool isValidSample() {
        if (isFloat)
            return format == SF_UINT32;
        else
            return format == SF_UINT8 || format == SF_INT16 || format == SF_INT24 || format == SF_INT32;
    }

    uint32_t getSampleBytes() const {
        return format / 8;
    }

    uint32_t getFramesCount() const {
        return samples / channels / getSampleBytes();
    }

};


class WavFile {
public:
    WavFile() = default;

    ~WavFile(){
        for (uint32_t i = 0; i < samples_num; i++){
            delete samples[i];
        }
    }

    Sample* addSample(uint32_t sample_id, const Resource& resource) {
        auto sample = getSample(sample_id);
        if (sample == NULL && samples_num < max_samples) {
            // Sample not found, create a new one
            sample = new Sample(resource, sample_id);
            samples[samples_num++] = sample;
        }
    }

    Sample* getSample(uint32_t sample_id) const {
        for (uint32_t i = 0; i < samples_num; i++){
            if (samples[i]->getId() == sample_id)
                return samples[i];
        }
    }
private:
    size_t samples_num = 0;
    Sample* samples[max_samples];
}

class WavParser {
public:
    WavParser(const char* resource_name) {
        resource = Resource::open(resource_name);
    }
    ~WavParser(){
        if (resource != NULL)
            Resource::destroy(resource);
        if (wav_file != NULL)
            delete wav_file;
    }
    bool parse(){        
        if (resource == NULL){
            debugMessage("Resource not found");
            return false;
        }
        
        offset = 0;
        auto header = getChunk<WaveHeader>();
        if (!header.isValid()) {
            debugMessage("Invalid header");
            return false;
        }

        BaseChunk chunk;
        while (offset < header.dataSize + 8) {
            loadChunk<BaseChunk>(chunk);
            switch(chunk.chunkId) {
            case CI_FORMAT: {
                auto format_chunk = getChunk<WaveHeader>();
                if (format_chunk.compressionCode == CC_PCM) {
                    is_float = false;
                }
                else if (format_chunk.compressionCode == CC_IEEE_FLOAT){
                    is_float = true;
                }
                else {
                    debugMessage("Unsupported format");
                    return false;
                }
                offset += format_chunk.dataSize - 16; // Correction in case if extra format bytes are present
                break;
            }
            case CI_CUE: {
                auto cue_chunk = getChunk<CueChunk>();
                if (wav_file == NULL){
                    debugMessage("Malformed file");
                    return false;
                }
                for (uint32_t i = 0; i < cue_chunk.cuePointsCount; i++) {
                    CuePoint cue_point;
                    loadChunk<CuePoint>(cue_point);
                    offset += sizeof(CuePoint);
                    if (cue_point.dataChunkId != CI_DATA) {
                        continue;
                    }
                    else {
                        auto sample = wav_file->addSample(cue_point.cuePointId, resource);
                        if (sample != NULL){
                            sample->setOffset(cuePoint.frameOffset);
                        }
                    }
                }
                break;
            }
            case CI_LIST: {
                auto ass_list = getChunk<AssociatedList>();
                if (ass_list.typeId != adtlTypeId) {
                    // Skip lists of unknown types
                    offset += ass_list.dataSize - sizeof(AssociatedList.cuePointId);
                    continue;
                }
                // No need to do anything here - contents would be read as labels or ignored
                break;
            }
            case CI_LABEL: {
                auto label = getChunk<Label>();
                auto sample = wav_file.getSample(label.cuePointId);                
                uint32_t num_bytes = min(sample_name_len, label.dataSize);
                uint8_t buffer[num_bytes];
                resource->read(reinterpret_cast<void*>(buffer), num_bytes, offset);
                if (sample != NULL) {
                    sample->setName(reinterpret_cast<const char*>(buffer));
                }
                offset += label.dataSize - sizeof(Label.cuePointId);
                break;
            }
            case CI_DATA: {
                auto data = getChunk<Data>();
                wav_file.setOffset(offset);
                offset += data.dataSize;
                break;
                }
            default:
                // Unsupported chunk ID - just ignore it
                break;
            }

        }

    }

protected:
    /**
     * Load chunk to preallocated buffer without changing offset
     */
    template<typename ChunkType>
    void loadChunk(ChunkType& chunk){
        resource->read(reinterpet_cast<void*>(&chunk), sizeof(ChunkType), offset);
    }

    /**
     * Allocate object on the stack, read it from resource and increment offset
     */
    template<typename ChunkType>
    const ChunkType& getChunk(){
        ChunkType& chunk;
        resource->read(reinterpet_cast<void*>(&chunk), sizeof(ChunkType), offset);
        offset += sizeof(ChunkType);
        return chunk;
    }    
private:
    Resource* resource;
    WavFile* wav_file;
    size_t offset ;
    uint32_t sample_rate;
    uint32_t num_channels;
    bool is_float;
};

#endif
