#include "DataCenter.h"

DataCenter::DataCenter(JobQueue* q)
{
	fpSupplyTemperatureLog = 0.0;
	vSupplyTemperatureSparseLog.clear();

	fpTotalPowerFromComputingMachinesLog = 0.0;
	fpTotalPowerFromServerFansLog = 0.0;
	vTotalPowerFromComputingMachinesSparseLog.clear();
	vTotalPowerFromServerFansSparseLog.clear();

	fpTotalComputingPowerLog = 0.0;
	vTotalComputingPowerSparseLog.clear();

	fpTotalPowerFromCRACLog = 0.0;
	vTotalPowerFromCRACSparseLog.clear();

	fpUtilizationLog = 0.0;
	vUtilizationSparseLog.clear();

	currentSupplyTemp = LOWEST_SUPPLY_TEMPERATURE;
	currentSupplyTempBase = LOWEST_SUPPLY_TEMPERATURE;

	pJobQueue = q;
	clock = 0;
	peakPower = 0.0;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i)
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j)
			pServers[i][j] = new Server(0);
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i)
		perServerPowerLog[i] = perServerTemperatureLog[i] = perServerComputingPowerLog[i] = 0.0;
	if (SCHEDULING_ALGORITHM == "best_performance")
		pSchedulingAlgorithm = new BestPerformanceSchedulingAlgorithm(&pServers, &qWaitingVMs, &HeatRecirculationMatrixD);
	else if (SCHEDULING_ALGORITHM == "uniform_task")
		pSchedulingAlgorithm = new UniformTaskSchedulingAlgorithm(&pServers, &qWaitingVMs, &HeatRecirculationMatrixD);
	else if (SCHEDULING_ALGORITHM == "low_temp_first")
		pSchedulingAlgorithm = new LowTemperatureFirstSchedulingAlgorithm(&pServers, &qWaitingVMs, &HeatRecirculationMatrixD);
	else if (SCHEDULING_ALGORITHM == "random")
		pSchedulingAlgorithm = new RandomSchedulingAlgorithm(&pServers, &qWaitingVMs, &HeatRecirculationMatrixD);
	else if (SCHEDULING_ALGORITHM == "min_hr")
		pSchedulingAlgorithm = new MinHRSchedulingAlgorithm(&pServers, &qWaitingVMs, &HeatRecirculationMatrixD);
	else if (SCHEDULING_ALGORITHM == "center_rack_first")
		pSchedulingAlgorithm = new CenterRackFirstSchedulingAlgorithm(&pServers, &qWaitingVMs, &HeatRecirculationMatrixD);
	else {
		cout << "Error: unknown scheduling algorithm. Use default value (best_performance)" << endl;
		pSchedulingAlgorithm = new BestPerformanceSchedulingAlgorithm(&pServers, &qWaitingVMs, &HeatRecirculationMatrixD);
	}
	perDataCenterUtilization = 0.0;

	for (int i=0; i<SIZE_OF_HR_MATRIX; ++i)
		ModedAirTravelTimeFromCRAC[i] = AirTravelTimeFromCRAC[i] * COOL_AIR_TRAVEL_TIME_MULTIPLIER;

	for (int i=0; i<SIZE_OF_HR_MATRIX; ++i) {
		if (ModedAirTravelTimeFromCRAC[i] > SIZE_OF_HEAT_TIMING_BUFFER)
			cout << "Error: HeatRecirculationTimingFromCRAC["<< i<<"] > " << SIZE_OF_HEAT_TIMING_BUFFER << endl;
	}

	for (int i=0; i<101; ++i) {
		HowManySecondsInThisInletAirTemperature[i] = 0;
		HowManySecondsInThisUtilization[i] = 0;
	}
	CRACDischargeAirTempChangeRate = 0.001 * CRAC_DISCHARGE_CHANGE_RATE_0_00x;
}

DataCenter::~DataCenter(void)
{
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i)
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j)
			delete pServers[i][j];
	delete pSchedulingAlgorithm;
}

