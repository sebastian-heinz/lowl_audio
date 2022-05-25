#ifdef LOWL_DRIVER_WASAPI

#include "lowl_audio_wasapi_com.h"

#include "lowl_logger.h"

#include <objbase.h>

// https://github.com/mozilla/cubeb/issues/534


std::unique_ptr<Lowl::Audio::WasapiCom> Lowl::Audio::WasapiCom::wasapi_com = std::unique_ptr<Lowl::Audio::WasapiCom>();

void Lowl::Audio::WasapiCom::initialize(Lowl::Error &error) {

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        error.set_vendor_error(hr, Error::VendorError::WasapiVendorError);
        LOWL_LOG_ERROR_F("Wasapi failed CoInitializeEx (HRESULT:%ld)", hr);
        return;
    }
    if (hr == RPC_E_CHANGED_MODE) {
        already_initialized = true;
        return;
    }
    already_initialized = false;
    initialized = true;
}

void Lowl::Audio::WasapiCom::terminate() {
    if (!initialized) {
        return;
    }
    if (already_initialized) {
        return;
    }
    CoUninitialize();
    initialized = false;
}

Lowl::Audio::WasapiCom::WasapiCom() {
    initialized = false;
    already_initialized = false;
}

Lowl::Audio::WasapiCom::~WasapiCom() {
    terminate();
}

#endif /* LOWL_DRIVER_WASAPI */
