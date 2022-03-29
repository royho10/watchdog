# watchdog
implementation of watchdog service to keep an eye on user app and restart it if it crashes

![image](https://user-images.githubusercontent.com/35746870/160709900-e0fc7136-7d42-4cde-96ab-182812d23363.png)

# Requirements
1. Watchdog is a process that guards the user process.
2. The communication between the processes will be by signals (SIGUSR1, SIGUSR2).
3. The signals will be scheduled by a product that I developed beforehand called Scheduler.
4. The user's process will guard the watchdog process as well and restart it if it crashes.

# API:

int WDStart(char **argv);

void WDStop(void);


