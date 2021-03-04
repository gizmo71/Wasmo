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
	cerr << "Spoilerzmo: init" << endl;

	if (FAILED(SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_LESS_SPOILER_ARM_GROUND, "SPOILERS_ARM_TOGGLE"))) {
		cerr << "Spoilerzmo: couldn't map less/arm client event" << endl;
	}
	if (FAILED(SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_MORE_SPOILER_TOGGLE, "SPOILERS_TOGGLE"))) {
		cerr << "Spoilerzmo: couldn't map more/toggle client event" << endl;
	}

	if (FAILED(SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_LESS_SPOILER_ARM_GROUND, TRUE))) {
		cerr << "Spoilerzmo: couldn't add less client event to group" << endl;
	}
	if (FAILED(SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_MORE_SPOILER_TOGGLE, TRUE))) {
		cerr << "Spoilerzmo: couldn't add more client event to group" << endl;
	}

	if (FAILED(SimConnect_SetNotificationGroupPriority(g_hSimConnect, GROUP_SPOILERS, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE))) {
		cerr << "Spoilerzmo: couldn't set notification group priority" << endl;
	}

	cout << "Spoilerzmo: add data definitions" << endl;
	if (FAILED(SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS ARMED", "Bool", SIMCONNECT_DATATYPE_INT32, 0.5))) {
		cerr << "Spoilerzmo: couldn't map arming state to spoiler data" << endl;
	}
	if (FAILED(SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS HANDLE POSITION", "percent", SIMCONNECT_DATATYPE_INT32, 2.5))) {
		cerr << "Spoilerzmo: couldn't map handle position to spoiler data" << endl;
	}

	if (FAILED(SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILER_HANDLE,
		"SPOILERS HANDLE POSITION", "percent", SIMCONNECT_DATATYPE_INT32, 2.5))) {
		cerr << "Spoilerzmo: couldn't map handle position to spoiler handle data" << endl;
	}
}

void Spoilerzmo::Handle(SIMCONNECT_RECV_EVENT* evt) {
	cout << "Spoilerzmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;

	SIMCONNECT_DATA_REQUEST_ID request = -1;

	switch (evt->uEventID) {
	case EVENT_LESS_SPOILER_ARM_GROUND:
		cout << "Spoilerzmo: user has asked for less speedbrake (using the arm command)" << endl;
		request = REQUEST_LESS_SPOILER;
		break;
	case EVENT_MORE_SPOILER_TOGGLE:
		cout << "Spoilerzmo: user has asked for more speedbrake (using the toggle command)" << endl;
		request = REQUEST_MORE_SPOILER;
		break;
	default:
		cerr << "Spoilerzmo: Received unknown event " << evt->uEventID << endl;
	}

	if (request != -1) {
		cout << "Spoilerzmo: requesting " << request << endl;
		if (FAILED(SimConnect_RequestDataOnSimObject(g_hSimConnect, request, DEFINITION_SPOILERS, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0))) {
			cerr << "Spoilerzmo: Could not request spoiler data as the result of an event" << endl;
		}
	} else {
		cout << "Spoilerzmo: no request to send" << endl;
	}
}

void Spoilerzmo::Handle(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
	cout << "Spoilerzmo: RX data " << pObjData->dwRequestID << endl;

	SpoilersData* spoilersData = (SpoilersData*)&pObjData->dwData;
	INT32 handleData = -1;
	SIMCONNECT_CLIENT_EVENT_ID toSend = -1;

	switch (pObjData->dwRequestID) {
	case REQUEST_LESS_SPOILER:
		if (spoilersData->spoilerHandle > 0) {
			handleData = max(spoilersData->spoilerHandle - 25, 0);
		} else if (spoilersData->spoilersArmed == 0) {
			toSend = EVENT_LESS_SPOILER_ARM_GROUND;
		}
		break;
	case REQUEST_MORE_SPOILER:
		if (spoilersData->spoilersArmed != 0) {
			toSend = EVENT_LESS_SPOILER_ARM_GROUND;
			handleData = 0;
		} else if (spoilersData->spoilerHandle < 100) {
			handleData = min(spoilersData->spoilerHandle + 25, 100);
		}
		break;
	default:
		cerr << "Spoilerzmo: Received unknown data: " << pObjData->dwRequestID << endl;
	}

	if (toSend != -1) {
		cout << "Spoilerzmo: sending " << toSend << endl;
		if (FAILED(SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, toSend, 0, GROUP_SPOILERS, SIMCONNECT_EVENT_FLAG_DEFAULT))) {
			cerr << "Spoilerzmo: Could not refire arm spoiler event" << endl;
		}
	} else {
		cout << "Spoilerzmo: no event to send" << endl;
	}

	if (handleData != -1) {
		cout << "Spoilerzmo: sending new handle position " << handleData << endl;
		if (FAILED(SimConnect_SetDataOnSimObject(g_hSimConnect, DEFINITION_SPOILER_HANDLE, SIMCONNECT_OBJECT_ID_USER,
			SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, sizeof(handleData), &handleData))) {
			cerr << "Spoilerzmo: Could not send spoiler handle position" << endl;
		}
	} else {
		cout << "Spoilerzmo: no new handle position to send" << endl;
	}
}
