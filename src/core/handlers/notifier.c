/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

#define EVENT_SIZE          (sizeof(struct inotify_event))
#define EVENT_BUF_LEN       (1024 * (EVENT_SIZE + 16))

Notifier *NOTIFIER;

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
        unitdLogError(LOG_UNITD_BOOT, "src/core/handlers/notifier.c", "notifierNew", rv, strerror(rv),
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
    int rv = 0;
    int *fd, *wd;
    fd = wd = NULL;

    if (notifierTemp) {
        /* Destroy and free the mutex */
        if ((mutex = notifierTemp->mutex)) {
            if ((rv = pthread_mutex_destroy(mutex)) != 0) {
                unitdLogError(LOG_UNITD_CONSOLE, "src/core/handlers/notifier.c", "notifierRelease", rv,
                              strerror(rv), "Unable to run pthread_mutex_destroy");
            }
            assert(rv == 0);
            objectRelease(&mutex);
        }

        if ((fd = notifierTemp->fd))
            objectRelease(&fd);

        if ((wd = notifierTemp->wd))
            objectRelease(&wd);

        /* Release notifier */
        objectRelease(notifier);
    }
}

void*
runNotifierThread()
{
    int rv, length, i;
    int *fd, *wd;
    pthread_mutex_t *mutex = NULL;
    char buffer[EVENT_BUF_LEN] = {0};

    assert(!NOTIFIER);
    NOTIFIER = notifierNew();
    mutex = NOTIFIER->mutex;
    fd = NOTIFIER->fd;
    wd = NOTIFIER->wd;
    rv = i = length = 0;

    /* Lock mutex */
    if ((rv = pthread_mutex_lock(mutex)) != 0) {
        unitdLogError(LOG_UNITD_ALL, "src/core/handlers/notifier.c", "runNotifierThread",
                      rv, strerror(rv), "Unable to acquire the notifier mutex lock");
        goto out;
    }

    /* Creating the inotify instance */
    if ((*fd = inotify_init()) == -1) {
        unitdLogError(LOG_UNITD_ALL, "src/core/handlers/notifier.c", "runNotifierThread",
                      errno, strerror(errno), "Inotify_init has returned -1");
        goto out;
    }

unitdLogWarning(LOG_UNITD_CONSOLE, "fd notifier = %d\n", *fd);

    /* Adding the “UNIT_UNITS_PATH” directory into watch list. */
    if ((*wd = inotify_add_watch(*fd, UNITS_PATH, IN_MODIFY | IN_CREATE)) == -1) {
        unitdLogError(LOG_UNITD_ALL, "src/core/handlers/notifier.c", "runNotifierThread",
                      errno, strerror(errno), "Inotify_add_watch has returned -1");
        goto out;
    }

    while (1) {
        if ((length = read(*fd, buffer, EVENT_BUF_LEN)) == -1) {
            unitdLogError(LOG_UNITD_ALL, "src/core/handlers/notifier.c", "runNotifierThread", errno,
                          strerror(errno), "Unable to read from fd for the notifier");
            goto out;
        }

        i = 0;
        while (i < length) {
unitdLogWarning(LOG_UNITD_CONSOLE, "length = %d...\n", length);

            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len) {
unitdLogWarning(LOG_UNITD_CONSOLE, "mask = %d\n", event->mask);
                if (event->mask & IN_CREATE) {
                    if (event->mask & IN_ISDIR)
                        printf( "Dir %s created.\n", event->name);
                    else
                        printf( "File %s created.\n", event->name);
                }
                else if (event->mask & IN_MODIFY) {
                    printf( "File %s modified.\n", event->name);
                }

                i += EVENT_SIZE + event->len;
            }
        }
    }


    out:
        if (*fd != -1) {
            if (*wd != -1) {
                /* Removing the “UNIT_UNITS_PATH” directory into watch list. */
                if ((rv = inotify_rm_watch(*fd, *wd)) == -1) {
                    unitdLogError(LOG_UNITD_ALL, "src/core/handlers/notifier.c", "runNotifierThread",
                                  errno, strerror(errno), "Inotify_rm_watch has returned -1");
                }
            }
            /* Close the inotify instance */
            if ((rv = close(*fd)) == -1) {
                unitdLogError(LOG_UNITD_ALL, "src/core/handlers/notifier.c", "runNotifierThread",
                              errno, strerror(errno), "Close has returned -1");
            }
        }

        /* Unlock mutex */
        if ((rv = pthread_mutex_unlock(mutex)) != 0) {
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/handlers/cleaner.c", "runCleanerThread",
                          rv, strerror(rv), "Unable to unlock the cleaner mutex");
        }
        pthread_exit(0);
}

