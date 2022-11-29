/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

#define EVENT_SIZE          (sizeof(struct inotify_event))
#define EVENT_BUF_LEN       (1024 * (EVENT_SIZE + 16))

bool USER_INSTANCE;
Array *NOTIFIERS;
bool NOTIFIER_WORKING;

Notifier*
notifierNew()
{
    int rv = 0;
    Notifier *notifier = NULL;
    pthread_mutex_t *mutex = NULL;
    int *fd, *wd;

    fd = wd = NULL;

    /* Notifier */
    notifier = calloc(1, sizeof(Notifier));
    assert(notifier);

    /* Fd */
    fd = calloc(1, sizeof(int));
    assert(fd);
    *fd = -1;
    notifier->fd = fd;

    /* Wd */
    wd = calloc(1, sizeof(int));
    assert(wd);
    *wd = -1;
    notifier->wd = wd;

    /* Mutex */
    mutex = calloc(1, sizeof(pthread_mutex_t));
    assert(mutex);
    notifier->mutex = mutex;
    if ((rv = pthread_mutex_init(mutex, NULL)) != 0) {
        logError(UNITD_BOOT_LOG, "src/core/handlers/notifier.c", "notifierNew", rv, strerror(rv),
                      "Unable to run pthread_mutex_init");
    }
    assert(rv == 0);

    return notifier;
}

void
notifierRelease(Notifier **notifier)
{
    Notifier *notifierTemp = *notifier;
    pthread_mutex_t *mutex = NULL;
    char *watchDir = NULL;
    int rv = 0;
    int *fd, *wd;

    fd = wd = NULL;

    if (notifierTemp) {
        /* Destroy and free the mutex */
        if ((mutex = notifierTemp->mutex)) {
            if ((rv = pthread_mutex_destroy(mutex)) != 0) {
                logError(CONSOLE, "src/core/handlers/notifier.c", "notifierRelease", rv,
                              strerror(rv), "Unable to run pthread_mutex_destroy");
            }
            assert(rv == 0);
            objectRelease(&mutex);
        }

        if ((fd = notifierTemp->fd))
            objectRelease(&fd);

        if ((wd = notifierTemp->wd))
            objectRelease(&wd);

        if ((watchDir = notifierTemp->watchDir))
            objectRelease(&watchDir);

        /* Release notifier */
        objectRelease(notifier);
    }
}

