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
            assert(rv == 0);
            objectRelease(&mutex);
        }
        /* Close fds */
        close(cleanerTemp->fds[0]);
        close(cleanerTemp->fds[1]);
        /* Release cleaner */
        objectRelease(cleaner);
    }
}

void*
runCleanerThread()
{
    int rv, input, fd;
    fd_set fds;
    struct timeval tv = {0};
    pthread_mutex_t *mutex = NULL;

    assert(!CLEANER);
    CLEANER = cleanerNew();
    mutex = CLEANER->mutex;
    rv = input = fd = 0;

    /* Open pipe */
    if ((rv = pipe(CLEANER->fds)) != 0) {
        logError(ALL, "src/core/handlers/cleaner.c", "runCleanerThread", errno,
                      strerror(errno), "Unable to run pipe for the cleaner");
        goto out;
    }
    /* Lock mutex */
    if ((rv = pthread_mutex_lock(mutex)) != 0) {
        logError(ALL, "src/core/handlers/cleaner.c", "runCleanerThread",
                      rv, strerror(rv), "Unable to acquire the cleaner mutex lock");
        goto out;
    }

    while (1) {
        FD_ZERO(&fds);
        fd = CLEANER->fds[0];
        FD_SET(fd, &fds);
        /* Reset the timeout */
        tv.tv_sec = CLEANER_TIMEOUT;

        if (select(fd + 1, &fds, NULL, NULL, &tv) == -1 && errno == EINTR)
            continue;
        if (FD_ISSET(fd, &fds)) {
            if ((rv = read(fd, &input, sizeof(int))) == -1) {
                logError(SYSTEM, "src/core/handlers/cleaner.c", "runCleanerThread",
                              errno, strerror(errno), "Unable to read from pipe for the cleaner!");
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
        if ((rv = pthread_mutex_unlock(mutex)) != 0) {
            logError(CONSOLE, "src/core/handlers/cleaner.c", "runCleanerThread",
                          rv, strerror(rv), "Unable to unlock the cleaner mutex");
        }
        pthread_exit(0);
}

void*
startCleanerThread(void *arg UNUSED)
{
    pthread_t thread;
    pthread_attr_t attr;
    int rv = 0;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if ((rv = pthread_create(&thread, &attr, runCleanerThread, NULL)) != 0) {
        logError(ALL, "src/core/handlers/cleaner.c", "startCleanerThread", errno,
                      strerror(errno), "Unable to create the runCleaner thread (detached)");
    }
    else {
        if (UNITD_DEBUG)
            logInfo(UNITD_BOOT_LOG, "Run cleaner thread (detached) created successfully\n");
    }
    return NULL;
}

void
startCleaner()
{
    pthread_t thread;
    int rv = 0;

    if ((rv = pthread_create(&thread, NULL, startCleanerThread, NULL)) != 0) {
        logError(ALL, "src/core/handlers/cleaner.c", "startCleaner", errno,
                      strerror(errno), "Unable to create the thread for the cleaner");
    }
    else {
        if (UNITD_DEBUG)
            logInfo(UNITD_BOOT_LOG, "Thread created successfully for the cleaner\n");
    }
    /* Waiting for the thread to terminate */
    if ((rv = pthread_join(thread, NULL)) != EXIT_SUCCESS) {
        logError(ALL, "src/core/handlers/cleaner.c", "startCleaner", rv,
                      strerror(rv), "Unable to join the thread for the cleaner");
    }
    else {
        if (UNITD_DEBUG)
            logInfo(UNITD_BOOT_LOG, "Thread joined successfully for the cleaner\n");
    }

}

void*
stopCleanerThread(void *arg UNUSED)
{
    int rv = 0;
    int output = THREAD_EXIT;
    assert(CLEANER);
    pthread_mutex_t *mutex = CLEANER->mutex;

    if ((rv = write(CLEANER->fds[1], &output, sizeof(int))) == -1) {
        logError(CONSOLE, "src/core/handlers/cleaner.c", "stopCleanerThread",
                      errno, strerror(errno), "Unable to write into pipe for the cleaner");
        goto out;
    }
    /* Lock mutex */
    if ((rv = pthread_mutex_lock(mutex)) != 0) {
        logError(CONSOLE, "src/core/handlers/cleaner.c", "stopCleanerThread",
                      rv, strerror(rv), "Unable to acquire the cleaner mutex lock");
        goto out;
    }
    /* Unlock mutex */
    if ((rv = pthread_mutex_unlock(mutex)) != 0) {
        logError(CONSOLE, "src/core/handlers/cleaner.c", "stopCleanerThread",
                      rv, strerror(rv), "Unable to unlock the cleaner mutex");
    }

    out:
        cleanerRelease(&CLEANER);
        return NULL;
}

void
stopCleaner()
{
    pthread_t thread;
    int rv = 0;

    if (CLEANER) {
        if ((rv = pthread_create(&thread, NULL, stopCleanerThread, NULL)) != 0) {
            logError(CONSOLE, "src/core/handlers/cleaner.c", "stopCleaner", errno,
                          strerror(errno), "Unable to create the thread for the cleaner (stop)");
        }
        else {
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Thread created successfully for the cleaner (stop)\n");
        }
        /* Waiting for the thread to terminate */
        if ((rv = pthread_join(thread, NULL)) != EXIT_SUCCESS) {
            logError(CONSOLE, "src/core/handlers/cleaner.c", "stopCleaner", rv,
                          strerror(rv), "Unable to join the thread for the cleaner (stop)");
        }
        else {
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Thread joined successfully for the cleaner (stop)\n");
        }
    }
}
