#ifndef __OL113_WD_H__
#define __OL113_WD_H__

/****************************************
*                                       *
* Version 1.0                           *
*                                       *
* Date: 27-Jan-22                       *
* Infinity Labs OL113                   *
*****************************************/
/* a process using the Watchdog must not use SIGUSR1 and SIGUSR2 or define new handlers for them */
/* If another instance of the APP is created, a new semaphore name must be used */

int WDStart(char **argv/*if cant be void, we will add parameters*/);

/*  when StopWD is called a timeout of 3 seconds will begin, during that time the calling thread might be blocked */
void WDStop(void);

#endif /*__OL113_WD_H__ */