#include <Wasmo.h>

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

enum GROUP_ID {
	GROUP_SPOILERS = 13,
};

enum EVENT_ID {
	EVENT_MORE_SPOILER = 7,
	EVENT_LESS_SPOILER,
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

struct SpoilerHandleData {
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

	if (FAILED(SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_LESS_SPOILER, "SPOILERS_ARM_TOGGLE"))) {
		cerr << "Spoilerzmo: couldn't map less/arm client event" << endl;
	}
	if (FAILED(SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_MORE_SPOILER, "SPOILERS_TOGGLE"))) {
		cerr << "Spoilerzmo: couldn't map more/toggle client event" << endl;
	}

	if (FAILED(SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_LESS_SPOILER, TRUE))) {
		cerr << "Spoilerzmo: couldn't add less client event to group" << endl;
	}
	if (FAILED(SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_SPOILERS, EVENT_MORE_SPOILER, TRUE))) {
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

	if (FAILED(SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPOILERS,
		"SPOILERS HANDLE POSITION", "percent", SIMCONNECT_DATATYPE_INT32, 2.5))) {
		cerr << "Spoilerzmo: couldn't map handle position to spoiler handle data" << endl;
	}
}

void Spoilerzmo::Handle(SIMCONNECT_RECV_EVENT* evt) {
	cout << "Spoilerzmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;
	switch (evt->uEventID) {
	case EVENT_LESS_SPOILER:
		cout << "Spoilerzmo: user has asked for less speedbrake (using the arm command)" << endl;
		if (FAILED(SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_LESS_SPOILER, DEFINITION_SPOILERS, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0))) {
			cerr << "Spoilerzmo: Could not request spoiler data as the result of an event" << endl;
		}
		break;
	case EVENT_MORE_SPOILER:
		cout << "Spoilerzmo: user has asked for more speedbrake (using the toggle command)" << endl;
		if (FAILED(SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_MORE_SPOILER, DEFINITION_SPOILERS, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0))) {
			cerr << "Spoilerzmo: Could not request spoiler data as the result of an event" << endl;
		}
		break;
	default:
		cerr << "Spoilerzmo: Received unknown event " << evt->uEventID << endl;
	}
}

void Spoilerzmo::Handle(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
	cout << "Spoilerzmo: RX data " << pObjData->dwRequestID << endl;
	SpoilersData* spoilersData = (SpoilersData*)&pObjData->dwData;
	SpoilerHandleData handleData;
	switch (pObjData->dwRequestID) {
	case REQUEST_LESS_SPOILER:
		if (spoilersData->spoilerHandle > 0) {
			handleData.spoilerHandle = max(spoilersData->spoilerHandle - 25, 0);
			if (FAILED(SimConnect_SetDataOnSimObject(g_hSimConnect, DEFINITION_SPOILER_HANDLE, SIMCONNECT_OBJECT_ID_USER,
				SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 1, sizeof(SpoilerHandleData), &handleData))) {
				cerr << "Spoilerzmo: Could not send spoiler handle position after reduction" << endl;
			}
		} else if (spoilersData->spoilersArmed == 0) {
			if (FAILED(SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_LESS_SPOILER, 0, GROUP_SPOILERS, SIMCONNECT_EVENT_FLAG_DEFAULT))) {
				cerr << "Spoilerzmo: Could not refire arm spoiler event" << endl;
			}
		}
		break;
	case REQUEST_MORE_SPOILER:
		if (spoilersData->spoilersArmed != 0) {
			if (FAILED(SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_LESS_SPOILER, 0, GROUP_SPOILERS, SIMCONNECT_EVENT_FLAG_DEFAULT))) {
				cerr << "Spoilerzmo: Could not refire arm spoiler event" << endl;
			}
		} else if (spoilersData->spoilerHandle <= 100) {
			handleData.spoilerHandle = min(spoilersData->spoilerHandle + 25, 100);
		}
		if (FAILED(SimConnect_SetDataOnSimObject(g_hSimConnect, DEFINITION_SPOILER_HANDLE, SIMCONNECT_OBJECT_ID_USER,
			SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 1, sizeof(SpoilerHandleData), &handleData))) {
			cerr << "Spoilerzmo: Could not send spoiler handle position after increase" << endl;
		}
		break;
	default:
		cerr << "Spoilerzmo: Received unknown data: " << pObjData->dwRequestID << endl;
	}
}
