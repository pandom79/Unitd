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
    { PATH_EXISTS_WATCHER, IN_DELETE_SELF | IN_MOVE_SELF | IN_ATTRIB | IN_CREATE },
    { PATH_EXISTS_GLOB_WATCHER, IN_DELETE_SELF | IN_MOVE_SELF | IN_ATTRIB | IN_CREATE },
    { PATH_RESOURCE_CHANGED_WATCHER, IN_DELETE_SELF | IN_MOVE_SELF | IN_DELETE | IN_CREATE |
                                     IN_MOVED_FROM | IN_MOVED_TO },
    { PATH_DIRECTORY_NOT_EMPTY_WATCHER, IN_DELETE_SELF | IN_MOVE_SELF | IN_DELETE | IN_CREATE |
                                        IN_MOVED_FROM | IN_MOVED_TO }
};

int
notifierInit(Notifier *notifier)
{
    Array *watchers = NULL;
    Watcher *watcher = NULL;
    int len = 0, rv = 0;

    assert(notifier);
    watchers = notifier->watchers;
    len = watchers ? watchers->size : 0;
    if ((*notifier->fd = inotify_init()) == -1) {
        rv = 1;
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "notifierInit",
                 errno, strerror(errno), "Inotify_init func returned -1");
    }
    for (int i = 0; i < len; i++) {
        watcher = arrayGet(watchers, i);
        if ((*watcher->wd = inotify_add_watch(*notifier->fd, watcher->path, watcher->watcherData.mask)) == -1) {
            rv = 1;
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "notifierInit",
                     errno, strerror(errno), "Inotify_add_watch returned -1 for '%s' path", watcher->path);
            break;
        }
    }

    return rv;
}

void
notifierClose(Notifier *notifier)
{
    Array *watchers = NULL;
    Watcher *watcher = NULL;
    int len, fd, rv;

    assert(notifier);
    rv = fd = -1;
    watchers = notifier->watchers;
    fd = *notifier->fd;
    len = watchers ? watchers->size : 0;
    for (int i = 0; i < len; i++) {
        watcher = arrayGet(watchers, i);
        if ((rv = inotify_rm_watch(fd, *watcher->wd)) == -1 && errno != EINVAL) {
            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "notifierClose",
                     errno, strerror(errno), "Inotify_rm_watch func returned -1 for %s path",
                     watcher->path);
        }
    }
    close(fd);
}

static bool
isEmptyFolder(const char *pathFolder)
{
    bool ret = true;

    if (pathFolder) {
        int rv = 0, len = 0;
        glob_t results;
        const char *entry = NULL;
        char *pattern = NULL;

        /* Building pattern end execute glob */
        pattern = stringNew(pathFolder);
        stringAppendStr(&pattern, "/*");
        if ((rv = glob(pattern, 0, NULL, &results)) == 0) {
            len = results.gl_pathc;
            for (int i = 0; i < len; i++) {
                entry = results.gl_pathv[i];
                /* The files "." and ".." are already discarded by glob.
                 * We only discard backup and swap files.
                 */
                if (!stringEndsWithChr(entry, '~') &&
                    !stringEndsWithStr(entry, ".swp") &&
                    !stringEndsWithStr(entry, ".swpx")) {
                    ret = false;
                    break;
                }
            }
        }
        globfree(&results);
        objectRelease(&pattern);
    }

    return ret;
}

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

static bool
checkUnitExecution(Unit *unit, WatcherType watcherType, const char *eventName)
{
    char *completeEventName = NULL;
    bool execUnit = false;

    assert(unit);
    assert(watcherType >= 0);
    assert(eventName);

    switch (watcherType) {
        case PATH_EXISTS_WATCHER:
            if (access(unit->pathExists, F_OK) == 0)
                execUnit = true;
            break;
        case PATH_EXISTS_GLOB_WATCHER:
            completeEventName = stringNew(unit->pathExistsGlobMonitor);
            stringAppendStr(&completeEventName, eventName);
            if (fnmatch(unit->pathExistsGlob, completeEventName, 0) == 0)
                execUnit = true;
            break;
        case PATH_RESOURCE_CHANGED_WATCHER:
            completeEventName = stringNew(unit->pathResourceChangedMonitor);
            stringAppendStr(&completeEventName, eventName);
            if (stringEquals(completeEventName, unit->pathResourceChanged))
                execUnit = true;
            break;
        case PATH_DIRECTORY_NOT_EMPTY_WATCHER:
            execUnit = !isEmptyFolder(unit->pathDirectoryNotEmptyMonitor);
            break;
        default:
            break;
    }
    if (execUnit) {
        *unit->processData->pStateData = PSTATE_DATA_ITEMS[RESTARTING];
        executeUnit(unit, UPATH);
        *unit->processData->pStateData = PSTATE_DATA_ITEMS[RUNNING];
    }

    objectRelease(&completeEventName);
    return execUnit;
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
    *fd = -1;
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
    if (notifierTemp) {
        notifierClose(notifierTemp);
        arrayRelease(&notifierTemp->watchers);
        objectRelease(&notifierTemp->fd);
        pipeRelease(&notifierTemp->pipe);
        objectRelease(notifier);
    }
}

