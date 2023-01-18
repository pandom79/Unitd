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
const WatcherData WATCHER_DATA_ITEMS[] = {
    { UNITD_WATCHER, IN_MODIFY },
};

static void
checkUnitChanging(const char *eventName)
{
    char *unitName = getUnitName(eventName);
    if (unitName) {
        int rv = 0;
        Unit *unit = getUnitByName(UNITD_DATA->units, unitName);
        if (unit) {
            if ((rv = pthread_mutex_lock(&NOTIFIER_MUTEX)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "checkUnitChanging",
                         rv, strerror(rv), "Unable to lock the notifier mutex!");
                kill(UNITD_PID, SIGTERM);
            }

            unit->isChanged = true;
            if (UNITD_DEBUG)
                syslog(LOG_DAEMON | LOG_DEBUG, "Unit '%s' is changed!!\n", unitName);

            if ((rv = pthread_mutex_unlock(&NOTIFIER_MUTEX)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "checkUnitChanging",
                         rv, strerror(rv), "Unable to unlock the notifier mutex!");
                kill(UNITD_PID, SIGTERM);
            }
        }
        objectRelease(&unitName);
    }
}

static WatcherType
getWatcherTypeByWd(Array *watchers, int eventWd)
{
    Watcher *watcher = NULL;
    int len = watchers ? watchers->size : 0;
    for (int i = 0; i < len; i++) {
        watcher = arrayGet(watchers, i);
        if (*watcher->wd == eventWd)
            return watcher->watcherData.watcherType;
    }
    return -1;
}

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
watcherNew(Notifier *notifier, const char *path, WatcherType watcherType)
{
    Watcher *watcher = NULL;

    assert(notifier && *notifier->fd != -1);
    assert(path && strlen(path) > 0);
    assert(watcherType >= 0);

    watcher = calloc(1, sizeof(Watcher));
    assert(watcher);

    /* Inotify instance fd */
    watcher->fd = notifier->fd;

    /* Path */
    watcher->path = stringNew(path);

    /* Watcher data */
    watcher->watcherData = WATCHER_DATA_ITEMS[watcherType];

    /* Watcher fd */
    int *wd = calloc(1, sizeof(int));
    assert(wd);
    /* Adding the path to the watch list. */
    if ((*wd = inotify_add_watch(*watcher->fd, path, watcher->watcherData.mask)) == -1) {
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
        objectRelease(watcher);
        /* Fd will be freed by notifierRelease func. */
    }
}

void*
startNotifierThread(void *arg)
{
    int rv, length, i, input, maxFd, allEvents;
    int *fd, *fdPipe;
    char buffer[EVENT_BUF_LEN] = {0};
    fd_set fds;
    Pipe *pipe = NULL;
    Array *watchers = NULL;
    Notifier *notifier = NULL;

    rv = i = length = allEvents = 0;
    maxFd = -1;
    notifier = (Notifier *)arg;
    assert(notifier);

    /* Get notifier data */
    pipe = notifier->pipe;
    fd = notifier->fd;
    watchers = notifier->watchers;
    assert(watchers);

    /* Lock pipe mutex */
    if ((rv = pthread_mutex_lock(pipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifierThread",
                 rv, strerror(rv), "Unable to acquire the pipe mutex lock");
        kill(UNITD_PID, SIGTERM);
    }

    fdPipe = &pipe->fds[0];
    maxFd = getMaxFileDesc(fd, fdPipe);

    /* Building all events */
    assert((length = watchers->size) > 0);
    for (i = 0; i < length; i++)
        allEvents |= ((Watcher *)arrayGet(watchers, i))->watcherData.mask;
    assert(allEvents > 0);

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
                /* Evaluating all events defined by watchers. */
                if (event->len && (event->mask & allEvents)) {
                    WatcherType watcherType = getWatcherTypeByWd(watchers, event->wd);
                    assert(watcherType >= 0);
                    switch (watcherType) {
                        case UNITD_WATCHER:
                            checkUnitChanging(event->name);
                            break;
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
        arrayAdd(watchers, watcherNew(NOTIFIER, UNITS_PATH, UNITD_WATCHER));
    else {
        /* For user instance we have to monitor two dirs */
        arrayAdd(watchers, watcherNew(NOTIFIER, UNITS_USER_PATH, UNITD_WATCHER));
        arrayAdd(watchers, watcherNew(NOTIFIER, UNITS_USER_LOCAL_PATH, UNITD_WATCHER));
    }
}

void
startNotifier(Notifier *notifier)
{
    int rv = 0;
    pthread_t thread;
    pthread_attr_t attr;

    if (!notifier) {
        setNotifier();
        notifier = NOTIFIER;
    }
    assert(notifier);

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
    if ((rv = pthread_create(&thread, &attr, startNotifierThread, notifier)) != 0) {
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
stopNotifier(Notifier *notifier)
{
    if (!notifier)
        notifier = NOTIFIER;

    if (notifier) {
        Pipe *pipe = notifier->pipe;
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
    }
}
