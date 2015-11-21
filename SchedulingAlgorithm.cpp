#include "SchedulingAlgorithm.h"

SchedulingAlgorithm::~SchedulingAlgorithm(void)
{
}

RandomSchedulingAlgorithm::RandomSchedulingAlgorithm(Server* (*ps)[SIZE_OF_HR_MATRIX][NUMBER_OF_SERVERS_IN_ONE_HR_MATRIX_CELL_MAX], queue<VirtualMachine*>* pqvm, const FLOATINGPOINT (*matrixD)[SIZE_OF_HR_MATRIX][SIZE_OF_HR_MATRIX])
{
	ppServers = ps;
	pqVMsToGo = pqvm;
	pHeatRecirculationMatrixD = matrixD;

	srand((unsigned int)time(NULL));
}

void RandomSchedulingAlgorithm::AssignVMs()
{
	while (!pqVMsToGo->empty()) {
		int bestI = rand()%NUMBER_OF_CHASSIS;
		int bestJ = rand()%NUMBER_OF_SERVERS_IN_ONE_CHASSIS;
		if (bestI < 0 || bestJ < 0 || bestI > NUMBER_OF_CHASSIS || bestJ > NUMBER_OF_SERVERS_IN_ONE_CHASSIS)
			cout << "Error: No servers to assign a VM" << endl;
		(*ppServers)[bestI][bestJ]->AssignOneVM(pqVMsToGo->front());
		pqVMsToGo->pop();
	}
}

LowTemperatureFirstSchedulingAlgorithm::LowTemperatureFirstSchedulingAlgorithm(Server* (*ps)[SIZE_OF_HR_MATRIX][NUMBER_OF_SERVERS_IN_ONE_HR_MATRIX_CELL_MAX], queue<VirtualMachine*>* pqvm, const FLOATINGPOINT (*matrixD)[SIZE_OF_HR_MATRIX][SIZE_OF_HR_MATRIX])
{
	ppServers = ps;
	pqVMsToGo = pqvm;
	pHeatRecirculationMatrixD = matrixD;

	srand((unsigned int)time(NULL));
}

void LowTemperatureFirstSchedulingAlgorithm::AssignVMs()
{
	// assign VMs to Servers
	while (!pqVMsToGo->empty()) {
		// assign with qWaitingVMs.top()
		int bestI, bestJ = -1;
		FLOATINGPOINT bestAvailability = 1000000.0; // any big number
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if ((*ppServers)[i][j]->IsOFF()) continue;
				FLOATINGPOINT sumofVM = (*ppServers)[i][j]->VMRequiresThisMuchCPUScale() + pqVMsToGo->front()->GetCPULoadRatio();
				if (sumofVM >= NUMBER_OF_CORES_IN_ONE_SERVER) // it will be over occupied ...
					continue;
				FLOATINGPOINT localInlet = (*ppServers)[i][j]->CurrentInletTemperature();
				if (localInlet < bestAvailability) {
					bestI = i; bestJ = j;
					bestAvailability = localInlet;
				}
			}
		}
		if (bestI < 0 || bestJ < 0) {
			// second iteration. all servers are busy. assign to the lowest inlet temp
			for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
				for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
					if ((*ppServers)[i][j]->IsOFF()) continue;
					FLOATINGPOINT sumofVM = (*ppServers)[i][j]->VMRequiresThisMuchCPUScale();
					if (sumofVM >= NUMBER_OF_CORES_IN_ONE_SERVER) // it will be over occupied ...
						continue;
					FLOATINGPOINT localInlet = (*ppServers)[i][j]->CurrentInletTemperature();
					if (localInlet < bestAvailability) {
						bestI = i; bestJ = j;
						bestAvailability = localInlet;
					}
				}
			}
		}
		if (bestI < 0 || bestJ < 0) {
			// still can not decide .. place randomly
			bestI = rand()%NUMBER_OF_CHASSIS;
			bestJ = rand()%NUMBER_OF_SERVERS_IN_ONE_CHASSIS;
		}
		(*ppServers)[bestI][bestJ]->AssignOneVM(pqVMsToGo->front());
		pqVMsToGo->pop();
	}
}

