/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef LOGLINE_H
#define LOGLINE_H

#define INTERNAL_NOPRI 0x10 /* the "no priority" priority */
#define INTERNAL_MARK LOG_MAKEPRI(LOG_NFACILITIES, 0) /* mark "facility" */

typedef struct {
    char *name;
    int val;
} Code;

typedef struct {
    char *pri;
    char *fac;
    char *hostName;
    char *timeStamp;
} LogLine;

LogLine *logLineNew();
void logLineRelease(LogLine **);
int processLine(char *);

#endif // LOGLINE_H
