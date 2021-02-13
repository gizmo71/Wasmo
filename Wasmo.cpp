﻿#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>

#include <iostream>

using namespace std;

HANDLE g_hSimConnect;

enum GROUP_ID {
	GROUP_RUDDER_TILLER = 13,
};

enum DATA_DEFINE_ID {
	DEFINITION_SPEED = 69,
};

enum DATA_REQUEST_ID {
	REQUEST_SPEED = 42,
};

struct AllData {
	double groundSpeed;
	double radioAlt;
	bool isOnGround;
};

enum eEvents {
	EVENT_RUDDER,
	EVENT_TILLER,
};

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);

extern "C" MSFS_CALLBACK void module_init(void) {
	cerr << "RudderTillerzmo: init" << endl;
	g_hSimConnect = 0;
	HRESULT hr = SimConnect_Open(&g_hSimConnect, "RudderTillerzmo", nullptr, 0, 0, 0);
	if (FAILED(hr)) {
		cerr << "RudderTillerzmo: Could not open SimConnect connection" << endl;
		return;
	}

// Does this have to all be done before CallDispatch starts, with no subsequent registrations possible?
	cout << "RudderTillerzmo: map client events" << endl;
	if (FAILED(SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_RUDDER, "AXIS_RUDDER_SET"))) {
		cerr << "RudderTillerzmo: couldn't map rudder set event" << endl;
	}
	if (FAILED(SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_TILLER, "RUDDER_AXIS_MINUS"))) {
		cerr << "RudderTillerzmo: couldn't map tiller set event" << endl;
	}

	cout << "RudderTillerzmo: OnOpen add to group" << endl;
	if (FAILED(SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_RUDDER_TILLER, EVENT_RUDDER, TRUE))) {
		cerr << "RudderTillerzmo: couldn't add rudder event to group" << endl;
	}
	if (FAILED(SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_RUDDER_TILLER, EVENT_TILLER, TRUE))) {
		cerr << "RudderTillerzmo: couldn't add steering event to group" << endl;
	}

	cout << "RudderTillerzmo: OnOpen set group priority" << endl;
	if (FAILED(SimConnect_SetNotificationGroupPriority(g_hSimConnect, GROUP_RUDDER_TILLER, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE))) {
		cerr << "RudderTillerzmo: couldn't set rudder/tiller notification group priority" << endl;
	}

	cout << "RudderTillerzmo: add data definition" << endl;
	if (FAILED(SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPEED,
		"GROUND VELOCITY", "Knots", SIMCONNECT_DATATYPE_FLOAT64, 0.5)))
	{
		cerr << "RudderTillerzmo: couldn't map ground velocity data" << endl;
	}
	if (FAILED(SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPEED,
		"PLANE ALT ABOVE GROUND", "Feet", SIMCONNECT_DATATYPE_FLOAT64, 0.5)))
	{
		cerr << "RudderTillerzmo: couldn't map radio alt data" << endl;
	}
	if (FAILED(SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPEED,
		"SIM ON GROUND", "Bool", SIMCONNECT_DATATYPE_INT32, 0.5)))
	{
		cerr << "RudderTillerzmo: couldn't map on ground data" << endl;
	}

	cout << "RudderTillerzmo: requesting data feed" << endl;
	if (FAILED(SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_SPEED, DEFINITION_SPEED,
		SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0)))
	{
		cerr << "RudderTillerzmo: Could not request data feed" << endl;
	}

	cout << "RudderTillerzmo: calling dispatch" << endl;
	if (FAILED(SimConnect_CallDispatch(g_hSimConnect, WasmoDispatch, nullptr))) {
		cerr << "RudderTillerzmo: CallDispatch failed" << endl;
	}
	cout << "RudderTillerzmo: module initialised" << endl;
}

extern "C" MSFS_CALLBACK void module_deinit(void) {
	if (!g_hSimConnect)
		return;
	HRESULT hr = SimConnect_Close(g_hSimConnect);
	if (hr != S_OK) {
		cerr << "RudderTillerzmo: Could not close SimConnect connection" << endl;
	}
	g_hSimConnect = 0;
}

double pedalsDemand = 0.0;
double tillerDemand = 0.0;
double speed = 0.0;

void HandleEvent(SIMCONNECT_RECV_EVENT* evt) {
	cout << "RudderTillerzmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;
	switch (evt->uEventID) {
	case EVENT_RUDDER:
		pedalsDemand = 6.0 * static_cast<long>(evt->dwData) / 16384.0;
		cout << "RudderTillerzmo: user has asked to set rudder to " << pedalsDemand << endl;
		if (FAILED(SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_RUDDER, evt->dwData, GROUP_RUDDER_TILLER, SIMCONNECT_EVENT_FLAG_DEFAULT))) {
			cerr << "RudderTillerzmo: Could not refire rudder event" << endl;
		}
		break;
	case EVENT_TILLER:
		tillerDemand = 75.0 * static_cast<long>(evt->dwData) / 16384.0;
		cout << "RudderTillerzmo: user has asked to set tiller to " << tillerDemand << endl;
		if (FAILED(SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_TILLER, evt->dwData, GROUP_RUDDER_TILLER, SIMCONNECT_EVENT_FLAG_DEFAULT))) {
			cerr << "RudderTillerzmo: Could not refire tiller event" << endl;
		}
		break;
	default:
		cerr << "RudderTillerzmo: Received unknown event " << evt->uEventID << endl;
	}
}

void HandleData(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
	cout << "RudderTillerzmo: received data " << pObjData->dwRequestID << endl;
	switch (pObjData->dwRequestID) {
	case REQUEST_SPEED: {
		AllData* pS = (AllData*)(&pObjData->dwData);
		speed = !pS->isOnGround ? 200 : (pS->radioAlt > 0 ? 190 : pS->groundSpeed);
		cout << "RudderTillerzmo: updated speed to " << speed << endl;
		break;
	}
	default:
		cerr << "RudderTillerzmo: Received unknown data: " << pObjData->dwRequestID << endl;
	}
}

void SendDemand() {
	cerr << "RudderTillerzmo: TODO: send demand" << endl;
}

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
	cout << "RudderTillerzmo: dispatch " << pData->dwID << endl;
	switch (pData->dwID) {
	case SIMCONNECT_RECV_ID_OPEN:
		cout << "RudderTillerzmo: OnOpen" << endl;
		break;
	case SIMCONNECT_RECV_ID_EXCEPTION: {
		cerr << "RudderTillerzmo: Exception :-(" << endl;
		SIMCONNECT_RECV_EXCEPTION* exception = (SIMCONNECT_RECV_EXCEPTION*)pData;
		// http://www.prepar3d.com/SDKv5/sdk/simconnect_api/references/structures_and_enumerations.html#SIMCONNECT_EXCEPTION
		cerr << "RudderTillerzmo: " << exception->dwException << endl;
		break;
	}
	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
		HandleData((SIMCONNECT_RECV_SIMOBJECT_DATA*)pData);
		SendDemand();
		break;
	}
	case SIMCONNECT_RECV_ID_EVENT:
		HandleEvent((SIMCONNECT_RECV_EVENT*)pData);
		SendDemand();
		break;
	default:
		cerr << "RudderTillerzmo: Received unknown dispatch ID " << pData->dwID << endl;
		break;
	}
	cout << "RudderTillerzmo: done responding" << endl;
}
