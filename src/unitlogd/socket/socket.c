/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

int UNITLOGD_PID;
const char *DEV_LOG_NAME;

SocketThread *socketThreadNew()
{
    SocketThread *socketThread = calloc(1, sizeof(SocketThread));
    assert(socketThread);
    socketThread->devName = NULL;
    socketThread->pipe = pipeNew();

    return socketThread;
}

void socketThreadRelease(SocketThread **socketThread)
{
    if (*socketThread) {
        objectRelease(&(*socketThread)->devName);
        pipeRelease(&(*socketThread)->pipe);
        objectRelease(socketThread);
    }
}

static int forwardToLog(char *buffer)
{
    int socketFd = -1, rv = 0;
    bool terminate = false;
    struct sockaddr_un sa = { 0 };

    assert(buffer);

    if ((socketFd = getSocketFd(&sa, DEV_LOG_NAME)) == -1) {
        rv = 1;
        goto out;
    }
    while ((rv = connect(socketFd, (const struct sockaddr *)&sa, sizeof(struct sockaddr_un))) ==
               -1 &&
           !UNITLOGD_EXIT) {
        if (DEBUG)
            logWarning(CONSOLE | SYSTEM, "%s is down! Retry to connect .....\n", DEV_LOG_NAME);
        if (!terminate) {
            msleep(1000);
            terminate = true;
        } else
            break;
    }
    if (rv != 0 && !UNITLOGD_EXIT) {
        logError(CONSOLE | SYSTEM, "src/unitlogd/socket/socket.c", "forwardToLog", errno,
                 strerror(errno), "Connect error");
        goto out;
    }
    if ((rv = uSend(socketFd, buffer, strlen(buffer), 0)) == -1 && !UNITLOGD_EXIT) {
        logError(CONSOLE | SYSTEM, "src/unitlogd/socket/socket.c", "forwardToLog", errno,
                 strerror(errno), "send error");
        goto out;
    } else
        rv = 0;

out:
    close(socketFd);
    return rv;
}

