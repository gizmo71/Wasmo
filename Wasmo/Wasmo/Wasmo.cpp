#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>

#include <iostream>
#include "Wasmo.h"

using namespace std;

HANDLE g_hSimConnect;

enum GROUP_ID {
	GROUP_SPOILERS = 13, // Unused
};

enum DATA_DEFINE_ID {
	DEFINITION_SPOILERS = 69,
};

enum DATA_REQUEST_ID {
	REQUEST_SPOILERS = 42,
};

struct SpoilersData {
	double spoilersArmed;
};

enum eEvents
{
	EVENT_TEXT,
	EVENT_SPOILERS_ARM_TOGGLE,
};

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);

// Interesting stuff in https://github.com/flybywiresim/a32nx/blob/fbw/src/fbw/src/interface/SimConnectInterface.cpp

extern "C" MSFS_CALLBACK void module_init(void)
{
	cerr << "Wasmo init" << endl;
	g_hSimConnect = 0;
	HRESULT hr = SimConnect_Open(&g_hSimConnect, "Standalone Module", nullptr, 0, 0, 0);
	if (hr != S_OK)
	{
		cerr << "Wasmo: Could not open SimConnect connection" << endl;
		return;
	}

	SIMCONNECT_RECV* pData;
	DWORD cbData;
	void* pContext = nullptr;
	while (SUCCEEDED(SimConnect_GetNextDispatch(g_hSimConnect, &pData, &cbData)))
	{
		WasmoDispatch(pData, cbData, pContext);
	}
	cerr << "Wasmo: GetNextDispatch failed" << endl;
}

extern "C" MSFS_CALLBACK void module_deinit(void)
{
	if (!g_hSimConnect)
		return;
	HRESULT hr = SimConnect_Close(g_hSimConnect);
	if (hr != S_OK)
	{
		cerr << "Wasmo: Could not close SimConnect connection" << endl;
		return;
	}
}

void ReceiveEvent(SIMCONNECT_RECV_EVENT* evt) {
	cout << "Wasmo: Received event " << evt->uEventID << "\n";
	switch (evt->uEventID) {
	case EVENT_TEXT:
		cerr << "Wasmo: Text event " << hex << evt->dwData << dec << endl;
		break;
	case EVENT_SPOILERS_ARM_TOGGLE:
		SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_SPOILERS, DEFINITION_SPOILERS, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE);
		break;
	default:
		cerr << "Wasmo: Received unknown event " << evt->uEventID << "\n";
		break;
	}
}

void ReceiveData(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
	cout << "Wasmo: received data " << pObjData->dwRequestID << endl;
	switch (pObjData->dwRequestID) {
	case REQUEST_SPOILERS: {
		SpoilersData* pS = (SpoilersData*)&pObjData->dwData;
		// This is a bit crap since two quick clicks shows correctly, then gets replaced by older one. Pff.
		char text[25];
		sprintf(text, "Ground Spoilers %d", (int)pS->spoilersArmed);
		SimConnect_Text(g_hSimConnect, SIMCONNECT_TEXT_TYPE_PRINT_WHITE, 1.0f, EVENT_TEXT, sizeof(text), (void*)text);
		break;
	}
	default:
		cerr << "Wasmo: Received unknown data: " << pObjData->dwRequestID << endl;
		break;
	}
}

void OnOpen() {
	cout << "Wasmo OnOpen\n";
	HRESULT hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_SPOILERS_ARM_TOGGLE, "SPOILERS_ARM_TOGGLE");
	if (hr != S_OK)
	{
		cerr << "Wasmo: couldn't map client event\n";
		return;
	}
	hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS ARMED",
		"Bool",
		SIMCONNECT_DATATYPE_FLOAT64); // Should be INT32 but that doesn't appear to work properly. :-(
	if (hr != S_OK)
	{
		cerr << "Wasmo: couldn't map spoiler data\n";
		return;
	}
}

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
	cout << "Wasmo: dispatch " << pData->dwID << "\n";
	switch (pData->dwID)
	{
	case SIMCONNECT_RECV_ID_OPEN:
		OnOpen();
		break;
	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
		ReceiveData((SIMCONNECT_RECV_SIMOBJECT_DATA*)pData);
		break;
	case SIMCONNECT_RECV_ID_EVENT:
		ReceiveEvent((SIMCONNECT_RECV_EVENT*)pData);
		break;
	default:
		break;
	}
}
