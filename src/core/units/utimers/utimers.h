/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#define EUIDOWN 120

extern int UTIMERS_SECTIONS_ITEMS_LEN;
extern SectionData UTIMERS_SECTIONS_ITEMS[];
extern int UTIMERS_PROPERTIES_ITEMS_LEN;
extern PropertyData UTIMERS_PROPERTIES_ITEMS[];

struct eventData {
    const char *timerUnitName;
};

int parseTimerUnit(Array **, Unit **, bool);
int checkInterval(Unit **unit);
int startTimerUnit(Unit *);
void *startTimerUnitThread(void *);
int setNextTimeFromDisk(Unit **);
int setNextTimeFromInterval(Unit **);
int saveTime(Unit *, const char *, Time *, int);
int executeUnit(Unit *, PType);
void setLeftTimeAndDuration(Unit **);
void setNextTimeDate(Unit **);
int resetNextTime(const char *);
void expired(union sigval timerData);
Timer *timerNew();
void timerRelease(Timer **);
void armTimer(Unit *);
void disarmTimer(Unit *unit);
