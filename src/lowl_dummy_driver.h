#ifndef LOWL_DUMMY_DRIVER_H
#define LOWL_DUMMY_DRIVER_H

#ifdef LOWL_DRIVER_DUMMY

#include "../include/lowl_driver.h"

class LowlDummyDriver : public LowlDriver {

public:
    void initialize(LowlError &error) override;


public:
    LowlDummyDriver();

    ~LowlDummyDriver();
};

#endif /* LOWL_DRIVER_DUMMY */
#endif /* LOWL_DUMMY_DRIVER_H */