void*
runNotifiersThread(void *arg)
{
    int rv, length, i, input, maxFd;
    int *fd, *wd, *fdPipe;
    pthread_mutex_t *mutex = NULL;
    char buffer[EVENT_BUF_LEN] = {0};
    fd_set fds;
    bool *isWorking;
    Notifier *notifier = NULL;
    const char *watchDir = NULL;

    assert(arg);
    notifier = (Notifier *)arg;
    /* Get notifier data */
    mutex = notifier->mutex;
    fd = notifier->fd;
    wd = notifier->wd;
    watchDir = notifier->watchDir;
    isWorking = &NOTIFIER_WORKING;
    rv = i = length = 0;
    maxFd = -1;

    /* Open pipe */
    if ((rv = pipe(notifier->fds)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "runNotifiersThread", errno,
                      strerror(errno), "Unable to run pipe for the notifier (%s)", watchDir);
        goto out;
    }
    /* Lock mutex */
    if ((rv = pthread_mutex_lock(mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "runNotifiersThread",
                      rv, strerror(rv), "Unable to acquire the notifier mutex lock (%s)", watchDir);
        goto out;
    }

    /* Creating the inotify instance */
    if ((*fd = inotify_init()) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "runNotifiersThread",
                      errno, strerror(errno), "Inotify_init returned -1");
        goto out;
    }

    /* Adding the "watchDir" directory into watch list. */
    if ((*wd = inotify_add_watch(*fd, watchDir, IN_MODIFY)) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "runNotifierThread",
                      errno, strerror(errno), "Inotify_add_watch returned -1");
        goto out;
    }

    fdPipe = &notifier->fds[0];
    maxFd = getMaxFileDesc(fd, fdPipe);
    while (1) {
        FD_ZERO(&fds);
        FD_SET(*fdPipe, &fds);
        FD_SET(*fd, &fds);

        /* Wait for data */
        select(maxFd, &fds, NULL, NULL, NULL);
        if (FD_ISSET(*fdPipe, &fds)) {
            if ((length = read(*fdPipe, &input, sizeof(int))) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "runNotifiersThread",
                              errno, strerror(errno), "Unable to read from pipe for the notifier (%s)!", watchDir);
                goto out;
            }
            if (input == THREAD_EXIT)
                goto out;
        }
        else if (FD_ISSET(*fd, &fds)) {
            if ((length = read(*fd, buffer, EVENT_BUF_LEN)) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "runNotifiersThread",
                              errno, strerror(errno), "Unable to read from inotify fd for the notifier (%s)!", watchDir);
                goto out;
            }

            i = 0;
            while (i < length) {
                struct inotify_event *event = (struct inotify_event *)&buffer[i];
                if (event->len) {
                    if (event->mask & IN_MODIFY) {
                        char *unitName = getUnitName(event->name);
                        if (unitName) {
                            Unit *unit = getUnitByName(UNITD_DATA->units, unitName);
                            if (unit) {
                                /* If another thread is working then we wait it (very extreme case) */
                                while (*isWorking)
                                    msleep(200);

                                *isWorking = true;
                                unit->isChanged = true;
                                if (UNITD_DEBUG)
                                    syslog(LOG_DAEMON | LOG_DEBUG, "Unit '%s' is changed!!\n", unitName);
                                *isWorking = false;
                            }
                        }
                        objectRelease(&unitName);
                    }
                    i += EVENT_SIZE + event->len;
                }
            }
        }
    }

    out:
        if (*fd != -1) {
            if (*wd != -1) {
                /* Removing the "watchDir" directory from watch list. */
                if ((rv = inotify_rm_watch(*fd, *wd)) == -1) {
                    logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "runNotifiersThread",
                                  errno, strerror(errno), "Inotify_rm_watch returned -1 (%s)", watchDir);
                }
            }
            /* Close the inotify instance */
            if ((rv = close(*fd)) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "runNotifiersThread",
                              errno, strerror(errno), "Close returned -1 (%s)", watchDir);
            }
        }

        /* Unlock mutex */
        if ((rv = pthread_mutex_unlock(mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "runNotifiersThread",
                          rv, strerror(rv), "Unable to unlock the notifier mutex (%s)", watchDir);
        }
        pthread_exit(0);
}

void*
startNotifiersThread(void *arg)
{
    pthread_t thread;
    pthread_attr_t attr;
    int rv = 0;
    Notifier *notifier = NULL;
    const char *watchDir = NULL;

    assert(arg);
    /* Get notifier data */
    notifier = (Notifier *)arg;
    watchDir = notifier->watchDir;

    /* Set pthread attributes and start */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if ((rv = pthread_create(&thread, &attr, runNotifiersThread, notifier)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifierThread", errno,
                      strerror(errno), "Unable to create the runNotifiersThread thread (detached) (%s)", watchDir);
    }
    else {
        if (UNITD_DEBUG)
            logInfo(UNITD_BOOT_LOG, "Run notifiers thread (detached) created successfully (%s)\n", watchDir);
    }
    return NULL;
}

void
setNotifiers()
{
    assert(!NOTIFIERS);
    NOTIFIERS = arrayNew(notifierRelease);
    if (!USER_INSTANCE) {
        Notifier *notifier = notifierNew();
        notifier->watchDir = stringNew(UNITS_PATH);
        arrayAdd(NOTIFIERS, notifier);
    }
    else {
        /* For user instance we have to monitor two dirs */
        for (int i = 0; i < 2; i++) {
            Notifier *notifier = notifierNew();
            if (i == 0)
                notifier->watchDir = stringNew(UNITS_USER_PATH);
            else if (i == 1)
                notifier->watchDir = stringNew(UNITS_USER_LOCAL_PATH);
            arrayAdd(NOTIFIERS, notifier);
        }
    }
}

