#ifndef LOWL_AUDIO_READER_H
#define LOWL_AUDIO_READER_H

#include <string>
#include <memory>

#include "../src/lowl_buffer.h"
#include "../src/lowl_file.h"
#include "lowl_audio_stream.h"
#include "lowl_error.h"

class LowlAudioReader {

private:
	std::string file_path;
	std::string file_extension;

protected:

public:
	virtual std::unique_ptr<LowlAudioStream> read_buffer(const std::unique_ptr<LowlBuffer>& p_buffer, LowlError &error) = 0;
	std::unique_ptr<LowlAudioStream> read_ptr(void* p_buffer, uint32_t p_length, LowlError &error);
	std::unique_ptr<LowlAudioStream> read_file(const std::string& p_path, LowlError &error);

	LowlAudioReader();
    virtual ~LowlAudioReader();
};

#endif