void *startForwarderThread(void *arg)
{
    int rv = 0, fileFd = -1, maxFd = -1, pipeFd = -1, input, length = 0, bufferSize;
    SocketThread *socketThread = (SocketThread *)arg;
    Pipe *socketPipe = NULL;
    pthread_mutex_t *mutexPipe = NULL;
    const char *devName = NULL;
    fd_set fds;
    char *buffer = NULL;
    bool kmsgAppended = false;
    struct timeval tv = { 0 };

    assert(socketThread);

    socketPipe = socketThread->pipe;
    mutexPipe = socketPipe->mutex;
    devName = socketThread->devName;
    if ((rv = pthread_mutex_lock(mutexPipe)) != 0) {
        logError(CONSOLE | SYSTEM, "src/unitlogd/socket/socket.c", "startForwarderThread", rv,
                 strerror(rv), "Unable to acquire the pipe mutex lock (%s)", devName);
        kill(UNITLOGD_PID, SIGTERM);
        goto out;
    }
    if ((fileFd = open(socketThread->devName, O_RDONLY)) == -1) {
        logError(CONSOLE | SYSTEM, "src/unitlogd/socket/socket.c", "startForwarderThread", errno,
                 strerror(errno), "Unable to open (%s)", devName);
        kill(UNITLOGD_PID, SIGTERM);
        goto out;
    }
    pipeFd = socketPipe->fds[0];
    maxFd = getMaxFileDesc(&fileFd, &pipeFd);
    while (1) {
        FD_ZERO(&fds);
        FD_SET(pipeFd, &fds);
        FD_SET(fileFd, &fds);
        tv.tv_usec = 100000; //100 ms
        if (select(maxFd, &fds, NULL, NULL, !kmsgAppended ? &tv : NULL) == -1 && errno == EINTR)
            continue;
        if (FD_ISSET(pipeFd, &fds)) {
            if ((length = uRead(pipeFd, &input, sizeof(int))) == -1) {
                logError(CONSOLE | SYSTEM, "src/unitlogd/socket/socket.c", "startForwarderThread",
                         errno, strerror(errno), "Unable to read from pipe for the dev (%s)!",
                         devName);
                goto out;
            }
            if (input == THREAD_EXIT)
                goto out;
        }
        if (FD_ISSET(fileFd, &fds)) {
            bufferSize = BUFFER_SIZE;
            buffer = calloc(bufferSize, sizeof(char));
            assert(buffer);
            if ((rv = uRead(fileFd, buffer, bufferSize)) == -1) {
                logError(CONSOLE | SYSTEM, "src/unitlogd/socket/socket.c", "startForwarderThread",
                         errno, strerror(errno), "Unable to read from dev (%s)!", devName);
                goto out;
            }
            /* The buffer records in /proc/kmsg will be removed after read, thus, we are not able
             * to append these data if we are restarting unitlog daemon.
             * To fix that, we check if the first record contains "] Linux version" string.
             * If not so, we append kmsg log file content
             * without lose facilities, priorities and colors accordingly.
            */
            if (!kmsgAppended) {
                if (stringContainsStr(buffer, "] Linux version")) {
                    if ((rv = createKmsgLog()) != 0) {
                        if (!UNITLOGD_EXIT)
                            kill(UNITLOGD_PID, SIGTERM);
                        goto out;
                    }
                } else
                    appendKmsg();
                kmsgAppended = true;
            }
            /* Populate kmsg.log and redirect to dev/log */
            Array *lines = stringSplit(buffer, NEW_LINE, true);
            int len = lines ? lines->size : 0;
            char *line = NULL;
            for (int i = 0; i < len; i++) {
                line = arrayGet(lines, i);
                writeKmsg(line);
                if ((rv = forwardToLog(line)) != 0)
                    break;
            }
            arrayRelease(&lines);
            objectRelease(&buffer);
            if (rv != 0) {
                if (!UNITLOGD_EXIT)
                    kill(UNITLOGD_PID, SIGTERM);
                goto out;
            }
        } else if (!kmsgAppended) {
            /* If we are restarting unitlog daemon and /proc/kmsg is already empty then
             * we append kmsg log file content to avoid having an empty boot.
            */
            appendKmsg();
            kmsgAppended = true;
        }
    }

out:
    close(fileFd);
    if ((rv = pthread_mutex_unlock(mutexPipe)) != 0) {
        logError(CONSOLE, "src/unitlogd/socket/socket.c", "startUnixThread", rv, strerror(rv),
                 "Unable to unlock the pipe mutex for (%s)", devName);
    }
    pthread_exit(0);
}

