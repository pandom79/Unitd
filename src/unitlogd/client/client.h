/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef CLIENT_H
#define CLIENT_H

#define PADDING             3
#define WIDTH_IDX           3
#define WIDTH_BOOT_ID       7
#define WIDTH_STARTED       7
#define WIDTH_FINISHED      8
#define WIDTH_DATE          19

typedef enum {
    NO_UL_COMMAND = -1,
    SHOW_LOG = 0,
    LIST_BOOTS = 1
} UlCommand;
typedef struct {
    UlCommand ulCommand;
    const char* name;
} UlCommandData;

UlCommand getUlCommand(const char *);
bool getSkipCheckAdmin(UlCommand);
int showBootsList();
int showLog(bool);
int showLogLines();
int sendToPager(int (*fn)());

#endif // CLIENT_H
