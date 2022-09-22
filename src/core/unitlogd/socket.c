/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

int UNITLOGD_PID;

SocketThread *
socketThreadNew()
{
    SocketThread *socketThread = calloc(1, sizeof(SocketThread));
    assert(socketThread);
    socketThread->devName = NULL;
    return socketThread;
}

void
socketThreadRelease(SocketThread **socketThread)
{
    if (*socketThread) {
        objectRelease(&(*socketThread)->devName);
        objectRelease(socketThread);
    }
}

void *
startSocketThread(void *arg)
{
    printf("Sono in startSocketThread, aspetto e ciao \n");
    msleep(10000);
    kill(UNITLOGD_PID, SIGTERM);
    pthread_exit(0);
}

void *
startSocket(void *arg)
{
    pthread_t thread;
    pthread_attr_t attr;
    int rv = 0, *rvThread;
    SocketThread *socketThread = NULL;
    const char *devName = NULL;

    assert(arg);
    /* Get data */
    socketThread = (SocketThread *)arg;
    devName = socketThread->devName;

    /* Set pthread attributes and start */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if ((rv = pthread_create(&thread, &attr, startSocketThread, socketThread)) != 0) {
//FIXME evaluate where to log.....
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/unitlogd/socket.c", "startSocket", errno,
                      strerror(errno), "Unable to create the socket thread (detached) (%s)", devName);
    }
    else {
//FIXME evaluate where to log.....
        if (UNITLOGD_DEBUG)
            unitdLogInfo(LOG_UNITD_CONSOLE, "Socket thread (detached) created successfully (%s)\n", devName);
    }

    /* It will be freed in startSockets func */
    rvThread = calloc(1, sizeof(int));
    assert(rvThread);
    *rvThread = rv;

    return rvThread;
}

int
startSockets(Array *socketThreads)
{
    SocketThread *socketThread = NULL;
    const char *devName = NULL;
    int rv, len, *rvThread;

    rv = len = 0;

    len = socketThreads ? socketThreads->size : 0;
    for (int i = 0; i < len; i++) {
        socketThread = arrayGet(socketThreads, i);
        devName = socketThread->devName;
        if ((rv = pthread_create(&socketThread->thread, NULL, startSocket, socketThread)) != 0) {
//FIXME evaluate where to log.....
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/unitlogd/socket.c", "startSockets", errno,
                          strerror(errno), "Unable to create the thread for the '%s' dev", devName);
            break;
        }
        else {
//FIXME evaluate where to log.....
            if (UNITLOGD_DEBUG)
                unitdLogInfo(LOG_UNITD_CONSOLE, "Thread created succesfully for the '%s' dev\n", devName);
        }

    }
    for (int i = 0; i < len; i++) {
        socketThread = arrayGet(socketThreads, i);
        devName = socketThread->devName;
        if (pthread_join(socketThread->thread, (void **)&rvThread) != EXIT_SUCCESS) {
            rv = 1;
//FIXME evaluate where to log.....
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "startSockets", rv,
                          strerror(rv), "Unable to join the thread for the '%s' dev", devName);
        }
        else {
//FIXME evaluate where to log.....
            if (UNITLOGD_DEBUG)
                unitdLogInfo(LOG_UNITD_CONSOLE, "Thread joined successfully for the '%s' dev! Return code = %d\n",
                             devName, *rvThread);
        }
        if (*rvThread == 1)
            rv = 1;
        objectRelease(&rvThread);
    }

    return rv;
}