void DataCenter::EveryASecond(void)
{
	while (!pJobQueue->IsFinished()) { // JobQueue is not empty
		SingleJob* sj = pJobQueue->TakeFirst();
		for (int i=0; i<sj->numCPUs; ++i) {
			qWaitingVMs.push(new VirtualMachine(sj->runTimeSec, sj->avgCPUTimeSec, sj->jobNumber));
		}
	}

	if (REASSIGN_VMS && (clock%16384==0)) { // re arranging
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) { 
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if (pServers[i][j]->IsOFF()) continue;
				VirtualMachine* pvm;
				while (pvm = (pServers[i][j]->TakeAVM())) {
					if (pvm == NULL)
						break;
					qWaitingVMs.push(pvm);
				}
			}
		}
	}

	// Assign jobs to the servers
	pSchedulingAlgorithm->AssignVMs();

	// call EveryASecond to every server instance
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i)
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j)
			pServers[i][j]->EveryASecond();

	++clock;

	RecalculateHeatDistribution();

	// decide supply temperature for next 1 second
	if (CRAC_SUPPLY_TEMP_IS_CONSTANT_AT > 0) {
		currentSupplyTemp = CRAC_SUPPLY_TEMP_IS_CONSTANT_AT;
	} else if (INSTANT_CHANGE_CRAC_SUPPLY_AIR) {
		FLOATINGPOINT highestAddedTemp = 0.0;
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				FLOATINGPOINT local = pServers[i][j]->CurrentAddedTemperature();
				if (local > highestAddedTemp)
					highestAddedTemp = local;
			}
		}
		currentSupplyTemp = EMERGENCY_TEMPERATURE-highestAddedTemp;
	} else {
		FLOATINGPOINT highestInletTemp = 0.0;
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				FLOATINGPOINT localInlet = pServers[i][j]->CurrentInletTemperature();
				if (localInlet > highestInletTemp)
					highestInletTemp = localInlet;
			}
		}
		if ((EMERGENCY_TEMPERATURE + TEMPERATURE_SENSING_PERFORMANCE_CAPPING_AGGRESSIVENESS + SUPPLY_TEMPERATURE_OFFSET_ALPHA) > highestInletTemp)
			currentSupplyTempBase += CRACDischargeAirTempChangeRate;
		else
			currentSupplyTempBase -= CRACDischargeAirTempChangeRate;
		if (currentSupplyTempBase < LOWEST_SUPPLY_TEMPERATURE)
			currentSupplyTempBase = LOWEST_SUPPLY_TEMPERATURE;
		currentSupplyTemp = currentSupplyTempBase;
	}

	// Write supply temperature to every server
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
			// use matrix only 
			if (INSTANT_COOL_AIR_TRAVEL_TIME)
				pServers[i][j]->SetSupplyTempToTimingBuffer(currentSupplyTemp, 0);
			else
				pServers[i][j]->SetSupplyTempToTimingBuffer(currentSupplyTemp, ModedAirTravelTimeFromCRAC[i]);
		}
	}

	// record current temperature of servers
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
			unsigned int local_temperature = (unsigned int)(pServers[i][j]->CurrentInletTemperature());
			if (local_temperature > 100)
				cout << "Error: local_temperature too big:" << local_temperature << endl;
			++HowManySecondsInThisInletAirTemperature[local_temperature];

			unsigned int local_utilization = (unsigned int)(pServers[i][j]->CurrentUtilization()*100);
			if (local_utilization > 100)
				cout << "Error: local_utilization too big:" << local_utilization << endl;
			++HowManySecondsInThisUtilization[local_utilization];
		}
	}

	// delete finished VMs
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
			if (pServers[i][j]->IsOFF()) continue;
			vector<VirtualMachine*>* pvFinishedVMs = pServers[i][j]->GetFinishedVMVector();
			for (vector<VirtualMachine*>::iterator it = pvFinishedVMs->begin(); it != pvFinishedVMs->end(); ++it) {
				pJobQueue->OneVMFinishedOnASingleJob((*it)->GetJobNumber(), (int)clock, (float)(i+j/10.0));
				delete (*it);
			}
			pvFinishedVMs->clear();
		}
	}

	// logs
	if (clock%PERIODIC_LOG_INTERVAL==0) {
		vSupplyTemperatureSparseLog.push_back(fpSupplyTemperatureLog/PERIODIC_LOG_INTERVAL);
		fpSupplyTemperatureLog = 0.0;

		vTotalPowerFromComputingMachinesSparseLog.push_back(fpTotalPowerFromComputingMachinesLog/PERIODIC_LOG_INTERVAL);
		fpTotalPowerFromComputingMachinesLog = 0.0;
		vTotalPowerFromServerFansSparseLog.push_back(fpTotalPowerFromServerFansLog/PERIODIC_LOG_INTERVAL);
		fpTotalPowerFromServerFansLog = 0.0;

		vTotalComputingPowerSparseLog.push_back(fpTotalComputingPowerLog/PERIODIC_LOG_INTERVAL);
		fpTotalComputingPowerLog = 0.0;

		vTotalPowerFromCRACSparseLog.push_back(fpTotalPowerFromCRACLog/PERIODIC_LOG_INTERVAL);
		fpTotalPowerFromCRACLog = 0.0;

		vUtilizationSparseLog.push_back(fpUtilizationLog/PERIODIC_LOG_INTERVAL/NUMBER_OF_CHASSIS/NUMBER_OF_SERVERS_IN_ONE_CHASSIS);
		fpUtilizationLog = 0.0;
	}

	fpSupplyTemperatureLog += currentSupplyTemp;
	FLOATINGPOINT totalPowerDrawIT = TotalPowerDrawFromComputingMachines();
	FLOATINGPOINT totalPowerDrawFans = TotalPowerDrawFromServerFans();
	fpTotalPowerFromComputingMachinesLog += totalPowerDrawIT;
	fpTotalPowerFromServerFansLog += totalPowerDrawFans;
	FLOATINGPOINT totalPowerDrawCRAC = CalculateCurrentCRACPower(totalPowerDrawIT);
	fpTotalPowerFromCRACLog += totalPowerDrawCRAC;
	if (peakPower < (totalPowerDrawCRAC+totalPowerDrawIT))
		peakPower = totalPowerDrawCRAC+totalPowerDrawIT;

	fpTotalComputingPowerLog += TotalComputingPower();
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
			perServerPowerLog[i] += pServers[i][j]->GetPowerDraw();
			perServerComputingPowerLog[i] += pServers[i][j]->MaxUtilization();
		}
		perServerTemperatureLog[i] += pServers[i][0]->CurrentInletTemperature();
	}
	FLOATINGPOINT sum = 0.0;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
		sum += pServers[i][j]->CurrentUtilization();
	}
	perDataCenterUtilization += sum;
	fpUtilizationLog += sum;
}

