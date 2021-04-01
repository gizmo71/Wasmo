#include <Wasmo.h>

#include <fstream>
#include <iostream>

using namespace std;

enum EVENT_ID {
	EVENT_TUG_DISABLE = 7,
	EVENT_TUG_HEADING,
};

enum DATA_DEFINE_ID {
	DEFINITION_STATE = 69,
	DEFINITION_HEADING,
};

enum DATA_REQUEST_ID {
	REQUEST_STATE = 42,
	REQUEST_HEADING,
};
struct HeadingData {
	int32_t pushbackState;
	float trueHeading;
	float rudderPedalPosition;
};
struct StateData {
	int32_t pushbackState;
};

struct PushbackRudderzmo : Wasmo {
	PushbackRudderzmo() : Wasmo("PushbackRudderzmo") { }
	void init();
	void Handle(SIMCONNECT_RECV_SIMOBJECT_DATA*);
private:
	const double fullCircleDegrees = 360.0;
	const double maxSteerOffsetDegrees = 50.0;
	const double dwordPerDegree = DWORD_MAX / fullCircleDegrees; // ~11930465
};

Wasmo* Wasmo::create() {
	return new PushbackRudderzmo();
}

void PushbackRudderzmo::init() {
#if _DEBUG
	cout << "PushbackRudderzmo: map client events" << endl;
#endif
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_TUG_HEADING, "KEY_TUG_HEADING");
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_TUG_DISABLE, "TUG_DISABLE");

#if _DEBUG
	cout << "PushbackRudderzmo: add data definition" << endl;
#endif

	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_STATE,
		"PUSHBACK STATE", "enum", SIMCONNECT_DATATYPE_INT32, 0.5f);

	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_HEADING,
		"PUSHBACK STATE", "enum", SIMCONNECT_DATATYPE_INT32, 0.5f);
	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_HEADING,
		"PLANE HEADING DEGREES TRUE", "Degrees", SIMCONNECT_DATATYPE_FLOAT32, 0.5f);
	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_HEADING,
		"RUDDER PEDAL POSITION", "Position", SIMCONNECT_DATATYPE_FLOAT32, 0.02f);

#if _DEBUG
	cout << "PushbackRudderzmo: requesting data feeds" << endl;
#endif
	SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_STATE, DEFINITION_STATE,
		SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0);
#if FALSE // Causes an exception
	SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_HEADING, DEFINITION_HEADING,
		SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_NEVER, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0);
#endif
}

void PushbackRudderzmo::Handle(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
#if _DEBUG
	cout << "PushbackRudderzmo: received data " << pObjData->dwRequestID << endl;
#endif
	switch (pObjData->dwRequestID) {
	case REQUEST_STATE: {
		bool isConnected = ((StateData*)(&pObjData->dwData))->pushbackState != 3;
#if _DEBUG
		cout << "PushbackRudderzmo: updated isConnected to " << isConnected << endl;
		SIMCONNECT_PERIOD headingPeriod;
		if (isConnected) {
			headingPeriod = SIMCONNECT_PERIOD_VISUAL_FRAME;
		} else {
			SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_TUG_DISABLE, 0, SIMCONNECT_GROUP_PRIORITY_STANDARD, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
			headingPeriod = SIMCONNECT_PERIOD_NEVER;
		}
		SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_HEADING, DEFINITION_HEADING,
			SIMCONNECT_OBJECT_ID_USER, headingPeriod, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0);
#endif
		break;
	}
	case REQUEST_HEADING: {
		HeadingData* data = (HeadingData*)(&pObjData->dwData);
#if _DEBUG
		cout << "PushbackRudderzmo: updated state to " << data->pushbackState << " , true heading to "
			<< data->trueHeading << ", rudder pedals to " << data->rudderPedalPosition << endl;
#endif
		if (data->pushbackState == 0) {
			auto degrees = fmod(data->trueHeading + fullCircleDegrees - data->rudderPedalPosition * maxSteerOffsetDegrees, fullCircleDegrees);
			auto eventData = static_cast<DWORD>(degrees * dwordPerDegree);
#if _DEBUG
			cout << "PushbackRudderzmo: sending new tug heading " << degrees << " as " << eventData << endl;
#endif
			SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_TUG_HEADING, eventData, SIMCONNECT_GROUP_PRIORITY_STANDARD, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
		}
		break;
	}
	default:
		cerr << "PushbackRudderzmo: Received unknown data: " << pObjData->dwRequestID << endl;
		return;
	}
}
