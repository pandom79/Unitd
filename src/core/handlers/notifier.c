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
Notifier *NOTIFIER;
bool NOTIFIER_WORKING;

Notifier*
notifierNew()
{
    Notifier *notifier = NULL;
    int *fd = NULL;

    /* Notifier */
    notifier = calloc(1, sizeof(Notifier));
    assert(notifier);

    /* Fd */
    fd = calloc(1, sizeof(int));
    assert(fd);
    /* Creating inotify instance */
    if ((*fd = inotify_init()) == -1) {
        logError(CONSOLE, "src/core/handlers/notifier.c", "notifierNew",
                 errno, strerror(errno), "Inotify_init returned -1");
    }
    assert(*fd != -1);
    notifier->fd = fd;

    /* Pipe */
    notifier->pipe = pipeNew();

    /* Watchers */
    notifier->watchers = arrayNew(watcherRelease);

    return notifier;
}

void
notifierRelease(Notifier **notifier)
{
    Notifier *notifierTemp = *notifier;
    int rv = 0;
    if (notifierTemp) {
        /* First, we release the watchers because we need the fd. */
        arrayRelease(&notifierTemp->watchers);

        /* Close and release the inotify instance */
        if ((rv = close(*notifierTemp->fd)) == -1) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "notifierRelease",
                     errno, strerror(errno), "Close returned -1");
        }
        objectRelease(&notifierTemp->fd);

        /* Pipe */
        pipeRelease(&notifierTemp->pipe);

        /* Release notifier */
        objectRelease(notifier);
    }
}

Watcher*
watcherNew(Notifier *notifier, const char *path, int mask)
{
    Watcher *watcher = NULL;

    assert(notifier && *notifier->fd != -1);
    assert(path && strlen(path) > 0);
    assert(mask > 0);

    watcher = calloc(1, sizeof(Watcher));
    assert(watcher);

    /* Inotify instance fd */
    watcher->fd = notifier->fd;

    /* Path */
    watcher->path = stringNew(path);

    /* Mask */
    int *watcherMask = calloc(1, sizeof(int));
    assert(watcherMask);
    *watcherMask = mask;
    watcher->mask = watcherMask;

    /* Watcher fd */
    int *wd = calloc(1, sizeof(int));
    assert(wd);
    /* Adding the path to the watch list. */
    if ((*wd = inotify_add_watch(*watcher->fd, path, mask)) == -1) {
        logError(CONSOLE, "src/core/handlers/notifier.c", "watcherNew",
                 errno, strerror(errno), "Inotify_add_watch returned -1");
    }
    assert(*wd != -1);
    watcher->wd = wd;

    return watcher;
}

void
watcherRelease(Watcher **watcher)
{
    if (*watcher) {
        int rv = 0;
        int *fd = (*watcher)->fd;
        int *wd = (*watcher)->wd;
        char *path = (*watcher)->path;
        if (*fd != -1 && *wd != -1) {
            /* Removing the path from watch list. */
            if ((rv = inotify_rm_watch(*fd, *wd)) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "watcherRelease",
                         errno, strerror(errno), "Inotify_rm_watch returned -1 (%s)", path);
            }
        }
        objectRelease(&wd);
        objectRelease(&path);
        objectRelease(&(*watcher)->mask);
        objectRelease(watcher);
        /* Fd will be freed by notifierRelease func. */
    }
}