UniformTaskSchedulingAlgorithm::UniformTaskSchedulingAlgorithm(Server* (*ps)[SIZE_OF_HR_MATRIX][NUMBER_OF_SERVERS_IN_ONE_HR_MATRIX_CELL_MAX], queue<VirtualMachine*>* pqvm, const FLOATINGPOINT (*matrixD)[SIZE_OF_HR_MATRIX][SIZE_OF_HR_MATRIX])
{
	ppServers = ps;
	pqVMsToGo = pqvm;
	pHeatRecirculationMatrixD = matrixD;
}

void UniformTaskSchedulingAlgorithm::AssignVMs()
{
	// assign VMs to Servers
	while (!pqVMsToGo->empty()) {
		// assign with qWaitingVMs.top()
		int bestI, bestJ = -1;
		FLOATINGPOINT bestAvailability = 1000000.0; // any big number
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if ((*ppServers)[i][j]->IsOFF()) continue;
				FLOATINGPOINT sumofVM = (*ppServers)[i][j]->VMRequiresThisMuchCPUScale();
				if (sumofVM < bestAvailability) { // assign to the machine with least jobs
					bestI = i; bestJ = j; 
					bestAvailability = sumofVM;
				}
			}
		}
		if (bestI < 0 || bestJ < 0)
			cout << "Error: No servers to assign a VM" << endl;
		(*ppServers)[bestI][bestJ]->AssignOneVM(pqVMsToGo->front());
		pqVMsToGo->pop();
	}
}

BestPerformanceSchedulingAlgorithm::BestPerformanceSchedulingAlgorithm(Server* (*ps)[SIZE_OF_HR_MATRIX][NUMBER_OF_SERVERS_IN_ONE_HR_MATRIX_CELL_MAX], queue<VirtualMachine*>* pqvm, const FLOATINGPOINT (*matrixD)[SIZE_OF_HR_MATRIX][SIZE_OF_HR_MATRIX])
{
	ppServers = ps;
	pqVMsToGo = pqvm;
	pHeatRecirculationMatrixD = matrixD;
}

void BestPerformanceSchedulingAlgorithm::AssignVMs()
{
	// assign VMs to Servers
	while (!pqVMsToGo->empty()) {
		// assign with qWaitingVMs.top()
		int bestI, bestJ = -1;
		FLOATINGPOINT bestAvailability = 1000000.0; // any big number
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if ((*ppServers)[i][j]->IsOFF()) continue;
				FLOATINGPOINT sumofVM = (*ppServers)[i][j]->VMRequiresThisMuchUtilization();
				FLOATINGPOINT maxutil = (*ppServers)[i][j]->MaxUtilization();
				FLOATINGPOINT ratio = (sumofVM/maxutil);
				if (ratio < bestAvailability) {
					bestI = i; bestJ = j;
					bestAvailability = ratio;
				}
			}
		}
		if (bestI < 0 || bestJ < 0)
			cout << "Error: No servers to assign a VM" << endl;
		(*ppServers)[bestI][bestJ]->AssignOneVM(pqVMsToGo->front());
		pqVMsToGo->pop();
	}
}

MinHRSchedulingAlgorithm::MinHRSchedulingAlgorithm(Server* (*ps)[SIZE_OF_HR_MATRIX][NUMBER_OF_SERVERS_IN_ONE_HR_MATRIX_CELL_MAX], queue<VirtualMachine*>* pqvm, const FLOATINGPOINT (*matrixD)[SIZE_OF_HR_MATRIX][SIZE_OF_HR_MATRIX])
{
	ppServers = ps;
	pqVMsToGo = pqvm;
	pHeatRecirculationMatrixD = matrixD;

	for (int i=0; i<SIZE_OF_HR_MATRIX; ++i) {
		HRF[i] = 0.0;
		for (int j=0; j<SIZE_OF_HR_MATRIX; ++j) {
			HRF[i] += (*pHeatRecirculationMatrixD)[j][i];
		}
	}
	for (int i=0; i<SIZE_OF_HR_MATRIX; ++i) {
		FLOATINGPOINT reference = 100000.0; // some big number
		int toInsert = -1;
		for (int j=0; j<SIZE_OF_HR_MATRIX; ++j) {
			if (HRF[j] < reference) {
				toInsert = j;
				reference = HRF[j];
			}
		}
		HRF[toInsert] = 10000000.0; // some bigger number
		HRFSortedIndex[i] = toInsert;
	}

	srand((unsigned int)time(NULL));
}

