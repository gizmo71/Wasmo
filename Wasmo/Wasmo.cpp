#include "Wasmo.h"

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

enum EVENT_ID {
	EVENT_AIRCRAFT_LOADED = 10000,
};

HANDLE g_hSimConnect;

Wasmo* wasmo;

void Wasmo::WriteDefaultSection(ofstream& out) {
	out << "config=file_default" << endl;
}

void Wasmo::HandleFilename(SIMCONNECT_RECV_EVENT_FILENAME* eventFilename) {
	static const auto aircraftNameRegex = regex(".*\\\\([^\\\\]+)\\\\SimObjects\\\\Airplanes\\\\([^\\\\]+)\\\\aircraft.CFG", regex_constants::icase);
	static const auto configFile = "\\work\\aircraft-config.ini";

	switch (eventFilename->uEventID) {
	case EVENT_AIRCRAFT_LOADED: {
#if _DEBUG
		cout << "Wasmo(" << appName << "): aircraft loaded " << eventFilename->szFileName << endl;
#endif
		auto configName = regex_replace(eventFilename->szFileName, aircraftNameRegex, "$1 $2");
		// https://github.com/flybywiresim/a32nx/blob/fbw/src/fbw/src/FlightDataRecorder.cpp
		INIReader configuration(configFile);
		ofstream testFile(configFile, ios::out | ios::app); // Fails silently if Bad Things happen.
		if (configuration.ParseError() < 0) {
			cout << "Wasmo(" << appName << "): Creating completely new " << configFile << endl;
			testFile << "[default]" << endl;
			wasmo->WriteDefaultSection(testFile);
#if _DEBUG
		} else {
			cout << "Wasmo(" << appName << "): using existing " << configFile << endl;
#endif
		}
		if (!configuration.HasSection(configName)) { // HasSection only works if there is at least one value.
			cout << "Wasmo(" << appName << "): adding section for " << configName << endl;
			testFile << endl << '[' << configName << ']' << endl << "alias=default" << endl;
#if _DEBUG
		} else {
			cout << "Wasmo(" << appName << "): using existing entry from " << configFile << endl;
#endif
		}
		testFile.close();
		configName = configuration.GetString(configName, "alias", configName);
		cout << "Wasmo(" << appName << "): aircraft-specific config is " << configuration.GetString(configName, "config", "default_default") << endl;
		wasmo->AircraftLoaded(configuration, configName);
		break;
	}
	default:
		cerr << "Wasmo(" << appName << "): Received unknown event filename " << eventFilename->uEventID << endl;
	}
}

DWORD Wasmo::GetLastSentPacketID() {
	DWORD id;
	SimConnect_GetLastSentPacketID(g_hSimConnect, &id);
	return id;
}

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
	wasmo->Dispatch(pData, cbData, pContext);
}

void Wasmo::Dispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
#if _DEBUG
	cout << "Wasmo(" << appName << "): dispatch " << pData->dwID << endl;
#endif
	HRESULT hr;
	switch (pData->dwID) {
	case SIMCONNECT_RECV_ID_OPEN:
		cout << "Wasmo(" << appName << "): RX OnOpen" << endl;
		break;
	case SIMCONNECT_RECV_ID_EVENT_FILENAME:
		cout << "Wasmo(" << appName << "): RX Filename" << endl;
		HandleFilename((SIMCONNECT_RECV_EVENT_FILENAME*)pData);
		break;
	case SIMCONNECT_RECV_ID_EXCEPTION: {
		SIMCONNECT_RECV_EXCEPTION* exception = (SIMCONNECT_RECV_EXCEPTION*)pData;
		// http://www.prepar3d.com/SDKv5/sdk/simconnect_api/references/structures_and_enumerations.html#SIMCONNECT_EXCEPTION
		cerr << "Wasmo(" << appName << "): RX Exception :-( " << exception->dwException << " from packet " << exception->dwSendID << endl;
		break;
	}
	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
		wasmo->Handle((SIMCONNECT_RECV_SIMOBJECT_DATA*)pData);
		break;
	case SIMCONNECT_RECV_ID_EVENT:
		wasmo->Handle((SIMCONNECT_RECV_EVENT*)pData);
		break;
	default:
		cout << "Wasmo(" << appName << "): Received unknown dispatch ID " << pData->dwID << endl;
		break;
	}
#if _DEBUG
	cout << "Wasmo(" << appName << "): done responding, will it call again?" << endl;
#endif
}

extern "C" MSFS_CALLBACK void module_init(void) {
	wasmo = Wasmo::create();
	wasmo->init();

	cout << "Wasmo(" << wasmo->appName << "): subscribing to system events" << endl;
	SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_AIRCRAFT_LOADED, "AircraftLoaded");

#if _DEBUG
	cout << "Wasmo(" << wasmo->appName << "): calling dispatch" << endl;
#endif
	SimConnect_CallDispatch(g_hSimConnect, WasmoDispatch, nullptr);

#if _DEBUG
	cout << "Wasmo(" << wasmo->appName << "): module initialised" << endl;
#endif
}

Wasmo::Wasmo(const char* appName) {
	this->appName = appName;
#if _DEBUG
	cout << boolalpha << nounitbuf << "Wasmo::Wasmo for " << appName << endl;
	cerr << boolalpha << nounitbuf << "cerr for " << appName << " stream set answer " << 42 << endl;
	clog << boolalpha << unitbuf << "clog for " << appName << " stream set answer " << 42 << endl;
#endif

	g_hSimConnect = 0;
	SimConnect_Open(&g_hSimConnect, appName, nullptr, 0, 0, 0);

#if _DEBUG
	cout << "Wasmo(" << appName << "): constructed and connected" << endl;
	cerr << "Wasmo(" << appName << "): constructed and connected" << endl;
	clog << "Wasmo(" << appName << "): constructed and connected" << endl;
#endif
}

extern "C" MSFS_CALLBACK void module_deinit(void) {
	if (g_hSimConnect) SimConnect_Close(g_hSimConnect);
	g_hSimConnect = 0;
	delete wasmo;
	wasmo = nullptr;
}
