/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

/* This variable represents the stop time */
Time *BOOT_STOP;
/* This variable is used by signal handler to invoke unitdShutdown command
 * if Ctrl+alt+del is pressed after that we are listening
*/
bool LISTEN_SOCK_REQUEST;
/* The following variable is useful for unitd daemon to know how it has to
 * poweroff the machine
*/
Command SHUTDOWN_COMMAND;
/* Socket user path for unitd user instance */
char *SOCKET_USER_PATH;
/*
* An array of file descriptors which the server
* is maintaining in order to talk with the connected clients.
* Master socket FD is also here.
*/
int MONITORED_FD_SET[MAX_CLIENT_SUPPORTED];
/* This variable represents the max socket buffer size */
int MAX_SOCKBUF_SIZE = 0;
/* The following the are some functions which handling
 * the monitored fd set.
*/
static void initializeMonitoredFdSet()
{
    for (int i = 0; i < MAX_CLIENT_SUPPORTED; i++)
        MONITORED_FD_SET[i] = -1;
}

static void addToMonitoredFdSet(int socketFd)
{
    int *currentFd;
    for (int i = 0; i < MAX_CLIENT_SUPPORTED; i++) {
        currentFd = &MONITORED_FD_SET[i];
        if (*currentFd == -1) {
            *currentFd = socketFd;
            break;
        }
    }
}

static void removeFromMonitoredFdSet(int socketFd)
{
    int *currentFd;
    for (int i = 0; i < MAX_CLIENT_SUPPORTED; i++) {
        currentFd = &MONITORED_FD_SET[i];
        if (*currentFd == socketFd) {
            *currentFd = -1;
            break;
        }
    }
}

static void refreshFdSet(fd_set *fdSetPtr)
{
    int *currentFd;
    FD_ZERO(fdSetPtr);
    for (int i = 0; i < MAX_CLIENT_SUPPORTED; i++) {
        currentFd = &MONITORED_FD_SET[i];
        if (*currentFd != -1)
            FD_SET(*currentFd, fdSetPtr);
    }
}

static int getMaxFd()
{
    int *currentFd, maxFd = -1;
    for (int i = 0; i < MAX_CLIENT_SUPPORTED; i++) {
        currentFd = &MONITORED_FD_SET[i];
        if (*currentFd > maxFd)
            maxFd = *currentFd;
    }

    return maxFd;
}

static void unlinkSocket()
{
    unlink(!USER_INSTANCE ? SOCKET_PATH : SOCKET_USER_PATH);
}

int listenSocketRequest()
{
    struct sockaddr_un name;
    int rv = -1, socketData = -1, socketConnection = -1, socketFd = -1, bufferSize, *currentFd;
    fd_set readFds;
    char *buffer = NULL;

    /* Initialize */
    initializeMonitoredFdSet();
    /* If unitd daemon not properly exited, could have not remove the socket */
    unlinkSocket();
    /* Connection */
    if ((socketConnection = initSocket(&name)) == -1) {
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto out;
    }
    /* Binding */
    if ((rv = bind(socketConnection, (const struct sockaddr *)&name, sizeof(struct sockaddr_un))) ==
        -1) {
        logError(CONSOLE, "src/core/socket/socket_server.c", "listenSocketRequest", errno,
                 strerror(errno), "Bind error");
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto out;
    }
    /* Listening */
    if ((rv = listen(socketConnection, BACK_LOG)) == -1) {
        logError(CONSOLE, "src/core/socket/socket_server.c", "listenSocketRequest", errno,
                 strerror(errno), "Listen error");
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto out;
    }
    /* Adding the master socket to monitored set of fd */
    addToMonitoredFdSet(socketConnection);
    /* Stop boot */
    BOOT_STOP = timeNew(NULL);
    /* Main loop */
    while (SHUTDOWN_COMMAND == NO_COMMAND) {
        /* Copy the entire monitored set of fd to readFds */
        refreshFdSet(&readFds);
        LISTEN_SOCK_REQUEST = true;
        /* 'Select' is a blocking system call */
        if (select(getMaxFd() + 1, &readFds, NULL, NULL, NULL) == -1 && errno == EINTR)
            continue;
        /* The data arrive on the master socket only when a new client connects to the server,
         * that is, when a client performs 'connect' system call.
        */
        if (FD_ISSET(socketConnection, &readFds)) {
            if ((socketData = accept(socketConnection, NULL, NULL)) == -1 && errno != EAGAIN) {
                logError(CONSOLE, "src/core/socket/socket_server.c", "listenSocketRequest", errno,
                         strerror(errno), "Accept error");
                goto out;
            }
            addToMonitoredFdSet(socketData);
        } else {
            /* Data arrives on some other client fd
             * Let's find it
            */
            for (int i = 0; i < MAX_CLIENT_SUPPORTED; i++) {
                currentFd = &MONITORED_FD_SET[i];
                if (*currentFd != -1 && FD_ISSET(*currentFd, &readFds)) {
                    socketFd = *currentFd;
                    /* Prepare the buffer */
                    bufferSize = INITIAL_SIZE;
                    buffer = calloc(bufferSize, sizeof(char));
                    assert(buffer);
                    /* Read the message */
                    if ((rv = readMessage(&socketFd, &buffer, &bufferSize)) == -1)
                        goto out;
                    /* Dispatch the request */
                    rv = socketDispatchRequest(buffer, &socketFd);
                    /* Release buffer */
                    objectRelease(&buffer);
                    /* Remove socketFd */
                    removeFromMonitoredFdSet(socketFd);
                    continue;
                }
            }
        }
    }

out:
    if (socketConnection != -1) {
        close(socketConnection);
        removeFromMonitoredFdSet(socketConnection);
    }
    unlinkSocket();
    return rv;
}

int socketDispatchRequest(char *buffer, int *socketFd)
{
    int rv = 0;
    Command command = NO_COMMAND;
    SockMessageIn *sockMessageIn = NULL;
    SockMessageOut *sockMessageOut = NULL;

    assert(buffer);
    assert(*socketFd != -1);

    sockMessageIn = sockMessageInNew();
    sockMessageOut = sockMessageOutNew();
    if (DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "SocketDispatchRequest::Buffer received (%lu): \n%s",
               strlen(buffer), buffer);
    /* Unmarshalling */
    if ((rv = unmarshallRequest(buffer, &sockMessageIn)) == 0) {
        command = sockMessageIn->command;
        switch (command) {
        case POWEROFF_COMMAND:
        case REBOOT_COMMAND:
        case HALT_COMMAND:
        case KEXEC_COMMAND:
            SHUTDOWN_COMMAND = command;
            NO_WTMP = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[NO_WTMP_OPT].name);
            goto out;
        case LIST_COMMAND:
            getUnitListServer(socketFd, sockMessageIn, &sockMessageOut);
            break;
        case STATUS_COMMAND:
            getUnitStatusServer(socketFd, sockMessageIn, &sockMessageOut);
            break;
        case STOP_COMMAND:
            stopUnitServer(socketFd, sockMessageIn, &sockMessageOut, true);
            break;
        case START_COMMAND:
        case RESTART_COMMAND:
            startUnitServer(socketFd, sockMessageIn, &sockMessageOut, true, false);
            break;
        case DISABLE_COMMAND:
            disableUnitServer(socketFd, sockMessageIn, &sockMessageOut, NULL, true);
            break;
        case ENABLE_COMMAND:
            enableUnitServer(socketFd, sockMessageIn, &sockMessageOut);
            break;
        case LIST_REQUIRES_COMMAND:
        case LIST_CONFLICTS_COMMAND:
        case LIST_STATES_COMMAND:
            getUnitDataServer(socketFd, sockMessageIn, &sockMessageOut);
            break;
        case GET_DEFAULT_STATE_COMMAND:
            getDefaultStateServer(socketFd, sockMessageIn, &sockMessageOut);
            break;
        case SET_DEFAULT_STATE_COMMAND:
            setDefaultStateServer(socketFd, sockMessageIn, &sockMessageOut);
            break;
        default:
            break;
        }
    }

