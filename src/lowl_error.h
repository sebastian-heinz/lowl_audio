#ifndef LOWL_ERROR_H
#define LOWL_ERROR_H

#include <string>

namespace Lowl {


    enum class ErrorCode {
        NoError = 0,
        Error = -1,

        InvalidParameter = -10,
        AlreadyInitialized = -11,
        InvalidOperationWhileActive = -12,

        // Lowl Audio
        NoAudioOutput = -100,
        UnsupportedAudioFormat = -101,
        DevicePropertiesNotSupported = -102,

        FileStreamOpenFailed = -200,
        AudioReaderNoData = -201,
        ReaderUnsupportedFormat = - 202,
        ReaderUndetectedFormat = -203,
        ReaderEmptyPath = -204,
        ReaderNotFound = 205,
        ReaderNoAudioData = 206,

        DeviceHasNoAudioProperties = -300,

        ConvertAudioChannelInvalid = -400,
        ConvertAudioChannelNotSupported = -401,

        // Vendor Error
        CoreAudioVendorError = -2000,
        CoreAudioNoSuitableComponentFound = -2001,

        WasapiVendorError = -3000,

        VorbisFileVendorError = -4000,
        VorbisFileInvalidOggFile = -4001,
        VorbisFileCanNotParseOggFile = -4002,

        OpusFileVendorError = -5000,
        OpusFileCanNotParseOpusFile = -5001,
    };

    class Error {

    public:
        enum class VendorError {
            CoreAudioVendorError = static_cast<int>(ErrorCode::CoreAudioVendorError),
            WasapiVendorError = static_cast<int>(ErrorCode::WasapiVendorError),
            VorbisFileVendorError = static_cast<int>(ErrorCode::VorbisFileVendorError),
            OpusFileVendorError = static_cast<int>(ErrorCode::OpusFileVendorError),
        };

    private:
        static constexpr long NoVendorError = 0;

        ErrorCode error;
        long vendor_error_code;

    public:
        static int to_error_code(ErrorCode p_error);

        static std::string to_error_text(ErrorCode p_error);

        void set_error(ErrorCode p_error);

        void set_vendor_error(long p_vendor_error_code, VendorError p_vendor_error);

        long get_vendor_error() const;

        void clear();

        ErrorCode get_error() const;

        int get_error_code() const;

        std::string get_error_text() const;

        bool has_error() const;

        bool ok() const;

        bool has_vendor_error() const;

        Error();
    };
}

#endif