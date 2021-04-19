#include <Wasmo.h>

#include <map>
#include <fstream>
#include <iostream>

using namespace std;

enum CONTROLZMO_ID {
	EVENT_6HZ,
	EVENT_1SEC,
	EVENT_4SEC,
	REQUEST_ID_LVAR_REQUEST,
	CLIENT_DATA_ID_LVAR_REQUEST,
	CLIENT_DATA_ID_LVAR_RESPONSE,
	CLIENT_DATA_DEFINITION_LVAR,
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
	Controlzmo() : Wasmo("Controlzmo") { }
	void init();
	void Handle(SIMCONNECT_RECV_EVENT*);
	void Handle(SIMCONNECT_RECV_CLIENT_DATA*);
private:
	void CheckEachAndSend(int32_t, bool);
	void Send(PCSTRINGZ lvarName, LVarState& state);
	map<string, LVarState> lvarStates;
};

Wasmo* Wasmo::create() {
	return new Controlzmo();
}

void Controlzmo::init() {
#if _DEBUG
	cout << "Controlzmo: init" << endl;
#endif

	SimConnect_MapClientDataNameToID(g_hSimConnect, "Controlzmo.LVarRequest", CLIENT_DATA_ID_LVAR_REQUEST);
	SimConnect_CreateClientData(g_hSimConnect, CLIENT_DATA_ID_LVAR_REQUEST, sizeof(LVarData), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
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

	SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_6HZ, "6Hz");
	SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_1SEC, "1sec");
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

void Controlzmo::CheckEachAndSend(int32_t period, bool checkId) {
	for (auto nameState = lvarStates.begin(); nameState != lvarStates.end(); ) {
		PCSTRINGZ lvarName = nameState->first.c_str();
		auto isOneOff = nameState->second.milliseconds == 0;
		if (checkId || isOneOff) {
			nameState->second.id = check_named_variable(lvarName);
		}

		if (nameState->second.milliseconds == period || isOneOff)
			Send(lvarName, nameState->second);

		if (isOneOff)
			nameState = lvarStates.erase(nameState);
		else
			++nameState;
	}
}

void Controlzmo::Send(PCSTRINGZ lvarName, LVarState& state) {
	auto newValue = get_named_variable_value(state.id);
	if (newValue == state.value) return;
	state.value = newValue;

	LVarData toSend;
	strcpy(toSend.varName, lvarName);
	toSend.id = state.id;
	toSend.value = newValue;

	SimConnect_SetClientData(g_hSimConnect, CLIENT_DATA_ID_LVAR_RESPONSE, CLIENT_DATA_DEFINITION_LVAR,
		SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0, sizeof(toSend), &toSend);
#if _DEBUG
	cout << "Controlzmo: set LVar client data "
		<< toSend.varName << " (" << toSend.id << ") = " << toSend.value
		<< " ; #" << GetLastSentPacketID() << endl;
#endif
}

void Controlzmo::Handle(SIMCONNECT_RECV_EVENT* eventData) {
#if _DEBUG && FALSE
	cout << "Controlzmo: received event " << eventData->uEventID << endl;
#endif
	switch (eventData->uEventID) {
	case EVENT_6HZ:
		CheckEachAndSend(LVAR_POLL_6Hz, false);
		break;
	case EVENT_1SEC:
		CheckEachAndSend(LVAR_POLL_1SEC, false);
		break;
	case EVENT_4SEC: {
		CheckEachAndSend(LVAR_POLL_4SEC, true);
		break;
	}
	default:
		cout << "Controlzmo: Received unknown event: " << eventData->uEventID << endl;
		return;
	}
}
