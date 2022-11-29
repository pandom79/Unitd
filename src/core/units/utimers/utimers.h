/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

struct eventData {
    const char *timerUnitName;
};

int parseTimerUnit(Array **, Unit **, bool);
int checkInterval(Unit **unit);
int startUnitTimer(Unit *);
void* startUnitTimerThread(void *);
int setNextTimeFromDisk(Unit **);
int setNextTimeFromInterval(Unit **);
int saveTime(Unit *, const char *, Time *, int);
int executeUnit(Unit *);
int startTimer(Unit *);
void* startTimerThread(void *);
int stopTimer(Unit *);
void setLeftTimeAndDuration(Unit **);
void setNextTimeDate(Unit **);
