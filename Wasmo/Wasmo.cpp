﻿#include "Wasmo.h"

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

void HandleFilename(SIMCONNECT_RECV_EVENT_FILENAME* eventFilename) {
	static const auto aircraftNameRegex = regex(".*\\\\([^\\\\]+)\\\\SimObjects\\\\Airplanes\\\\([^\\\\]+)\\\\aircraft.CFG", regex_constants::icase);
	static const auto configFile = "\\work\\aircraft-config.ini";

	switch (eventFilename->uEventID) {
	case EVENT_AIRCRAFT_LOADED: {
#if _DEBUG
		cout << "Wasmo: aircraft loaded " << eventFilename->szFileName << endl;
#endif
		auto configName = regex_replace(eventFilename->szFileName, aircraftNameRegex, "$1 $2");
		// https://github.com/flybywiresim/a32nx/blob/fbw/src/fbw/src/FlightDataRecorder.cpp
		INIReader configuration(configFile);
		ofstream testFile(configFile, ios::out | ios::app); // Fails silently if Bad Things happen.
		if (configuration.ParseError() < 0) {
			cout << "Wasmo: Creating completely new " << configFile << endl;
			testFile << "[default]" << endl;
			wasmo->WriteDefaultSection(testFile);
#if _DEBUG
		} else {
			cout << "Wasmo: using existing " << configFile << endl;
#endif
		}
		if (!configuration.HasSection(configName)) { // HasSection only works if there is at least one value.
			cout << "Wasmo: adding section for " << configName << endl;
			testFile << endl << '[' << configName << ']' << endl << "alias=default" << endl;
#if _DEBUG
		} else {
			cout << "Wasmo: using existing entry from " << configFile << endl;
#endif
		}
		testFile.close();
		configName = configuration.GetString(configName, "alias", configName);
		cout << "Wasmo: aircraft-specific config is " << configuration.GetString(configName, "config", "default_default") << endl;
		wasmo->AircraftLoaded(configuration, configName);
		break;
	}
	default:
		cerr << "Wasmo: Received unknown event filename " << eventFilename->uEventID << endl;
	}
}

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
#if _DEBUG
	cout << "Wasmo: dispatch " << pData->dwID << endl;
#endif
	HRESULT hr;
	switch (pData->dwID) {
	case SIMCONNECT_RECV_ID_OPEN:
		cout << "Wasmo: RX OnOpen" << endl;
		break;
	case SIMCONNECT_RECV_ID_EVENT_FILENAME:
		cout << "Wasmo: RX Filename" << endl;
		HandleFilename((SIMCONNECT_RECV_EVENT_FILENAME*)pData);
		break;
	case SIMCONNECT_RECV_ID_EXCEPTION: {
		SIMCONNECT_RECV_EXCEPTION* exception = (SIMCONNECT_RECV_EXCEPTION*)pData;
		// http://www.prepar3d.com/SDKv5/sdk/simconnect_api/references/structures_and_enumerations.html#SIMCONNECT_EXCEPTION
		cerr << "Wasmo: RX Exception :-( " << exception->dwException << endl;
		break;
	}
	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
		wasmo->Handle((SIMCONNECT_RECV_SIMOBJECT_DATA*)pData);
		break;
	case SIMCONNECT_RECV_ID_EVENT:
		wasmo->Handle((SIMCONNECT_RECV_EVENT*)pData);
		break;
	default:
		cerr << "Wasmo: Received unknown dispatch ID " << pData->dwID << endl;
		break;
	}
#if _DEBUG
	cout << "Wasmo: done responding, will it call again?" << endl;
#endif
}

extern "C" MSFS_CALLBACK void module_init(void) {
	wasmo = Wasmo::create();
	wasmo->init();

	cout << "Wasmo: subscribing to system events" << endl;
	if (FAILED(SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_AIRCRAFT_LOADED, "AircraftLoaded"))) {
		cerr << "Wasmo: couldn't subscribe to AircraftLoaded" << endl;
	}

#if _DEBUG
	cout << "Wasmo: calling dispatch" << endl;
#endif
	if (FAILED(SimConnect_CallDispatch(g_hSimConnect, WasmoDispatch, nullptr))) {
		cerr << "Wasmo: CallDispatch failed" << endl;
	}

#if _DEBUG
	cout << "Wasmo: module initialised" << endl;
#endif
}

Wasmo::Wasmo(const char* appName) {
#if _DEBUG
	clog << boolalpha << nounitbuf << "Wasmo::Wasmo for " << appName << endl;
#endif

	g_hSimConnect = 0;
	if (FAILED(SimConnect_Open(&g_hSimConnect, appName, nullptr, 0, 0, 0))) {
		cerr << "Wasmo: Could not open SimConnect connection" << endl;
		return;
	}

#if _DEBUG
	cout << "Wasmo: constructed and connected" << endl;
#endif
}

extern "C" MSFS_CALLBACK void module_deinit(void) {
	if (g_hSimConnect && FAILED(SimConnect_Close(g_hSimConnect))) {
		cerr << "Wasmo: Could not close SimConnect connection" << endl;
	}
	g_hSimConnect = 0;
	delete wasmo;
	wasmo = nullptr;
}
