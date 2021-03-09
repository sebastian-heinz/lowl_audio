#ifdef LOWL_LIBRARY

#include "lowl_interface.h"
#include "../src/lowl.h"

struct LowlDriverInterfaceImpl : LowlDriverInterface
{
	LowlDriver* driver;

	LowlDriverInterfaceImpl(LowlDriver* p_driver)
	{
		driver = p_driver;
	}

	virtual const char* get_name() const {
		return driver->get_name().c_str();
	}

	virtual LowlError initialize() {
		return driver->initialize();
	}

	virtual void release() {
		delete this;
	}
};

struct LowlInterfaceImpl : LowlInterface
{
	Lowl* oam;

	LowlInterfaceImpl()
	{
		oam = new Lowl();
	}

	virtual LowlDriverInterface* get_driver(int p_driver_index) const {
		LowlDriver* driver = oam->get_drivers().at(p_driver_index);
		LowlDriverInterface *driver_interface = new LowlDriverInterfaceImpl(driver);
		return driver_interface;
	}

	virtual int get_driver_count() const {
		return oam->get_drivers().size();
	}

	virtual LowlError initialize() {
		return oam->initialize();
	}

	virtual LowlError terminate() {
		return oam->terminate();
	}

	virtual void release() {
		delete oam;
		delete this;
	}
};

DllExport LowlInterface* GetLowlInterface()
{
	LowlInterface* interface = new LowlInterfaceImpl();
	return interface;
}

#endif