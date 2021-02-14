#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>

#include <iostream>

using namespace std;

HANDLE g_hSimConnect;

enum GROUP_ID {
	GROUP_RUDDER_TILLER = 13,
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

enum eEvents {
	EVENT_RUDDER,
	EVENT_TILLER,
};

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);

extern "C" MSFS_CALLBACK void module_init(void) {
	cout << boolalpha << nounitbuf << "RudderTillerzmo: init" << endl;

	g_hSimConnect = 0;
	if (FAILED(SimConnect_Open(&g_hSimConnect, "RudderTillerzmo", nullptr, 0, 0, 0))) {
		cerr << "RudderTillerzmo: Could not open SimConnect connection" << endl;
		return;
	}

// Does this have to all be done before CallDispatch starts, with no subsequent registrations possible?
	cout << "RudderTillerzmo: map client events" << endl;
	if (FAILED(SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_RUDDER, "AXIS_RUDDER_SET"))) {
		cerr << "RudderTillerzmo: couldn't map rudder set event" << endl;
	}
	if (FAILED(SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_TILLER, "RUDDER_AXIS_MINUS"))) {
		cerr << "RudderTillerzmo: couldn't map tiller set event" << endl;
	}

	cout << "RudderTillerzmo: OnOpen add to group" << endl;
	if (FAILED(SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_RUDDER_TILLER, EVENT_RUDDER, TRUE))) {
		cerr << "RudderTillerzmo: couldn't add rudder event to group" << endl;
	}
	if (FAILED(SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, GROUP_RUDDER_TILLER, EVENT_TILLER, TRUE))) {
		cerr << "RudderTillerzmo: couldn't add steering event to group" << endl;
	}

	cout << "RudderTillerzmo: OnOpen set group priority" << endl;
	if (FAILED(SimConnect_SetNotificationGroupPriority(g_hSimConnect, GROUP_RUDDER_TILLER, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE))) {
		cerr << "RudderTillerzmo: couldn't set rudder/tiller notification group priority" << endl;
	}

	cout << "RudderTillerzmo: add data definition" << endl;
	if (FAILED(SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_SPEED,
		"GROUND VELOCITY", "Knots", SIMCONNECT_DATATYPE_FLOAT64, 0.5)))
	{
		cerr << "RudderTillerzmo: couldn't map ground velocity data" << endl;
	}
	if (FAILED(SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ON_GROUND,
		"SIM ON GROUND", "Bool", SIMCONNECT_DATATYPE_INT32, 0.5)))
	{
		cerr << "RudderTillerzmo: couldn't map on ground data" << endl;
	}

	cout << "RudderTillerzmo: requesting data feeds" << endl;
	if (FAILED(SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_SPEED, DEFINITION_SPEED,
		SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0)))
	{
		cerr << "RudderTillerzmo: Could not request speed feed" << endl;
	}
	if (FAILED(SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_ON_GROUND, DEFINITION_ON_GROUND,
		SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0)))
	{
		cerr << "RudderTillerzmo: Could not request on ground feed" << endl;
	}

	cout << "RudderTillerzmo: calling dispatch" << endl;
	if (FAILED(SimConnect_CallDispatch(g_hSimConnect, WasmoDispatch, nullptr))) {
		cerr << "RudderTillerzmo: CallDispatch failed" << endl;
	}
	cout << "RudderTillerzmo: module initialised" << endl;
}

extern "C" MSFS_CALLBACK void module_deinit(void) {
	if (g_hSimConnect && FAILED(SimConnect_Close(g_hSimConnect))) {
		cerr << "RudderTillerzmo: Could not close SimConnect connection" << endl;
	}
	g_hSimConnect = 0;
}

auto pedalsDemand = 0.0;
auto tillerDemand = 0.0;
auto speed = 0.0;
bool onGround = FALSE;

const auto maxRawMagnitude = 16384.0;