void MinHRSchedulingAlgorithm::AssignVMs()
{
	// assign VMs to Servers
	while (!pqVMsToGo->empty()) {
		// assign with qWaitingVMs.top()
		int bestI, bestJ = -1;
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if ((*ppServers)[HRFSortedIndex[i]][j]->IsOFF()) continue;

				FLOATINGPOINT sumofVM = (*ppServers)[HRFSortedIndex[i]][j]->VMRequiresThisMuchCPUScale() + pqVMsToGo->front()->GetCPULoadRatio();
				if (sumofVM >= NUMBER_OF_CORES_IN_ONE_SERVER) // it's full with the new workload
					continue;
				bestI = HRFSortedIndex[i];
				bestJ = j;
				goto ASSIGNING_FINISHED;
			}
		}
		// data center is full. assign similarly but more aggressively.
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if ((*ppServers)[HRFSortedIndex[i]][j]->IsOFF()) continue;

				FLOATINGPOINT sumofVM = (*ppServers)[HRFSortedIndex[i]][j]->VMRequiresThisMuchCPUScale();
				if (sumofVM >= NUMBER_OF_CORES_IN_ONE_SERVER) // it's full even before placing the workload
					continue;
				bestI = HRFSortedIndex[i];
				bestJ = j;
				goto ASSIGNING_FINISHED;
			}
		}
		// still could not decide, which means, every one is full. locate workload randomly.
		bestI = rand()%NUMBER_OF_CHASSIS;
		bestJ = rand()%NUMBER_OF_SERVERS_IN_ONE_CHASSIS;
ASSIGNING_FINISHED:
		(*ppServers)[bestI][bestJ]->AssignOneVM(pqVMsToGo->front());
		pqVMsToGo->pop();
	}
}

XintSchedulingAlgorithm::XintSchedulingAlgorithm(Server* (*ps)[SIZE_OF_HR_MATRIX][NUMBER_OF_SERVERS_IN_ONE_HR_MATRIX_CELL_MAX], queue<VirtualMachine*>* pqvm, const FLOATINGPOINT (*matrixD)[SIZE_OF_HR_MATRIX][SIZE_OF_HR_MATRIX])
{
	ppServers = ps;
	pqVMsToGo = pqvm;
	pHeatRecirculationMatrixD = matrixD;
}

void XintSchedulingAlgorithm::AssignVMs()
{
	// assign VMs to Servers
	while (!pqVMsToGo->empty()) {
		int bestI, bestJ = -1;
		FLOATINGPOINT thishastobemin = 1000.0;
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if ((*ppServers)[i][j]->IsOFF()) continue;
				(*ppServers)[i][j]->AssignOneVM(pqVMsToGo->front()); // temporarilly assign to i,j
				FLOATINGPOINT local_thishastobemin = GetHighestTemperatureIncrease();
				if (thishastobemin > local_thishastobemin) {
					bestI = i; bestJ = j;
					thishastobemin = local_thishastobemin;
				}
				(*ppServers)[i][j]->RemoveTheLastAssignedVM(); // de-assign after calculating hr effect
			}
		}
		(*ppServers)[bestI][bestJ]->AssignOneVM(pqVMsToGo->front());
		pqVMsToGo->pop();
	}
}

FLOATINGPOINT XintSchedulingAlgorithm::GetHighestTemperatureIncrease()
{
	FLOATINGPOINT powerDraw[SIZE_OF_HR_MATRIX];
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		powerDraw[i] = 0.0;
		for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
			powerDraw[i] += (*ppServers)[i][j]->GetPowerDraw();
		}
	}
	FLOATINGPOINT tempIncrease;
	FLOATINGPOINT biggestTempIncrease = -100.0;
	for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
		tempIncrease = 0.0;
		for (int j=0; j<NUMBER_OF_CHASSIS; ++j) {
			tempIncrease += powerDraw[j]*(*pHeatRecirculationMatrixD)[i][j];
		}
		if (tempIncrease > biggestTempIncrease)
			biggestTempIncrease = tempIncrease;
	}
	return biggestTempIncrease;
}


