/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../unitd_impl.h"

int UNITLOGD_PID;

SocketThread*
socketThreadNew()
{
    SocketThread *socketThread = calloc(1, sizeof(SocketThread));
    assert(socketThread);
    socketThread->devName = NULL;
    socketThread->pipe = pipeNew();
    return socketThread;
}

void
socketThreadRelease(SocketThread **socketThread)
{
    if (*socketThread) {
        objectRelease(&(*socketThread)->devName);
        pipeRelease(&(*socketThread)->pipe);
        objectRelease(socketThread);
    }
}

void*
startSocketThread(void *arg)
{
    int rv, socketFd, maxFd, pipeFd, input, length, bufferSize;
    SocketThread *socketThread = (SocketThread *)arg;
    assert(socketThread);
    Pipe *socketPipe = socketThread->pipe;
    pthread_mutex_t *mutexPipe = socketPipe->mutex;
    const char *devName = socketThread->devName;
    fd_set fds;
    char *buffer = NULL;

    rv = length = 0;
    socketFd = maxFd = pipeFd = -1;

    /* Open pipe */
    if ((rv = pipe(socketPipe->fds)) != 0) {
        logError(ALL, "src/core/unitlogd/socket.c", "startSocketThread", errno,
                      strerror(errno), "Unable to run pipe for the dev (%s)", devName);
        goto out;
    }
    /* Lock mutex */
    if ((rv = pthread_mutex_lock(mutexPipe)) != 0) {
        logError(ALL, "src/core/unitlogd/socket.c", "startSocketThread",
                      rv, strerror(rv), "Unable to acquire the pipe mutex lock (%s)", devName);
        goto out;
    }
    /* Get socket fd */
    if ((socketFd = getUnixSocketFd(socketThread->devName)) == -1)
        goto out;

    /* Get max fd */
    pipeFd = socketPipe->fds[0];
    maxFd = getMaxFileDesc(&socketFd, &pipeFd);

    while (1) {
        FD_ZERO(&fds);
        FD_SET(pipeFd, &fds);
        FD_SET(socketFd, &fds);

        /* Wait for data */
        select(maxFd, &fds, NULL, NULL, NULL);
        if (FD_ISSET(pipeFd, &fds)) {
            if ((length = read(pipeFd, &input, sizeof(int))) == -1) {
                logError(SYSTEM, "src/core/unitlogd/socket.c", "startSocketThread",
                              errno, strerror(errno), "Unable to read from pipe for the dev (%s)!", devName);
                goto out;
            }
            if (input == THREAD_EXIT)
                goto out;
        }
        else if (FD_ISSET(socketFd, &fds)) {
            /* Prepare the buffer */
            bufferSize = BUFFER_SIZE;
            buffer = calloc(bufferSize, sizeof(char));
            assert(buffer);
            if ((rv = recv(socketFd, buffer, bufferSize, 0)) == -1) {
                logError(SYSTEM, "src/core/unitlogd/socket.c", "startSocketThread",
                              errno, strerror(errno), "Unable to read from socket for the dev (%s)!", devName);
                goto out;
            }
            printf("\nBuffer of %s = \n%s", devName, buffer);
            objectRelease(&buffer);
        }
    }

    out:
        close(socketFd);
        unlink(devName);
        if ((rv = pthread_mutex_unlock(mutexPipe)) != 0) {
            logError(ALL, "src/core/unitlogd/socket.c", "startSocketThread",
                          rv, strerror(rv), "Unable to unlock the pipe mutex for (%s)", devName);
        }
        pthread_exit(0);
}

void*
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
        logError(CONSOLE, "src/core/unitlogd/socket.c", "startSocket", errno,
                      strerror(errno), "Unable to create the socket thread (detached) (%s)", devName);
    }
    else {
//FIXME evaluate where to log.....
        if (UNITLOGD_DEBUG)
            logInfo(CONSOLE, "Socket thread (detached) created successfully (%s)\n", devName);
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
            logError(CONSOLE, "src/core/unitlogd/socket.c", "startSockets", errno,
                          strerror(errno), "Unable to create the thread for the '%s' dev", devName);
            break;
        }
        else {
//FIXME evaluate where to log.....
            if (UNITLOGD_DEBUG)
                logInfo(CONSOLE, "Thread created succesfully for the '%s' dev\n", devName);
        }

    }
    for (int i = 0; i < len; i++) {
        socketThread = arrayGet(socketThreads, i);
        devName = socketThread->devName;
        if (pthread_join(socketThread->thread, (void **)&rvThread) != EXIT_SUCCESS) {
            rv = 1;
//FIXME evaluate where to log.....
            logError(CONSOLE, "src/core/processes/process.c", "startSockets", rv,
                          strerror(rv), "Unable to join the thread for the '%s' dev", devName);
        }
        else {
//FIXME evaluate where to log.....
            if (UNITLOGD_DEBUG)
                logInfo(CONSOLE, "Thread joined successfully for the '%s' dev! Return code = %d\n",
                             devName, *rvThread);
        }
        if (*rvThread == 1)
            rv = 1;
        objectRelease(&rvThread);
    }

    return rv;
}

