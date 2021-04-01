#include <Wasmo.h>
#include <MSFS/Legacy/gauges.h>

#include <fstream>
#include <iostream>

using namespace std;

// Can we just wrap all the definitions up into a single enum?!
enum DATA_DEFINE_ID {
	DEFINITION_VSPEED_CALLS = 69,
};

struct VSpeedCallsData {
	double airSpeed;
};

enum DATA_REQUEST_ID {
	REQUEST_VSPEED_CALLS = 42,
};

enum CLIENT_DATA_ID {
	CLIENT_DATA_VSPEED_CALLS = 13,
};
enum CLIENT_DATA_DEFINITION_ID {
	CLIENT_DATA_DEFINITION_VSPEED_CALLS = CLIENT_DATA_VSPEED_CALLS,
};

struct VSpeedCallsClientData {
	double airSpeed;
	double v1;
	double vr;
	double autobrake; // 0 for off, 1 for lo, 2 for med, 3 for max
};

struct Controlzmo : Wasmo {
	Controlzmo() : Wasmo("Controlzmo") { }
	void init();
	void Handle(SIMCONNECT_RECV_SIMOBJECT_DATA*);
private:
};

Wasmo* Wasmo::create() {
	return new Controlzmo();
}

void Controlzmo::init() {
#if _DEBUG
	cout << "Controlzmo: init" << endl;
#endif
	SimConnect_MapClientDataNameToID(g_hSimConnect, "Controlzmo.VSpeeds", CLIENT_DATA_VSPEED_CALLS);
	SimConnect_CreateClientData(g_hSimConnect, CLIENT_DATA_VSPEED_CALLS, sizeof(VSpeedCallsClientData), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_READ_ONLY);
#if _DEBUG
	cout << "Controlzmo: created client data; #" << GetLastSentPacketID() << endl;
#endif

	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
#if _DEBUG
	cout << "Controlzmo: added client data defs; #" << GetLastSentPacketID() << endl;
#endif

	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_VSPEED_CALLS,
		"AIRSPEED INDICATED", "Knots", SIMCONNECT_DATATYPE_FLOAT64, 1.0f);
	SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_VSPEED_CALLS, DEFINITION_VSPEED_CALLS,
		SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0);
#if _DEBUG
	cout << "Controlzmo: requested sim data; #" << GetLastSentPacketID() << endl;
#endif
}

void Controlzmo::Handle(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
#if _DEBUG
	cout << "Controlzmo: received data " << pObjData->dwRequestID << endl;
#endif
	switch (pObjData->dwRequestID) {
	case REQUEST_VSPEED_CALLS: {
		VSpeedCallsData* data = (VSpeedCallsData*)(&pObjData->dwData);
		// IDs are -1 when not known
		ID v1Id = check_named_variable("AIRLINER_V1_SPEED");
		ID vrId = check_named_variable("AIRLINER_VR_SPEED");
		ID autobrakeId = check_named_variable("XMLVAR_Autobrakes_Level");
		if (vrId == -1 || v1Id == -1 || autobrakeId == -1) {
#if _DEBUG
			cout << "Controlzmo: one or more ID not found; skipping send" << endl;
#endif
			break;
		}
		VSpeedCallsClientData clientData{ data->airSpeed,
			get_named_variable_value(v1Id), get_named_variable_value(vrId), get_named_variable_value(autobrakeId) };
#if _DEBUG
		cout << "Controlzmo: VSpeed calls data RX"
			<< " airspeed " << clientData.airSpeed
			<< " V1 " << clientData.v1 << " id " << v1Id
			<< " VR " << clientData.vr << " id " << vrId
			<< " autobrake " << clientData.autobrake << " id " << autobrakeId
			<< endl;
		// Phase isn't 100% reliable; goes through 3 and 4 on the first run, but sometimes a second takeoff run goes straight from 2 to 8, especially if using FLEX.
		ID fwcPhaseId = check_named_variable("A32NX_FWC_FLIGHT_PHASE"); // See src/systems/systems/src/shared/mod.rs - 3 and 4 are the relevant ones
		int fwcPhase = get_named_variable_value(fwcPhaseId);
		cout << "Controlzmo: phase FWC >>>" << fwcPhase << "<<< id " << fwcPhaseId << endl;
#endif
		if (clientData.v1 != -1 || clientData.vr != -1) {
			SimConnect_SetClientData(g_hSimConnect, CLIENT_DATA_VSPEED_CALLS, CLIENT_DATA_DEFINITION_VSPEED_CALLS,
				SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0, sizeof(clientData), &clientData);
#if _DEBUG
			cout << "Controlzmo: set client data; #" << GetLastSentPacketID() << endl;
#endif
		}
		break;
	}
	default:
		cout << "Controlzmo: Received unknown data: " << pObjData->dwRequestID << endl;
		return;
	}
}
