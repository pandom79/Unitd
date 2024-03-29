/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef CLIENT_H
#define CLIENT_H

#define PADDING 3
#define WIDTH_IDX 3
#define WIDTH_BOOT_ID 7
#define WIDTH_STARTED 7
#define WIDTH_FINISHED 8
#define WIDTH_DATE 19
#define RANGE_TOKEN ".."
#define TMP_SUFFIX ".tmp"

typedef enum {
    NO_UL_COMMAND = -1,
    SHOW_LOG = 0,
    LIST_BOOTS = 1,
    SHOW_BOOT = 2,
    INDEX_REPAIR = 3,
    VACUUM = 4,
    SHOW_SIZE = 5,
    SHOW_CURRENT = 6
} UlCommand;
typedef struct {
    UlCommand ulCommand;
    const char *name;
} UlCommandData;

UlCommand getUlCommand(const char *);
bool getSkipCheckAdmin(UlCommand);
int showBootsList();
int showLog(bool, bool);
int showLogLines(off_t, off_t);
int sendToPager(int (*fn)(off_t, off_t), off_t, off_t);
int showBoot(bool, bool, const char *);
int showCurrentBoot(bool, bool);
int followLog();
int createIndexFile();
int vacuum(const char *);
int runTmpLogOperation(const char *);
void printLogSizeInfo(off_t, off_t, off_t);

#endif // CLIENT_H
