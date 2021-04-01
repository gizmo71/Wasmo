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
	virtual void Handle(SIMCONNECT_RECV_EVENT*) { }
	virtual void Handle(SIMCONNECT_RECV_SIMOBJECT_DATA*) = 0;
	virtual void WriteDefaultSection(std::ofstream& out);
	virtual void AircraftLoaded(INIReader&, std::string section) { }
	DWORD GetLastSentPacketID();
	//TODO: hide these somehow
	const char* appName;
	void Dispatch(SIMCONNECT_RECV*, DWORD, void*);
private:
	void HandleFilename(SIMCONNECT_RECV_EVENT_FILENAME* eventFilename);
};

extern HANDLE g_hSimConnect;
