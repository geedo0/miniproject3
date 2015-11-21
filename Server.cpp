#include "Server.h"

Server::Server(unsigned int cpuGen)
{
	clock = 0;
	howManySecondsOverEmergencyTemp = 0;
	howManyTimesDVFSChanged = 0;
	currentPerformanceStateOutof100 = 100;
	currentPowerDraw = 0.0;
	currentFanPowerDraw = 0.0;
	vRunningVMs.clear();
	vFinishedVMs.clear();
	currentCPUPowerFactor = 1.0;
	currentPerformanceFactor = 1.0;
	isOFF = false;
	for (int i=0; i<SIZE_OF_HEAT_TIMING_BUFFER; ++i) {
		additionalHeatTimingBuffer[i] = 0.0;
		supplyTempTimingBuffer[i] = LOWEST_SUPPLY_TEMPERATURE;
	}

	cpuGeneration = cpuGen;
	switch (NUMBER_OF_CORES_IN_ONE_SERVER) {
	case 2: case 4:
		cpuTDP = 80.0; // Xeon E5502 / Xeon E3-1235
		break;
	case 8:
		cpuTDP = 105.0; // Xeon E3-2820
		break;
	case 10:
		cpuTDP = 130.0; // Xeon E7-2850
		break;
	case 16:
		cpuTDP = 210.0; // 8 cores X2
		break;
	default:
		cpuTDP = 100.0;
	}
	if (cpuTDP < 1.0)
		cout << "Error: cpu tdp is less than 1W" << endl;

	for (unsigned int i=0; i<cpuGeneration; ++i) {
		cpuTDP *= 0.8;
	}

	coolerMaxRPM = 3000.0;
	coolerMinRPM = 500.0;
	coolerMaxPower = 15.0;
	coolerMaxDieTemperature = 70.0;
	coolerMinTemperatureGap = coolerMaxDieTemperature - EMERGENCY_TEMPERATURE;
}

Server::~Server(void)
{
}

void Server::TurnOFF()
{
	isOFF = true;
}

FLOATINGPOINT Server::VMRequiresThisMuchUtilization()
{
	if (isOFF) return 0.0;
	FLOATINGPOINT sum = 0.0;
	for (vector<VirtualMachine *>::iterator it = vRunningVMs.begin(); it != vRunningVMs.end(); ++it)
		sum += (*it)->HowMuchCPULoadWillThisVMRequire();
	return sum/NUMBER_OF_CORES_IN_ONE_SERVER; // this can be more than (1.0)
}

FLOATINGPOINT Server::VMRequiresThisMuchCPUScale()
{
	if (isOFF) return 0.0;
	FLOATINGPOINT sum = 0.0;
	for (vector<VirtualMachine *>::iterator it = vRunningVMs.begin(); it != vRunningVMs.end(); ++it)
		sum += (*it)->HowMuchCPULoadWillThisVMRequire();
	return sum; // this can be more than (NUMBER_OF_CORES_IN_ONE_SERVER)
}

FLOATINGPOINT Server::GetPowerDraw()
{
	if (isOFF) return 0.0;
	return currentPowerDraw;
}
FLOATINGPOINT Server::GetFanPower()
{
	if (isOFF) return 0.0;
	return currentFanPowerDraw;
}

FLOATINGPOINT Server::MaxUtilization()
{
	if (isOFF) return 0.0;
	return (FLOATINGPOINT)currentPerformanceStateOutof100/100.0;
}

FLOATINGPOINT Server::CurrentUtilization()
{
	if (isOFF) return 0.0;
	FLOATINGPOINT required = VMRequiresThisMuchUtilization();
	FLOATINGPOINT max = MaxUtilization();
	return ((required > max) ? max : required);
}

FLOATINGPOINT Server::AvailableUtilization()
{
	if (isOFF) return 0.0;
	FLOATINGPOINT avail = MaxUtilization() - CurrentUtilization();
	if (avail < 0.0)
		avail = 0.0;
	return avail;
}

