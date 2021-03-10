#include "lowl_error.h"

Lowl::ErrorCode Lowl::LowlError::get_error() {
    return error;
}

void Lowl::LowlError::set_error(Lowl::ErrorCode p_error) {
    error = p_error;
}

bool Lowl::LowlError::has_error() {
    return error != ErrorCode::NoError;
}

Lowl::LowlError::LowlError() {
    error = ErrorCode::NoError;
}

int Lowl::LowlError::to_error_code(Lowl::ErrorCode p_error) {
    return static_cast<int>(p_error);
}

std::string Lowl::LowlError::to_error_text(Lowl::ErrorCode p_error) {
    switch (p_error) {
        case ErrorCode::NoError:
            return "NoError";
        case ErrorCode::Error:
            return "Error";

            // wav reader
        case ErrorCode::WavReaderUnsupportedAudioFormat:
            return "WavReaderUnsupportedAudioFormat";

            // Pa Error
        case ErrorCode::paNotInitialized:
            return "paNotInitialized";
        case ErrorCode::paUnanticipatedHostError:
            return "paUnanticipatedHostError";
        case ErrorCode::paInvalidChannelCount:
            return "paInvalidChannelCount";
        case ErrorCode::paInvalidSampleRate:
            return "paInvalidSampleRate";
        case ErrorCode::paInvalidDevice:
            return "paInvalidDevice";
        case ErrorCode::paInvalidFlag:
            return "paInvalidFlag";
        case ErrorCode::paSampleFormatNotSupported:
            return "paSampleFormatNotSupported";
        case ErrorCode::paBadIODeviceCombination:
            return "paBadIODeviceCombination";
        case ErrorCode::paInsufficientMemory:
            return "paInsufficientMemory";
        case ErrorCode::paBufferTooBig:
            return "paBufferTooBig";
        case ErrorCode::paBufferTooSmall:
            return "paBufferTooSmall";
        case ErrorCode::paNullCallback:
            return "paNullCallback";
        case ErrorCode::paBadStreamPtr:
            return "paBadStreamPtr";
        case ErrorCode::paTimedOut:
            return "paTimedOut";
        case ErrorCode::paInternalError:
            return "paInternalError";
        case ErrorCode::paDeviceUnavailable:
            return "paDeviceUnavailable";
        case ErrorCode::paIncompatibleHostApiSpecificStreamInfo:
            return "paIncompatibleHostApiSpecificStreamInfo";
        case ErrorCode::paStreamIsStopped:
            return "paStreamIsStopped";
        case ErrorCode::paStreamIsNotStopped:
            return "paStreamIsNotStopped";
        case ErrorCode::paInputOverflowed:
            return "paInputOverflowed";
        case ErrorCode::paOutputUnderflowed:
            return "paOutputUnderflowed";
        case ErrorCode::paHostApiNotFound:
            return "paHostApiNotFound";
        case ErrorCode::paInvalidHostApi:
            return "paInvalidHostApi";
        case ErrorCode::paCanNotReadFromACallbackStream:
            return "paCanNotReadFromACallbackStream";
        case ErrorCode::paCanNotWriteToACallbackStream:
            return "paCanNotWriteToACallbackStream";
        case ErrorCode::paCanNotReadFromAnOutputOnlyStream:
            return "paCanNotReadFromAnOutputOnlyStream";
        case ErrorCode::paCanNotWriteToAnInputOnlyStream:
            return "paCanNotWriteToAnInputOnlyStream";
        case ErrorCode::paIncompatibleStreamHostApi:
            return "paIncompatibleStreamHostApi";
        case ErrorCode::paBadBufferPtr:
            return "paBadBufferPtr";
            // PA Wrapper
        case ErrorCode::Pa_GetDeviceInfo:
            return "Pa_GetDeviceInfo";
    }

}

int Lowl::LowlError::get_error_code() {
    return to_error_code(error);
}

std::string Lowl::LowlError::get_error_text() {
    return to_error_text(error);
}
