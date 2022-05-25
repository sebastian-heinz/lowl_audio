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

        // Vendor Error
        PortAudioVendorError = -1000,
        PortAudioNoDeviceInfo = -1001,
        PortAudioNoHostApiInfo = -1002,
        PortAudioUnknownSampleFormat = -1004,

        CoreAudioVendorError = -2000,
        CoreAudioNoSuitableComponentFound = -2001,

        WasapiVendorError = -3000,
    };

    class Error {

    public:
        enum class VendorError {
            PortAudioVendorError = static_cast<int>(ErrorCode::PortAudioVendorError),
            CoreAudioVendorError = static_cast<int>(ErrorCode::CoreAudioVendorError),
            WasapiVendorError = static_cast<int>(ErrorCode::WasapiVendorError),
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

        bool has_error();

        bool ok();

        bool has_vendor_error();

        Error();
    };
}

#endif