void Server::EveryASecond()
{
	if (isOFF) return;

	// Recalculate how much this server can perform
	RecalculatePerformanceByTemperature();

	// run VMs
	FLOATINGPOINT sumUtil = VMRequiresThisMuchUtilization();
	FLOATINGPOINT maxUtil = MaxUtilization();
	if (sumUtil <= maxUtil) { // run every VM for a sec
		for (vector<VirtualMachine *>::iterator it = vRunningVMs.begin(); it != vRunningVMs.end(); ++it)
			(*it)->RunVMAndReturnActualTime(1.0);
	}
	else { // partially run every VM
		for (vector<VirtualMachine *>::iterator it = vRunningVMs.begin(); it != vRunningVMs.end(); ++it)
			(*it)->RunVMAndReturnActualTime(maxUtil/sumUtil);
	}

	// detect finished VMs. TODO: this goto statement is correct but ugly
	BEFORE_DEFINING_ITERATOR:
	for (vector<VirtualMachine *>::iterator it = vRunningVMs.begin(); it != vRunningVMs.end(); ++it) {
		if ((*it)->IsFinished()) {
			vFinishedVMs.push_back((*it));
			vRunningVMs.erase(it);
			goto BEFORE_DEFINING_ITERATOR;
		}
	}

	CalculatePowerDraw(CurrentUtilization(), CurrentInletTemperature());

	ClockPlusPlus();
}

void Server::ClockPlusPlus()
{
	// empty current additionalHeatTimingBuffer
	unsigned int slotIndex = clock % SIZE_OF_HEAT_TIMING_BUFFER;
	additionalHeatTimingBuffer[slotIndex] = 0.0;

	clock++;
}

void Server::AssignOneVM(VirtualMachine *vm)
{
	if (isOFF) cout << "Error: VM assigned to a turned off machine" << endl;
	vRunningVMs.push_back(vm);
	CalculatePowerDraw(CurrentUtilization(), CurrentInletTemperature());
}

VirtualMachine* Server::TakeAVM()
{
	if (vRunningVMs.empty())
		return NULL;
	VirtualMachine* retVal = vRunningVMs.back();
	vRunningVMs.pop_back();
	CalculatePowerDraw(CurrentUtilization(), CurrentInletTemperature());
	return retVal;
}

void Server::RemoveTheLastAssignedVM()
{
	if (isOFF) cout << "Error: VM removed from a turned off machine" << endl;
	vRunningVMs.pop_back();
}

vector<VirtualMachine *>* Server::GetFinishedVMVector()
{
	if (isOFF) cout << "Error: Get Finished VM Vector called to a turned off machine" << endl;
	return &vFinishedVMs;
}

void Server::RecalculatePerformanceByTemperature()
{
	if (isOFF) cout << "Error: SetInletTemperature called to a turned off machine" << endl;
	int oldPerformanceStateOutof100 = currentPerformanceStateOutof100;

	FLOATINGPOINT inletTempNow = CurrentInletTemperature();

	if (inletTempNow <= (EMERGENCY_TEMPERATURE))
		currentPerformanceStateOutof100 = 100;
	else {
		currentPerformanceStateOutof100 = 100;
		howManySecondsOverEmergencyTemp++;
	}

	if (TEMPERATURE_SENSING_PERFORMANCE_CAPPING)
	{
		// 1/30 CPU power down for every 1'c up
		currentCPUPowerFactor = 1.0 - (inletTempNow-(EMERGENCY_TEMPERATURE))/30;
		currentPerformanceFactor = sqrt(currentCPUPowerFactor);
		//currentPerformanceFactor = currentCPUPowerFactor;
		if (currentCPUPowerFactor > 1.0)
			currentCPUPowerFactor = 1.0;
		if (currentPerformanceFactor > 1.0)
			currentPerformanceFactor = 1.0;
		currentPerformanceStateOutof100 = (int)(currentPerformanceFactor*100);
	}

	if (currentPerformanceStateOutof100 > 100)
		currentPerformanceStateOutof100 = 100;

	if (currentPerformanceStateOutof100 != oldPerformanceStateOutof100)
		howManyTimesDVFSChanged++;
}

unsigned int Server::HowManySecondsOverEmergencyTemp()
{
	return howManySecondsOverEmergencyTemp;
}