void
startNotifiers()
{
    int rv = 0, len;
    Notifier *notifier = NULL;
    const char *watchDir = NULL;

    setNotifiers();
    assert(NOTIFIERS);
    len = NOTIFIERS->size;
    pthread_t threads[len];
    for (int i = 0; i < len; i++) {
        notifier = arrayGet(NOTIFIERS, i);
        watchDir = notifier->watchDir;
        if ((rv = pthread_create(&threads[i], NULL, startNotifiersThread, notifier)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiers", errno,
                          strerror(errno), "Unable to create the thread for the notifier (%s)",
                          watchDir);
        }
        else {
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Thread created successfully for the notifier (%s)\n",
                             watchDir);
        }
    }
    /* Waiting for the thread to terminate */
    for (int i = 0; i < len; i++) {
        notifier = arrayGet(NOTIFIERS, i);
        watchDir = notifier->watchDir;
        if ((rv = pthread_join(threads[i], NULL)) != EXIT_SUCCESS) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiers", rv,
                          strerror(rv), "Unable to join the thread for the notifier (%s)", watchDir);
        }
        else {
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Thread joined successfully for the notifier (%s)\n", watchDir);
        }
    }
}

void*
stopNotifiersThread(void *arg)
{
    int rv = 0, output = THREAD_EXIT;
    Notifier *notifier = NULL;
    pthread_mutex_t *mutex = NULL;
    const char *watchDir = NULL;

    assert(arg);
    notifier = (Notifier *)arg;
    mutex = notifier->mutex;
    watchDir = notifier->watchDir;

    if ((rv = write(notifier->fds[1], &output, sizeof(int))) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifiersThread",
                      errno, strerror(errno), "Unable to write into fd for the notifier (%s)", watchDir);
        goto out;
    }

    /* Lock mutex */
    if ((rv = pthread_mutex_lock(mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifiersThread",
                      rv, strerror(rv), "Unable to acquire the notifier mutex lock (%s)", watchDir);
        goto out;
    }

    /* Unlock mutex */
    if ((rv = pthread_mutex_unlock(mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifiersThread",
                      rv, strerror(rv), "Unable to unlock the notifier mutex (%s)", watchDir);
    }

    out:
        return NULL;
}

void
stopNotifiers()
{
    int rv = 0, len;
    Notifier *notifier = NULL;
    const char *watchDir = NULL;

    if (NOTIFIERS) {
        len = NOTIFIERS->size;
        pthread_t threads[len];
        for (int i = 0; i < len; i++) {
            notifier = arrayGet(NOTIFIERS, i);
            watchDir = notifier->watchDir;
            if ((rv = pthread_create(&threads[i], NULL, stopNotifiersThread, notifier)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifiers", errno,
                              strerror(errno), "Unable to create the thread for the notifier (stop) (%s)",
                              watchDir);
            }
            else {
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Thread created successfully for the notifier (stop) (%s)\n",
                                 watchDir);
            }
        }
        /* Waiting for the thread to terminate */
        for (int i = 0; i < len; i++) {
            notifier = arrayGet(NOTIFIERS, i);
            watchDir = notifier->watchDir;
            if ((rv = pthread_join(threads[i], NULL)) != EXIT_SUCCESS) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifiers", rv,
                              strerror(rv), "Unable to join the thread for the notifier (stop) (%s)",
                              watchDir);
            }
            else {
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Thread joined successfully for the notifier (stop) (%s)\n",
                                 watchDir);
            }
        }

        /* Release notifiers */
        arrayRelease(&NOTIFIERS);
    }
}
