#ifndef LOWL_ERROR_H
#define LOWL_ERROR_H

#include <string>

namespace Lowl {

    enum class ErrorCode {
        NoError = 0,
        Error = -1,

        WavReaderUnsupportedAudioFormat = -100,

        AudioStreamAlreadyInitialized = -200,

        Pa_GetDeviceInfo = -11000,
        PaUnknownSampleFormat = -19999,

        paNotInitialized = -10000,
        paUnanticipatedHostError = -9999,
        paInvalidChannelCount,
        paInvalidSampleRate,
        paInvalidDevice,
        paInvalidFlag,
        paSampleFormatNotSupported,
        paBadIODeviceCombination,
        paInsufficientMemory,
        paBufferTooBig,
        paBufferTooSmall,
        paNullCallback,
        paBadStreamPtr,
        paTimedOut,
        paInternalError,
        paDeviceUnavailable,
        paIncompatibleHostApiSpecificStreamInfo,
        paStreamIsStopped,
        paStreamIsNotStopped,
        paInputOverflowed,
        paOutputUnderflowed,
        paHostApiNotFound,
        paInvalidHostApi,
        paCanNotReadFromACallbackStream,
        paCanNotWriteToACallbackStream,
        paCanNotReadFromAnOutputOnlyStream,
        paCanNotWriteToAnInputOnlyStream,
        paIncompatibleStreamHostApi,
        paBadBufferPtr,
    };

    class Error {

    private:
        ErrorCode error;

    public:
        static int to_error_code(ErrorCode p_error);

        static std::string to_error_text(ErrorCode p_error);

        void set_error(ErrorCode p_error);

        void clear();

        ErrorCode get_error() const;

        int get_error_code() const;

        std::string get_error_text() const;

        bool has_error();

        Error();
    };
}

#endif