out:
    sockMessageInRelease(&sockMessageIn);
    sockMessageOutRelease(&sockMessageOut);
    return rv;
}

static void resetListRes(SockMessageOut **sockMessageOut, char **buffer)
{
    assert(*sockMessageOut);
    assert(*buffer);

    Array **errors = &(*sockMessageOut)->errors;
    Array **messages = &(*sockMessageOut)->messages;
    arrayRelease(&(*sockMessageOut)->unitsDisplay);
    objectRelease(buffer);
    if (!(*errors))
        *errors = arrayNew(objectRelease);
    if (!(*messages))
        *messages = arrayNew(objectRelease);
    arrayAdd(*errors, getMsg(-1, UNITD_ERRORS_ITEMS[UNITD_GENERIC_ERR].desc));
    arrayAdd(*messages, getMsg(-1, UNITD_MESSAGES_ITEMS[UNITD_SYSTEM_LOG_MSG].desc));
    *buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE_UNITLIST);
}

int getUnitListServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    char *buffer = NULL;
    Array *unitsDisplay = arrayNew(unitRelease), **messages = &(*sockMessageOut)->messages,
          **errors = &(*sockMessageOut)->errors, *options = sockMessageIn->options;
    int rv = 0, bufferLen = 0;
    bool bootAnalyze = false;
    socklen_t optlen = sizeof(int);
    ListFilter listFilter = getListFilterByOpt(options);
    (*sockMessageOut)->unitsDisplay = unitsDisplay;

    assert(*socketFd != -1);

    //Get bootAnalyze
    bootAnalyze = arrayContainsStr(options, OPTIONS_DATA[ANALYZE_OPT].name);
    if (!bootAnalyze) {
        fillUnitsDisplayList(&UNITD_DATA->units, &unitsDisplay, listFilter);
        /* If the filter is TIMER or UPATH then we can pull out the units from glob
         * rather than load all and then to filter.
        */
        if (listFilter == TIMERS_FILTER || listFilter == UPATH_FILTER) {
            if (!USER_INSTANCE) {
                /* Loading only timer units */
                loadOtherUnits(&unitsDisplay, UNITS_PATH, NULL, false, true, listFilter);
            } else {
                /* Loading only timer units */
                loadOtherUnits(&unitsDisplay, UNITS_USER_PATH, NULL, false, true, listFilter);
                loadOtherUnits(&unitsDisplay, UNITS_USER_LOCAL_PATH, NULL, false, true, listFilter);
            }
        } else {
            if (!USER_INSTANCE) {
                /* Loading all the units */
                loadUnits(&unitsDisplay, UNITS_PATH, NULL, NO_STATE, false, NULL,
                          PARSE_SOCK_RESPONSE_UNITLIST, true);
            } else {
                /* Loading all the units */
                loadUnits(&unitsDisplay, UNITS_USER_PATH, NULL, NO_STATE, false, NULL,
                          PARSE_SOCK_RESPONSE_UNITLIST, true);
                loadUnits(&unitsDisplay, UNITS_USER_LOCAL_PATH, NULL, NO_STATE, false, NULL,
                          PARSE_SOCK_RESPONSE_UNITLIST, true);
            }
            /* Try to apply an eventual filter */
            if (listFilter != NO_FILTER)
                applyListFilter(&unitsDisplay, listFilter);
        }
    } else {
        fillUnitsDisplayList(&UNITD_DATA->bootUnits, &unitsDisplay, NO_FILTER);
        fillUnitsDisplayList(&UNITD_DATA->initUnits, &unitsDisplay, NO_FILTER);
        /* Adding "boot and system execution time like messages" */
        Time *current = timeNew(NULL);
        /* Computing the duration */
        char *diffBooTime = stringGetDiffTime(BOOT_STOP, BOOT_START);
        char *diffExecTime = stringGetDiffTime(current, BOOT_START);
        /* Adding the messages */
        if (!(*messages))
            *messages = arrayNew(objectRelease);
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[TIME_MSG].desc, "Boot", diffBooTime));
        arrayAdd(*messages,
                 getMsg(-1, UNITS_MESSAGES_ITEMS[TIME_MSG].desc, "Execution", diffExecTime));
        /* Release resources */
        objectRelease(&diffBooTime);
        objectRelease(&diffExecTime);
        timeRelease(&current);
    }
    /* unitsDisplay could be empty */
    if (!bootAnalyze && unitsDisplay->size == 0) {
        if (!(*messages))
            *messages = arrayNew(objectRelease);
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_NO_DATA_FOUND_MSG].desc));
        goto out;
    }
    /* Sorting the array by name */
    qsort(unitsDisplay->arr, unitsDisplay->size, sizeof(Unit *), sortUnitsByName);

out:
    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE_UNITLIST);
    bufferLen = strlen(buffer);
    /* Check socket buffer size */
    if (MAX_SOCKBUF_SIZE == 0 &&
        getsockopt(*socketFd, SOL_SOCKET, SO_SNDBUF, &MAX_SOCKBUF_SIZE, &optlen) == -1) {
        logError(ALL, "src/core/socket/socket_server.c", "getUnitListServer", errno,
                 strerror(errno), "Getsockopt func returned -1 exit code!");
        resetListRes(sockMessageOut, &buffer);
        bufferLen = strlen(buffer);
    } else if (bufferLen >= MAX_SOCKBUF_SIZE) {
        arrayRelease(&(*sockMessageOut)->unitsDisplay);
        objectRelease(&buffer);
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        if (!(*messages))
            *messages = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITD_ERRORS_ITEMS[UNITD_SOCKBUF_ERR].desc));
        arrayAdd(*messages, getMsg(-1, UNITD_MESSAGES_ITEMS[UNITD_SOCKBUF_MSG].desc, bufferLen,
                                   MAX_SOCKBUF_SIZE));
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE_UNITLIST);
        bufferLen = strlen(buffer);
    }
    if (DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "getUnitsListServer::Buffer sent (%d): \n%s", bufferLen,
               buffer);
    /* Sending the response */
    if ((rv = uSend(*socketFd, buffer, bufferLen, 0)) == -1) {
        logError(SYSTEM, "src/core/socket/socket_server.c", "getUnitListServer", errno,
                 strerror(errno), "Send func returned -1 exit code!");
        if (errno == EMSGSIZE) {
            resetListRes(sockMessageOut, &buffer);
            bufferLen = strlen(buffer);
            uSend(*socketFd, buffer, bufferLen, 0);
        }
    }

    objectRelease(&buffer);
    return rv;
}