void HandleEvent(SIMCONNECT_RECV_EVENT* evt) {
	cout << "RudderTillerzmo: Received event " << evt->uEventID << " in group " << evt->uGroupID << endl;
	switch (evt->uEventID) {
	case EVENT_RUDDER:
		pedalsDemand = static_cast<long>(evt->dwData) / maxRawMagnitude;
		cout << "RudderTillerzmo: user has asked to set rudder to " << pedalsDemand << endl;
		break;
	case EVENT_TILLER:
		tillerDemand = static_cast<long>(evt->dwData) / maxRawMagnitude;
		cout << "RudderTillerzmo: user has asked to set tiller to " << tillerDemand << endl;
		break;
	default:
		cerr << "RudderTillerzmo: Received unknown event " << evt->uEventID << endl;
	}
}

void HandleData(SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData) {
	cout << "RudderTillerzmo: received data " << pObjData->dwRequestID << endl;
	switch (pObjData->dwRequestID) {
	case REQUEST_SPEED: {
		speed = ((SpeedData*)(&pObjData->dwData))->groundSpeed;
		cout << "RudderTillerzmo: updated speed to " << speed << endl;
		break;
	}
	case REQUEST_ON_GROUND: {
		onGround = ((GroundData*)(&pObjData->dwData))->isOnGround;
		cout << "RudderTillerzmo: updated on ground to " << onGround << endl;
		break;
	}
	default:
		cerr << "RudderTillerzmo: Received unknown data: " << pObjData->dwRequestID << endl;
	}
}

void SendDemand() {
	//TODO: why are these all coming out on different lines?!
	cout << "RudderTillerzmo: " << pedalsDemand << " plus " << tillerDemand
		<< " at " << speed << "kts " << (onGround ? "on ground" : "in air") << endl;
	// Could this actually be enough to solve our immediate problem of braking buggering up the tiller?
	// See also https://github.com/flybywiresim/a32nx/pull/769
	auto modulatedDemand = max(min(pedalsDemand + tillerDemand, 1.0), -1.0);
	cout << "RudderTillerzmo: modulated demand " << modulatedDemand << endl;
//TODO: the double version is right but the conversion back to DWORD fails for negative values. :-(
	auto value = static_cast<DWORD>(maxRawMagnitude * modulatedDemand);
	if (FAILED(SimConnect_TransmitClientEvent(g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_RUDDER, value, GROUP_RUDDER_TILLER, SIMCONNECT_EVENT_FLAG_DEFAULT))) {
		cerr << "RudderTillerzmo: Could not fire modulated rudder event" << endl;
	}
}

void CALLBACK WasmoDispatch(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
	cout << "RudderTillerzmo: dispatch " << pData->dwID << endl;
	switch (pData->dwID) {
	case SIMCONNECT_RECV_ID_OPEN:
		cout << "RudderTillerzmo: OnOpen" << endl;
		break;
	case SIMCONNECT_RECV_ID_EXCEPTION: {
		cerr << "RudderTillerzmo: Exception :-(" << endl;
		SIMCONNECT_RECV_EXCEPTION* exception = (SIMCONNECT_RECV_EXCEPTION*)pData;
		// http://www.prepar3d.com/SDKv5/sdk/simconnect_api/references/structures_and_enumerations.html#SIMCONNECT_EXCEPTION
		cerr << "RudderTillerzmo: " << exception->dwException << endl;
		break;
	}
	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
		HandleData((SIMCONNECT_RECV_SIMOBJECT_DATA*)pData);
		SendDemand();
		break;
	}
	case SIMCONNECT_RECV_ID_EVENT:
		HandleEvent((SIMCONNECT_RECV_EVENT*)pData);
		SendDemand();
		break;
	default:
		cerr << "RudderTillerzmo: Received unknown dispatch ID " << pData->dwID << endl;
		break;
	}
	cout << "RudderTillerzmo: done responding" << endl;
}
