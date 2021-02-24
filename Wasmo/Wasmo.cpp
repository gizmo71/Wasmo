#include "Wasmo.h"

#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

HANDLE g_hSimConnect;

extern "C" MSFS_CALLBACK void module_deinit(void) {
	if (g_hSimConnect && FAILED(SimConnect_Close(g_hSimConnect))) {
		cerr << "Wasmo: Could not close SimConnect connection" << endl;
	}
	g_hSimConnect = 0;
}
