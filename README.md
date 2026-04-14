# Operating Systems Assignment 3
## Group Members
1) Sushant Subramanian - 2023A8PS0295H
2) Yohann Miglani - 2023A8PS1146H
3) Rishit Shah - 2023A3PS0376H
4) Aditya R Goel - 2023A3PS0323H

## Running Files:
To compile: `g++ file.c -o file.exe` <br>
To run: `./file.exe`

## Q1) Title
> filename

### Functionality
- test

### Coding
- test

## Q2) Mars Rover
> question2.c

A multithreaded C simulation of autonomous rovers competing for charging cables on Mars — a classic Dining Philosophers problem with battery-based priority and emergency preemption.
### Functionality
- Five autonomous rovers (numbered `0–4`) orbit a circular solar-charging hub.
- The hub has **five heavy-duty cables** positioned between adjacent rovers. Due to extreme power requirements, each rover must plug in **two adjacent cables** (left and right) simultaneously to charge.
- Each rover alternates between two states: Rover is out in the field (1–5 seconds), or Rover is at the hub using cables (1–3 seconds)

### Coding
- Uses strictly `pthread_mutex_t` and `pthread_cond_t` (no semaphores)
- Cable acquisition is **atomic** — both cables are grabbed simultaneously while holding the mutex, eliminating circular-wait deadlock
- Cables are only acquired when **both** the left and right cable are simultaneously free, checked inside a single critical section. This prevents the classic "hold-one, wait-for-other" deadlock pattern.
- Before a normal rover blocks waiting for cables, it checks whether any other rover is in emergency state. If so, it backs off and retries — giving emergency rovers first access as soon as cables become available.
- Each rover runs as an independent `pthread`. When a rover strands (battery = 0%), it logs the event and calls `pthread_exit()` to cleanly terminate. The main thread joins all rover threads before exiting.
```
gcc -o rover rover.c -lpthread
./rover
```

### Output

```
Rover 0 is exploring (Battery: 50%)
Rover 1 is exploring (Battery: 50%)
Rover 3 is exploring (Battery: 50%)
Rover 0 is waiting for cables (Battery: 10%)
Rover 0 ENTERING EMERGENCY STATE (Battery: 10%)
Rover 0 is charging (Battery: 10%)
Rover 3 finished charging (Battery: 85%)
Rover 3 is exploring (Battery: 85%)
Rover 2 is charging (Battery: 15%)
Rover 1 STRANDED: Battery depleted to 0% during exploration.
```

## Q3) Title
> filename

### Functionality
- test

### Coding
- test

### Output
![A screenshot of the output terminal:](link)
