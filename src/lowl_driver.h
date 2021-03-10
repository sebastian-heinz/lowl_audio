#ifndef LOWL_DRIVER_H
#define LOWL_DRIVER_H

#include "lowl_device.h"

#include <string>
#include <vector>

namespace Lowl {
    class Driver {

    protected:
        std::vector<Device *> devices;
        std::string name;

    public:
        virtual void initialize(Error &error) = 0;

    public:
        std::vector<Device *> get_devices() const;

        std::string get_name() const;

        Driver();

        virtual ~Driver() = default;;

    };
}

#endif
