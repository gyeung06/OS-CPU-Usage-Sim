1. Multiple Sends to the same PID will be saved, receive shows one message at a time

2. Multiple Replies to the same PID will be overwritten by the latest reply due to likely use cases
and to reduce space and save time from searching

3. Init cannot send or receive since if it is running there should be no other processes that can run, blocked processes
can be killed by users by making new processes to unblock blocked processes

4. If current running process is higher priority than the next process that is ready, the current process will run again
unless knocked out by priority manager

5. To prevent starvation a counter has been added to give normal and low queue priority after a certain amount of cycles
amount can be set at the top with NORM_PRIO_MIN_QUANTUM and LOW_PRIO_MIN_QUANTUM at 0 the next cycle will prioritize
that queue instead of high priority