static void setOtherDataForUnit(Unit **unit, PType pType)
{
    int rv = 1;
    char *otherName = NULL;
    Unit *otherUnit = NULL;
    Array *tmpUnits = NULL, *errors = NULL;

    assert(*unit);
    assert(pType == TIMER || pType == UPATH);

    /* Get eventual other data for the unit */
    otherName = getOtherNameByUnitName((*unit)->name, pType);
    otherUnit = getUnitByName(UNITD_DATA->units, otherName);
    if (!otherUnit) {
        /* The unit is not in memory then let's to find on the disk. */
        tmpUnits = arrayNew(unitRelease);
        errors = arrayNew(objectRelease);
        rv = loadAndCheckUnit(&tmpUnits, false, otherName, false, &errors);
        if (rv == 0) {
            assert(tmpUnits->size == 1);
            otherUnit = arrayGet(tmpUnits, 0);
            assert(otherUnit);
        }
    }
    if (otherUnit) {
        switch (pType) {
        case TIMER:
            /* Timer name */
            stringSet(&(*unit)->timerName, otherUnit->name);
            /* Timer PState */
            objectRelease(&(*unit)->timerPState);
            (*unit)->timerPState = calloc(1, sizeof(PState));
            assert((*unit)->timerPState);
            *(*unit)->timerPState = otherUnit->processData->pStateData->pState;
            break;
        case UPATH:
            /* Path unit name */
            stringSet(&(*unit)->pathUnitName, otherUnit->name);
            /* Path unit pState */
            objectRelease(&(*unit)->pathUnitPState);
            (*unit)->pathUnitPState = calloc(1, sizeof(PState));
            assert((*unit)->pathUnitPState);
            *(*unit)->pathUnitPState = otherUnit->processData->pStateData->pState;
            break;
        default:
            break;
        }
    }

    arrayRelease(&tmpUnits);
    arrayRelease(&errors);
    objectRelease(&otherName);
}

int getUnitStatusServer(int *socketFd, SockMessageIn *sockMessageIn,
                        SockMessageOut **sockMessageOut)
{
    int rv = 0;
    Array **unitsDisplay, **errors, **messages, *units;
    char *buffer = NULL, *unitName = NULL;
    Unit *unit = NULL;

    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    messages = &(*sockMessageOut)->messages;
    units = UNITD_DATA->units;

    assert(sockMessageIn);
    assert(*socketFd != -1);

    /* Get unit name */
    unitName = getUnitName(sockMessageIn->arg);
    /* Create the array */
    *unitsDisplay = arrayNew(unitRelease);
    /* Try to get the unit from memory */
    unit = getUnitByName(units, unitName);
    if (unit) {
        /* Waiting for notifier. */
        handleMutex(&NOTIFIER_MUTEX, true);
        handleMutex(&NOTIFIER_MUTEX, false);
        if (unit->isChanged) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_CHANGED_ERR].desc));
            if (!(*messages))
                *messages = arrayNew(objectRelease);
            arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CHANGED_MSG].desc,
                                       !USER_INSTANCE ? "" : "--user ", unitName));
            goto out;
        }
        /* We lock the mutex for the same reasons explained in listenPipeThread Func.
         * Take a quick look there.
        */
        handleMutex(unit->mutex, true);
        if (unit->type == DAEMON || unit->type == ONESHOT) {
            /* Set eventual other data for the unit */
            setOtherDataForUnit(&unit, TIMER);
            setOtherDataForUnit(&unit, UPATH);
        }
        /* Create copy for client */
        arrayAdd(*unitsDisplay, unitNew(unit, PARSE_SOCK_RESPONSE));
        /* Unlock */
        handleMutex(unit->mutex, false);
    } else {
        /* Check and parse unitName. We don't consider the units into memory
        * because we show only syntax errors, not logic errors.
        */
        rv = loadAndCheckUnit(unitsDisplay, true, unitName, true, errors);
        if (rv == 0) {
            assert((*unitsDisplay)->size == 1);
            unit = arrayGet((*unitsDisplay), 0);
            if (unit->type == DAEMON || unit->type == ONESHOT) {
                /* Set eventual other data for the unit */
                setOtherDataForUnit(&unit, TIMER);
                setOtherDataForUnit(&unit, UPATH);
            }
        }
    }

out:
    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
    if (DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitStatusServer::Buffer sent (%lu): \n%s",
               strlen(buffer), buffer);
    /* Sending the response */
    if ((rv = uSend(*socketFd, buffer, strlen(buffer), 0)) == -1) {
        logError(SYSTEM, "src/core/socket/socket_server.c", "getUnitStatusServer", errno,
                 strerror(errno), "Send func returned -1 exit code!");
    }

    objectRelease(&unitName);
    objectRelease(&buffer);
    return rv;
}

int stopUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut,
                   bool sendResponse)
{
    int rv = 0;
    Array **units, **unitsDisplay, **errors, **unitErrors;
    char *buffer = NULL, *unitName = NULL;
    Unit *unit = NULL;
    ProcessData **pData = NULL;
    PState *pState = NULL;
    PType *pType = NULL;
    bool isDead = false;

    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    units = &UNITD_DATA->units;

    assert(sockMessageIn);
    assert(*socketFd != -1);

    /* Get unit name */
    unitName = getUnitName(sockMessageIn->arg);
    /* Create the array */
    if (!(*unitsDisplay))
        *unitsDisplay = arrayNew(unitRelease);
    /* Try to get the unit from memory */
    unit = getUnitByName(*units, unitName);
    if (unit) {
        /* Close eventual pipe except for timer unit which will be closed by stopProcess func. */
        if (unit->pipe && unit->type != TIMER)
            closePipes(NULL, unit);
        /* Get unit data */
        pData = &unit->processData;
        pState = &(*pData)->pStateData->pState;
        pType = &unit->type;
        unitErrors = &unit->errors;
        /* Waiting for notifier. */
        handleMutex(&NOTIFIER_MUTEX, true);
        handleMutex(&NOTIFIER_MUTEX, false);
        if (*pState == DEAD) {
            if (unit->isChanged || (*unitErrors && (*unitErrors)->size > 0)) {
                /* Release the unit and load "dead" data */
                arrayRemove(*units, unit);
                unit = NULL;
                if (sendResponse) {
                    rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors);
                    goto out;
                }
            } else
                isDead = true;
        }
    }
    if (unit && !isDead) {
        /* Stop the process */
        if ((*pType == DAEMON && *pState == RUNNING) ||
            ((*pType == TIMER || *pType == UPATH) &&
             (*pState == RUNNING || *pState == RESTARTING))) {
            /* We don't show the result on the console and don't catch it by signal handler */
            unit->showResult = false;
            unit->isStopping = true;
            stopProcesses(NULL, unit);
        }
        /* Waiting for notifier. */
        handleMutex(&NOTIFIER_MUTEX, true);
        handleMutex(&NOTIFIER_MUTEX, false);
        if (unit->isChanged || *pType == ONESHOT || (unit->errors && unit->errors->size > 0) ||
            (*pType == DAEMON && (*pState == EXITED || *pState == KILLED))) {
            /* Release the unit and load "dead" data */
            arrayRemove(*units, unit);
            unit = NULL;
            if (sendResponse) {
                rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors);
                if (rv != 0)
                    goto out;
            }
        } else if (sendResponse)
            /* Create a copy for client */
            arrayAdd(*unitsDisplay, unitNew(unit, PARSE_SOCK_RESPONSE));
    } else {
        /* When we come from disabledUnitFunc unitsDisplay is already here */
        if (sendResponse) {
            /* We don't parse this unit. We only check the unitname */
            rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors);
            if (rv == 0)
                isDead = true;
        }
    }
    if (isDead && sendResponse) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ALREADY_ERR].desc, "dead"));
        rv = 1;
    }

