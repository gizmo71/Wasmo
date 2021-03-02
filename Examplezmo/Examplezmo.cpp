#pragma clang diagnostic ignored "-Wignored-attributes"

#include <Wasmo.h>

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

enum GROUP_ID {
	GROUP_SPOILERS = 13,
};

enum DATA_DEFINE_ID {
	DEFINITION_SPOILERS = 69,
};

enum DATA_REQUEST_ID {
	REQUEST_SPOILERS = 42,
};

struct SpoilersData {
	INT32 spoilersArmed;
	INT32 spoilerHandle;
};

enum eEvents {
	EVENT_AIRCRAFT_LOADED = 4,
	EVENT_TEXT,
	EVENT_SPOILERS_ARM_TOGGLE,
};

// Interesting stuff in https://github.com/flybywiresim/a32nx/blob/fbw/src/fbw/src/interface/SimConnectInterface.cpp

struct Examplezmo : Wasmo {
	Examplezmo() : Wasmo("Examplezmo") { }
	void init();
};

Wasmo* Wasmo::create() {
	return new Examplezmo();
}

void Examplezmo::init() {
	cerr << "Examplezmo: init" << endl;

	cout << "Examplezmo: map client event" << endl;
	HRESULT hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_SPOILERS_ARM_TOGGLE, "SPOILERS_ARM_TOGGLE");
	if (FAILED(hr)) {
		cerr << "Examplezmo: couldn't map client event" << endl;
	}
	cout << "Examplezmo: OnOpen add to group" << endl;
	hr = SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_SPOILERS_ARM_TOGGLE, TRUE);
	if (FAILED(hr)) {
		cerr << "Examplezmo: couldn't add client event to group" << endl;
	}
	cout << "Examplezmo: OnOpen set group priority" << endl;
	hr = SimConnect_SetNotificationGroupPriority(g_hSimConnect, GROUP_SPOILERS, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE);
	if (FAILED(hr)) {
		cerr << "Examplezmo: couldn't set notification group priority" << endl;
	}

	cout << "Examplezmo: add data definition" << endl;
	hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS ARMED",
		"Bool",
		SIMCONNECT_DATATYPE_INT32,
		0.5);
	if (FAILED(hr)) {
		cerr << "Examplezmo: couldn't map arming state to spoiler data" << endl;
	}
	hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS HANDLE POSITION",
		"percent",
		SIMCONNECT_DATATYPE_INT32,
		2.5);
	if (FAILED(hr)) {
		cerr << "Examplezmo: couldn't map handle position to spoiler data" << endl;
	}

	cout << "Examplezmo: subscribing to system events" << endl;
	if (FAILED(SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_AIRCRAFT_LOADED, "AircraftLoaded"))) {
		cerr << "Examplezmo: couldn't subscribe to AircraftLoaded" << endl;
	}
}

void HandleEvent(SIMCONNECT_RECV_EVENT* evt) {
	cout << "Examplezmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;
	switch (evt->uEventID) {
	case EVENT_TEXT:
		cout << "Examplezmo: Text event " << hex << evt->dwData << dec << endl;
		break;
	case EVENT_SPOILERS_ARM_TOGGLE:
		cout << "Examplezmo: user has asked for less speedbrake (using the arm command)" << endl;
		if (FAILED(SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_SPOILERS, DEFINITION_SPOILERS, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0))) {
			cerr << "Examplezmo: Could not request spoiler data as the result of an event" << endl;
		}
		if (FAILED(SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_SPOILERS_ARM_TOGGLE, 0, GROUP_SPOILERS, SIMCONNECT_EVENT_FLAG_DEFAULT))) {
			cerr << "Examplezmo: Could not refire arm spoiler event" << endl;
		}
		break;
	default:
		cerr << "Examplezmo: Received unknown event " << evt->uEventID << endl;
	}
}

