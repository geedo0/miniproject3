#include "Users.h"


Users::Users(JobQueue* jq, vector<SWFLine*>* swf)
{
	pJobQueue = jq;
	pvSWFToGo = swf;
	clock = 0;
}

Users::~Users(void)
{
}

void Users::EveryASecond()
{
	// launch any job to the job queue
	while (!pvSWFToGo->empty()) {
		SWFLine *pLine = *(pvSWFToGo->begin());
		if (pLine->submitTimeSec <= clock) {
			if (!pJobQueue->IsTheSingleJobFinished(pLine->dependencyJobNum))
				continue;
			pJobQueue->InsertASingleJob(pLine->jobNumber, pLine->numCPUs, pLine->runTimeSec, pLine->avgCPUTimeSec, pLine->submitTimeSec);
			delete pLine;
			pvSWFToGo->erase(pvSWFToGo->begin());
		} else {
			break;
		}
	}

	// local clock = global clock
	++clock;
}

bool Users::IsFinished()
{
	if (!pvSWFToGo->empty())
		return false;
	return true;
}