void*
startNotifierThread(void *arg UNUSED)
{
    pthread_t thread;
    pthread_attr_t attr;
    int rv = 0;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if ((rv = pthread_create(&thread, &attr, runNotifierThread, NULL)) != 0) {
        unitdLogError(LOG_UNITD_BOOT, "src/core/handlers/notifier.c", "startNotifierThread", errno,
                      strerror(errno), "Unable to create the runNotifier thread (detached)");
    }
    else {
        if (UNITD_DEBUG)
            unitdLogInfo(LOG_UNITD_BOOT, "Run notifier thread (detached) created successfully");
    }
    return NULL;
}

void
startNotifier()
{
    pthread_t thread;
    int rv = 0;

    if ((rv = pthread_create(&thread, NULL, startNotifierThread, NULL)) != 0) {
        unitdLogError(LOG_UNITD_BOOT, "src/core/handlers/notifier.c", "startNotifier", errno,
                      strerror(errno), "Unable to create the thread for the notifier");
    }
    else {
        if (UNITD_DEBUG)
            unitdLogInfo(LOG_UNITD_BOOT, "Thread created successfully for the notifier\n");
    }
    /* Waiting for the thread to terminate */
    if ((rv = pthread_join(thread, NULL)) != EXIT_SUCCESS) {
        unitdLogError(LOG_UNITD_BOOT, "src/core/handlers/notifier.c", "startNotifier", rv,
                      strerror(rv), "Unable to join the thread for the notifier");
    }
    else {
        if (UNITD_DEBUG)
            unitdLogInfo(LOG_UNITD_BOOT, "Thread joined successfully for the notifier\n");
    }
}

void*
stopNotifierThread(void *arg UNUSED)
{
    int rv = 0;
    const char *notifierExit = NOTIFIER_EXIT;
    assert(NOTIFIER);
    pthread_mutex_t *mutex = NOTIFIER->mutex;

//    if ((rv = write(NOTIFIER->fds[1], &notifierExit, strlen(notifierExit))) == -1) {
//        unitdLogError(LOG_UNITD_CONSOLE, "src/core/handlers/notifier.c", "stopNotifierThread",
//                      errno, strerror(errno), "Unable to write into fd for the notifier");
//        goto out;
//    }

    /* Lock mutex */
    if ((rv = pthread_mutex_lock(mutex)) != 0) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/handlers/notifier.c", "stopNotifierThread",
                      rv, strerror(rv), "Unable to acquire the notifier mutex lock");
        goto out;
    }
    /* Unlock mutex */
    if ((rv = pthread_mutex_unlock(mutex)) != 0) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/handlers/notifier.c", "stopNotifierThread",
                      rv, strerror(rv), "Unable to unlock the notifier mutex");
    }

    out:
        notifierRelease(&NOTIFIER);
        return NULL;
}

void
stopNotifier()
{
    pthread_t thread;
    int rv = 0;

    if (NOTIFIER) {
        if ((rv = pthread_create(&thread, NULL, stopNotifierThread, NULL)) != 0) {
            unitdLogError(LOG_UNITD_BOOT, "src/core/handlers/notifier.c", "stopNotifier", errno,
                          strerror(errno), "Unable to create the thread for the notifier (stop)");
        }
        else {
            if (UNITD_DEBUG)
                unitdLogInfo(LOG_UNITD_BOOT, "Thread created successfully for the notifier (stop)\n");
        }
        /* Waiting for the thread to terminate */
        if ((rv = pthread_join(thread, NULL)) != EXIT_SUCCESS) {
            unitdLogError(LOG_UNITD_BOOT, "src/core/handlers/notifier.c", "stopNotifier", rv,
                          strerror(rv), "Unable to join the thread for the notifier (stop)");
        }
        else {
            if (UNITD_DEBUG)
                unitdLogInfo(LOG_UNITD_BOOT, "Thread joined successfully for the notifier (stop)\n");
        }
    }
}
