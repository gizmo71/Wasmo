#include <Wasmo.h>

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

enum GROUP_ID {
	GROUP_THING = 13,
};

enum EVENT_ID {
	EVENT_TEXT = 4,
};

enum DATA_DEFINE_ID {
	DEFINITION_THING = 42,
};

enum DATA_REQUEST_ID {
	REQUEST_THING = 69,
};

struct Examplezmo : Wasmo {
	Examplezmo() : Wasmo("Examplezmo") { }
	void init();
	void Handle(SIMCONNECT_RECV_EVENT*);
	void Handle(SIMCONNECT_RECV_SIMOBJECT_DATA*);
};

Wasmo* Wasmo::create() {
	return new Examplezmo();
}

void Examplezmo::init() {
	cerr << "Examplezmo: init" << endl;
}

void Examplezmo::Handle(SIMCONNECT_RECV_EVENT* evt) {
	cout << "Examplezmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;
	switch (evt->uEventID) {
	case EVENT_TEXT:
		cout << "Examplezmo: Text event " << hex << evt->dwData << dec << endl;
		break;
	default:
		cerr << "Examplezmo: Received unknown event " << evt->uEventID << endl;
	}
}

void Examplezmo::Handle(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
	cout << "Examplezmo: RX data " << pObjData->dwRequestID << endl;
	switch (pObjData->dwRequestID) {
	default:
		cerr << "Examplezmo: Received unknown data: " << pObjData->dwRequestID << endl;
	}
}