CenterRackFirstSchedulingAlgorithm::CenterRackFirstSchedulingAlgorithm(Server* (*ps)[SIZE_OF_HR_MATRIX][NUMBER_OF_SERVERS_IN_ONE_HR_MATRIX_CELL_MAX], queue<VirtualMachine*>* pqvm, const FLOATINGPOINT (*matrixD)[SIZE_OF_HR_MATRIX][SIZE_OF_HR_MATRIX])
{
	ppServers = ps;
	pqVMsToGo = pqvm;
	pHeatRecirculationMatrixD = matrixD;

	int machineNumber[SIZE_OF_HR_MATRIX] = {
		11,36,37,12,13,38,39,14,15,40,
		6,16,31,41,42,32,17,7,8,18,33,43,44,34,19,9,10,20,35,45,
		1,21,26,46,47,27,22,2,3,23,28,48,49,29,24,4,5,25,30,50
	};
/*
	11,12,13,14,15,
	36,37,38,39,40,

	6,7,8,9,10,
	16,17,18,19,20,
	31,32,33,34,35,
	41,42,43,44,45,

	1,2,3,4,5,
	21,22,23,24,25,
	26,27,28,29,30,
	46,47,48,49,50,
*/

	for (int i=0; i<SIZE_OF_HR_MATRIX; ++i)
		HRFSortedIndex[i] = machineNumber[i] -1;
}

void CenterRackFirstSchedulingAlgorithm::AssignVMs()
{
	// assign VMs to Servers
	while (!pqVMsToGo->empty()) {
		// assign with qWaitingVMs.top()
		int bestI, bestJ = -1;
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if ((*ppServers)[HRFSortedIndex[i]][j]->IsOFF()) continue;

				FLOATINGPOINT sumofVM = (*ppServers)[HRFSortedIndex[i]][j]->VMRequiresThisMuchCPUScale() + pqVMsToGo->front()->GetCPULoadRatio();
				if (sumofVM >= NUMBER_OF_CORES_IN_ONE_SERVER) // it's full with the new workload
					continue;
				bestI = HRFSortedIndex[i];
				bestJ = j;
				goto ASSIGNING_FINISHED;
			}
		}
		// data center is full. assign similarly but more aggressively.
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if ((*ppServers)[HRFSortedIndex[i]][j]->IsOFF()) continue;

				FLOATINGPOINT sumofVM = (*ppServers)[HRFSortedIndex[i]][j]->VMRequiresThisMuchCPUScale();
				if (sumofVM >= NUMBER_OF_CORES_IN_ONE_SERVER) // it's full even before placing the workload
					continue;
				bestI = HRFSortedIndex[i];
				bestJ = j;
				goto ASSIGNING_FINISHED;
			}
		}
		// still could not decide, which means, every one is full. locate workload randomly.
		bestI = rand()%NUMBER_OF_CHASSIS;
		bestJ = rand()%NUMBER_OF_SERVERS_IN_ONE_CHASSIS;
ASSIGNING_FINISHED:
		(*ppServers)[bestI][bestJ]->AssignOneVM(pqVMsToGo->front());
		pqVMsToGo->pop();
	}
}

BestEdpSchedulingAlgorithm::BestEdpSchedulingAlgorithm(Server* (*ps)[SIZE_OF_HR_MATRIX][NUMBER_OF_SERVERS_IN_ONE_HR_MATRIX_CELL_MAX], queue<VirtualMachine*>* pqvm, const FLOATINGPOINT (*matrixD)[SIZE_OF_HR_MATRIX][SIZE_OF_HR_MATRIX])
{
	fprintf(stderr, "Created Best EDP Algorithm\n");
	ppServers = ps;
	pqVMsToGo = pqvm;
	pHeatRecirculationMatrixD = matrixD;
}

void BestEdpSchedulingAlgorithm::AssignVMs()
{
	// assign VMs to Servers
	while (!pqVMsToGo->empty()) {
		// assign with qWaitingVMs.top()
		int bestI, bestJ = -1;
		FLOATINGPOINT bestAvailability = 1000000.0; // any big number
		for (int i=0; i<NUMBER_OF_CHASSIS; ++i) {
			for (int j=0; j<NUMBER_OF_SERVERS_IN_ONE_CHASSIS; ++j) {
				if ((*ppServers)[i][j]->IsOFF()) continue;
				FLOATINGPOINT sumofVM = (*ppServers)[i][j]->VMRequiresThisMuchCPUScale();
				if (sumofVM < bestAvailability) { // assign to the machine with least jobs
					bestI = i; bestJ = j; 
					bestAvailability = sumofVM;
				}
			}
		}
		if (bestI < 0 || bestJ < 0)
			cout << "Error: No servers to assign a VM" << endl;
		(*ppServers)[bestI][bestJ]->AssignOneVM(pqVMsToGo->front());
		pqVMsToGo->pop();
	}
}