FLOATINGPOINT DataCenter::CalculateCurrentCRACPower(FLOATINGPOINT totalPowerDrawIT)
{
	FLOATINGPOINT cop = 0.0068 * currentSupplyTemp * currentSupplyTemp + 0.0008 * currentSupplyTemp + 0.458;
	return (totalPowerDrawIT/cop);
}

FLOATINGPOINT DataCenter::TotalComputingPower()
{
	FLOATINGPOINT retval = 0.0;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i)
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j)
			retval += pServers[i][j]->MaxUtilization();
	return retval;
}

FLOATINGPOINT DataCenter::TotalPowerDrawFromComputingMachines()
{
	FLOATINGPOINT retval = 0.0;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i)
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j)
			retval += pServers[i][j]->GetPowerDraw();
	return retval;
}

FLOATINGPOINT DataCenter::TotalPowerDrawFromServerFans()
{
	FLOATINGPOINT retval = 0.0;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i)
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j)
			retval += pServers[i][j]->GetFanPower();
	return retval;
}

void DataCenter::RecalculateHeatDistribution()
{
	// calcuate power draw of each CHASSIS
	FLOATINGPOINT powerDraw[SIZE_OF_HR_MATRIX];
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		powerDraw[i] = 0.0;
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
			powerDraw[i] += pServers[i][j]->GetPowerDraw();
		}
	}

	// calculate added heat
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		for (int j=0; j<NUMBER_OF_CHASSIS; ++j) {
			FLOATINGPOINT heatFromJtoI = powerDraw[j]*HeatRecirculationMatrixD[i][j];
			for (int k=0; k<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++k)
				pServers[i][k]->AddHeatToTimingBuffer(heatFromJtoI, 0);
		}
	}
}

bool DataCenter::IsFinished()
{
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
			if (!(pServers[i][j]->IsFinished()))
				return false;
		}
	}
	return true;
}

