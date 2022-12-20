/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

Cleaner *CLEANER;

Cleaner*
cleanerNew()
{
    int rv = 0;
    Cleaner *cleaner = NULL;
    pthread_mutex_t *mutex = NULL;

    /* Cleaner */
    cleaner = calloc(1, sizeof(Cleaner));
    assert(cleaner);

    /* Mutex */
    mutex = calloc(1, sizeof(pthread_mutex_t));
    assert(mutex);
    cleaner->mutex = mutex;
    if ((rv = pthread_mutex_init(mutex, NULL)) != 0) {
        logError(UNITD_BOOT_LOG, "src/core/handlers/cleaner.c", "cleanerNew", rv, strerror(rv),
                      "Unable to run pthread_mutex_init");
    }
    assert(rv == 0);

    /* Pipe */
    cleaner->pipe = pipeNew();

    return cleaner;
}

void
cleanerRelease(Cleaner **cleaner)
{
    Cleaner *cleanerTemp = *cleaner;
    pthread_mutex_t *mutex = NULL;
    int rv = 0;

    if (cleanerTemp) {
        /* Destroy and free the mutex */
        if ((mutex = cleanerTemp->mutex)) {
            if ((rv = pthread_mutex_destroy(mutex)) != 0) {
                logError(CONSOLE, "src/core/handlers/cleaner.c", "cleanerRelease", rv,
                              strerror(rv), "Unable to run pthread_mutex_destroy");
            }
            objectRelease(&mutex);
        }
        /* Pipe */
        pipeRelease(&cleanerTemp->pipe);
        /* Release cleaner */
        objectRelease(cleaner);
    }
}

void*
startCleanerThread(void *arg UNUSED)
{
    int rv, input, fd;
    fd_set fds;
    struct timeval tv = {0};
    pthread_mutex_t *mutex = NULL;
    Pipe *pipe = NULL;

    mutex = CLEANER->mutex;
    pipe = CLEANER->pipe;
    fd = pipe->fds[0];
    rv = input = 0;

    /* Lock pipe mutex */
    if ((rv = pthread_mutex_lock(pipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleanerThread",
                      rv, strerror(rv), "Unable to lock the pipe mutex");
        kill(UNITD_PID, SIGTERM);
    }

    /* Unlock mutex just before main loop. */
    if ((rv = pthread_mutex_unlock(mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleanerThread",
                      rv, strerror(rv), "Unable to unlock the cleaner mutex");
        kill(UNITD_PID, SIGTERM);
    }

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
        }
        else {
            /* Reap all pending child processes */
            reapPendingChild();
        }
    }

    out:
        /* Unlock mutex */
        if ((rv = pthread_mutex_unlock(pipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleanerThread",
                          rv, strerror(rv), "Unable to unlock the pipe mutex");
        }
        if (UNITD_DEBUG)
            logInfo(UNITD_BOOT_LOG, "Start cleaner thread (detached) exited successfully\n");
        pthread_exit(0);
}

void
startCleaner()
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
                      strerror(errno), "pthread_attr_setdetachstate returned bad exit code %d",
                 rv);
        kill(UNITD_PID, SIGTERM);
    }

    /* Lock. It will be unlocked in startCleanerThread. */
    handleMutex(CLEANER->mutex, true);

    if ((rv = pthread_create(&thread, &attr, startCleanerThread, NULL)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "startCleaner", rv,
                      strerror(rv), "Unable to create the start cleaner thread (detached)");
        kill(UNITD_PID, SIGTERM);
    }
    else {
        if (UNITD_DEBUG)
            logInfo(UNITD_BOOT_LOG, "Start cleaner thread (detached) created successfully\n");
    }

    /* Assure us that the pipe of the cleaner is listening */
    handleMutex(CLEANER->mutex, true);
    handleMutex(CLEANER->mutex, false);
    pthread_attr_destroy(&attr);
}

void
stopCleaner()
{
    int rv = 0, output = THREAD_EXIT;
    if (CLEANER) {
        Pipe *pipe = CLEANER->pipe;
        if ((rv = uWrite(pipe->fds[1], &output, sizeof(int))) == -1) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "stopCleaner",
                          errno, strerror(errno), "Unable to write into pipe for the cleaner");
        }
        /* Lock mutex */
        if ((rv = pthread_mutex_lock(pipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "stopCleaner",
                          rv, strerror(rv), "Unable to acquire the pipe mutex lock");
        }
        /* Unlock mutex */
        if ((rv = pthread_mutex_unlock(pipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/cleaner.c", "stopCleaner",
                          rv, strerror(rv), "Unable to unlock the pipe mutex");
        }
        cleanerRelease(&CLEANER);
    }
}
