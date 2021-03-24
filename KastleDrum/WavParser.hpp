#ifndef __WAV_PARSER_HPP__
#define __WAV_PARSER_HPP__

#include <cstring>
#include "Resource.h"

/**
 * Recommended reading:
 * https://bleepsandpops.com/post/37792760450/adding-cue-points-to-wav-files-in-c
 * https://web.archive.org/web/20141226210234/http://www.sonicspot.com/guide/wavefiles.html#list
 */

#define min(a, b) (a < b ? a : b)

static constexpr size_t sample_name_len  = 12;
static constexpr size_t max_samples      = 32;
static constexpr uint32_t headerRiffType = 0x57415645; // "WAVE"
static constexpr uint32_t adtlTypeId     = 0x6164746C; // "adtl"

enum chunk_id : uint32_t {
    CI_HEADER  = 0x52494646, // "RIFF"
    CI_FORMAT  = 0x666D7420, // "fmt "
    CI_DATA    = 0x64617461, // "data"
    CI_CUE     = 0x63756520, // "cue "
    CI_LIST    = 0x4c495354, // "LIST" - Note that "list" (0x6C696E74) is also used, seems to be less common
    CI_LABEL   = 0x6C61626C, // "labl"
};

enum sample_format : uint16_t {
    SF_UNKNOWN      = 0,
    SF_UINT8        = 8,
    SF_INT16        = 16,
    SF_INT24        = 24,
    SF_INT32        = 32,
};

/**
 * This is a common header for all chunk objects
 */
struct ChunkHeader {
	chunk_id chunkId;
	uint32_t dataSize;
};

/**
 * Wave header is the top level chunk
 */
struct WaveHeader {
	char riffType[4];

    bool isValid() const {
        return (uint32_t)riffType == headerRiffType;
    }
};

/**
 * Format description chunk
 */
struct FormatChunk {
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

	comp_code compressionCode;
	uint16_t numberOfChannels;
	uint32_t sampleRate;
	uint32_t averageBytesPerSecond;
	uint16_t blockAlign;
	sample_format significantBitsPerSample;
};

/**
 * Cue chunk contains the list of cue points
 */
struct CueChunk {
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

struct AssociatedList {
    uint32_t typeId;
    // Followed by variable number of bytes as list items
};

struct Label {
    uint32_t cuePointId;
    // Followed by variable number of bytes as label text
};

class Sample {
public:
    Sample(Resource& resource, uint32_t sample_id)
        : resource(resource)
        , sample_id(sample_id) {}

    uin32_t getId() const {
        return sample_id;
    }

    void setName(const char* new_name) {
        strcpy(name, new_name, sample_name_len - 1);
    }
    
