#ifdef LOWL_LIBRARY

#ifndef LOWL_INTERFACE_H
#define LOWL_INTERFACE_H

#define APIENTRY __stdcall
#ifdef LOWL_EXPORT
#define DllExport __declspec(dllexport)
#else
#define DllExport __declspec(dllimport)
#endif

#include "../src/lowl_error.h"

struct LowlDriverInterface {

public:
    virtual const char *get_name() const = 0;

    virtual LowlError initialize() = 0;

    virtual void release() = 0;
};


struct LowlInterface {

public:
    virtual LowlDriverInterface *get_driver(int p_driver_index) const = 0;

    virtual int get_driver_count() const = 0;

    virtual LowlError initialize() = 0;

    virtual LowlError terminate() = 0;

    virtual void release() = 0;
};

extern "C" DllExport LowlInterface *APIENTRY GetLowlInterface();

#endif
