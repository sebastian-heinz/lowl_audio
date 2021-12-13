#include "lowl_error.h"

Lowl::Error::Error() {
    error = ErrorCode::NoError;
    vendor_error_code = NoVendorError;
}

void Lowl::Error::set_error(ErrorCode p_error) {
    error = p_error;
    vendor_error_code = NoVendorError;
}

void Lowl::Error::set_vendor_error(long p_vendor_error_code, Lowl::Error::VendorError p_vendor_error) {
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

bool Lowl::Error::has_error() {
    return error != ErrorCode::NoError;
}

bool Lowl::Error::has_vendor_error() {
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

        case ErrorCode::CoreAudioVendorError:
            return "CoreAudioVendorError";
        case ErrorCode::CoreAudioNoSuitableComponentFound:
            return "CoreAudioNoSuitableComponentFound";

        case ErrorCode::PortAudioVendorError:
            return "PortAudioVendorError";
        case ErrorCode::PortAudioNoDeviceInfo:
            return "PortAudioNoDeviceInfo";
        case ErrorCode::PortAudioNoHostApiInfo:
            return "PortAudioNoHostApiInfo";
        case ErrorCode::PortAudioUnknownSampleFormat:
            return "PortAudioUnknownSampleFormat";
    }
    return "NOT DECLARED";
}
