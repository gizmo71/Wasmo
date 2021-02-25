#pragma once
#pragma clang diagnostic ignored "-Wignored-attributes"

#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>

#include <INIReader.h>

struct Wasmo {
	Wasmo(const char* appName);
	static Wasmo* create();
	virtual void init() = 0;
};

void CALLBACK WasmoDispatch(SIMCONNECT_RECV*, DWORD, void*);

extern HANDLE g_hSimConnect;
