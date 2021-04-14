#include <Wasmo.h>

#include <cstddef>
#include <map>
#include <fstream>
#include <iostream>

using namespace std;

enum CONTROLZMO_ID {
	EVENT_6HZ,
	EVENT_1SEC,
	EVENT_4SEC,
	CLIENT_DATA_ID_VSPEED_CALLS,
	CLIENT_DATA_DEFINITION_VSPEED_CALLS,
	REQUEST_ID_LVAR_REQUEST,
	CLIENT_DATA_ID_LVAR_REQUEST,
	CLIENT_DATA_ID_LVAR_RESPONSE,
	CLIENT_DATA_DEFINITION_LVAR,
};

// Safer to go from largest to smallest, to avoid padding anywhere but at the end...
struct alignas(8) PMCallsClientData {
	INT16 v1;
	INT16 vr;
	INT8 autobrake; // 0 for off, 1 for lo, 2 for med, 3 for max
	INT8 weatherRadar; // 0 = 1, 1 = off, 2 = 2
	INT8 pws;
	INT8 tcas; // 0 standby, 1 TA, 2 TA/RA
	INT8 tcasTraffic; // 0-3
};

const int32_t LVAR_POLL_ONCE = 0;
const int32_t LVAR_POLL_6Hz = 167;
const int32_t LVAR_POLL_1SEC = 1000;
const int32_t LVAR_POLL_4SEC = 4000;
struct alignas(8) LVarData {
	char varName[52];
	int32_t id; // One of LVAR_POLL_* on input; -1 on output if variable not defined.
	double value; // On request, sets the "current" value.
};

struct alignas(8) LVarState {
	int32_t id;
	int32_t milliseconds;
	double value;
};

struct Controlzmo : Wasmo {
	Controlzmo() : Wasmo("Controlzmo") {
#if TRUE
		lvarStates["A32NX_OVHD_APU_MASTER_SW_PB_IS_ON"] = LVarState { -1, LVAR_POLL_4SEC, 66.6 };
#endif
	}
	void init();
	void Handle(SIMCONNECT_RECV_EVENT*);
	void Handle(SIMCONNECT_RECV_CLIENT_DATA*);
private:
	void CheckAndSend(int32_t, bool);
	map<string, LVarState> lvarStates;
};

Wasmo* Wasmo::create() {
	return new Controlzmo();
}

void Controlzmo::init() {
#if _DEBUG
	cout << "Controlzmo: init" << endl;
#endif
	SimConnect_MapClientDataNameToID(g_hSimConnect, "Controlzmo.VSpeeds", CLIENT_DATA_ID_VSPEED_CALLS);
	SimConnect_CreateClientData(g_hSimConnect, CLIENT_DATA_ID_VSPEED_CALLS, sizeof(PMCallsClientData), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_READ_ONLY);
#if _DEBUG
	cout << "Controlzmo: created VSPeeds client data of size " << sizeof(PMCallsClientData) << "; #" << GetLastSentPacketID() << endl;
#endif

	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT16);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT16);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT8);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT8);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT8);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT8);
	// Handle the last one differently so that we can pad properly.
	DWORD offset = SIMCONNECT_CLIENTDATAOFFSET_AUTO; //offsetof(struct PMCallsClientData, tcasTraffic);
	DWORD size = sizeof(PMCallsClientData::tcasTraffic);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, offset, size);
	DWORD padding = alignof(PMCallsClientData) - (size + offset) % alignof(PMCallsClientData);
	if (padding) SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_VSPEED_CALLS, SIMCONNECT_CLIENTDATAOFFSET_AUTO, padding);
#if _DEBUG
	cout << "Controlzmo: added VSpeeds client data defs, padding " << padding << " from " << size << "@" << offset << "; #" << GetLastSentPacketID() << endl;
#endif

	SimConnect_MapClientDataNameToID(g_hSimConnect, "Controlzmo.LVarRequest", CLIENT_DATA_ID_LVAR_REQUEST);
	SimConnect_CreateClientData(g_hSimConnect, CLIENT_DATA_ID_LVAR_REQUEST, sizeof(LVarData), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_READ_ONLY);
	SimConnect_MapClientDataNameToID(g_hSimConnect, "Controlzmo.LVarResponse", CLIENT_DATA_ID_LVAR_RESPONSE);
	SimConnect_CreateClientData(g_hSimConnect, CLIENT_DATA_ID_LVAR_RESPONSE, sizeof(LVarData), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_READ_ONLY);
#if _DEBUG
	cout << "Controlzmo: created LVar client data of size " << sizeof(LVarData) << "; #" << GetLastSentPacketID() << endl;
#endif
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_LVAR, SIMCONNECT_CLIENTDATAOFFSET_AUTO, sizeof(LVarData::varName));
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_LVAR, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_INT32);
	SimConnect_AddToClientDataDefinition(g_hSimConnect, CLIENT_DATA_DEFINITION_LVAR, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
#if _DEBUG
	cout << "Controlzmo: added lvar client data defs; #" << GetLastSentPacketID() << endl;
#endif
	SimConnect_RequestClientData(g_hSimConnect, CLIENT_DATA_ID_LVAR_REQUEST, REQUEST_ID_LVAR_REQUEST,
		CLIENT_DATA_DEFINITION_LVAR, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_DEFAULT, 0, 0, 0);
#if _DEBUG
	cout << "Controlzmo: requested lvar client data requests; #" << GetLastSentPacketID() << endl;
#endif

#if FALSE
	SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_6HZ, "6Hz");
	SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_1SEC, "1sec");
