#pragma clang diagnostic ignored "-Wignored-attributes"

#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>

#include <iostream>

using namespace std;

HANDLE g_hSimConnect;

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
	double spoilersArmed;
	double spoilerHandle;
};

enum eEvents {
	EVENT_AIRCRAFT_LOADED = 4,
	EVENT_TEXT,
	EVENT_SPOILERS_ARM_TOGGLE,
};

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);

// Interesting stuff in https://github.com/flybywiresim/a32nx/blob/fbw/src/fbw/src/interface/SimConnectInterface.cpp

extern "C" MSFS_CALLBACK void module_init(void) {
	cerr << "Wasmo: init" << endl;
	g_hSimConnect = 0;
	HRESULT hr = SimConnect_Open(&g_hSimConnect, "Wasmo", nullptr, 0, 0, 0);
	if (FAILED(hr)) {
		cerr << "Wasmo: Could not open SimConnect connection" << endl;
		return;
	}

// Does this have to all be done before CallDispatch starts, with no subsequent registrations possible?
	cout << "Wasmo: map client event" << endl;
	hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_SPOILERS_ARM_TOGGLE, "SPOILERS_ARM_TOGGLE");
	if (FAILED(hr)) {
		cerr << "Wasmo: couldn't map client event" << endl;
	}
	cout << "Wasmo: OnOpen add to group" << endl;
	hr = SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_SPOILERS_ARM_TOGGLE, TRUE);
	if (FAILED(hr)) {
		cerr << "Wasmo: couldn't add client event to group" << endl;
	}
	cout << "Wasmo: OnOpen set group priority" << endl;
	hr = SimConnect_SetNotificationGroupPriority(g_hSimConnect, GROUP_SPOILERS, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE);
	if (FAILED(hr)) {
		cerr << "Wasmo: couldn't set notification group priority" << endl;
	}

	cout << "Wasmo: add data definition" << endl;
	hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS ARMED",
		"Bool",
		SIMCONNECT_DATATYPE_FLOAT64, // Should be INT32 but that doesn't appear to work properly. :-(
		0.5);
	if (FAILED(hr)) {
		cerr << "Wasmo: couldn't map arming state to spoiler data" << endl;
	}
	hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS HANDLE POSITION",
		"percent",
		SIMCONNECT_DATATYPE_FLOAT64, // Should be INT32 but that doesn't appear to work properly. :-(
		2.5);
	if (FAILED(hr)) {
		cerr << "Wasmo: couldn't map handle position to spoiler data" << endl;
	}

	cout << "Wasmo: subscribing to system events" << endl;
	if (FAILED(SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_AIRCRAFT_LOADED, "AircraftLoaded"))) {
		cerr << "Wasmo: couldn't subscribe to AircraftLoaded" << endl;
	}

	cout << "Wasmo: calling dispatch" << endl;
	if (FAILED(SimConnect_CallDispatch(g_hSimConnect, WasmoDispatch, nullptr))) {
		cerr << "Wasmo: CallDispatch failed" << endl;
	}
	cout << "Wasmo: module initialised" << endl;
}

extern "C" MSFS_CALLBACK void module_deinit(void) {
	if (!g_hSimConnect)
		return;
	HRESULT hr = SimConnect_Close(g_hSimConnect);
	if (hr != S_OK) {
		cerr << "Wasmo: Could not close SimConnect connection" << endl;
	}
	g_hSimConnect = 0;
}

void HandleEvent(SIMCONNECT_RECV_EVENT* evt) {
	cout << "Wasmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;
	HRESULT hr;
	switch (evt->uEventID) {
	case EVENT_TEXT:
		cout << "Wasmo: Text event " << hex << evt->dwData << dec << endl;
		break;
	case EVENT_AIRCRAFT_LOADED:
		SIMCONNECT_RECV_EVENT_FILENAME* eventFilename = (SIMCONNECT_RECV_EVENT_FILENAME*)evt;
		cout << "Wasmo: aircraft loaded  " << eventFilename->szFileName << endl;
		break;
	case EVENT_SPOILERS_ARM_TOGGLE:
		cout << "Wasmo: user has asked for less speedbrake (using the arm command)" << endl;
		hr = SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_SPOILERS, DEFINITION_SPOILERS, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0);
		if (FAILED(hr)) {
			cerr << "Wasmo: Could not request spoiler data as the result of an event" << endl;
		}
		hr = SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_SPOILERS_ARM_TOGGLE, 0, GROUP_SPOILERS, SIMCONNECT_EVENT_FLAG_DEFAULT);
		if (FAILED(hr)) {
			cerr << "Wasmo: Could not refire arm spoiler event" << endl;
		}
		break;
	default:
		cerr << "Wasmo: Received unknown event " << evt->uEventID << endl;
	}
}

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
	cout << "Wasmo: dispatch " << pData->dwID << endl;
	HRESULT hr;
	switch (pData->dwID) {
	case SIMCONNECT_RECV_ID_OPEN:
		cout << "Wasmo: OnOpen" << endl;
		break;
	case SIMCONNECT_RECV_ID_EXCEPTION: {
		cerr << "Wasmo: Exception :-(" << endl;
		SIMCONNECT_RECV_EXCEPTION* exception = (SIMCONNECT_RECV_EXCEPTION*)pData;
		// http://www.prepar3d.com/SDKv5/sdk/simconnect_api/references/structures_and_enumerations.html#SIMCONNECT_EXCEPTION
		cerr << "Wasmo: " << exception->dwException << endl;
		break;
	}
	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
		SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;
		cout << "Wasmo: received data " << pObjData->dwRequestID << endl;
		switch (pObjData->dwRequestID) {
		case REQUEST_SPOILERS: {
			SpoilersData* pS = (SpoilersData*)&pObjData->dwData;
			// This is a bit crap since two quick clicks shows correctly, then gets replaced by older one. Pff.
			char text[100];
			sprintf(text, "Ground Spoilers armed? %d Speed brake position %d", (int)pS->spoilersArmed, (int)pS->spoilerHandle);
			//SimConnect_Text(g_hSimConnect, SIMCONNECT_TEXT_TYPE_PRINT_WHITE, 1.0f, EVENT_TEXT, sizeof(text), (void*)text);
			cout << "Wasmo: not bothering to show " << text << endl;
			break;
		}
		default:
			cerr << "Wasmo: Received unknown data: " << pObjData->dwRequestID << endl;
		}
		break;
	}
	case SIMCONNECT_RECV_ID_EVENT:
		HandleEvent((SIMCONNECT_RECV_EVENT*)pData);
		break;
	default:
		cerr << "Wasmo: Received unknown dispatch ID " << pData->dwID << endl;
		break;
	}
	cout << "Wasmo: done responding, will it call again?" << endl;
}
