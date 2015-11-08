/*
 * RequestTimer.h
 *
 * Created: 06/11/2015 14:22:05
 *  Author: David
 */ 


#ifndef REQUESTTIMER_H_
#define REQUESTTIMER_H_

class RequestTimer
{
	enum { stopped, running, ready } timerState;
	uint32_t startTime;
	uint32_t delayTime;
	const char *command;
	
	public:
	RequestTimer(uint32_t del, const char *cmd);
	void SetPending() { timerState = ready; }
	void Stop() { timerState = stopped; }
	bool Process();
};

#endif /* REQUESTTIMER_H_ */