void HandleFilename(SIMCONNECT_RECV_EVENT_FILENAME* eventFilename) {
	static const auto aircraftNameRegex = regex(".*\\\\([^\\\\]+)\\\\SimObjects\\\\Airplanes\\\\([^\\\\]+)\\\\aircraft.CFG", regex_constants::icase);
	static const auto configFile = "\\work\\aircraft-config.ini";

	switch (eventFilename->uEventID) {
	case EVENT_AIRCRAFT_LOADED: {
		cout << "Examplezmo: aircraft loaded " << eventFilename->szFileName << endl;
		auto configName = regex_replace(eventFilename->szFileName, aircraftNameRegex, "$1 $2");
		// https://github.com/flybywiresim/a32nx/blob/fbw/src/fbw/src/FlightDataRecorder.cpp
		INIReader configuration(configFile);
		ofstream testFile(configFile, ios::out | ios::app); // Fails silently if Bad Things happen.
		if (configuration.ParseError() < 0) {
			cout << "Examplezmo: Creating completely new " << configFile << endl;
			testFile << "[default]" << endl << "config=file_default" << endl;
		} else {
			cout << "Examplezmo: using existing " << configFile << endl;
		}
		if (!configuration.HasSection(configName)) { // HasSection only works if there is at least one value.
			cout << "Examplezmo: adding section for " << configName << endl;
			testFile << endl << '[' << configName << ']' << endl << "alias=default" << endl;
		} else {
			cout << "Examplezmo: using existing entry from " << configFile << endl;
		}
		testFile.close();
		configName = configuration.GetString(configName, "alias", configName);
		cout << "Examplezmo: aircraft-specific config is " << configuration.GetString(configName, "config", "default_default") << endl;
		break;
	}
	default:
		cerr << "Examplezmo: Received unknown event filename " << eventFilename->uEventID << endl;
	}
}

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
	cout << "Examplezmo: dispatch " << pData->dwID << endl;
	HRESULT hr;
	switch (pData->dwID) {
	case SIMCONNECT_RECV_ID_OPEN:
		cout << "Examplezmo: RX OnOpen" << endl;
		break;
	case SIMCONNECT_RECV_ID_EVENT_FILENAME:
		cout << "Examplezmo: RX Filename" << endl;
		HandleFilename((SIMCONNECT_RECV_EVENT_FILENAME*)pData);
		break;
	case SIMCONNECT_RECV_ID_EXCEPTION: {
		cerr << "Examplezmo: RX Exception :-(" << endl;
		SIMCONNECT_RECV_EXCEPTION* exception = (SIMCONNECT_RECV_EXCEPTION*)pData;
		// http://www.prepar3d.com/SDKv5/sdk/simconnect_api/references/structures_and_enumerations.html#SIMCONNECT_EXCEPTION
		cerr << "Examplezmo: " << exception->dwException << endl;
		break;
	}
	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
		SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;
		cout << "Examplezmo: RX data " << pObjData->dwRequestID << endl;
		switch (pObjData->dwRequestID) {
		case REQUEST_SPOILERS: {
			SpoilersData* pS = (SpoilersData*)&pObjData->dwData;
			// This is a bit crap since two quick clicks shows correctly, then gets replaced by older one. Pff.
			char text[100];
			sprintf(text, "Ground Spoilers armed? %d Speed brake position %d", (int)pS->spoilersArmed, (int)pS->spoilerHandle);
			//SimConnect_Text(g_hSimConnect, SIMCONNECT_TEXT_TYPE_PRINT_WHITE, 1.0f, EVENT_TEXT, sizeof(text), (void*)text);
			cout << "Examplezmo: not bothering to show " << text << endl;
			break;
		}
		default:
			cerr << "Examplezmo: Received unknown data: " << pObjData->dwRequestID << endl;
		}
		break;
	}
	case SIMCONNECT_RECV_ID_EVENT:
		HandleEvent((SIMCONNECT_RECV_EVENT*)pData);
		break;
	default:
		cerr << "Examplezmo: Received unknown dispatch ID " << pData->dwID << endl;
		break;
	}
	cout << "Examplezmo: done responding, will it call again?" << endl;
}
