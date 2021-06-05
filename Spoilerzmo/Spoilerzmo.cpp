#include <Wasmo.h>

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

enum ENUMERATED_VALUES {
	GROUP_SPOILERS = 7,
	EVENT_MORE_SPOILER_TOGGLE = 13,
	EVENT_LESS_SPOILER_ARM_GROUND,
	EVENT_SPOILER_ARM_ON,
	EVENT_SPOILER_ARM_OFF,
	EVENT_SPOILER_SET,
	DEFINITION_SPOILERS = 42,
	REQUEST_MORE_SPOILER = 69,
	REQUEST_LESS_SPOILER,
};

struct SpoilersData {
	INT32 spoilersArmed;
	INT32 spoilerHandle;
};

struct Spoilerzmo : Wasmo {
	Spoilerzmo() : Wasmo("Spoilerzmo") { }
	void init();
	void Handle(SIMCONNECT_RECV_EVENT*);
	void Handle(SIMCONNECT_RECV_SIMOBJECT_DATA*);
};

Wasmo* Wasmo::create() {
	return new Spoilerzmo();
}

void Spoilerzmo::init() {
	cout << "Spoilerzmo: init" << endl;

	// These are the input events.
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_LESS_SPOILER_ARM_GROUND, "SPOILERS_ARM_TOGGLE");
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_MORE_SPOILER_TOGGLE, "SPOILERS_TOGGLE");
	// These are output events only.
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_SPOILER_ARM_ON, "SPOILERS_ARM_ON");
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_SPOILER_ARM_OFF, "SPOILERS_ARM_OFF");
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_SPOILER_SET, "SPOILERS_SET");

	SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_LESS_SPOILER_ARM_GROUND, TRUE);
	SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_MORE_SPOILER_TOGGLE, TRUE);

	SimConnect_SetNotificationGroupPriority(g_hSimConnect, GROUP_SPOILERS, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE);

	cout << "Spoilerzmo: add data definitions" << endl;
	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS ARMED", "Bool", SIMCONNECT_DATATYPE_INT32, 0.5);
	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS HANDLE POSITION", "percent", SIMCONNECT_DATATYPE_INT32, 2.5);
}

void Spoilerzmo::Handle(SIMCONNECT_RECV_EVENT* evt) {
#if _DEBUG
	cout << "Spoilerzmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;
#endif

	SIMCONNECT_DATA_REQUEST_ID request = -1;

	switch (evt->uEventID) {
	case EVENT_LESS_SPOILER_ARM_GROUND:
#if _DEBUG
		cout << "Spoilerzmo: user has asked for less speedbrake (using the arm command)" << endl;
#endif
		request = REQUEST_LESS_SPOILER;
		break;
	case EVENT_MORE_SPOILER_TOGGLE:
#if _DEBUG
		cout << "Spoilerzmo: user has asked for more speedbrake (using the toggle command)" << endl;
#endif
		request = REQUEST_MORE_SPOILER;
		break;
	default:
		cout << "Spoilerzmo: Received unknown event " << evt->uEventID << endl;
	}

	if (request != -1) {
		SimConnect_RequestDataOnSimObject(g_hSimConnect, request, DEFINITION_SPOILERS, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0);
#if _DEBUG
		cout << "Spoilerzmo: requested " << request << " #" << GetLastSentPacketID() << endl;
#endif
	} else {
#if _DEBUG
		cout << "Spoilerzmo: no request to send" << endl;
#endif
	}
}

void Spoilerzmo::Handle(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
	SpoilersData* spoilersData = (SpoilersData*)&pObjData->dwData;
	INT32 handleData = -1;
	SIMCONNECT_CLIENT_EVENT_ID eventToSend = -1;

#if _DEBUG
	cout << "Spoilerzmo: RX data " << pObjData->dwRequestID << " current position " << spoilersData->spoilerHandle << ", armed? " << spoilersData->spoilersArmed << endl;
#endif
	// Sadly, the standard simvar no longer works with the A32NX. :-(
	ID idArmed = check_named_variable("A32NX_SPOILERS_ARMED");
	ID idActive = check_named_variable("A32NX_SPOILERS_GROUND_SPOILERS_ACTIVE");
	if (idArmed != -1 && idActive != -1) {
		spoilersData->spoilersArmed |= get_named_variable_value(idArmed) || get_named_variable_value(idActive);
#if _DEBUG
		cout << "Spoilerzmo: after massage with armed/active? " << spoilersData->spoilersArmed << endl;
#endif
	}

	switch (pObjData->dwRequestID) {
	case REQUEST_LESS_SPOILER:
		if (spoilersData->spoilerHandle > 0) {
			handleData = max(spoilersData->spoilerHandle - 25, 0);
		} else {
			eventToSend = EVENT_SPOILER_ARM_ON;
		}
		break;
	case REQUEST_MORE_SPOILER:
		if (spoilersData->spoilersArmed != 0) {
			eventToSend = EVENT_SPOILER_ARM_OFF;
			handleData = 0;
		} else if (spoilersData->spoilerHandle < 100) {
			handleData = min(spoilersData->spoilerHandle + 25, 100);
		}
		break;
	default:
		cout << "Spoilerzmo: Received unknown data: " << pObjData->dwRequestID << endl;
	}

	if (eventToSend != -1) {
		SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, eventToSend, 0, GROUP_SPOILERS, SIMCONNECT_EVENT_FLAG_DEFAULT);
#if _DEBUG
		cout << "Spoilerzmo: sent event " << eventToSend << " #" << GetLastSentPacketID() << endl;
#endif
	}

	if (handleData != -1) {
		DWORD eventData = handleData * 164;
		eventData = min(eventData, 16383);
		SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_SPOILER_SET, eventData, GROUP_SPOILERS, SIMCONNECT_EVENT_FLAG_DEFAULT);
#if _DEBUG
		cout << "Spoilerzmo: sent new handle position " << handleData << "->" << eventData << " #" << GetLastSentPacketID() << endl;
#endif
	}

#if _DEBUG
	if (handleData == -1 && eventToSend == -1)
		cout << "Spoilerzmo: RX handled but no action taken" << endl;
#endif
}