void DataCenter::PrintResults(void)
{
	// print timing information
	cout << "Total Execution Time (seconds)" << endl << clock << endl;

	// print average supply temperature
	FLOATINGPOINT sum = 0.0;
	FLOATINGPOINT sum2 = 0.0;
	FLOATINGPOINT sum3 = 0.0;
	for (size_t i=0; i<vSupplyTemperatureSparseLog.size(); ++i)
		sum += vSupplyTemperatureSparseLog[i];
	cout << "Average Supply Temperature" << endl << (sum/vSupplyTemperatureSparseLog.size()) << endl;
	
	// print peakPower
	cout << "peakPower" << endl << peakPower << endl;

	// Average Power Consumption
	sum = 0.0;
	for (size_t i=0; i<vTotalPowerFromComputingMachinesSparseLog.size(); ++i)
		sum += vTotalPowerFromComputingMachinesSparseLog[i];
	cout << "Average Power Consumption (Servers = Fans + Computing components)" << endl << (sum/vTotalPowerFromComputingMachinesSparseLog.size()) << endl;

	sum3 = 0.0;
	for (size_t i=0; i<vTotalPowerFromServerFansSparseLog.size(); ++i)
		sum3 += vTotalPowerFromServerFansSparseLog[i];
	cout << "\tAverage Power Consumption (Fans)" << endl << "\t" << (sum3/vTotalPowerFromServerFansSparseLog.size()) << endl;
	cout << "\tAverage Power Consumption (Computing components)" << endl << "\t" << (sum/vTotalPowerFromComputingMachinesSparseLog.size()) - (sum3/vTotalPowerFromServerFansSparseLog.size()) << endl;

	sum2 = 0.0;
	for (size_t i=0; i<vTotalPowerFromCRACSparseLog.size(); ++i)
		sum2 += vTotalPowerFromCRACSparseLog[i];
	cout << "Average Power Consumption (CRAC)" << endl << (sum2/vTotalPowerFromCRACSparseLog.size()) << endl;
	cout << "Average Power Consumption (Total = CRAC + Servers)" << endl << ((sum + sum2)/vTotalPowerFromCRACSparseLog.size()) << endl;
	cout << "Energy (Servers = Fans + Computing components)" << endl << (sum*PERIODIC_LOG_INTERVAL) << endl;
	cout << "\tEnergy (Fans)" << endl << "\t" << (sum3*PERIODIC_LOG_INTERVAL) << endl;
	cout << "\tEnergy (Computing components)" << endl << "\t" << ((sum-sum3)*PERIODIC_LOG_INTERVAL) << endl;
	cout << "Energy (CRAC)" << endl << (sum2*PERIODIC_LOG_INTERVAL) << endl;
	cout << "Energy (Total = CRAC + Servers)" << endl << ((sum+sum2)*PERIODIC_LOG_INTERVAL) << endl;
	cout << "PUE" << endl << ((sum+sum2)/(sum)) << endl;
	cout << "tPUE" << endl << ((sum+sum2)/(sum-sum3)) << endl;
	cout << endl;

	// average data center utilization
	cout << "Average Utilization (Data Center Level)" << endl << (perDataCenterUtilization/NUMBER_OF_CHASSIS/NUMBER_OF_SERVERS_IN_ONE_CHASSIS/clock) << endl;

	// utilization log
	cout << "Utilization of the data center (every " << PERIODIC_LOG_INTERVAL << " secs)" << endl;
	for (size_t i=0; i<vUtilizationSparseLog.size(); ++i)
		cout << vUtilizationSparseLog[i] << "\t";
	cout << endl;
	
	// computing power log
	cout << "Total computing power (every " << PERIODIC_LOG_INTERVAL << " secs)" << endl;
	for (size_t i=0; i<vTotalComputingPowerSparseLog.size(); ++i)
		cout << vTotalComputingPowerSparseLog[i] << "\t";
	cout << endl;

	// per Chassis log
	cout << "Per Chassis Avg Power" << endl;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		cout << (perServerPowerLog[i]/clock/NUMBER_OF_SERVERS_IN_ONE_CHASSIS) << "\t";
		if (i%10==9)
			cout << endl;
	}
	cout << endl;
	cout << "Per Chassis Avg Computing Power (max utilization)" << endl;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		cout << (perServerComputingPowerLog[i]/clock/NUMBER_OF_SERVERS_IN_ONE_CHASSIS) << "\t";
		if (i%10==9)
			cout << endl;
	}
	cout << endl;
	cout << "Per Chassis Avg Inlet Temperature" << endl;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		cout << (perServerTemperatureLog[i]/clock) << "\t";
		if (i%10==9)
			cout << endl;
	}
	cout << endl;
	cout << "Per Chassis Seconds (avg) over emergency temp" << endl;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		unsigned int timeOverEmergencyTemp = 0;
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j)
			timeOverEmergencyTemp += pServers[i][j]->HowManySecondsOverEmergencyTemp();
		cout << ((FLOATINGPOINT)timeOverEmergencyTemp/(FLOATINGPOINT)NUMBER_OF_SERVERS_IN_ONE_CHASSIS) << "\t";
		if (i%10==9)
			cout << endl;
	}
	cout << endl;
	cout << "Per Server - How many times DVFS changed" << endl;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j)
			cout << pServers[i][j]->HowManyTimesDVFSChanged() << "\t";
		if (i%10==9)
			cout << endl;
	}
	cout << endl;

	// Power (every)
	cout << "Total Power Draw (Computing) (every " << PERIODIC_LOG_INTERVAL << " secs)" << endl;
	for (size_t i=0; i<vTotalPowerFromComputingMachinesSparseLog.size(); ++i)
		cout << vTotalPowerFromComputingMachinesSparseLog[i] << "\t";
	cout << endl;
	cout << "Total Power Draw (CRAC) (every " << PERIODIC_LOG_INTERVAL << " secs)" << endl;
	for (size_t i=0; i<vTotalPowerFromCRACSparseLog.size(); ++i)
		cout << (vTotalPowerFromCRACSparseLog[i]) << "\t";
	cout << endl;
	cout << "Total Power Draw (Computing + CRAC) (every " << PERIODIC_LOG_INTERVAL << " secs)" << endl;
	for (size_t i=0; i<vTotalPowerFromCRACSparseLog.size(); ++i)
		cout << (vTotalPowerFromComputingMachinesSparseLog[i]+vTotalPowerFromCRACSparseLog[i]) << "\t";
	cout << endl;

	// energy (every)
	cout << "Total Energy Draw (Computing) (every " << PERIODIC_LOG_INTERVAL << " secs)" << endl;
	for (size_t i=0; i<vTotalPowerFromComputingMachinesSparseLog.size(); ++i)
		cout << (vTotalPowerFromComputingMachinesSparseLog[i]*PERIODIC_LOG_INTERVAL) << "\t";
	cout << endl;
	cout << "Total Energy Draw (CRAC) (every " << PERIODIC_LOG_INTERVAL << " secs)" << endl;
	for (size_t i=0; i<vTotalPowerFromCRACSparseLog.size(); ++i)
		cout << (vTotalPowerFromCRACSparseLog[i]*PERIODIC_LOG_INTERVAL) << "\t";
	cout << endl;
	cout << "Total Energy Draw (Computing + CRAC) (every " << PERIODIC_LOG_INTERVAL << " secs)" << endl;
	for (size_t i=0; i<vTotalPowerFromCRACSparseLog.size(); ++i)
		cout << ((vTotalPowerFromComputingMachinesSparseLog[i]+vTotalPowerFromCRACSparseLog[i])*PERIODIC_LOG_INTERVAL) << "\t";
	cout << endl;

	// print supply temperature log
	cout << "Supply Temperature (every " << PERIODIC_LOG_INTERVAL << " secs)" << endl;
	for (size_t i=0; i<vSupplyTemperatureSparseLog.size(); ++i)
		cout << vSupplyTemperatureSparseLog[i] << "\t";
	cout << endl;

	// print HowManySecondsInThisInletAirTemperature
	cout << "Inlet Air Temperature Distribution of the server at the highest temperature (seconds)" << endl;
	for (unsigned int i=0; i<100; ++i) {
		if (HowManySecondsInThisInletAirTemperature[i]!=0)
			cout << HowManySecondsInThisInletAirTemperature[i] << "\t" << i << endl;
	}
	cout << endl;

	// print HowManySecondsInThisUtilization
	cout << "Utilization Distribution (seconds)" << endl;
	for (unsigned int i=0; i<100; ++i) {
		cout << HowManySecondsInThisUtilization[i] << "\t" << i << endl;
	}
	cout << endl;

}