void *startUnixThread(void *arg)
{
    int rv = 0, socketFd = -1, maxFd = -1, pipeFd = -1, input, length = 0, bufferSize;
    SocketThread *socketThread = (SocketThread *)arg;
    Pipe *socketPipe = NULL;
    pthread_mutex_t *mutexPipe = NULL;
    const char *devName = NULL;
    fd_set fds;
    char *buffer = NULL;
    struct sockaddr_un sa = { 0 };

    assert(socketThread);

    socketPipe = socketThread->pipe;
    mutexPipe = socketPipe->mutex;
    devName = socketThread->devName;
    if ((rv = pthread_mutex_lock(mutexPipe)) != 0) {
        logError(CONSOLE, "src/unitlogd/socket/socket.c", "startUnixThread", rv, strerror(rv),
                 "Unable to acquire the pipe mutex lock (%s)", devName);
        kill(UNITLOGD_PID, SIGTERM);
        goto out;
    }
    unlink(socketThread->devName);
    if ((socketFd = getSocketFd(&sa, socketThread->devName)) == -1) {
        kill(UNITLOGD_PID, SIGTERM);
        goto out;
    }
    if (bind(socketFd, (const struct sockaddr *)&sa, sizeof(struct sockaddr_un)) == -1) {
        logError(CONSOLE, "src/unitlogd/socket/socket.c", "startUnixThread", errno, strerror(errno),
                 "Bind error");
        kill(UNITLOGD_PID, SIGTERM);
        goto out;
    }
    if (chmod(socketThread->devName, 666) == -1) {
        logError(CONSOLE, "src/unitlogd/socket/socket.c", "startUnixThread", errno, strerror(errno),
                 "Chmod error");
        kill(UNITLOGD_PID, SIGTERM);
        goto out;
    }
    pipeFd = socketPipe->fds[0];
    maxFd = getMaxFileDesc(&socketFd, &pipeFd);
    while (1) {
        FD_ZERO(&fds);
        FD_SET(pipeFd, &fds);
        FD_SET(socketFd, &fds);
        if (select(maxFd, &fds, NULL, NULL, NULL) == -1 && errno == EINTR)
            continue;
        if (FD_ISSET(pipeFd, &fds)) {
            if ((length = uRead(pipeFd, &input, sizeof(int))) == -1) {
                logError(CONSOLE, "src/unitlogd/socket/socket.c", "startUnixThread", errno,
                         strerror(errno), "Unable to read from pipe for the dev (%s)!", devName);
                kill(UNITLOGD_PID, SIGTERM);
                goto out;
            }
            if (input == THREAD_EXIT)
                goto out;
        } else if (FD_ISSET(socketFd, &fds)) {
            /* Prepare the buffer */
            bufferSize = BUFFER_SIZE;
            buffer = calloc(bufferSize, sizeof(char));
            assert(buffer);
            if ((rv = uRecv(socketFd, buffer, bufferSize, 0)) == -1) {
                logError(CONSOLE, "src/unitlogd/socket/socket.c", "startUnixThread", errno,
                         strerror(errno), "Unable to read from socket for the dev (%s)!", devName);
                kill(UNITLOGD_PID, SIGTERM);
                goto out;
            }
            /* Lock
             * We cannot write into log if the unitlogctl is cutting it!
             * See vacuum func.
            */
            if ((rv = handleLockFile(true)) != 0) {
                kill(UNITLOGD_PID, SIGTERM);
                goto out;
            }
            /* Process buffer and release */
            processLine(buffer);
            objectRelease(&buffer);
            /* UnLock */
            if ((rv = handleLockFile(false)) != 0) {
                kill(UNITLOGD_PID, SIGTERM);
                goto out;
            }
        }
    }

out:
    close(socketFd);
    unlink(devName);
    if ((rv = pthread_mutex_unlock(mutexPipe)) != 0) {
        logError(CONSOLE, "src/unitlogd/socket/socket.c", "startUnixThread", rv, strerror(rv),
                 "Unable to unlock the pipe mutex for (%s)", devName);
    }
    pthread_exit(0);
}

void *startSocket(void *arg)
{
    pthread_t thread;
    pthread_attr_t attr;
    int rv = 0, *rvThread;
    SocketThread *socketThread = NULL;
    const char *devName = NULL;

    assert(arg);

    socketThread = (SocketThread *)arg;
    devName = socketThread->devName;
    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0);
    rv = pthread_create(&thread, &attr,
                        socketThread->sockType == UNIX ? startUnixThread : startForwarderThread,
                        socketThread);
    if (rv != 0) {
        logError(CONSOLE | SYSTEM, "src/unitlogd/socket/socket.c", "startSocket", rv, strerror(rv),
                 "Unable to create the socket thread (detached) (%s)", devName);
    } else {
        if (DEBUG)
            logInfo(CONSOLE | SYSTEM, "Socket thread (detached) created successfully (%s)\n",
                    devName);
    }
    pthread_attr_destroy(&attr);

    rvThread = calloc(1, sizeof(int));
    assert(rvThread);
    *rvThread = rv;
    return rvThread;
}

int startSockets(Array *socketThreads)
{
    SocketThread *socketThread = NULL;
    const char *devName = NULL;
    int rv = 0, len = 0, *rvThread;

    len = socketThreads ? socketThreads->size : 0;
    for (int i = 0; i < len; i++) {
        socketThread = arrayGet(socketThreads, i);
        devName = socketThread->devName;
        if ((rv = pthread_create(&socketThread->thread, NULL, startSocket, socketThread)) != 0) {
            logError(CONSOLE | SYSTEM, "src/unitlogd/socket/socket.c", "startSockets", rv,
                     strerror(rv), "Unable to create the thread for the '%s' dev", devName);
            kill(UNITLOGD_PID, SIGTERM);
        } else {
            if (DEBUG)
                logInfo(CONSOLE | SYSTEM, "Thread created (Start) succesfully for the '%s' dev\n",
                        devName);
        }
    }
    for (int i = 0; i < len; i++) {
        socketThread = arrayGet(socketThreads, i);
        devName = socketThread->devName;
        if ((rv = pthread_join(socketThread->thread, (void **)&rvThread)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "startSockets", rv,
                     strerror(rv), "Unable to join the thread for the '%s' dev", devName);
            kill(UNITLOGD_PID, SIGTERM);
        } else {
            if (DEBUG)
                logInfo(CONSOLE | SYSTEM,
                        "Thread joined (Start) successfully for the '%s' dev! Return code = %d\n",
                        devName, *rvThread);
        }
        if (*rvThread == 1)
            rv = 1;
        objectRelease(&rvThread);
    }

    return rv;
}

