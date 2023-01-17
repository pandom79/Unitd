/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef INIT_H
#define INIT_H

typedef enum {
    NO_TYPE = -1,
    UNIX = 0,
    FORWARDER = 1
} SocketType;

typedef struct {
    pthread_t thread;
    char *devName;
    Pipe *pipe;
    SocketType sockType;
} SocketThread;

char *getBootIdStr();
void assertMacros();
int unitlogdInit();
int unitlogdShutdown();
int createKmsgLog();
void appendKmsg();

#endif // INIT_H
