#include "VirtualMachine.h"

UINT64 VirtualMachine::GetJobNumber()
{
	return jobNumber;
}

VirtualMachine::VirtualMachine(int runtime, int cputime, UINT64 jobnum)
{
	if (cputime < 0) // no information about cputime
		cputime = runtime; // assuming 100% utilization
	runTimeSec = (FLOATINGPOINT)runtime;
	avgCPUTimeSec = (FLOATINGPOINT)cputime;
	cpuLoadRatio = avgCPUTimeSec/runTimeSec;
	isFinished = true;
	if (runTimeSec > 0.0)
		isFinished = false;
	jobNumber = jobnum;
}

VirtualMachine::~VirtualMachine(void)
{
}

FLOATINGPOINT VirtualMachine::HowMuchCPULoadWillThisVMRequire()
{
	return cpuLoadRatio;
}

FLOATINGPOINT VirtualMachine::RunVMAndReturnActualTime(FLOATINGPOINT sec)
{
	if (isFinished)
		cout << "Error: Finished VM consumed cpu time" << endl;
	if (runTimeSec < sec) {
		isFinished = true;
		runTimeSec = avgCPUTimeSec = 0.0;
		return runTimeSec;
	}
	runTimeSec -= sec;
	avgCPUTimeSec -= sec*cpuLoadRatio;
	return sec;
}

bool VirtualMachine::IsFinished()
{
	return isFinished;
}

FLOATINGPOINT VirtualMachine::GetCPULoadRatio()
{
	return cpuLoadRatio;
}
