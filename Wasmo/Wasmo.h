#pragma once
#pragma clang diagnostic ignored "-Wignored-attributes"

#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>

#include <INIReader.h>

struct Wasmo {
	Wasmo(const char* appName);
	virtual ~Wasmo() { }
	static Wasmo* create();
	virtual void init() = 0;
	virtual void Handle(SIMCONNECT_RECV_EVENT*) = 0;
	virtual void Handle(SIMCONNECT_RECV_SIMOBJECT_DATA*) = 0;
};

extern HANDLE g_hSimConnect;
