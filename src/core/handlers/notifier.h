/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

typedef struct {
    Pipe *pipe;
    int *fd;
    Array *watchers;
} Notifier;

typedef struct {
    int *fd;
    int *wd;
    char *path;
    int *mask;
} Watcher;

extern Notifier *NOTIFIER;
extern bool NOTIFIER_WORKING;

Notifier* notifierNew();
void notifierRelease(Notifier **);
Watcher* watcherNew(Notifier *, const char *, int);
void watcherRelease(Watcher **);
void setNotifier();
void startNotifier();
void* startNotifierThread(void *);
void stopNotifier();
