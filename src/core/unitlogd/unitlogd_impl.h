/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef UNITLOGD_IMPL_H
#define UNITLOGD_IMPL_H

#define BUFFER_SIZE 1024

typedef struct {
    pthread_t thread;
    char *devName;
    Pipe *pipe;
} SocketThread;

extern bool UNITLOGD_DEBUG;
extern int SELF_PIPE[2];
extern int UNITLOGD_PID;
extern FILE *UNITLOGD_INDEX;
extern FILE *UNITLOGD_LOG;
extern FILE *UNITLOGD_BOOT_LOG;

/* Common */
void assertMacros();
int unitlogdInit();

/* Signals */
int setUnitlogdSigAction();
void exitSignal(int, siginfo_t *, void *);

/* Socket */
SocketThread* socketThreadNew();
void socketThreadRelease(SocketThread **);
int startSockets(Array *);
void* startSocket(void *);
void* startSocketThread(void *);
int getUnixSocketFd(const char *);
int stopSockets(Array *);
void* stopSocket(void *);

#endif // UNITLOGD_IMPL_H
