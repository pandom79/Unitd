/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

typedef struct {
    int fds[2];
    int *fd;
    int *wd;
    pthread_mutex_t *mutex;
    char *watchDir;
} Notifier;

extern Array *NOTIFIERS;
extern bool NOTIFIER_WORKING;

Notifier* notifierNew();
void notifierRelease(Notifier **);
void setNotifiers();
void startNotifiers();
void* startNotifiersThread(void *);
void* runNotifiersThread(void *);
void stopNotifiers();
void* stopNotifiersThread(void *);