#endif
	SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_4SEC, "4sec");
#if _DEBUG
	cout << "Controlzmo: requested sim data; #" << GetLastSentPacketID() << endl;
#endif
}

void ensureTerminated(char* s, size_t n) {
	s[n - 1] = '\0';
}

void Controlzmo::Handle(SIMCONNECT_RECV_CLIENT_DATA* clientData) {
#if _DEBUG
	cout << "Controlzmo: received client data " << clientData->dwRequestID << " def " << clientData->dwDefineID << endl;
#endif
	switch (clientData->dwRequestID) {
	case REQUEST_ID_LVAR_REQUEST: {
		LVarData* data = (LVarData*)&(clientData->dwData);
		ensureTerminated(data->varName, sizeof(data->varName));
#if _DEBUG
		cout << "Controlzmo: asking for " << data->varName << " value code " << data->value << endl;
#endif
		lvarStates[data->varName] = LVarState { -1, data->id, data->value };
		break;
	}
	default:
		cout << "Controlzmo: Received unknown client data request: " << clientData->dwRequestID << endl;
		return;
	}
}

void Controlzmo::CheckAndSend(int32_t period, bool checkId) {
	LVarData toSend;
	for (auto nameState = lvarStates.begin(); nameState != lvarStates.end(); ++nameState) {
		if (nameState->second.milliseconds != period) continue;

		PCSTRINGZ lvarName = nameState->first.c_str();
		if (checkId) {
			toSend.id = check_named_variable(lvarName);
		} else if (toSend.id == -1) {
			continue;
		}

		toSend.value = get_named_variable_value(toSend.id);
		if (toSend.value == nameState->second.value) continue;

		strcpy(toSend.varName, lvarName);
		SimConnect_SetClientData(g_hSimConnect, CLIENT_DATA_ID_LVAR_RESPONSE, CLIENT_DATA_DEFINITION_LVAR,
			SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0, sizeof(toSend), &toSend);
#if _DEBUG
		cout << "Controlzmo: set LVar client data "
			<< toSend.varName << " (" << toSend.id << ") = " << toSend.value
			<< " ; #" << GetLastSentPacketID() << endl;
#endif
	}
}

void Controlzmo::Handle(SIMCONNECT_RECV_EVENT* eventData) {
#if _DEBUG && FALSE
	cout << "Controlzmo: received event " << eventData->uEventID << endl;
#endif
	switch (eventData->uEventID) {
	case EVENT_6HZ:
		CheckAndSend(LVAR_POLL_6Hz, false);
		break;
	case EVENT_1SEC:
		CheckAndSend(LVAR_POLL_1SEC, false);
		break;
	case EVENT_4SEC: {
		CheckAndSend(LVAR_POLL_4SEC, true);

		// IDs are -1 when not known
		ID v1Id = check_named_variable("AIRLINER_V1_SPEED");
		ID vrId = check_named_variable("AIRLINER_VR_SPEED");
		ID autobrakeId = check_named_variable("XMLVAR_Autobrakes_Level");
		ID radarId = check_named_variable("XMLVAR_A320_WeatherRadar_Sys");
		ID pwsId = check_named_variable("A32NX_SWITCH_RADAR_PWS_Position");
		ID tcasId = check_named_variable("A32NX_SWITCH_TCAS_Position");
		ID tcasTrafficId = check_named_variable("A32NX_SWITCH_TCAS_Traffic_Position");

		if (vrId == -1 && v1Id == -1 && autobrakeId == -1 && radarId == -1 && pwsId == -1 && tcasId == -1 && tcasTrafficId == -1) {
#if _DEBUG
			cout << "Controlzmo: no IDs found; skipping send" << endl;
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
			get_named_variable_value(tcasTrafficId),
		};
#if _DEBUG
		cout << "Controlzmo: pilot monitoring calls data RX:"
			<< " V1 " << clientData.v1 << " id " << v1Id
			<< " VR " << clientData.vr << " id " << vrId
			<< " autobrake " << (int)clientData.autobrake << " id " << autobrakeId << endl;
		cout << "Controlzmo:\tRX continued:"
			<< " radar " << (int)clientData.weatherRadar << " id " << radarId
			<< " pws " << (int)clientData.pws << " id " << pwsId
			<< " TCAS " << (int)clientData.tcas << " id " << tcasId
			<< " traffic " << (int)clientData.tcasTraffic << " id " << tcasTrafficId
			<< endl;
#endif
#if FALSE
		// Phase isn't 100% reliable; goes through 3 and 4 on the first run, but sometimes a second takeoff run goes straight from 2 to 8, especially if using FLEX.
		ID fwcPhaseId = check_named_variable("A32NX_FWC_FLIGHT_PHASE"); // See src/systems/systems/src/shared/mod.rs - 3 and 4 are the relevant ones
		int fwcPhase = get_named_variable_value(fwcPhaseId);
		cout << "Controlzmo: phase FWC >>>" << fwcPhase << "<<< id " << fwcPhaseId << endl;
#endif
		SimConnect_SetClientData(g_hSimConnect, CLIENT_DATA_ID_VSPEED_CALLS, CLIENT_DATA_DEFINITION_VSPEED_CALLS,
			SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0, sizeof(clientData), &clientData);
#if _DEBUG
		cout << "Controlzmo: set client data; #" << GetLastSentPacketID() << endl;
#endif
		break;
	}
	default:
		cout << "Controlzmo: Received unknown event: " << eventData->uEventID << endl;
		return;
	}
}
