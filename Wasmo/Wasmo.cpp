#include "Wasmo.h"

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

extern "C" MSFS_CALLBACK void module_init(void) {
#if _DEBUG
	cerr << "Wasmo: init" << endl;
#endif

	wasmo.g_hSimConnect = 0;
	if (FAILED(SimConnect_Open(&wasmo.g_hSimConnect, "Wasmo", nullptr, 0, 0, 0))) {
		cerr << "Examplezmo: Could not open SimConnect connection" << endl;
		return;
	}

	wasmo.init();

#if _DEBUG
	cout << "Wasmo: calling dispatch" << endl;
#endif
	if (FAILED(SimConnect_CallDispatch(wasmo.g_hSimConnect, WasmoDispatch, nullptr))) {
		cerr << "Wasmo: CallDispatch failed" << endl;
	}
#if _DEBUG
	cout << "Wasmo: module initialised" << endl;
#endif
}

extern "C" MSFS_CALLBACK void module_deinit(void) {
	if (!wasmo.g_hSimConnect)
		return;
	if (FAILED(SimConnect_Close(wasmo.g_hSimConnect))) {
		cerr << "Wasmo: Could not close SimConnect connection" << endl;
	}
	wasmo.g_hSimConnect = 0;
}
