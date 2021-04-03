#include <Wasmo.h>

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

enum GROUP_ID {
	GROUP_SPOILERS = 13,
};

enum EVENT_ID {
	EVENT_MORE_SPOILER_TOGGLE = 7,
	EVENT_LESS_SPOILER_ARM_GROUND,
	EVENT_SPOILER_SET,
};

enum DATA_DEFINE_ID {
	DEFINITION_SPOILERS = 42,
	DEFINITION_SPOILER_HANDLE,
};

enum DATA_REQUEST_ID {
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

	//TODO: consider using S_A_ON/OFF instead of _TOGGLE to avoid some conditionals.
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_LESS_SPOILER_ARM_GROUND, "SPOILERS_ARM_TOGGLE");
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_MORE_SPOILER_TOGGLE, "SPOILERS_TOGGLE");
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_MORE_SPOILER_TOGGLE, "SPOILERS_SET");

	SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_LESS_SPOILER_ARM_GROUND, TRUE);
	SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_MORE_SPOILER_TOGGLE, TRUE);

	SimConnect_SetNotificationGroupPriority(g_hSimConnect, GROUP_SPOILERS, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE);

	cout << "Spoilerzmo: add data definitions" << endl;
	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS ARMED", "Bool", SIMCONNECT_DATATYPE_INT32, 0.5);
	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS HANDLE POSITION", "percent", SIMCONNECT_DATATYPE_INT32, 2.5);

	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILER_HANDLE,
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

	//TODO: sadly, the standard simvars no longer work with the A32NX. :-(
	ID idArmed = check_named_variable("A32NX_SPOILERS_ARMED");
#if _DEBUG
	cout << "Spoilerzmo: RX data " << pObjData->dwRequestID << " current position " << spoilersData->spoilerHandle << ", armed? " << spoilersData->spoilersArmed << endl;
#endif
	if (idArmed != -1) {
		ENUM boolUnits = get_units_enum("Bool");
		spoilersData->spoilersArmed = get_named_variable_typed_value(idArmed, boolUnits);
	}
#if _DEBUG
	cout << "Spoilerzmo: after massage, armed?" << spoilersData->spoilersArmed << endl;
#endif

	switch (pObjData->dwRequestID) {
	case REQUEST_LESS_SPOILER:
		if (spoilersData->spoilerHandle > 0) {
			handleData = max(spoilersData->spoilerHandle - 25, 0);
		} else if (spoilersData->spoilersArmed == 0) {
			eventToSend = EVENT_LESS_SPOILER_ARM_GROUND;
		}
		break;
	case REQUEST_MORE_SPOILER:
		if (spoilersData->spoilersArmed != 0) {
			eventToSend = EVENT_LESS_SPOILER_ARM_GROUND;
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
#if FALSE
		SimConnect_SetDataOnSimObject(g_hSimConnect, DEFINITION_SPOILER_HANDLE, SIMCONNECT_OBJECT_ID_USER,
			SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, sizeof(handleData), &handleData);
#else
		DWORD eventData = handleData / 100.0 * 32767;
		SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_SPOILER_SET, eventData, GROUP_SPOILERS, SIMCONNECT_EVENT_FLAG_DEFAULT);
#endif
#if _DEBUG
		cout << "Spoilerzmo: sent new handle position " << handleData << " #" << GetLastSentPacketID() << endl;
#endif
		ID idPosition = check_named_variable("A32NX_SPOILERS_HANDLE_POSITION");
		if (idPosition != -1) {
			set_named_variable_value(idPosition, handleData / 100.0);
		}
	}

#if _DEBUG
	cout << "Spoilerzmo: RX handled" << endl;
#endif
}
