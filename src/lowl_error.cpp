#include "../include/lowl_error.h"

LowlError::Code LowlError::get_error() {
    return error;
}

void LowlError::set_error(LowlError::Code p_error) {
    error = p_error;
}

bool LowlError::has_error() {
    return error != Code::NoError;
}

LowlError::LowlError() {
    error = Code::NoError;
}

int LowlError::to_error_code(LowlError::Code p_error) {
    return static_cast<int>(p_error);
}

std::string LowlError::to_error_text(LowlError::Code p_error) {
    switch (p_error) {
        case Code::NoError:
            return "NoError";
        case Code::Error:
            return "Error";

            // wav reader
        case Code::WavReaderUnsupportedAudioFormat:
            return "WavReaderUnsupportedAudioFormat";

            // Pa Error
        case Code::paNotInitialized:
            return "paNotInitialized";
        case Code::paUnanticipatedHostError:
            return "paUnanticipatedHostError";
        case Code::paInvalidChannelCount:
            return "paInvalidChannelCount";
        case Code::paInvalidSampleRate:
            return "paInvalidSampleRate";
        case Code::paInvalidDevice:
            return "paInvalidDevice";
        case Code::paInvalidFlag:
            return "paInvalidFlag";
        case Code::paSampleFormatNotSupported:
            return "paSampleFormatNotSupported";
        case Code::paBadIODeviceCombination:
            return "paBadIODeviceCombination";
        case Code::paInsufficientMemory:
            return "paInsufficientMemory";
        case Code::paBufferTooBig:
            return "paBufferTooBig";
        case Code::paBufferTooSmall:
            return "paBufferTooSmall";
        case Code::paNullCallback:
            return "paNullCallback";
        case Code::paBadStreamPtr:
            return "paBadStreamPtr";
        case Code::paTimedOut:
            return "paTimedOut";
        case Code::paInternalError:
            return "paInternalError";
        case Code::paDeviceUnavailable:
            return "paDeviceUnavailable";
        case Code::paIncompatibleHostApiSpecificStreamInfo:
            return "paIncompatibleHostApiSpecificStreamInfo";
        case Code::paStreamIsStopped:
            return "paStreamIsStopped";
        case Code::paStreamIsNotStopped:
            return "paStreamIsNotStopped";
        case Code::paInputOverflowed:
            return "paInputOverflowed";
        case Code::paOutputUnderflowed:
            return "paOutputUnderflowed";
        case Code::paHostApiNotFound:
            return "paHostApiNotFound";
        case Code::paInvalidHostApi:
            return "paInvalidHostApi";
        case Code::paCanNotReadFromACallbackStream:
            return "paCanNotReadFromACallbackStream";
        case Code::paCanNotWriteToACallbackStream:
            return "paCanNotWriteToACallbackStream";
        case Code::paCanNotReadFromAnOutputOnlyStream:
            return "paCanNotReadFromAnOutputOnlyStream";
        case Code::paCanNotWriteToAnInputOnlyStream:
            return "paCanNotWriteToAnInputOnlyStream";
        case Code::paIncompatibleStreamHostApi:
            return "paIncompatibleStreamHostApi";
        case Code::paBadBufferPtr:
            return "paBadBufferPtr";
            // PA Wrapper
        case Code::Pa_GetDeviceInfo:
            return "Pa_GetDeviceInfo";
    }

}

int LowlError::get_error_code() {
    return to_error_code(error);
}

std::string LowlError::get_error_text() {
    return to_error_text(error);
}