Watcher*
watcherNew(Notifier *notifier, const char *path, WatcherType watcherType)
{
    Watcher *watcher = NULL;

    assert(notifier);
    assert(path && strlen(path) > 0);
    assert(watcherType >= 0);

    watcher = calloc(1, sizeof(Watcher));
    assert(watcher);

    /* Path */
    watcher->path = stringNew(path);

    /* Watcher data */
    watcher->watcherData = WATCHER_DATA_ITEMS[watcherType];

    /* Watcher fd */
    int *wd = calloc(1, sizeof(int));
    assert(wd);
    *wd = -1;
    watcher->wd = wd;

    return watcher;
}

void
watcherRelease(Watcher **watcher)
{
    if (*watcher) {
        objectRelease(&(*watcher)->wd);
        objectRelease(&(*watcher)->path);
        objectRelease(watcher);
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
    Unit *unit = NULL;
    bool executedUnit = false;

    rv = i = length = allEvents = 0;
    maxFd = -1;
    unit = (Unit *)arg;
    notifier = unit ? unit->notifier : NOTIFIER;
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

    /* Before to start, we wait for system is up.
     * We check if ctrl+alt+del is pressed as well.
    */
    while (!LISTEN_SOCK_REQUEST && SHUTDOWN_COMMAND == NO_COMMAND)
        msleep(50);
    if (SHUTDOWN_COMMAND != NO_COMMAND)
        goto out;

    fdPipe = &pipe->fds[0];
    maxFd = getMaxFileDesc(fd, fdPipe);

    /* At least one watcher must be here. */
    if ((length = watchers->size) == 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifierThread",
                 rv, strerror(rv), "No watcher found!");
        kill(UNITD_PID, SIGTERM);
        goto out;
    }
    /* Building and checking the events */
    for (i = 0; i < length; i++)
        allEvents |= ((Watcher *)arrayGet(watchers, i))->watcherData.mask;
    if (allEvents == 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifierThread",
                 rv, strerror(rv), "No event found!");
        kill(UNITD_PID, SIGTERM);
        goto out;
    }

    while (1) {
        FD_ZERO(&fds);
        FD_SET(*fdPipe, &fds);
        FD_SET(*fd, &fds);
        executedUnit = false;
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

            /* Evaluating all events defined by watchers. */
            i = 0;
            while (i < length) {
                /* If the unit has been executed then break and restart. */
                if (executedUnit)
                    break;
                struct inotify_event *event = (struct inotify_event *)&buffer[i];
                if (UNITD_DEBUG)
                    logInfo(SYSTEM, "Event: name = %s, length = %d, wd = %d, mask = %d",
                            event->name, event->len, event->wd, event->mask);
                if (event->len && (event->mask & allEvents)) {
                    WatcherType watcherType = getWatcherTypeByWd(watchers, event->wd);
                    switch (watcherType) {
                        case UNITD_WATCHER:
                            checkUnitChanging(event->name);
                            break;
                        case PATH_EXISTS_WATCHER:
                        case PATH_EXISTS_GLOB_WATCHER:
                        case PATH_RESOURCE_CHANGED_WATCHER:
                        case PATH_DIRECTORY_NOT_EMPTY_WATCHER:
                            executedUnit = checkUnitExecution(unit, watcherType, event->name);
                            break;
                        default:
                            logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifierThread",
                                     EPERM, strerror(EPERM), "No watcher type (%d) found!", watcherType);
                            kill(UNITD_PID, SIGTERM);
                            break;
                    }
                }
                i += EVENT_SIZE + event->len;
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
setUnitdNotifier()
{
    int rv = 0;
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
    if ((rv = notifierInit(NOTIFIER)) != 0)
        kill(UNITD_PID, SIGTERM);
}

int
startNotifier(Unit *unit)
{
    int rv = 0;
    pthread_t thread;
    pthread_attr_t attr;
    Notifier *notifier = NULL;

    if (!unit) {
        setUnitdNotifier();
        notifier = NOTIFIER;
    }
    else
        notifier = unit->notifier;
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
    if ((rv = pthread_create(&thread, &attr, startNotifierThread, unit)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/handlers/notifier.c", "startNotifier", rv,
                 strerror(rv), "Unable to create detached thread");
        kill(UNITD_PID, SIGTERM);
    }
    else {
        if (UNITD_DEBUG)
            logInfo(CONSOLE | SYSTEM, "Thread created successfully for the notifier\n");
    }
    pthread_attr_destroy(&attr);
    if (unit && rv == 0)
        *unit->processData->pStateData = PSTATE_DATA_ITEMS[RUNNING];
    return rv;
}

int
stopNotifier(Unit *unit)
{
    int rv = 0, output = THREAD_EXIT;
    Notifier *notifier = !unit ? NOTIFIER : unit->notifier;

    if (notifier) {
        Pipe *pipe = notifier->pipe;
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
    return rv;
}