int
getUnixSocketFd(const char *socketPath) {
    int socketFd = -1;
    struct sockaddr_un sa = {0};

    assert(socketPath);

    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, socketPath, sizeof(sa.sun_path));
    unlink(socketPath);

    if ((socketFd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
//FIXME check where to log
        logError(CONSOLE, "src/core/unitlogd/socket.c", "socketUnix",
                      errno, strerror(errno), "Socket error");
        goto out;
    }

    if (bind(socketFd, (const struct sockaddr *)&sa, sizeof(struct sockaddr_un)) == -1)
//FIXME check where to log
        logError(CONSOLE, "src/core/unitlogd/socket.c", "socketUnix",
                      errno, strerror(errno), "Bind error");
    out:
        return socketFd;
}

void*
stopSocket(void *arg)
{
    SocketThread *socketThread = (SocketThread *)arg;
    assert(socketThread);
    Pipe *pipe = socketThread->pipe;
    pthread_mutex_t *mutex = pipe->mutex;
    const char *devName = socketThread->devName;
    int rv = 0, output = THREAD_EXIT, *rvThread;

    if ((rv = write(pipe->fds[1], &output, sizeof(int))) == -1) {
//FIXME where to log.....
        logError(CONSOLE, "src/core/unitlogd/socket.c", "stopSocket",
                      errno, strerror(errno), "Unable to write into pipe for the %s dev", devName);
        goto out;
    }
    if ((rv = pthread_mutex_lock(mutex)) != 0) {
//FIXME where to log.....
        logError(CONSOLE, "src/core/unitlogd/socket.c", "stopSocket",
                      rv, strerror(rv), "Unable to acquire the lock of the pipe mutex for the %s dev",
                      devName);
        goto out;
    }
    if ((rv = pthread_mutex_unlock(mutex)) != 0) {
//FIXME where to log.....
        logError(CONSOLE, "src/core/unitlogd/socket.c", "stopSocket",
                      rv, strerror(rv), "Unable to unlock the pipe mutex for the %s dev", devName);
        goto out;
    }

    out:
        /* It will be freed in stopSockets func */
        rvThread = calloc(1, sizeof(int));
        assert(rvThread);
        *rvThread = rv;
        return rvThread;
}

int
stopSockets(Array *socketThreads)
{
    SocketThread *socketThread = NULL;
    const char *devName = NULL;
    int rv, len, *rvThread;

    rv = len = 0;

    len = socketThreads ? socketThreads->size : 0;
    for (int i = 0; i < len; i++) {
        socketThread = arrayGet(socketThreads, i);
        devName = socketThread->devName;
        if ((rv = pthread_create(&socketThread->thread, NULL, stopSocket, socketThread)) != 0) {
//FIXME evaluate where to log.....
            logError(CONSOLE, "src/core/unitlogd/socket.c", "stopSockets", errno,
                          strerror(errno), "Unable to create the thread for the '%s' dev", devName);
            break;
        }
        else {
//FIXME evaluate where to log.....
            if (UNITLOGD_DEBUG)
                logInfo(CONSOLE, "Thread created succesfully for the '%s' dev\n", devName);
        }
    }
    for (int i = 0; i < len; i++) {
        socketThread = arrayGet(socketThreads, i);
        devName = socketThread->devName;
        if (pthread_join(socketThread->thread, (void **)&rvThread) != EXIT_SUCCESS) {
            rv = 1;
//FIXME evaluate where to log.....
            logError(CONSOLE, "src/core/processes/process.c", "stopSockets", rv,
                          strerror(rv), "Unable to join the thread for the '%s' dev", devName);
        }
        else {
//FIXME evaluate where to log.....
            if (UNITLOGD_DEBUG)
                logInfo(CONSOLE, "Thread joined successfully for the '%s' dev! Return code = %d\n",
                             devName, *rvThread);
        }
        if (*rvThread == 1)
            rv = 1;
        objectRelease(&rvThread);
    }

    return rv;
}
