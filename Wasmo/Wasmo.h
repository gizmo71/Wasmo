#pragma once
#pragma clang diagnostic ignored "-Wignored-attributes"

#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>
#include <MSFS/Legacy/gauges.h>

#include <INIReader.h>

extern "C" MSFS_CALLBACK void module_init(void);
extern "C" MSFS_CALLBACK void module_deinit(void);

struct Wasmo {
	Wasmo(const char* appName);
	virtual ~Wasmo() { }
	static Wasmo* create();
	virtual void init() = 0;
	virtual void Handle(SIMCONNECT_RECV_EVENT*) { }
	virtual void Handle(SIMCONNECT_RECV_CLIENT_DATA* data) { Handle((SIMCONNECT_RECV_SIMOBJECT_DATA*)data); }
	virtual void Handle(SIMCONNECT_RECV_SIMOBJECT_DATA*) { }
	virtual void WriteDefaultSection(std::ofstream& out);
	virtual void AircraftLoaded(INIReader&, std::string section) { }
	DWORD GetLastSentPacketID();
private:
	const char* appName;
	void Dispatch(SIMCONNECT_RECV*, DWORD, void*);
	void HandleFilename(SIMCONNECT_RECV_EVENT_FILENAME* eventFilename);
	friend void CALLBACK WasmoDispatch(SIMCONNECT_RECV*, DWORD, void*);
	friend void module_init(void);
	friend void module_deinit(void);
};

extern HANDLE g_hSimConnect;
