/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

Cleaner *CLEANER;

Cleaner *cleanerNew()
{
    Cleaner *cleaner = calloc(1, sizeof(Cleaner));
    assert(cleaner);
    cleaner->pipe = pipeNew();
    return cleaner;
}

void cleanerRelease(Cleaner **cleaner)
{
    Cleaner *cleanerTemp = *cleaner;
    if (cleanerTemp) {
        pipeRelease(&cleanerTemp->pipe);
        objectRelease(cleaner);
    }
}

void *startCleanerThread(void *arg UNUSED)
{
    int rv, input, fd;
    fd_set fds;
    struct timeval tv = { 0 };
    Pipe *pipe = NULL;

    pipe = CLEANER->pipe;
    fd = pipe->fds[0];
    rv = input = 0;

    if ((rv = pthread_mutex_lock(pipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleanerThread", rv,
                 strerror(rv), "Unable to lock the pipe mutex");
        kill(UNITD_PID, SIGTERM);
    }
    /* Before to start, we wait for system is up.
     * We check if ctrl+alt+del is pressed as well.
    */
    while (!LISTEN_SOCK_REQUEST && SHUTDOWN_COMMAND == NO_COMMAND)
        msleep(50);
    if (SHUTDOWN_COMMAND != NO_COMMAND)
        goto out;
    while (1) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        /* Reset the timeout */
        tv.tv_sec = CLEANER_TIMEOUT;
        if (select(fd + 1, &fds, NULL, NULL, &tv) == -1 && errno == EINTR)
            continue;
        if (FD_ISSET(fd, &fds)) {
            if ((rv = uRead(fd, &input, sizeof(int))) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleanerThread",
                         errno, strerror(errno), "Unable to read from pipe for the cleaner!");
                kill(UNITD_PID, SIGTERM);
                goto out;
            }
            if (input == THREAD_EXIT)
                goto out;
        } else {
            /* Reap all pending child processes */
            reapPendingChild();
        }
    }

out:
    if ((rv = pthread_mutex_unlock(pipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleanerThread", rv,
                 strerror(rv), "Unable to unlock the pipe mutex");
    }
    if (DEBUG)
        logInfo(CONSOLE | SYSTEM, "Cleaner thread exited successfully\n");
    pthread_exit(0);
}

void startCleaner()
{
    pthread_t thread;
    pthread_attr_t attr;
    int rv = 0;

    assert(!CLEANER);
    CLEANER = cleanerNew();
    if ((rv = pthread_attr_init(&attr)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleaner", errno,
                 strerror(errno), "pthread_attr_init returned bad exit code %d", rv);
        kill(UNITD_PID, SIGTERM);
    }
    if ((rv = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleaner", errno,
                 strerror(errno), "pthread_attr_setdetachstate returned bad exit code %d", rv);
        kill(UNITD_PID, SIGTERM);
    }
    if ((rv = pthread_create(&thread, &attr, startCleanerThread, NULL)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleaner", rv, strerror(rv),
                 "Unable to create the start cleaner thread (detached)");
        kill(UNITD_PID, SIGTERM);
    } else {
        if (DEBUG)
            logInfo(CONSOLE | SYSTEM, "Thread created successfully for the cleaner\n");
    }
    pthread_attr_destroy(&attr);
}

void stopCleaner()
{
    if (CLEANER) {
        int rv = 0, output = THREAD_EXIT;
        Pipe *pipe = CLEANER->pipe;
        if ((rv = uWrite(pipe->fds[1], &output, sizeof(int))) == -1) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "stopCleaner", errno,
                     strerror(errno), "Unable to write into pipe for the cleaner");
        }
        if ((rv = pthread_mutex_lock(pipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "stopCleaner", rv,
                     strerror(rv), "Unable to acquire the pipe mutex lock");
        }
        if ((rv = pthread_mutex_unlock(pipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "stopCleaner", rv,
                     strerror(rv), "Unable to unlock the pipe mutex");
        }
    }
}