out:
    /* Marshall response */
    if (sendResponse) {
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "StopUnitServer::Buffer sent (%lu): \n%s",
                   strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = uSend(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            logError(SYSTEM, "src/core/socket/socket_server.c", "stopUnitServer", errno,
                     strerror(errno), "Send func returned -1 exit code!");
        }
        objectRelease(&buffer);
    }

    objectRelease(&unitName);
    return rv;
}

int startUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut,
                    bool sendResponse, bool isTimer)
{
    int rv = 0, len = 0, rvMutex = 0;
    Array **unitsDisplay, **errors, **units, *conflicts, *stopConflictsArr = NULL, **messages,
                                                         *requires;
    char *buffer = NULL, *unitName = NULL;
    Unit *unit = NULL, *unitDisplay, *unitConflict = NULL, *unitDep = NULL;
    bool force = false, restart = false, hasError = false, reset = false;
    const char *dep = NULL, *conflict = NULL;
    PState *pState = NULL, *pStateConflict = NULL;
    ProcessData *pData = NULL, *pDataConflict = NULL;

    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    messages = &(*sockMessageOut)->messages;
    units = &UNITD_DATA->units;

    assert(sockMessageIn);
    assert(*socketFd != -1);

    /* Get unit name */
    unitName = getUnitName(sockMessageIn->arg);
    if ((rvMutex = pthread_mutex_lock(&START_MUTEX)) != 0) {
        logError(SYSTEM, "src/core/socket/socket_server.c", "startUnitServer", rv, strerror(rv),
                 "Unable to lock the start mutex for %s!", unitName);
        kill(UNITD_PID, SIGTERM);
    }
    force = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[FORCE_OPT].name);
    restart = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RESTART_OPT].name);
    reset = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RESET_OPT].name);
    /* Create the array */
    if (!(*unitsDisplay))
        *unitsDisplay = arrayNew(unitRelease);
    unit = getUnitByName(*units, unitName);
    if (unit) {
        pData = unit->processData;
        pState = &pData->pStateData->pState;
        if (!restart && *pState != DEAD) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ALREADY_ERR].desc, "started"));
            /* If we are come from enableUnitServer func then we don't add
             * the following msg.
            */
            if (sendResponse) {
                if (!(*messages))
                    *messages = arrayNew(objectRelease);
                arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_START_MSG].desc,
                                           !USER_INSTANCE ? "" : "--user ", unitName));
            }
            goto out;
        } else {
            if (*pState != DEAD || (*pState == DEAD && unit->errors && unit->errors->size > 0))
                stopUnitServer(socketFd, sockMessageIn, sockMessageOut, false);
            /* stopUnitServer function could not removed it but it must be always removed */
            unit = getUnitByName(*units, unitName);
            if (unit) {
                /* We always remove the unit */
                arrayRemove(*units, unit);
                unit = NULL;
            }
        }
    }
    /* Check unit name.
     * When we are come from enableUnit func unitDisplay is already checked and parsed.
    */
    if (sendResponse) {
        rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors);
        if (rv != 0)
            goto out;
    }
    assert((*unitsDisplay)->size == 1);
    /* Get unitDisplay to retrieve the path */
    unitDisplay = arrayGet(*unitsDisplay, 0);
    /* We only start the dead unit */
    unit = unitNew(NULL, PARSE_UNIT);
    unit->name = stringNew(unitName);
    unit->path = stringNew(unitDisplay->path);
    unit->enabled = unitDisplay->enabled;
    /* Aggregate all the errors */
    unit->type = getPTypeByUnitName(unitName);
    assert(unit->type != NO_PROCESS_TYPE);
    switch (unit->type) {
    case DAEMON:
    case ONESHOT:
        parseUnit(unitsDisplay, &unit, true, NO_STATE);
        break;
    case TIMER:
        parseTimerUnit(unitsDisplay, &unit, true);
        checkInterval(&unit);
        break;
    case UPATH:
        parsePathUnit(unitsDisplay, &unit, true);
        checkWatchers(&unit, true);
        break;
    default:
        break;
    }
    /* Check wanted by */
    if (!USER_INSTANCE)
        checkWantedBy(&unit, STATE_CMDLINE != NO_STATE ? STATE_CMDLINE : STATE_DEFAULT, true);
    else
        checkWantedBy(&unit, STATE_USER, true);
    if (unit->errors->size > 0) {
        *errors = arrayStrCopy(unit->errors);
        unitRelease(&unit);
        goto out;
    }
    /* Check dependencies */
    requires = unit->requires;
    len = (requires ? requires->size : 0);
    for (int i = 0; i < len; i++) {
        dep = arrayGet(requires, i);
        if (!(unitDep = getUnitByName(*units, dep)) ||
            unitDep->processData->pStateData->pState == DEAD ||
            *unitDep->processData->finalStatus != FINAL_STATUS_SUCCESS ||
            unitDep->errors->size > 0) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors,
                     getMsg(-1, UNITS_ERRORS_ITEMS[UNSATISFIED_DEP_ERR].desc, dep, unitName));
            hasError = true;
        }
    }
    if (hasError) {
        unitRelease(&unit);
        goto out;
    }
    /* Check conflict */
    conflicts = unit->conflicts;
    len = (conflicts ? conflicts->size : 0);
    for (int i = 0; i < len; i++) {
        conflict = arrayGet(conflicts, i);
        unitConflict = getUnitByName(*units, conflict);
        if (unitConflict) {
            pDataConflict = unitConflict->processData;
            pStateConflict = &pDataConflict->pStateData->pState;
            if (*pStateConflict != DEAD ||
                (*pStateConflict == DEAD && *pDataConflict->finalStatus != FINAL_STATUS_SUCCESS)) {
                if (!force) {
                    if (!(*errors))
                        *errors = arrayNew(objectRelease);
                    arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_EXEC_ERROR].desc,
                                             unitName, conflict));
                    hasError = true;
                } else {
                    /* If we have to force then we create the new array of unit pointers */
                    if (!stopConflictsArr)
                        stopConflictsArr = arrayNew(NULL);
                    arrayAdd(stopConflictsArr, unitConflict);
                    unitConflict->isStopping = true;
                    unitConflict->showResult = false;
                }
            }
        }
    }
    if (!force && hasError) {
        if (!(*messages))
            *messages = arrayNew(objectRelease);
        arrayAdd(*messages,
                 getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_FORCE_START_CONFLICT_MSG].desc, unitName));
        unitRelease(&unit);
        goto out;
    } else if (stopConflictsArr) {
        /* Stopping all conflicts (parallelized) */
        closePipes(&stopConflictsArr, NULL);
        stopProcesses(&stopConflictsArr, NULL);
        len = stopConflictsArr->size;
        for (int i = 0; i < len; i++) {
            unitConflict = arrayGet(stopConflictsArr, i);
            /* Waiting for notifier. */
            handleMutex(&NOTIFIER_MUTEX, true);
            handleMutex(&NOTIFIER_MUTEX, false);
            pDataConflict = unitConflict->processData;
            pStateConflict = &pDataConflict->pStateData->pState;
            /* Release */
            if (unitConflict->isChanged || unitConflict->type == ONESHOT ||
                (unitConflict->errors && unitConflict->errors->size > 0) ||
                (unitConflict->type == DAEMON &&
                 (*pStateConflict == EXITED || *pStateConflict == KILLED)))
                arrayRemove(*units, unitConflict);
        }
        arrayRelease(&stopConflictsArr);
    }
    /* Adding the unit into memory */
    arrayAdd(*units, unit);
    /* Creating eventual pipe */
    if (unit->type == TIMER || hasPipe(unit)) {
        unit->pipe = pipeNew();
        if (unit->type != TIMER) {
            /* Create process data history array accordingly */
            unit->processDataHistory = arrayNew(processDataRelease);
            /* Put to listen the pipe */
            listenPipes(NULL, unit);
        } else if (unit->type == TIMER && reset)
            resetNextTime(unitName);
    } else if (unit->type == UPATH)
        addWatchers(&unit);
    startProcesses(units, unit);