void*
startNotifierThread(void *arg UNUSED)
{
    int rv, length, i, input, maxFd;
    int *fd, *fdPipe;
    char buffer[EVENT_BUF_LEN] = {0};
    fd_set fds;
    bool *isWorking;
    Pipe *pipe = NULL;

    assert(NOTIFIER);

    /* Get notifier data */
    pipe = NOTIFIER->pipe;
    fd = NOTIFIER->fd;
    isWorking = &NOTIFIER_WORKING;
    rv = i = length = 0;
    maxFd = -1;

    /* Lock pipe mutex */
    if ((rv = pthread_mutex_lock(pipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifierThread",
                 rv, strerror(rv), "Unable to acquire the pipe mutex lock");
        kill(UNITD_PID, SIGTERM);
    }

    fdPipe = &pipe->fds[0];
    maxFd = getMaxFileDesc(fd, fdPipe);

    while (1) {
        FD_ZERO(&fds);
        FD_SET(*fdPipe, &fds);
        FD_SET(*fd, &fds);

        /* Wait for data */
        if (select(maxFd, &fds, NULL, NULL, NULL) == -1 && errno == EINTR)
            continue;
        if (FD_ISSET(*fdPipe, &fds)) {
            if ((length = uRead(*fdPipe, &input, sizeof(int))) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifierThread",
                         errno, strerror(errno), "Unable to read from pipe for the notifier!");
                kill(UNITD_PID, SIGTERM);
            }
            if (input == THREAD_EXIT)
                goto out;
        }
        else if (FD_ISSET(*fd, &fds)) {
            if ((length = uRead(*fd, buffer, EVENT_BUF_LEN)) == -1) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifierThread",
                         errno, strerror(errno), "Unable to read from inotify fd for the notifier!");
                kill(UNITD_PID, SIGTERM);
            }

            i = 0;
            while (i < length) {
                struct inotify_event *event = (struct inotify_event *)&buffer[i];
                /* In this case we only evaluate 'IN_MODIFY' event */
                if (event->len && (event->mask & IN_MODIFY)) {
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
                        objectRelease(&unitName);
                    }
                    i += EVENT_SIZE + event->len;
                }
            }
        }
    }

    out:
        /* Unlock pipe mutex */
        if ((rv = pthread_mutex_unlock(pipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifierThread",
                     rv, strerror(rv), "Unable to unlock the pipe mutex");
        }
        if (UNITD_DEBUG)
            logInfo(CONSOLE | SYSTEM, "Notifier thread exited successfully\n");
        pthread_exit(0);
}

void
setNotifier()
{
    assert(!NOTIFIER);
    NOTIFIER = notifierNew();
    Array *watchers = NOTIFIER->watchers;

    if (!USER_INSTANCE)
        arrayAdd(watchers, watcherNew(NOTIFIER, UNITS_PATH, IN_MODIFY));
    else {
        /* For user instance we have to monitor two dirs */
        arrayAdd(watchers, watcherNew(NOTIFIER, UNITS_USER_PATH, IN_MODIFY));
        arrayAdd(watchers, watcherNew(NOTIFIER, UNITS_USER_LOCAL_PATH, IN_MODIFY));
    }
}

void
startNotifier()
{
    int rv = 0;
    setNotifier();
    assert(NOTIFIER);
    pthread_t thread;
    pthread_attr_t attr;

    if ((rv = pthread_attr_init(&attr)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifier", errno,
                 strerror(errno), "pthread_attr_init returned %d exit code", rv);
        kill(UNITD_PID, SIGTERM);
    }
    if ((rv = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifier", errno,
                 strerror(errno), "pthread_attr_setdetachstate returned %d exit code", rv);
        kill(UNITD_PID, SIGTERM);
    }
    if ((rv = pthread_create(&thread, &attr, startNotifierThread, NULL)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifier", rv,
                 strerror(rv), "Unable to create detached thread");
        kill(UNITD_PID, SIGTERM);
    }
    else {
        if (UNITD_DEBUG)
            logInfo(CONSOLE | SYSTEM, "Thread created successfully for the notifier\n");
    }
    pthread_attr_destroy(&attr);
}

void
stopNotifier()
{
    if (NOTIFIER) {
        Pipe *pipe = NOTIFIER->pipe;
        int rv = 0, output = THREAD_EXIT;

        if ((rv = uWrite(pipe->fds[1], &output, sizeof(int))) == -1) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifier",
                     errno, strerror(errno), "Unable to write into pipe for the notifier");
        }
        /* Lock pipe mutex */
        if ((rv = pthread_mutex_lock(pipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifier",
                     rv, strerror(rv), "Unable to acquire the pipe mutex lock");
        }
        /* Unlock pipe mutex */
        if ((rv = pthread_mutex_unlock(pipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "stopNotifier",
                     rv, strerror(rv), "Unable to unlock the pipe mutex");
        }
        notifierRelease(&NOTIFIER);
    }
}
