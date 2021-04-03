#include <Wasmo.h>

#include <fstream>
#include <iostream>

using namespace std;

// Can we just wrap all the definitions up into a single enum?!
enum DATA_DEFINE_ID {
	EVENT_TICK = 69,
//enum CLIENT_DATA_ID {
	CLIENT_DATA_VSPEED_CALLS = 13,
//enum CLIENT_DATA_DEFINITION_ID {
	CLIENT_DATA_DEFINITION_VSPEED_CALLS = CLIENT_DATA_VSPEED_CALLS,
};

struct PMCallsClientData {
	INT16 v1;
	INT16 vr;
	INT8 autobrake; // 0 for off, 1 for lo, 2 for med, 3 for max
	INT8 weatherRadar; // 0 = 1, 1 = off, 2 = 2
	INT8 pws;
	INT8 tcas; // 0 standby, 1 TA, 2 TA/RA
};

struct Controlzmo : Wasmo {
	Controlzmo() : Wasmo("Controlzmo") { }
	void init();
	void Handle(SIMCONNECT_RECV_EVENT*);
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
	SimConnect_CreateClientData(g_hSimConnect, CLIENT_DATA_VSPEED_CALLS, sizeof(PMCallsClientData), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_READ_ONLY);
#if _DEBUG
	cout << "Controlzmo: created client data; #" << GetLastSentPacketID() << endl;
#endif

	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT16);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT16);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT8);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT8);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT8);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT8);
#if _DEBUG
	cout << "Controlzmo: added client data defs; #" << GetLastSentPacketID() << endl;
#endif

	SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_TICK, "1sec");
#if _DEBUG
	cout << "Controlzmo: requested sim data; #" << GetLastSentPacketID() << endl;
#endif
}

void Controlzmo::Handle(SIMCONNECT_RECV_EVENT* pEvtData) {
#if _DEBUG
	cout << "Controlzmo: received event " << pEvtData->uEventID << endl;
#endif
	switch (pEvtData->uEventID) {
	case EVENT_TICK: {
		// IDs are -1 when not known
		ID v1Id = check_named_variable("AIRLINER_V1_SPEED");
		ID vrId = check_named_variable("AIRLINER_VR_SPEED");
		ID autobrakeId = check_named_variable("XMLVAR_Autobrakes_Level");
		ID radarId = check_named_variable("XMLVAR_A320_WeatherRadar_Sys");
		ID pwsId = check_named_variable("A32NX_SWITCH_RADAR_PWS_Position");
		ID tcasId = check_named_variable("A32NX_SWITCH_TCAS_Position");

		if (vrId == -1 || v1Id == -1 || autobrakeId == -1 || radarId == -1 || pwsId == -1 || tcasId == -1) {
#if _DEBUG
			cout << "Controlzmo: at least one ID not found; skipping send" << endl;
#endif
			break;
		}
		PMCallsClientData clientData {
			get_named_variable_value(v1Id),
			get_named_variable_value(vrId),
			get_named_variable_value(autobrakeId),
			get_named_variable_value(radarId),
			get_named_variable_value(pwsId),
			get_named_variable_value(tcasId),
		};
		if (clientData.weatherRadar == 0 || clientData.weatherRadar == 1) clientData.weatherRadar = 1 - clientData.weatherRadar;
#if _DEBUG
		cout << "Controlzmo: pilot monitoring calls data RX:"
			<< " V1 " << clientData.v1 << " id " << v1Id
			<< " VR " << clientData.vr << " id " << vrId
			<< " autobrake " << (int)clientData.autobrake << " id " << autobrakeId << endl;
		cout << "Controlzmo:\tRX continued:"
			<< " radar " << (int)clientData.weatherRadar << " id " << radarId
			<< " pws " << (int)clientData.pws << " id " << pwsId
			<< " TCAS " << (int)clientData.tcas << " id " << tcasId
			<< endl;
#endif
#if FALSE
		// Phase isn't 100% reliable; goes through 3 and 4 on the first run, but sometimes a second takeoff run goes straight from 2 to 8, especially if using FLEX.
		ID fwcPhaseId = check_named_variable("A32NX_FWC_FLIGHT_PHASE"); // See src/systems/systems/src/shared/mod.rs - 3 and 4 are the relevant ones
		int fwcPhase = get_named_variable_value(fwcPhaseId);
		cout << "Controlzmo: phase FWC >>>" << fwcPhase << "<<< id " << fwcPhaseId << endl;
#endif
		SimConnect_SetClientData(g_hSimConnect, CLIENT_DATA_VSPEED_CALLS, CLIENT_DATA_DEFINITION_VSPEED_CALLS,
			SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0, sizeof(clientData), &clientData);
#if _DEBUG
		cout << "Controlzmo: set client data; #" << GetLastSentPacketID() << endl;
#endif
		break;
	}
	default:
		cout << "Controlzmo: Received unknown event: " << pEvtData->uEventID << endl;
		return;
	}
}
