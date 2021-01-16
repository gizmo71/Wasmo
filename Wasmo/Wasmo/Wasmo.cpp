#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>

#include <iostream>
#include "Wasmo.h"

HANDLE g_hSimConnect;

static enum GROUP_ID {
	GROUP_SPOILERS = 13, // Unused
};

static enum DATA_DEFINE_ID {
	DEFINITION_SPOILERS = 69,
};

static enum DATA_REQUEST_ID {
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

void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);

extern "C" MSFS_CALLBACK void module_init(void)
{
	fprintf(stderr, "Wasmo init\n");
	g_hSimConnect = 0;
	HRESULT hr = SimConnect_Open(&g_hSimConnect, "Standalone Module", nullptr, 0, 0, 0);
	if (hr != S_OK)
	{
		fprintf(stderr, "Could not open SimConnect connection.\n");
		return;
	}
	//TODO: does this need to be in a loop?
	hr = SimConnect_CallDispatch(g_hSimConnect, MyDispatchProc, nullptr);
	if (hr != S_OK)
	{
		fprintf(stderr, "Could not call dispatch.\n");
		return;
	}
}

extern "C" MSFS_CALLBACK void module_deinit(void)
{
	if (!g_hSimConnect)
		return;
	HRESULT hr = SimConnect_Close(g_hSimConnect);
	if (hr != S_OK)
	{
		fprintf(stderr, "Could not close SimConnect connection.\n");
		return;
	}
}

void ReceiveEvent(SIMCONNECT_RECV_EVENT* evt) {
	std::cout << "Wasmo received event " << evt->uEventID << "\n";
	switch (evt->uEventID) {
	case EVENT_TEXT:
		fprintf(stderr, "Text event %lx\n", evt->dwData);
		break;
	case EVENT_SPOILERS_ARM_TOGGLE:
		SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_SPOILERS, DEFINITION_SPOILERS, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE);
		break;
	default:
		fprintf(stderr, "Received unknown event: %d\n", evt->uEventID);
		break;
	}
}

void ReceiveData(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
	std::cout << "Wasmo received data " << pObjData->dwRequestID << "\n";
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
		fprintf(stderr, "Received unknown data: %d\n", pObjData->dwRequestID);
		break;
	}
}

void OnOpen() {
	std::cout << "Wasmo OnOpen\n";
	HRESULT hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_SPOILERS_ARM_TOGGLE, "SPOILERS_ARM_TOGGLE");
	if (hr != S_OK)
	{
		std::cerr << "Wasmo couldn't map client event\n";
		return;
	}
	hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS ARMED",
		"Bool",
		SIMCONNECT_DATATYPE_FLOAT64); // Should be INT32 but that doesn't appear to work properly. :-(
	if (hr != S_OK)
	{
		std::cerr << "Wasmo couldn't map spoiler data\n";
		return;
	}
}

void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
	std::cout << "Wasmo dispatch " << pData->dwID << "\n";
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
