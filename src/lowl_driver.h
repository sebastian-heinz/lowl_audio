#ifndef LOWL_DRIVER_H
#define LOWL_DRIVER_H

#include "lowl_device.h"

namespace Lowl {
    class Driver {

    protected:
        std::vector<std::shared_ptr<Device>> devices;
        std::shared_ptr<Device> default_device;
        std::string name;

    public:
        virtual void initialize(Error &error) = 0;

        virtual std::shared_ptr<Device> get_default_device() const;

        std::vector<std::shared_ptr<Device>> get_devices() const;

        std::string get_name() const;

        Driver();

        virtual ~Driver() = default;;

    };
}

#endif
