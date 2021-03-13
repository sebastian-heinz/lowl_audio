#ifndef LOWL_DUMMY_DRIVER_H
#define LOWL_DUMMY_DRIVER_H

#ifdef LOWL_DRIVER_DUMMY

#include "lowl_driver.h"

namespace Lowl {

    class DummyDriver : public Lowl::Driver {

    public:
        void initialize(Error &error) override {};

    public:
        DummyDriver();

        ~DummyDriver() override = default;
    };
}

#endif /* LOWL_DRIVER_DUMMY */
#endif /* LOWL_DUMMY_DRIVER_H */
