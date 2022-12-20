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

    /* Pipe */
    notifier->pipe = pipeNew();

    return notifier;
}

void
notifierRelease(Notifier **notifier)
{
    Notifier *notifierTemp = *notifier;
    pthread_mutex_t *mutex = NULL;
    int rv = 0;

    if (notifierTemp) {
        /* Destroy and free the mutex */
        if ((mutex = notifierTemp->mutex)) {
            if ((rv = pthread_mutex_destroy(mutex)) != 0) {
                logError(CONSOLE, "src/core/handlers/notifier.c", "notifierRelease", rv,
                              strerror(rv), "Unable to run pthread_mutex_destroy");
            }
            objectRelease(&mutex);
        }

        objectRelease(&notifierTemp->fd);
        objectRelease(&notifierTemp->wd);
        objectRelease(&notifierTemp->watchDir);
        pipeRelease(&notifierTemp->pipe);

        /* Release notifier */
        objectRelease(notifier);
    }
}

void*
startNotifiersThread(void *arg)
{
    int rv, length, i, input, maxFd;
    int *fd, *wd, *fdPipe;
    char buffer[EVENT_BUF_LEN] = {0};
    fd_set fds;
    bool *isWorking;
    Notifier *notifier = NULL;
    const char *watchDir = NULL;
    Pipe *pipe = NULL;

    assert(arg);
    notifier = (Notifier *)arg;

    /* Get notifier data */
    pipe = notifier->pipe;
    fd = notifier->fd;
    wd = notifier->wd;
    watchDir = notifier->watchDir;
    isWorking = &NOTIFIER_WORKING;
    rv = i = length = 0;
    maxFd = -1;

    /* Lock pipe mutex */
    if ((rv = pthread_mutex_lock(pipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiersThread",
                      rv, strerror(rv), "Unable to acquire the pipe mutex lock (%s)", watchDir);
        kill(UNITD_PID, SIGTERM);
    }

    /* Creating the inotify instance */
    if ((*fd = inotify_init()) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiersThread",
                      errno, strerror(errno), "Inotify_init returned -1");
        kill(UNITD_PID, SIGTERM);
    }

    /* Adding the "watchDir" directory into watch list. */
    if ((*wd = inotify_add_watch(*fd, watchDir, IN_MODIFY)) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiersThread",
                      errno, strerror(errno), "Inotify_add_watch returned -1");
        kill(UNITD_PID, SIGTERM);
    }

    fdPipe = &pipe->fds[0];
    maxFd = getMaxFileDesc(fd, fdPipe);

    /* Unlock mutex just before main loop. */
    if ((rv = pthread_mutex_unlock(notifier->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiersThread",
                      rv, strerror(rv), "Unable to unlock the mutex (%s)", watchDir);
        kill(UNITD_PID, SIGTERM);
    }

    while (1) {
        FD_ZERO(&fds);
        FD_SET(*fdPipe, &fds);
        FD_SET(*fd, &fds);

        /* Wait for data */
        if (select(maxFd, &fds, NULL, NULL, NULL) == -1 && errno == EINTR)
            continue;
        if (FD_ISSET(*fdPipe, &fds)) {
            if ((length = uRead(*fdPipe, &input, sizeof(int))) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiersThread",
                              errno, strerror(errno), "Unable to read from pipe for the notifier (%s)!", watchDir);
                kill(UNITD_PID, SIGTERM);
            }
            if (input == THREAD_EXIT)
                goto out;
        }
        else if (FD_ISSET(*fd, &fds)) {
            if ((length = uRead(*fd, buffer, EVENT_BUF_LEN)) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiersThread",
                              errno, strerror(errno), "Unable to read from inotify fd for the notifier (%s)!", watchDir);
                kill(UNITD_PID, SIGTERM);
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
                    logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiersThread",
                                  errno, strerror(errno), "Inotify_rm_watch returned -1 (%s)", watchDir);
                }
            }
            /* Close the inotify instance */
            if ((rv = close(*fd)) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiersThread",
                              errno, strerror(errno), "Close returned -1 (%s)", watchDir);
            }
        }

        /* Unlock pipe mutex */
        if ((rv = pthread_mutex_unlock(pipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiersThread",
                          rv, strerror(rv), "Unable to unlock the pipe mutex (%s)", watchDir);
        }
        pthread_exit(0);
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
    pthread_attr_t attr[len];

    for (int i = 0; i < len; i++) {
        notifier = arrayGet(NOTIFIERS, i);
        watchDir = notifier->watchDir;

        /* Set pthread attributes and start */
        if ((rv = pthread_attr_init(&attr[i])) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiers", errno,
                  strerror(errno), "pthread_attr_init returned bad exit code %d for the notifier (%s)",
                      rv, watchDir);
            kill(UNITD_PID, SIGTERM);
        }
        if ((rv = pthread_attr_setdetachstate(&attr[i], PTHREAD_CREATE_DETACHED)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiers", errno,
                  strerror(errno), "pthread_attr_setdetachstate returned bad exit code %d for the notifier (%s)",
                      rv, watchDir);
            kill(UNITD_PID, SIGTERM);
        }

        /* It will be unlocked in startNotifiersThread func. */
        handleMutex(notifier->mutex, true);

        if ((rv = pthread_create(&threads[i], &attr[i], startNotifiersThread, notifier)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifiers", rv,
                          strerror(rv), "Unable to create deatched thread for the notifier (%s)",
                          watchDir);
            kill(UNITD_PID, SIGTERM);
        }
        else {
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Thread created successfully for the notifier (%s)\n",
                             watchDir);
        }
        /* We assure that all the notifers pipes are listening. */
        handleMutex(notifier->mutex, true);
        handleMutex(notifier->mutex, false);
        pthread_attr_destroy(&attr[i]);
    }
}

void*
stopNotifiersThread(void *arg)
{
    int rv = 0, output = THREAD_EXIT;
    Notifier *notifier = NULL;
    const char *watchDir = NULL;
    Pipe *pipe = NULL;

    assert(arg);
    notifier = (Notifier *)arg;
    watchDir = notifier->watchDir;
    pipe = notifier->pipe;

    if ((rv = uWrite(pipe->fds[1], &output, sizeof(int))) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifiersThread",
                      errno, strerror(errno), "Unable to write into pipe for the notifier (%s)", watchDir);
    }
    /* Lock pipe mutex */
    if ((rv = pthread_mutex_lock(pipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifiersThread",
                      rv, strerror(rv), "Unable to acquire the pipe mutex lock (%s)", watchDir);
    }
    /* Unlock pipe mutex */
    if ((rv = pthread_mutex_unlock(pipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifiersThread",
                      rv, strerror(rv), "Unable to unlock the pipe mutex (%s)", watchDir);
    }
    pthread_exit(0);
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
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifiers", rv,
                              strerror(rv), "Unable to create the thread for the notifier (stop) (%s)",
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
            if ((rv = pthread_join(threads[i], NULL)) != 0) {
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
