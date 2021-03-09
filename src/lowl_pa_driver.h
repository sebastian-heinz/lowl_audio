#ifndef LOWL_PA_DRIVER_H
#define LOWL_PA_DRIVER_H

#ifdef LOWL_DRIVER_PORTAUDIO
#include <portaudio.h>

#include "../include/lowl_driver.h"

class LowlPaDriver : public LowlDriver {

private:
    void create_devices(LowlError &error);

public:
	void initialize(LowlError &error) override;

public:
	LowlPaDriver();
	~LowlPaDriver();
};

#endif /* LOWL_DRIVER_PORTAUDIO */
#endif /* LOWL_PA_DRIVER_H */