int getSocketFd(struct sockaddr_un *sa, const char *devName)
{
    int socketFd = -1;

    assert(devName);

    sa->sun_family = AF_UNIX;
    strncpy(sa->sun_path, devName, sizeof(sa->sun_path));
    if ((socketFd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        logError(CONSOLE | SYSTEM, "src/unitlogd/socket/socket.c", "getSocketFd", errno,
                 strerror(errno), "Unable to get the socket fd for %s dev", devName);
        goto out;
    }

out:
    return socketFd;
}

void *stopSocket(void *arg)
{
    SocketThread *socketThread = (SocketThread *)arg;
    Pipe *pipe = NULL;
    pthread_mutex_t *mutex = NULL;
    const char *devName = NULL;
    int rv = 0, output = THREAD_EXIT, *rvThread;

    assert(socketThread);

    pipe = socketThread->pipe;
    mutex = pipe->mutex;
    devName = socketThread->devName;
    if ((rv = uWrite(pipe->fds[1], &output, sizeof(int))) == -1) {
        logError(CONSOLE, "src/unitlogd/socket/socket.c", "stopSocket", errno, strerror(errno),
                 "Unable to write into pipe for the %s dev", devName);
    }
    if ((rv = pthread_mutex_lock(mutex)) != 0) {
        logError(CONSOLE, "src/unitlogd/socket/socket.c", "stopSocket", rv, strerror(rv),
                 "Unable to acquire the lock of the pipe mutex for the %s dev", devName);
    }
    if ((rv = pthread_mutex_unlock(mutex)) != 0) {
        logError(CONSOLE, "src/unitlogd/socket/socket.c", "stopSocket", rv, strerror(rv),
                 "Unable to unlock the pipe mutex for the %s dev", devName);
    }

    rvThread = calloc(1, sizeof(int));
    assert(rvThread);
    *rvThread = rv;
    return rvThread;
}

int stopSockets(Array *socketThreads)
{
    SocketThread *socketThread = NULL;
    const char *devName = NULL;
    int rv = 0, len = 0, *rvThread;

    len = socketThreads ? socketThreads->size : 0;
    for (int i = 0; i < len; i++) {
        socketThread = arrayGet(socketThreads, i);
        devName = socketThread->devName;
        if ((rv = pthread_create(&socketThread->thread, NULL, stopSocket, socketThread)) != 0) {
            logError(CONSOLE, "src/unitlogd/socket/socket.c", "stopSockets", rv, strerror(rv),
                     "Unable to create the thread for the '%s' dev", devName);
        } else {
            if (DEBUG)
                logInfo(CONSOLE, "Thread created (Stop) succesfully for the '%s' dev\n", devName);
        }
    }
    for (int i = 0; i < len; i++) {
        socketThread = arrayGet(socketThreads, i);
        devName = socketThread->devName;
        if ((rv = pthread_join(socketThread->thread, (void **)&rvThread)) != 0) {
            logError(CONSOLE, "src/unitlogd/socket/socket.c", "stopSockets", rv, strerror(rv),
                     "Unable to join the thread for the '%s' dev", devName);
        } else {
            if (DEBUG)
                logInfo(CONSOLE,
                        "Thread joined (Stop) successfully for the '%s' dev! Return code = %d\n",
                        devName, *rvThread);
        }
        if (*rvThread == 1)
            rv = 1;
        objectRelease(&rvThread);
    }

    return rv;
}
