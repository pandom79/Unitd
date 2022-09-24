/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#define TIMEOUT_INC_MS             10
//Start
#define TIMEOUT_MS              15000
#define MIN_TIMEOUT_MS           3500
//Stop
#define TIMEOUT_STOP_MS          1000

int execScript(const char *, const char *, char **, char **);
int execProcess(const char *, char **, Unit **);
char** cmdlineSplit(const char *);
void cmdlineRelease(char **);
int stopDaemon(const char *, char **, Unit **);
void reapPendingChild();
