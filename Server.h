#pragma once

#include <vector>
#include <math.h>

#include "Constants.h"
#include "VirtualMachine.h"

using namespace std;

extern bool TEMPERATURE_SENSING_PERFORMANCE_CAPPING;
extern int TEMPERATURE_SENSING_PERFORMANCE_CAPPING_AGGRESSIVENESS;
extern int NUMBER_OF_CORES_IN_ONE_SERVER;
extern bool CONSTANT_FAN_POWER;
extern bool FAN_RPM_NO_LIMIT;

class Server
{
public:
	Server(unsigned int cpuGen);
	~Server(void);
	void RecalculatePerformanceByTemperature();
	FLOATINGPOINT GetPowerDraw();
	FLOATINGPOINT GetFanPower();
	void EveryASecond();
	bool IsFinished();
	FLOATINGPOINT AvailableUtilization();
	FLOATINGPOINT CurrentUtilization();
	void AssignOneVM(VirtualMachine *);
	void RemoveTheLastAssignedVM();
	vector<VirtualMachine *>* GetFinishedVMVector();
	FLOATINGPOINT VMRequiresThisMuchUtilization();
	FLOATINGPOINT VMRequiresThisMuchCPUScale();
	FLOATINGPOINT MaxUtilization();
	void TurnOFF();
	bool IsOFF() { return isOFF; }
	VirtualMachine* TakeAVM();
	void AddHeatToTimingBuffer(FLOATINGPOINT temperature, int timing);
	void SetSupplyTempToTimingBuffer(FLOATINGPOINT temperature, int timing);
	unsigned int HowManySecondsOverEmergencyTemp();
	unsigned int HowManyTimesDVFSChanged();
	FLOATINGPOINT CurrentInletTemperature();
	FLOATINGPOINT CurrentAddedTemperature();
	void SetCPUGeneration(unsigned int gen);
	//Work with the currentPerformanceStateOutof100 value
	void SetServerPowerState(int max_utilization);
	int GetServerPowerState();

private:
	unsigned int cpuGeneration;
	unsigned int clock;
	unsigned int howManySecondsOverEmergencyTemp;
	unsigned int howManyTimesDVFSChanged;
	bool isOFF;
	vector<VirtualMachine *> vRunningVMs;
	vector<VirtualMachine *> vFinishedVMs;
	FLOATINGPOINT currentPowerDraw;
	FLOATINGPOINT currentFanPowerDraw;
	FLOATINGPOINT currentCPUPowerFactor;
	FLOATINGPOINT currentPerformanceFactor;
	int currentPerformanceStateOutof100;
	int preferredPowerState;
	FLOATINGPOINT supplyTempTimingBuffer[SIZE_OF_HEAT_TIMING_BUFFER];
	FLOATINGPOINT additionalHeatTimingBuffer[SIZE_OF_HEAT_TIMING_BUFFER];
	FLOATINGPOINT cpuTDP;
	FLOATINGPOINT coolerMinTemperatureGap;
	FLOATINGPOINT coolerMaxRPM;
	FLOATINGPOINT coolerMinRPM;
	FLOATINGPOINT coolerMaxDieTemperature;
	FLOATINGPOINT coolerMaxPower;
	
	void ClockPlusPlus();
	void CalculatePowerDraw(FLOATINGPOINT utilization, FLOATINGPOINT temperature);
	FLOATINGPOINT ReadHeatFromTimingBuffer();

};
