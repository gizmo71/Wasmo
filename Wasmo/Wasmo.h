#pragma once
#pragma clang diagnostic ignored "-Wignored-attributes"

#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>

#include <INIReader.h>

struct Wasmo {
	HANDLE g_hSimConnect;
	virtual void init() { }
};

// Must be supplied and initialised by consuming application.
extern Wasmo wasmo;

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);
