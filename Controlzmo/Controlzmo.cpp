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
	byte phase;
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

	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT8);

	SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_VSPEED_CALLS,
		"AIRSPEED INDICATED", "Knots", SIMCONNECT_DATATYPE_FLOAT64, 1.0f);
	SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_VSPEED_CALLS, DEFINITION_VSPEED_CALLS,
		SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0);
}

void Controlzmo::Handle(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
#if _DEBUG
	cout << "Controlzmo: received data " << pObjData->dwRequestID << endl;
#endif
	switch (pObjData->dwRequestID) {
	case REQUEST_VSPEED_CALLS: {
		VSpeedCallsData* data = (VSpeedCallsData*)(&pObjData->dwData);
		ID v1Id = check_named_variable("AIRLINER_V1_SPEED");
		ID vrId = check_named_variable("AIRLINER_VR_SPEED");
		ID phaseId = check_named_variable("A32NX_FMGC_FLIGHT_PHASE");
		VSpeedCallsClientData clientData { data->airSpeed,
			get_named_variable_value(v1Id), get_named_variable_value(vrId),
			get_named_variable_value(phaseId) };
#if _DEBUG
		cout << "Controlzmo: VSpeed calls data RX " << clientData.airSpeed << " V1 " << clientData.v1 << " VR " << clientData.vr << " phase " << clientData.phase << endl;
#endif
		if (clientData.v1 != -1 || clientData.vr != -1)
			SimConnect_SetClientData(g_hSimConnect, CLIENT_DATA_VSPEED_CALLS, CLIENT_DATA_DEFINITION_VSPEED_CALLS,
				SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0, sizeof(clientData), &clientData);
		break;
	}
	default:
		cerr << "Controlzmo: Received unknown data: " << pObjData->dwRequestID << endl;
		return;
	}
}
