/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef FILE_H
#define FILE_H

int unitlogdOpenLog(const char *);
int unitlogdOpenBootLog(const char *);
int unitlogdOpenIndex(const char *);
int unitlogdOpenKmsg(const char *);
int unitlogdCloseLog();
int unitlogdCloseBootLog();
int unitlogdCloseIndex();
int unitlogdCloseKmsg();
void logEntry(FILE **, const char *);
char* getLogOffset();
bool matchLogLine(bool, IndexEntry *);
int execUlScript(Array **, const char *);
int handleLockFile(bool);
int getLockFileFd();
off_t getFileSize(const char *);
void writeKmsg(char *);

#endif // FILE_H
