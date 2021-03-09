#ifndef LOWL_DUMMY_DRIVER_H
#define LOWL_DUMMY_DRIVER_H

#ifdef LOWL_DRIVER_DUMMY

#include "../../lowl_driver.h"
#include "../../lowl_error.h"

namespace Lowl {

    class DummyDriver : public Lowl::Driver {

    public:
        void initialize(LowlError &error) override {};

    public:
        DummyDriver();

        ~DummyDriver() override = default;
    };
}

#endif /* LOWL_DRIVER_DUMMY */
#endif /* LOWL_DUMMY_DRIVER_H */