out:
    if (sendResponse && !isTimer) {
        /* Marshall response */
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "StartUnitServer::Buffer sent (%lu): \n%s",
                   strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = uSend(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            logError(SYSTEM, "src/core/socket/socket_server.c", "startUnitServer", errno,
                     strerror(errno), "Send func returned -1 exit code!");
        }
        objectRelease(&buffer);
    }
    if ((rvMutex = pthread_mutex_unlock(&START_MUTEX)) != 0) {
        logError(SYSTEM, "src/core/socket/socket_server.c", "startUnitServer", rv, strerror(rv),
                 "Unable to unlock the start mutex for %s!", unitName);
        kill(UNITD_PID, SIGTERM);
    }

    objectRelease(&unitName);
    return rv;
}

int disableUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut,
                      const char *unitNameArg, bool sendResponse)
{
    Array **unitsDisplay, **errors, **messages, *scriptParams = NULL, *statesData = NULL;
    Unit *unit = NULL, *unitDisplay = NULL;
    char *unitName = NULL, *buffer = NULL, *stateStr = NULL, *from = NULL, *to = NULL;
    bool run = false, reEnable = false;
    int rv = 0, len = 0;
    StateData *stateData = NULL;
    State state = NO_STATE;

    assert(sockMessageIn);
    assert(*socketFd != -1);

    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    if (!(*errors))
        *errors = arrayNew(objectRelease);
    messages = &(*sockMessageOut)->messages;
    if (!(*messages))
        *messages = arrayNew(objectRelease);
    /* UnitNameArg is comes from enableUnitServer */
    if (unitNameArg)
        unitName = stringNew(unitNameArg);
    else
        /* Get unit name */
        unitName = getUnitName(sockMessageIn->arg);
    run = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RUN_OPT].name);
    reEnable = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RE_ENABLE_OPT].name);
    /* Create the array */
    if (!(*unitsDisplay))
        *unitsDisplay = arrayNew(unitRelease);
    /* Try to get the unit from memory */
    unit = getUnitByName(UNITD_DATA->units, unitName);
    if (unit) {
        if (!unit->enabled && !reEnable) {
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ALREADY_ERR].desc, "disabled"));
            goto out;
        }
        arrayAdd(*unitsDisplay, unitNew(unit, PARSE_SOCK_RESPONSE));
    } else {
        /* If we are come from 'enableUnitServer' func then we have to run loadAndCheckUnit
         * even if its size > 0 because we have to check unitNameArg (conflict name) which is not present into array.
        */
        if (sendResponse || unitNameArg) {
            /* If the unit is not in memory then check that unitname is valid.
             * No need to parse unit because 'unitDisplay->enabled' (required later) is already
             * available with 'false' parameter.
             * Additionally, we only have to remove a symlink regardless by
             * its eventual configuration errors.
            */
            rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors);
            if (rv != 0)
                goto out;
        }
    }
    /* If we are come from enableUnitServer func (conflict case) then unitdisplay has index = 1.
     * To avoid to accumulate unit we always release it.
     * In this way its index will be always equal to 1.
     * Additionally, even if run = true, we stop nothing because they will be stopped (parallelized)
     * in enableUnitServer func to optimize performance.
     */
    if (unitNameArg) {
        run = false;
        assert((*unitsDisplay)->size == 2);
        unitDisplay = arrayGet(*unitsDisplay, 1);
    } else {
        assert((*unitsDisplay)->size == 1);
        unitDisplay = arrayGet(*unitsDisplay, 0);
    }
    if (!unit && !unitDisplay->enabled) {
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ALREADY_ERR].desc, "disabled"));
        goto out;
    }
    /* Call the script to remove symlinks from the states.
     * We always consider the default state (or cmdline ), reboot and poweroff (system instance) or
     * user state (user instance)
    */
    statesData = arrayNew(NULL);
    if (!USER_INSTANCE) {
        arrayAdd(statesData, (void *)&STATE_DATA_ITEMS[REBOOT]);
        arrayAdd(statesData, (void *)&STATE_DATA_ITEMS[POWEROFF]);
        arrayAdd(
            statesData,
            (void *)&STATE_DATA_ITEMS[STATE_CMDLINE != NO_STATE ? STATE_CMDLINE : STATE_DEFAULT]);
    } else
        arrayAdd(statesData, (void *)&STATE_DATA_ITEMS[USER]);
    len = statesData->size;
    assert(len >= 1);
    for (int i = 0; i < len; ++i) {
        stateData = arrayGet(statesData, i);
        stateStr = stringNew(stateData->desc);
        stringAppendStr(&stateStr, ".state");
        state = stateData->state;
        /* Re-execute isEnabledUnit because the symlink could be missing.
         * We show the message only if it is really there.
        */
        if (isEnabledUnit(unitName, state)) {
            /* Get script parameters */
            scriptParams = getScriptParams(unitName, stateStr, SYML_REMOVE_OP, unitDisplay->path);
            from = arrayGet(scriptParams, 2);
            to = arrayGet(scriptParams, 3);
            /* Execute the script */
            rv = execScript(UNITD_DATA_PATH, "/scripts/symlink-handle.sh", scriptParams->arr, NULL);
            if (rv != 0) {
                arrayAdd(*errors, getMsg(-1, UNITD_ERRORS_ITEMS[UNITD_GENERIC_ERR].desc));
                arrayAdd(*messages, getMsg(-1, UNITD_MESSAGES_ITEMS[UNITD_SYSTEM_LOG_MSG].desc));
                /* Write the details into system log */
                logError(SYSTEM, "src/core/socket/socket_server.c", "disableUnitServer", rv,
                         strerror(rv), "ExecScript error!");
                goto out;
            } else {
                if (unit)
                    unit->enabled = false;
                /* Put the result into response */
                arrayAdd(*messages,
                         getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_REMOVED_SYML_MSG].desc, to, from));
            }
            arrayRelease(&scriptParams);
        }
        objectRelease(&stateStr);
    }
    /* If 'run' = true then stop it */
    if (run)
        stopUnitServer(socketFd, sockMessageIn, sockMessageOut, false);

