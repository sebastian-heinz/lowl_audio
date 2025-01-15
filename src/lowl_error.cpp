#include "lowl_error.h"

Lowl::Error::Error() {
    error = ErrorCode::NoError;
    vendor_error_code = NoVendorError;
}

void Lowl::Error::set_error(const ErrorCode p_error) {
    error = p_error;
    vendor_error_code = NoVendorError;
}

void Lowl::Error::set_vendor_error(const long p_vendor_error_code, VendorError p_vendor_error) {
    error = static_cast<ErrorCode>(p_vendor_error);
    vendor_error_code = p_vendor_error_code;
}

void Lowl::Error::clear() {
    error = ErrorCode::NoError;
    vendor_error_code = NoVendorError;
}

Lowl::ErrorCode Lowl::Error::get_error() const {
    return error;
}

int Lowl::Error::get_error_code() const {
    return to_error_code(error);
}

long Lowl::Error::get_vendor_error() const {
    return vendor_error_code;
}

std::string Lowl::Error::get_error_text() const {
    return to_error_text(error);
}

bool Lowl::Error::ok() const {
    return error == ErrorCode::NoError;
}

bool Lowl::Error::has_error() const {
    return error != ErrorCode::NoError;
}

bool Lowl::Error::has_vendor_error() const {
    return vendor_error_code != NoVendorError;
}

int Lowl::Error::to_error_code(ErrorCode p_error) {
    return static_cast<int>(p_error);
}

std::string Lowl::Error::to_error_text(ErrorCode p_error) {
    switch (p_error) {
        case ErrorCode::NoError:
            return "NoError";
        case ErrorCode::Error:
            return "Error";

        case ErrorCode::InvalidParameter:
            return "InvalidParameter";
        case ErrorCode::AlreadyInitialized:
            return "AlreadyInitialized";
        case ErrorCode::InvalidOperationWhileActive:
            return "InvalidOperationWhileActive";

        case ErrorCode::NoAudioOutput:
            return "NoAudioOutput";
        case ErrorCode::UnsupportedAudioFormat:
            return "UnsupportedAudioFormat";
        case ErrorCode::DevicePropertiesNotSupported:
            return "DevicePropertiesNotSupported";

        case ErrorCode::FileStreamOpenFailed:
            return "FileStreamOpenFailed";
        case ErrorCode::AudioReaderNoData:
            return "AudioReaderNoData";
        case ErrorCode::ReaderUnsupportedFormat:
            return "ReaderUnsupportedFormat";
        case ErrorCode::ReaderUndetectedFormat:
            return "ReaderUndetectedFormat";
        case ErrorCode::ReaderEmptyPath:
            return "ReaderEmptyPath";
        case ErrorCode::ReaderNotFound:
            return "ReaderNotFound";
        case ErrorCode::ReaderNoAudioData:
            return "ReaderNoAudioData";

        case ErrorCode::DeviceHasNoAudioProperties:
            return "DeviceHasNoAudioProperties";

        case ErrorCode::ConvertAudioChannelInvalid:
            return "ConvertAudioChannelInvalid";
        case ErrorCode::ConvertAudioChannelNotSupported:
            return "ConvertAudioChannelNotSupported";


        case ErrorCode::CoreAudioVendorError:
            return "CoreAudioVendorError";
        case ErrorCode::CoreAudioNoSuitableComponentFound:
            return "CoreAudioNoSuitableComponentFound";

        case ErrorCode::WasapiVendorError:
            return "WasapiVendorError";

        case ErrorCode::VorbisFileVendorError:
            return "VorbisFileVendorError";
        case ErrorCode::VorbisFileInvalidOggFile:
            return "VorbisFileInvalidOggFile";
        case ErrorCode::VorbisFileCanNotParseOggFile:
            return "VorbisFileCanNotParseOggFile";

        case ErrorCode::OpusFileVendorError:
            return "OpusFileVendorError";
        case ErrorCode::OpusFileCanNotParseOpusFile:
            return "OpusFileCanNotParseOpusFile";

    }
    return "NOT DECLARED";
}
