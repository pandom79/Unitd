/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

typedef enum {
    UNITD_WATCHER = 0,
    PATH_EXISTS_WATCHER = 1,
    PATH_EXISTS_GLOB_WATCHER = 2,
    PATH_RESOURCE_CHANGED_WATCHER = 3,
    PATH_DIRECTORY_NOT_EMPTY_WATCHER = 4
} WatcherType;

typedef struct {
    WatcherType watcherType;
    int mask;
} WatcherData;

typedef struct {
    int *wd;
    char *path;
    WatcherData watcherData;
} Watcher;

extern const WatcherData WATCHER_DATA_ITEMS[];
extern Notifier *NOTIFIER;
extern bool NOTIFIER_WORKING;

Notifier* notifierNew();
int notifierInit(Notifier *);
void notifierRelease(Notifier **);
void notifierClose(Notifier *);
Watcher* watcherNew(Notifier *, const char *, WatcherType);
void watcherRelease(Watcher **);
void setUnitdNotifier();
int startNotifier(Unit *);
void* startNotifierThread(void *);
int stopNotifier(Unit *);