out:
    if (sendResponse) {
        /* Marshall response */
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "DisableUnitServer::Buffer sent (%lu): \n%s",
                   strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = uSend(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            logError(SYSTEM, "src/core/socket/socket_server.c", "disableUnitServer", errno,
                     strerror(errno), "Send func returned -1 exit code!");
        }
        objectRelease(&buffer);
    }

    /* Release resources */
    if (unitNameArg)
        arrayRemove(*unitsDisplay, unitDisplay);
    objectRelease(&unitName);
    arrayRelease(&statesData);
    objectRelease(&stateStr);
    arrayRelease(&scriptParams);
    return rv;
}

int enableUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv = 0, len = 0;
    Array **units, **unitsDisplay, **errors, **messages, **unitDisplayErrors,
        *conflicts = NULL, *conflictNames = NULL, *unitsConflicts = NULL, *wantedBy,
        *scriptParams = NULL, *requires = NULL;
    Unit *unit = NULL, *unitDisplay = NULL, *unitConflict = NULL;
    char *unitName = NULL, *buffer = NULL, *stateStr = NULL, *from = NULL, *to = NULL;
    const char *conflictName = NULL;
    bool force = false, run = false, hasError = false, reEnable, isAlreadyDisabled = false;
    ProcessData *pDataConflict = NULL;
    PState *pStateConflict = NULL;

    assert(sockMessageIn);
    assert(*socketFd != -1);

    units = &UNITD_DATA->units;
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    if (!(*errors))
        *errors = arrayNew(objectRelease);
    messages = &(*sockMessageOut)->messages;
    if (!(*messages))
        *messages = arrayNew(objectRelease);
    /* Get unit name */
    unitName = getUnitName(sockMessageIn->arg);
    force = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[FORCE_OPT].name);
    run = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RUN_OPT].name);
    reEnable = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RE_ENABLE_OPT].name);
    /* Create the array */
    *unitsDisplay = arrayNew(unitRelease);
    /* try to get the unit from memory */
    unit = getUnitByName(*units, unitName);
    if (unit) {
        if (reEnable) {
            /* Waiting for notifier. */
            handleMutex(&NOTIFIER_MUTEX, true);
            handleMutex(&NOTIFIER_MUTEX, false);
            if (!run && unit->isChanged) {
                arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_CHANGED_ERR].desc));
                arrayAdd(*messages,
                         getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CHANGED_RE_ENABLE_MSG].desc));
                goto out;
            } else {
                disableUnitServer(socketFd, sockMessageIn, sockMessageOut, NULL, false);
                isAlreadyDisabled = true;
                /* disableUnitServer func could fully release the unit thus retry to get it */
                unit = getUnitByName(*units, unitName);
                /* Release and create unitsDisplay array to satisfy loadAndCheckUnit */
                arrayRelease(unitsDisplay);
                *unitsDisplay = arrayNew(unitRelease);
            }
        }
        /* disableUnitServer func could remove the unit */
        if (unit) {
            if (unit->enabled) {
                arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ALREADY_ERR].desc, "enabled"));
                arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_RE_ENABLE_MSG].desc,
                                           !USER_INSTANCE ? "" : "--user ", unitName));
                goto out;
            }
            /* Waiting for notifier. */
            handleMutex(&NOTIFIER_MUTEX, true);
            handleMutex(&NOTIFIER_MUTEX, false);
            if (unit->isChanged) {
                arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_CHANGED_ERR].desc));
                arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CHANGED_MSG].desc,
                                           !USER_INSTANCE ? "" : "--user ", unitName));
                goto out;
            }
        }
    }
    /* Check the unitName and parse the unit */
    rv = loadAndCheckUnit(unitsDisplay, true, unitName, true, errors);
    if (rv != 0)
        goto out;
    assert((*unitsDisplay)->size == 1);
    unitDisplay = arrayGet(*unitsDisplay, 0);
    /* The unit could have some configuration errors */
    unitDisplayErrors = &unitDisplay->errors;
    if (!(*unitDisplayErrors))
        *unitDisplayErrors = arrayNew(objectRelease);
    if ((*unitDisplayErrors)->size > 0) {
        arrayRelease(errors);
        *errors = arrayStrCopy(*unitDisplayErrors);
        goto out;
    }
    /* Check if it is already enabled */
    if (!unit && !reEnable && unitDisplay->enabled) {
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ALREADY_ERR].desc, "enabled"));
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_RE_ENABLE_MSG].desc,
                                   !USER_INSTANCE ? "" : "--user ", unitName));
        goto out;
    } else if (!unit && reEnable && unitDisplay->enabled && !isAlreadyDisabled)
        disableUnitServer(socketFd, sockMessageIn, sockMessageOut, NULL, false);
    /* Before to perform the checks and enabling, we test that at least a valid state is here */
    wantedBy = unitDisplay->wantedBy;
    if (!USER_INSTANCE) {
        if (!arrayContainsStr(wantedBy, STATE_DATA_ITEMS[REBOOT].desc) &&
            !arrayContainsStr(wantedBy, STATE_DATA_ITEMS[POWEROFF].desc) &&
            !arrayContainsStr(
                wantedBy,
                STATE_DATA_ITEMS[STATE_CMDLINE != NO_STATE ? STATE_CMDLINE : STATE_DEFAULT].desc))
            hasError = true;
    } else {
        if (!arrayContainsStr(wantedBy, STATE_DATA_ITEMS[USER].desc))
            hasError = true;
    }
    if (hasError) {
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ENABLE_STATE_ERR].desc));
        /* Building message */
        char *msg = NULL;
        if (!USER_INSTANCE) {
            msg = getMsg(
                -1, UNITS_MESSAGES_ITEMS[UNIT_ENABLE_STATE_MSG].desc,
                STATE_DATA_ITEMS[STATE_CMDLINE != NO_STATE ? STATE_CMDLINE : STATE_DEFAULT].desc);
            stringAppendStr(&msg, " - ");
            stringAppendStr(&msg, STATE_DATA_ITEMS[REBOOT].desc);
            stringAppendStr(&msg, " - ");
            stringAppendStr(&msg, STATE_DATA_ITEMS[POWEROFF].desc);
        } else
            msg = getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_ENABLE_STATE_MSG].desc,
                         STATE_DATA_ITEMS[USER].desc);
        arrayAdd(*messages, msg);
        goto out;
    }
    /* Check dependencies */
    requires = unitDisplay->requires;
    len = (requires ? requires->size : 0);
    for (int i = 0; i < len; i++) {
        const char *depName = arrayGet(requires, i);
        if (!isEnabledUnit(depName, NO_STATE)) {
            arrayAdd(*errors,
                     getMsg(-1, UNITS_ERRORS_ITEMS[UNSATISFIED_DEP_ERR].desc, depName, unitName));
            hasError = true;
        }
    }
    if (hasError) {
        unitRelease(&unit);
        goto out;
    }
    /* Before the enable it, we perform the "requires" specific check.
     * Actually, this check has been already called but now, we re-execute it considerating all memory data
     * to satisfy all checks (syntax and logic errors).
     * For logic errors, we mean the errors regard to the "backwards" dependencies
     * which cause a block to the starting of the threads.
     * Anyway, the check is there when we start as well.
    */
    checkRequires(units, &unitDisplay, true);
    if ((*unitDisplayErrors)->size > 0) {
        arrayRelease(errors);
        *errors = arrayStrCopy(*unitDisplayErrors);
        goto out;
    }
    /* Check conflicts */
    conflicts = unitDisplay->conflicts;
    len = (conflicts ? conflicts->size : 0);
    for (int i = 0; i < len; i++) {
        conflictName = arrayGet(conflicts, i);
        unitConflict = getUnitByName(*units, conflictName);
        if ((unitConflict && unitConflict->enabled) ||
            (!unitConflict && isEnabledUnit(conflictName, NO_STATE))) {
            if (!force) {
                arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_EXEC_ERROR].desc, unitName,
                                         conflictName));
                hasError = true;
            } else {
                /* If we have to force then we create the array of names and the unit pointers */
                if (!conflictNames)
                    conflictNames = arrayNew(objectRelease);
                arrayAdd(conflictNames, stringNew(conflictName));
                if (run && unitConflict) {
                    if (!unitsConflicts)
                        unitsConflicts = arrayNew(NULL);
                    unitConflict->isStopping = true;
                    unitConflict->showResult = false;
                    arrayAdd(unitsConflicts, unitConflict);
                }
            }
        }
    }
    if (!force && hasError) {
        arrayAdd(*messages,
                 getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_FORCE_START_CONFLICT_MSG].desc, unitName));
        goto out;
    }
    if (conflictNames) {
        len = conflictNames->size;
        for (int i = 0; i < len; i++) {
            conflictName = arrayGet(conflictNames, i);
            /* Disable the conflicts (only links) (serialized) */
            disableUnitServer(socketFd, sockMessageIn, sockMessageOut, conflictName, false);
        }
        arrayRelease(&conflictNames);
    }
    if (run && unitsConflicts) {
        /* Stopping all conflicts (parallelized) */
        closePipes(&unitsConflicts, NULL);
        stopProcesses(&unitsConflicts, NULL);
        len = unitsConflicts->size;
        for (int i = 0; i < len; i++) {
            unitConflict = arrayGet(unitsConflicts, i);
            /* Waiting for notifier. */
            handleMutex(&NOTIFIER_MUTEX, true);
            handleMutex(&NOTIFIER_MUTEX, false);
            pDataConflict = unitConflict->processData;
            pStateConflict = &pDataConflict->pStateData->pState;
            /* Release */
            if (unitConflict->isChanged || unitConflict->type == ONESHOT ||
                (unitConflict->errors && unitConflict->errors->size > 0) ||
                (unitConflict->type == DAEMON &&
                 (*pStateConflict == EXITED || *pStateConflict == KILLED)))
                arrayRemove(*units, unitConflict);
        }
        arrayRelease(&unitsConflicts);
    }
    /* Call the script to add symlinks.
     * We always consider the default state (or cmdline ), reboot and poweroff (system instance)
     * or user state (user instance)
    */
    len = wantedBy->size;
    for (int i = 0; i < len; ++i) {
        /* Get wanted state */
        stateStr = stringNew(arrayGet(wantedBy, i));
        stringAppendStr(&stateStr, ".state");
        State state = getStateByStr(stateStr);
        assert(state != NO_STATE);
        if (state == STATE_DEFAULT || state == STATE_CMDLINE || state == REBOOT ||
            state == POWEROFF || state == USER) {
            /* Get script parameter array */
            scriptParams = getScriptParams(unitName, stateStr, SYML_ADD_OP, unitDisplay->path);
            from = arrayGet(scriptParams, 2);
            to = arrayGet(scriptParams, 3);
            /* Execute the script */
            rv = execScript(UNITD_DATA_PATH, "/scripts/symlink-handle.sh", scriptParams->arr, NULL);
            if (rv != 0) {
                arrayAdd(*errors, getMsg(-1, UNITD_ERRORS_ITEMS[UNITD_GENERIC_ERR].desc));
                arrayAdd(*messages, getMsg(-1, UNITD_MESSAGES_ITEMS[UNITD_SYSTEM_LOG_MSG].desc));
                /* Write the details into system log */
                logError(SYSTEM, "src/core/socket/socket_server.c", "enableUnitServer", rv,
                         strerror(rv), "ExecScript error!");
                goto out;
            } else {
                if (unit)
                    unit->enabled = true;
                unitDisplay->enabled = true;
                /* Put the result into response */
                arrayAdd(*messages,
                         getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CREATED_SYML_MSG].desc, to, from));
            }
            arrayRelease(&scriptParams);
        }
        objectRelease(&stateStr);
    }
    /* If 'run' = true then start it */
    if (run)
        startUnitServer(socketFd, sockMessageIn, sockMessageOut, false, false);