unsigned int Server::HowManyTimesDVFSChanged()
{
	return howManyTimesDVFSChanged;
}

void Server::CalculatePowerDraw(FLOATINGPOINT utilization, FLOATINGPOINT temperature)
{
	if (isOFF) {
		currentPowerDraw = currentFanPowerDraw = 0.0;
		return;
	}

	FLOATINGPOINT idlePower = cpuTDP; // assuming half power when idle
	FLOATINGPOINT currentCPUpower = cpuTDP*utilization;
	FLOATINGPOINT temperatureGap = coolerMaxDieTemperature - temperature;
	FLOATINGPOINT additionalFanPower;

	if (CONSTANT_FAN_POWER) {
		currentPowerDraw = currentCPUpower + idlePower;
		currentFanPowerDraw = coolerMaxPower*2; // two fans
		return;
	}

	// cpu cooling
	{
		FLOATINGPOINT targetRPM = (currentCPUpower/temperatureGap)/(cpuTDP/coolerMinTemperatureGap)*coolerMaxRPM;
		if (targetRPM < coolerMinRPM)
			targetRPM = coolerMinRPM;
		if (!FAN_RPM_NO_LIMIT) {
			if (targetRPM > coolerMaxRPM)
				targetRPM = coolerMaxRPM;
		}
		FLOATINGPOINT fanFactor = (targetRPM/coolerMaxRPM)*(targetRPM/coolerMaxRPM)*(targetRPM/coolerMaxRPM);
		additionalFanPower = coolerMaxPower * fanFactor;
	}

	// case cooling (two fans)
	{
		FLOATINGPOINT targetRPM = (currentCPUpower/cpuTDP*(coolerMaxRPM-coolerMinRPM) + coolerMinRPM /* -> for removing idle power */ ) * temperature/EMERGENCY_TEMPERATURE;
		if (targetRPM < coolerMinRPM)
			targetRPM = coolerMinRPM;
		if (!FAN_RPM_NO_LIMIT) {
			if (targetRPM > coolerMaxRPM)
				targetRPM = coolerMaxRPM;
		}
		FLOATINGPOINT fanFactor = (targetRPM/coolerMaxRPM)*(targetRPM/coolerMaxRPM)*(targetRPM/coolerMaxRPM);
		additionalFanPower += 2 *(coolerMaxPower * fanFactor);
	}

	currentPowerDraw = currentCPUpower + idlePower + additionalFanPower;
	currentFanPowerDraw = additionalFanPower;
}

bool Server::IsFinished()
{
	if (isOFF) return true;
	if (!vRunningVMs.empty())
		return false;
	return true;
}

void Server::AddHeatToTimingBuffer(FLOATINGPOINT temperature, int timing)
{
	unsigned int slotIndex = (clock + timing) % SIZE_OF_HEAT_TIMING_BUFFER;
	additionalHeatTimingBuffer[slotIndex] += temperature;
}

void Server::SetSupplyTempToTimingBuffer(FLOATINGPOINT temperature, int timing)
{
	unsigned int slotIndex = (clock + timing) % SIZE_OF_HEAT_TIMING_BUFFER;
	supplyTempTimingBuffer[slotIndex] = temperature;
}

FLOATINGPOINT Server::ReadHeatFromTimingBuffer()
{
	unsigned int slotIndex = clock % SIZE_OF_HEAT_TIMING_BUFFER;
	return additionalHeatTimingBuffer[slotIndex];
}

FLOATINGPOINT Server::CurrentInletTemperature()
{
	unsigned int slotIndex = clock % SIZE_OF_HEAT_TIMING_BUFFER;
	return (supplyTempTimingBuffer[slotIndex]+additionalHeatTimingBuffer[slotIndex]);
}

FLOATINGPOINT Server::CurrentAddedTemperature()
{
	unsigned int slotIndex = clock % SIZE_OF_HEAT_TIMING_BUFFER;
	return additionalHeatTimingBuffer[slotIndex];
}

void Server::SetCPUGeneration(unsigned int gen)
{
	cpuGeneration = gen;
}
