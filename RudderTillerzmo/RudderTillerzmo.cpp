﻿#include <Wasmo.h>

#include <fstream>
#include <iostream>

using namespace std;

enum GROUP_ID {
	GROUP_RUDDER_TILLER = 13,
};

enum EVENT_ID {
	EVENT_RUDDER = 7,
	EVENT_TILLER,
};

enum DATA_DEFINE_ID {
	DEFINITION_SPEED = 69,
	DEFINITION_ON_GROUND,
};

enum DATA_REQUEST_ID {
	REQUEST_SPEED = 42,
	REQUEST_ON_GROUND,
};

struct SpeedData {
	double groundSpeed;
};
struct GroundData {
	int32_t isOnGround;
};

struct RudderTillerzmo : Wasmo {
	RudderTillerzmo() : Wasmo("RudderTillerzmo") { }
	void init();
	void Handle(SIMCONNECT_RECV_EVENT*);
	void Handle(SIMCONNECT_RECV_SIMOBJECT_DATA*);
	void WriteDefaultSection(std::ofstream& out);
	void AircraftLoaded(INIReader&, std::string);

	void SendDemand();

private:
	const double speedEpsilon = 1.0 / 3.0;
	const double maxRawMagnitude = 16384.0;

	bool isTillerMerged = false;
	double pedalsDemand = 0.0;
	double tillerDemand = 0.0;
	double speed = 0.0;
	bool onGround = FALSE;
};

Wasmo* Wasmo::create() {
	return new RudderTillerzmo();
}

void RudderTillerzmo::init() {
#if _DEBUG
	cout << "RudderTillerzmo: map client events" << endl;
#endif
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_RUDDER, "AXIS_RUDDER_SET");
	SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_TILLER, "RUDDER_AXIS_MINUS");

#if _DEBUG
	cout << "RudderTillerzmo: OnOpen add to group" << endl;
#endif
	SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_RUDDER_TILLER, EVENT_RUDDER, TRUE);
	SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_RUDDER_TILLER, EVENT_TILLER, TRUE);

#if _DEBUG
	cout << "RudderTillerzmo: OnOpen set group priority" << endl;
#endif
	SimConnect_SetNotificationGroupPriority(g_hSimConnect, GROUP_RUDDER_TILLER, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE);

#if _DEBUG
	cout << "RudderTillerzmo: add data definition" << endl;
#endif
	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPEED,
		"GROUND VELOCITY", "Knots", SIMCONNECT_DATATYPE_FLOAT64, speedEpsilon);
	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ON_GROUND,
		"SIM ON GROUND", "Bool", SIMCONNECT_DATATYPE_INT32, 0.5);

#if _DEBUG
	cout << "RudderTillerzmo: requesting data feeds" << endl;
#endif
	SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_SPEED, DEFINITION_SPEED,
		SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, 10, 0);
	SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_ON_GROUND, DEFINITION_ON_GROUND,
		SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0);
}

void RudderTillerzmo::Handle(SIMCONNECT_RECV_EVENT* evt) {
#if _DEBUG
	cout << "RudderTillerzmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;
#endif
	switch (evt->uEventID) {
	case EVENT_RUDDER:
		pedalsDemand = static_cast<long>(evt->dwData) / maxRawMagnitude;
#if _DEBUG
		cout << "RudderTillerzmo: user has asked to set rudder to " << pedalsDemand << endl;
#endif
		break;
	case EVENT_TILLER:
		tillerDemand = static_cast<long>(evt->dwData) / maxRawMagnitude;
#if _DEBUG
		cout << "RudderTillerzmo: user has asked to set tiller to " << tillerDemand << endl;
#endif
		break;
	default:
		cout << "RudderTillerzmo: Received unknown event " << evt->uEventID << endl;
		return;
	}
	SendDemand();
}

void RudderTillerzmo::Handle(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
#if _DEBUG
	cout << "RudderTillerzmo: received data " << pObjData->dwRequestID << endl;
#endif
	switch (pObjData->dwRequestID) {
	case REQUEST_SPEED: {
		speed = ((SpeedData*)(&pObjData->dwData))->groundSpeed;
#if _DEBUG
		cout << "RudderTillerzmo: updated speed to " << speed << endl;
#endif
		break;
	}
	case REQUEST_ON_GROUND: {
		onGround = ((GroundData*)(&pObjData->dwData))->isOnGround;
#if _DEBUG
		cout << "RudderTillerzmo: updated on ground to " << onGround << endl;
#endif
		break;
	}
	default:
		cout << "RudderTillerzmo: Received unknown data: " << pObjData->dwRequestID << endl;
		return;
	}
	SendDemand();
}

void RudderTillerzmo::WriteDefaultSection(ofstream& out) {
	out << "MergeTiller=false" << endl;
}

void RudderTillerzmo::AircraftLoaded(INIReader& config, std::string section) {
	isTillerMerged = config.GetBoolean(section, "MergeTiller", FALSE);
}

void RudderTillerzmo::SendDemand() {
#if _DEBUG
	cout << "RudderTillerzmo: " << pedalsDemand << " plus " << tillerDemand
		<< " at " << speed << "kts " << (onGround ? "on ground" : "in air") << endl;
#endif
	// See also https://github.com/flybywiresim/a32nx/pull/769
	// When stopped, allow full pedal control (for control check).
	// Up to 20 knots, tiller should get full control, pedals only 6/75ths.
	// The tiller should change linearly up to 80 knots, when the tiller should have no effect.
	// The pedals should change linearly up to 40 knots, above which they should have full effect.
	auto tillerFactor = isTillerMerged && onGround ?
		min(1.0, max(0.0, (80.0 - speed) / 60.0)) : 0.0;
	auto pedalsFactor = isTillerMerged && onGround && speed > speedEpsilon ?
		min(1.0, max(0.08, 1.0 - 0.92 * ((40.0 - speed) / 20.0))) : 1.0;
	auto modulatedDemand = max(min(pedalsDemand * pedalsFactor + tillerDemand * tillerFactor, 1.0), -1.0);
#if _DEBUG
	cout << "RudderTillerzmo: modulated demand " << modulatedDemand
		<< " from factors for tiller " << tillerFactor << " and pedals " << pedalsFactor << endl;
#endif
	auto value = static_cast<long>(maxRawMagnitude * modulatedDemand);
	SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_RUDDER, value,
		GROUP_RUDDER_TILLER, SIMCONNECT_EVENT_FLAG_DEFAULT);
}
