/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#define PID_CMD_VAR             "$PID"
#define SHOW_MAX_RESULTS        10
#define THREAD_EXIT             -1

typedef struct {
    pthread_t thread;
    Unit *unit;
    Array *units;
} UnitThreadData;

int startProcesses(Array **, Unit *);
void* startProcess(void *);
Array* getRunningUnits(Array **);
int stopProcesses(Array **, Unit *);
void* stopProcess(void *);
bool hasPipe(Unit *);
Array* getRestartableUnits(Array **);
int listenPipes(Array **, Unit *);
void* listenPipe(void *);
int closePipes(Array **, Unit *);
void* closePipe(void *);