    void setOffset(uint32_t offset){
        start_offset = offset;
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

    template<typename Data>
    void loadData(Data** dst) {

    }
private:
    Resource& resource;
    uint32_t sample_id;
	uint32_t start_offset; // in bytes
	uint32_t size; // in bytes
    bool isFloat;
    uint32_t channels;
    sample_format format;
    char name[sample_name_len];

    bool isValidSample() {
        if (isFloat)
            return format == SF_INT32;
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
    WavFile(uint8_t num_samples)
        : num_samples(num_samples) {
            samples = new Sample[num_samples];
        }

    ~WavFile(){
        for (uint32_t i = 0; i < samples_num; i++){
            delete samples[i];
        }
    }

    Sample* addSample(const char* name, size_t length, size_t channels, void* src) {
        if (last_sample < num_samples) {
            Sample* sample = Sample::create(length, channels);
            sample->setName(name);
            sample->loadData(src);
            samples[samples_num++] = sample;
            return sample;
        }
        else {
            return NULL;
        }
    }

    Sample* getSample(uint32_t sample_id) const {
        for (uint32_t i = 0; i < samples_num; i++){
            if (samples[i]->getId() == sample_id)
                return samples[i];
        }
    }
private:
    size_t samples_num;
    size_t last_sample = 0;
    Sample* samples;
};

class CuePointEntry {
public:
    const char* name;
    size_t offset;
};

/**
 * Chunk pointer
 * 
 * @param offset - offset for data belonging to this chunk
 * @param size - data size
 */
class ChunkPointer {
public:
    ChunkPointer();
    ChunkPointer(const ChunkHeader& header, uint32_t offset)
        : offset(offset)
        , size(header.dataSize) {}
    void loadData(void* dst, Resource& resource) {
        resource.read(dst, size, offset);
    }
    bool isFound(){
        return size > 0;
    }
private:
    uint32_t offset = 0;
    uint32_t size = 0;
};

template<bool use_cues, size_t max_cues = 256>
class WavParser {
public:
    Sample* data = NULL;
    Sample* cues[] = NULL;

    WavParser(const char* resource_name) {
        resource = Resource::open(resource_name);
        if (use_cues)
            cues = new Sample[max_cues];
    }
    ~WavParser(){
        if (resource != NULL)
            Resource::destroy(resource);
        if (wav_file != NULL)
            delete wav_file;
    }
    /**
     * Override this to add extra chunks support
     */
    virtual handleUnknownChunk(ChunkHeader chunk){
        skip(chunk.dataSize);
    }
    const FormatChunk* loadFormatChunk(){
        if (!format_chunk.isFound())
            return NULL;
        // Read format chunk.
        const FormatChunk* fchunk = new FormatChunk();
        format_chunk.loadData(fchunk, resource);
        // Check int/float format. Compressed formats are not supported.
        if (format_chunk->compressionCode == CC_PCM) {
            is_float = false;
        }
        else if (format_chunk.compressionCode == CC_IEEE_FLOAT){
            is_float = true;
        }
        else {
            debugMessage("Unsupported format");
            delete fchunk;
            return NULL;
        }
        return format_chunk;
    }
    /*
     * Loads whole data into a single sample
     */
    const Sample* loadData(){
    }
    /*
     * Load sample from a cue point by index
     */
    const Sample* loadSample(uint8_t cue_id){

    }
    /*
     * Load sample from a cue point by name
     */
    const Sample* loadSample(const char* name){        
    }
    bool parseHeader(){
        if (resource == NULL){
            debugMessage("Resource not found");
            return false;
        }
        
        offset = 0;
        loadChunk(header_chunk);

        if (header_chunk.chunkId == CI_HEADER && header_chunk.dataSize == sizeof(WaveHeader)
            header = (const WaveHeader*)getChunk()
        if (header == NULL || !header->isValid()) {
            debugMessage("Invalid header");
            return false;
        }
        return true;
    }
    bool parseCuePoints(){

    }
    bool parseData(){
        
    }
    bool parseChunks(){        
        bool header_parsed = parseHeader();
        if (!header_parsed)
            return false;

        ChunkHeader chunk_header;
        do {
            loadHeader(chunk_header);
            switch(chunk_header.chunkId) {
            case CI_FORMAT: {
                format_chunk = ChunkPointer(chunk_header, offset);
                skip(chunk_header.dataSize);
                break;
            }
            case CI_CUE: 
                if (use_cues) {
                    cue_list_chunk = ChunkPointer(chunk_header, offset);
                    CueChunk cue_chunk;
                    loadChunk(cue_chunk);
                    cue_list_size = min(max_samples, cue_chunk.cuePointsCount);                    
                    skip(cue_chunk.cuePointsCount * sizeof(CuePoint));
                }
                break;
            case CI_LIST:
                if (use_cues) {
                    AssociatedList adtl;
                    loadChunk(adtl);
                    if (adtl.AssociatedListTypeId == adtlTypeId) {
                        // Only set chunk pointer if expected type is found
                        label_list_chunk = ChunkPointer(chunk_header, offset);
                    }
                    skip(chunk_header.dataSize - sizeof(AssociatedList);
                }
                else {
                    skip(cue_chunk.dataSize);
                }
                break;
            case CI_LABEL:
                if (use_cues) {
                    /*
                    auto* label = getChunk<Label>();
                    auto sample = wav_file.getSample(label->cuePointId);                
                    uint32_t num_bytes = min(sample_name_len, chunk.dataSize);
                    uint8_t buffer[num_bytes];
                    resource->read(reinterpret_cast<void*>(buffer), num_bytes, offset);
                    if (sample != NULL) {
                        sample->setName(reinterpret_cast<const char*>(buffer));
                    }
                    offset += chunk.dataSize - sizeof(Label.cuePointId);
                    */
                }
                else {
                    skip(cue_chunk.dataSize);
                }
                break;
            case CI_DATA:
                data_chunk = ChunkPointer(chunk_header, offset);
                skip(cue_chunk.dataSize);
                break;
            default:
                // Unsupported chunk ID - just ignore it
                handleUnknownChunk(chunk_header);
                break;
            }

        } while (offset < header->dataSize + sizeof(ChunkHeader));
    }
    static WavParser* create(const char* resource_name) {
        WavParser* parser = new WavParser(resource_name);
        if (parser->resource->hasData())
            header = new WaveHeader();
    }
    static void destroy(WavParser* parser){
        /*
         * Destroy loaded data.
         */
        if (parser->sample != NULL)
            delete sample;
        if (parser->cue_samples != NULL)
            delete[] cue_samples;
    }

protected:
    /**
     * Load chunk to preallocated buffer without changing offset
     */
    template<typename Chunk>
    void loadChunk(Chunk& chunk){
        offset += resource->read(reinterpet_cast<void*>(&chunk), sizeof(Chunk), offset);
    }
    /**
     * Allocate object on the stack, read it from resource and increment offset
     */
    template<typename Chunk>
    void* getChunk(){
        Chunk* chunk = new Chunk();
        offset += resource->read((void*)chunk, sizeof(Chunk), offset);
        return chunk;
    }
    /**
     * Rewind offset forward
     */
    void skip(size_t size){
        offset += size;
    }
    /**
     * Parse cue points
     */
    uin8_t parseCuePoints(){
        for (int i = 0; i < cue_list_size; i++){
            
        }

                        auto* cue_chunk = getChunk<CueChunk>();
                    for (uint32_t i = 0; i < cue_chunk->cuePointsCount; i++) {
                        CuePoint cue_point;
                        loadChunk<CuePoint>(cue_point);
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
private:
    Resource* resource = NULL;
    WavFile* wav_file = NULL;
    WaveHeader* header;
    CueList* cue_points;
    ChunkPointer header_chunk;
    ChunkPointer format_chunk;
    ChunkPointer cue_list_chunk;
    ChunkPointer label_list_chunk;
    size_t offset ;
    uint32_t sample_rate;
    uint32_t num_channels;
    uin8_t cue_list_size = 0;
    bool is_float;
};

typedef WavParser<false> SimpleWavParser;
typedef WavParser<true> CueWavParser;

#endif
