/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#define CLEANER_TIMEOUT         10

typedef struct {
    pthread_mutex_t *mutex;
    Pipe *pipe;
} Cleaner;

extern Cleaner *CLEANER;

Cleaner* cleanerNew();
void cleanerRelease(Cleaner **);
void startCleaner();
void* startCleanerThread(void *);
void stopCleaner();
