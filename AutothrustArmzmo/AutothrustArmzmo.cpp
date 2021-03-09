#include <Wasmo.h>

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

enum GROUP_ID {
	GROUP_AUTOTHRUST = 13,
};

enum EVENT_ID {
	EVENT_ARM_AUTOTHRUST_TOGGLE = 7,
	EVENT_N1_HOLD,
};

enum DATA_DEFINE_ID {
	DEFINITION_AUTOTHRUST = 42,
};

enum DATA_REQUEST_ID {
	REQUEST_AUTOTHRUST = 69,
};

struct AutothrustData {
	INT32 autothrustArmed;
};

struct AutothrustArmzmo : Wasmo {
	AutothrustArmzmo() : Wasmo("AutothrustArmzmo") { }
	void init();
	void Handle(SIMCONNECT_RECV_EVENT*);
	void Handle(SIMCONNECT_RECV_SIMOBJECT_DATA*);
};

Wasmo* Wasmo::create() {
	return new AutothrustArmzmo();
}

void AutothrustArmzmo::init() {
#if _DEBUG
	cout << "AutothrustArmzmo: init" << endl;
#endif

	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_ARM_AUTOTHRUST_TOGGLE, "AUTO_THROTTLE_ARM");
	// Use a different binding to avoid clashing with FBW A32NX.
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_N1_HOLD, "AP_N1_HOLD");

	SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_AUTOTHRUST, EVENT_N1_HOLD, TRUE);

	SimConnect_SetNotificationGroupPriority(g_hSimConnect, GROUP_AUTOTHRUST, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE);

	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_AUTOTHRUST,
		"AUTOPILOT THROTTLE ARM", "Bool", SIMCONNECT_DATATYPE_INT32, 0.5);
}

#define LVAR_EXPERIMENT 0 // https://forums.flightsimulator.com/t/demo-lvar-write-access-for-any-aircraft-control/353443
#if LVAR_EXPERIMENT
#include <MSFS/Legacy/gauges.h>
#endif

void AutothrustArmzmo::Handle(SIMCONNECT_RECV_EVENT* evt) {
#if LVAR_EXPERIMENT // Both work! :-D
	ID idA320 = check_named_variable("A320_Neo_MFD_NAV_MODE_1");
	double previousValue = get_named_variable_value(idA320);
	set_named_variable_value(idA320, 4); // 4 is "plan" - other modes are 0-3

	// Press MCDU button DIR
	FLOAT64* a = nullptr;
	SINT32* b = nullptr;
	PCSTRINGZ* c = nullptr;
	execute_calculator_code("(>H:A320_Neo_CDU_1_BTN_DIR)", a, b, c);

	// https://forums.flightsimulator.com/t/demo-lvar-write-access-for-any-aircraft-control/353443/35?u=dgymer
#endif

#if _DEBUG
	cout << "AutothrustArmzmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;
#endif

	switch (evt->uEventID) {
	case EVENT_N1_HOLD:
#if _DEBUG
		cout << "AutothrustArmzmo: user has demanded autothrust is armed or engaged; requesting data" << endl;
#endif
		SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_AUTOTHRUST, DEFINITION_AUTOTHRUST,
			SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0);
		break;
	default:
		cerr << "AutothrustArmzmo: Received unknown event " << evt->uEventID << endl;
	}
}

void AutothrustArmzmo::Handle(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
#if _DEBUG
	cout << "AutothrustArmzmo: RX data " << pObjData->dwRequestID << endl;
#endif

	switch (pObjData->dwRequestID) {
	case REQUEST_AUTOTHRUST: {
		AutothrustData* autothrustData = (AutothrustData*)&pObjData->dwData;
		if (autothrustData->autothrustArmed == 0) {
#if _DEBUG
			cout << "AutothrustArmzmo: sending event to arm autothrust" << endl;
#endif
			SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER,
				EVENT_ARM_AUTOTHRUST_TOGGLE, 0, GROUP_AUTOTHRUST, SIMCONNECT_EVENT_FLAG_DEFAULT);
		} else {
#if _DEBUG
			cout << "AutothrustArmzmo: already armed, not resending" << endl;
#endif
		}
		break;
	}
	default:
		cerr << "AutothrustArmzmo: Received unknown data: " << pObjData->dwRequestID << endl;
	}
}
