#pragma once

#include <iostream>

#include "Constants.h"

using namespace std;

class VirtualMachine
{
public:
	VirtualMachine(int runtime, int cputime, UINT64 jobnum);
	~VirtualMachine(void);
	FLOATINGPOINT HowMuchCPULoadWillThisVMRequire();
	FLOATINGPOINT RunVMAndReturnActualTime(FLOATINGPOINT sec);
	bool IsFinished();
	UINT64 GetJobNumber();
	FLOATINGPOINT GetCPULoadRatio();

private:
	FLOATINGPOINT runTimeSec;
	FLOATINGPOINT avgCPUTimeSec;
	FLOATINGPOINT cpuLoadRatio;
	UINT64 jobNumber;
	bool isFinished;
};

