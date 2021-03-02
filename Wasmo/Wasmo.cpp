#include "Wasmo.h"

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

HANDLE g_hSimConnect;

Wasmo* wasmo;

extern "C" MSFS_CALLBACK void module_init(void) {
	wasmo = Wasmo::create();
	wasmo->init();

#if _DEBUG
	cout << "Wasmo: calling dispatch" << endl;
#endif
	if (FAILED(SimConnect_CallDispatch(g_hSimConnect, WasmoDispatch, nullptr))) {
		cerr << "Wasmo: CallDispatch failed" << endl;
	}

#if _DEBUG
	cout << "Wasmo: module initialised" << endl;
#endif
}

Wasmo::Wasmo(const char* appName) {
#if _DEBUG
	clog << boolalpha << nounitbuf << "Wasmo::Wasmo for " << appName << endl;
#endif

	g_hSimConnect = 0;
	if (FAILED(SimConnect_Open(&g_hSimConnect, appName, nullptr, 0, 0, 0))) {
		cerr << "Wasmo: Could not open SimConnect connection" << endl;
		return;
	}

#if _DEBUG
	cout << "Wasmo: constructed and connected" << endl;
#endif
}

extern "C" MSFS_CALLBACK void module_deinit(void) {
	if (g_hSimConnect && FAILED(SimConnect_Close(g_hSimConnect))) {
		cerr << "Wasmo: Could not close SimConnect connection" << endl;
	}
	g_hSimConnect = 0;
	delete wasmo;
	wasmo = nullptr;
}