out:
    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
    if (DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "EnableUnitServer::Buffer sent (%lu): \n%s", strlen(buffer),
               buffer);
    /* Sending the response */
    if ((rv = uSend(*socketFd, buffer, strlen(buffer), 0)) == -1) {
        logError(SYSTEM, "src/core/socket/socket_server.c", "enableUnitServer", errno,
                 strerror(errno), "Send func returned -1 exit code!");
    }

    objectRelease(&unitName);
    arrayRelease(&scriptParams);
    objectRelease(&stateStr);
    objectRelease(&buffer);
    return rv;
}

int getUnitDataServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv = 0;
    char *unitName = NULL, *buffer = NULL;
    bool requires = false, conflicts = false, states = false;
    Array **unitsDisplay, **units, **errors, **messages, **unitDisplayErrors;
    Unit *unit = NULL, *unitDisplay = NULL;

    assert(sockMessageIn);
    assert(*socketFd != -1);

    units = &UNITD_DATA->units;
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    messages = &(*sockMessageOut)->messages;
    requires = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[REQUIRES_OPT].name);
    if (!requires) {
        conflicts = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[CONFLICTS_OPT].name);
        if (!conflicts)
            states = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[STATES_OPT].name);
    }
    assert(requires || conflicts || states);
    /* Unit name could contain ".unit" suffix */
    unitName = getUnitName(sockMessageIn->arg);
    if (!unitName)
        unitName = sockMessageIn->arg;
    else {
        objectRelease(&sockMessageIn->arg);
        sockMessageIn->arg = unitName;
    }
    /* Create the array */
    *unitsDisplay = arrayNew(unitRelease);
    /* If the unit is into memory and the file content is changed then show the error.
     * That means the data are not synchronized between memory and disk.
     */
    unit = getUnitByName(*units, unitName);
    if (unit) {
        /* Waiting for notifier. */
        handleMutex(&NOTIFIER_MUTEX, true);
        handleMutex(&NOTIFIER_MUTEX, false);
        /* If the unit content is changed then we force the user to stop the unit.
         * In this way, we will read the data on the disk (updated) which is that we expect.
         */
        if (unit->isChanged) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_CHANGED_ERR].desc));
            if (!(*messages))
                *messages = arrayNew(objectRelease);
            arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CHANGED_MSG].desc,
                                       !USER_INSTANCE ? "" : "--user ", unitName));
            goto out;
        }
        /* Create a copy for client */
        arrayAdd(*unitsDisplay, unitNew(unit, PARSE_SOCK_RESPONSE));
    } else {
        rv = loadAndCheckUnit(unitsDisplay, true, unitName, true, errors);
        if (rv != 0)
            goto out;
    }
    assert((*unitsDisplay)->size == 1);
    unitDisplay = arrayGet(*unitsDisplay, 0);
    unitDisplayErrors = &unitDisplay->errors;
    if (*unitDisplayErrors && (*unitDisplayErrors)->size > 0) {
        *errors = arrayStrCopy(*unitDisplayErrors);
        goto out;
    }
    if (requires)
        *messages = arrayStrCopy(unitDisplay->requires);
    else if (conflicts)
        *messages = arrayStrCopy(unitDisplay->conflicts);
    else if (states)
        *messages = arrayStrCopy(unitDisplay->wantedBy);
    if (!(*messages))
        *messages = arrayNew(objectRelease);
    if ((*messages)->size == 0)
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_NO_DATA_FOUND_MSG].desc));

