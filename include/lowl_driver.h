#ifndef LOWL_DRIVER_H
#define LOWL_DRIVER_H

#include <string>
#include <vector>

#include "lowl_error.h"
#include "lowl_device.h"

class LowlDriver {

protected:
	std::vector<LowlDevice*> devices;
	std::string name;

public:
	virtual void initialize(LowlError &error) = 0;

public:
	std::vector<LowlDevice*> get_devices() const;
	std::string get_name() const;
	LowlDriver();
    virtual ~LowlDriver();

};

#endif