out:
    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
    if (DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitDataServer::Buffer sent (%lu): \n%s", strlen(buffer),
               buffer);
    /* Sending the response */
    if ((rv = uSend(*socketFd, buffer, strlen(buffer), 0)) == -1) {
        logError(SYSTEM, "src/core/socket/socket_server.c", "getUnitDataServer", errno,
                 strerror(errno), "Send func returned -1 exit code!");
    }

    objectRelease(&buffer);
    return rv;
}

int getDefaultStateServer(int *socketFd, SockMessageIn *sockMessageIn,
                          SockMessageOut **sockMessageOut)
{
    int rv = 0;
    char *buffer = NULL;
    Array **messages = NULL;

    assert(sockMessageIn);
    assert(*socketFd != -1);
    assert(STATE_DEFAULT != NO_STATE);

    messages = &(*sockMessageOut)->messages;
    *messages = arrayNew(objectRelease);
    /* Default state */
    arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[STATE_MSG].desc, "Default",
                               STATE_DATA_ITEMS[STATE_DEFAULT].desc));
    /* Current state */
    if (STATE_CMDLINE != NO_STATE) {
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[STATE_MSG].desc, "Current",
                                   STATE_DATA_ITEMS[STATE_CMDLINE].desc));
    }
    /* New default state */
    if (STATE_NEW_DEFAULT != NO_STATE) {
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[STATE_MSG].desc, "New default",
                                   STATE_DATA_ITEMS[STATE_NEW_DEFAULT].desc));
    }
    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
    if (DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetDefaultStateServer::Buffer sent (%lu): \n%s",
               strlen(buffer), buffer);
    /* Sending the response */
    if ((rv = uSend(*socketFd, buffer, strlen(buffer), 0)) == -1) {
        logError(SYSTEM, "src/core/socket/socket_server.c", "getDefaultStateServer", errno,
                 strerror(errno), "Send func returned -1 exit code!");
    }

    objectRelease(&buffer);
    return rv;
}

int setDefaultStateServer(int *socketFd, SockMessageIn *sockMessageIn,
                          SockMessageOut **sockMessageOut)
{
    int rv = 0;
    char *buffer = NULL, *newDefaultStateStr = NULL;
    Array **messages = NULL, **errors = NULL;
    State newDefaultState = NO_STATE;
    State defaultState = STATE_DEFAULT;

    assert(sockMessageIn);
    assert(*socketFd != -1);

    errors = &(*sockMessageOut)->errors;
    *errors = arrayNew(objectRelease);
    messages = &(*sockMessageOut)->messages;
    *messages = arrayNew(objectRelease);
    newDefaultStateStr = sockMessageIn->arg;
    newDefaultState = getStateByStr(newDefaultStateStr);
    assert(newDefaultState != NO_STATE);
    if (newDefaultState == defaultState) {
        if (STATE_NEW_DEFAULT != NO_STATE) {
            /* Create symlink */
            if ((rv = setNewDefaultStateSyml(defaultState, messages, errors)) == 0) {
                STATE_NEW_DEFAULT = NO_STATE;
                arrayAdd(*messages,
                         getMsg(-1, UNITS_MESSAGES_ITEMS[DEFAULT_STATE_SYML_RESTORED_MSG].desc,
                                STATE_DATA_ITEMS[defaultState].desc));
            }
        } else {
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[DEFAULT_SYML_SET_ERR].desc,
                                     STATE_DATA_ITEMS[defaultState].desc));
        }
    } else {
        STATE_NEW_DEFAULT = newDefaultState;
        /* Create symlink */
        rv = setNewDefaultStateSyml(STATE_NEW_DEFAULT, messages, errors);
    }
    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
    if (DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "SetDefaultStateServer::Buffer sent (%lu): \n%s",
               strlen(buffer), buffer);
    /* Sending the response */
    if ((rv = uSend(*socketFd, buffer, strlen(buffer), 0)) == -1) {
        logError(SYSTEM, "src/core/socket/socket_server.c", "setDefaultStateServer", errno,
                 strerror(errno), "Send func returned -1 exit code!");
    }

    objectRelease(&buffer);
    return rv;
}
