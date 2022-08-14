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

/* This boolean is useful to interrupt the main loop */
bool IS_SHUTDOWN_COMMAND = false;

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

/* The following the are some functions which handling
 * the monitored fd set.
*/
static void
initializeMonitoredFdSet()
{
    for (int i = 0; i < MAX_CLIENT_SUPPORTED; i++)
        MONITORED_FD_SET[i] = -1;
}

static void
addToMonitoredFdSet(int socketFd)
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

static void
removeFromMonitoredFdSet(int socketFd)
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

static void
refreshFdSet(fd_set *fdSetPtr)
{
    int *currentFd;
    FD_ZERO(fdSetPtr);
    for (int i = 0; i < MAX_CLIENT_SUPPORTED; i++) {
        currentFd = &MONITORED_FD_SET[i];
        if (*currentFd != -1)
            FD_SET(*currentFd, fdSetPtr);
    }
}

static int
getMaxFd()
{
    int *currentFd;
    int maxFd = -1;
    for (int i = 0; i < MAX_CLIENT_SUPPORTED; i++) {
        currentFd = &MONITORED_FD_SET[i];
        if (*currentFd > maxFd)
            maxFd = *currentFd;
    }
    return maxFd;
}

static void
unlinkSocket()
{
    if (!USER_INSTANCE)
        unlink(SOCKET_PATH);
    else
        unlink(SOCKET_USER_PATH);
}

int
listenSocketRequest()
{
    struct sockaddr_un name;
    int rv, socketData, socketConnection, socketFd, bufferSize;
    int *currentFd;
    fd_set readFds;
    char *buffer = NULL;

    rv = socketConnection = socketData = socketFd = -1;

    /* Initialize */
    initializeMonitoredFdSet();
    /* If unitd daemon not properly exited, could have not remove the socket */
    unlinkSocket();
    /* Connection */
    if ((socketConnection = initSocket(&name)) == -1)
        goto out;
    /* Binding */
    if ((rv = bind(socketConnection, (const struct sockaddr *)&name,
                   sizeof(struct sockaddr_un))) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_server.c", "listenSocketRequest",
                      errno, strerror(errno), "Bind error");
        goto out;
    }    
    /* Listening */
    if ((rv = listen(socketConnection, BACK_LOG)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_server.c", "listenSocketRequest",
                      errno, strerror(errno), "Listen error");
        goto out;

    }    
    /* Adding the master socket to monitored set of fd */
    addToMonitoredFdSet(socketConnection);

    /* Stop boot */
    timeSetCurrent(&BOOT_STOP);

    /* Main loop */
    while (!IS_SHUTDOWN_COMMAND) {
        /* Copy the entire monitored set of fd to readFds */
        refreshFdSet(&readFds);
        LISTEN_SOCK_REQUEST = true;
        /* 'Select' is a blocking system call */
        select(getMaxFd() + 1, &readFds, NULL, NULL, NULL);
        /* The data arrive on the master socket only when a new client connects to the server,
         * that is, when a client performs 'connect' system call.
        */
        if (FD_ISSET(socketConnection, &readFds)) {
            if ((socketData = accept(socketConnection, NULL, NULL)) == -1 && errno != EAGAIN) {
                unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_server.c",
                              "listenSocketRequest", errno, strerror(errno), "Accept error");
                goto out;
            }
            addToMonitoredFdSet(socketData);
        }
        else {
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

int
socketDispatchRequest(char *buffer, int *socketFd)
{
    int rv = 0;
    Command command = NO_COMMAND;
    SockMessageIn *sockMessageIn = NULL;
    SockMessageOut *sockMessageOut = NULL;

    assert(buffer);
    assert(*socketFd != -1);

    sockMessageIn = sockMessageInNew();
    sockMessageOut = sockMessageOutNew();

    if (UNITD_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "SocketDispatchRequest::Buffer received (%lu): \n%s", strlen(buffer), buffer);

    /* Unmarshalling */
    if ((rv = unmarshallRequest(buffer, &sockMessageIn)) == 0) {
        command = sockMessageIn->command;
        switch (command) {
            case POWEROFF_COMMAND:
            case REBOOT_COMMAND:
            case HALT_COMMAND:
            case KEXEC_COMMAND:
                IS_SHUTDOWN_COMMAND = true;
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
                startUnitServer(socketFd, sockMessageIn, &sockMessageOut, true);
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

int
getUnitListServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    char *buffer = NULL;
    Array *unitsDisplay = NULL;
    Array **messages = &(*sockMessageOut)->messages;
    Array **errors = &(*sockMessageOut)->errors;
    int rv, lenUnits;
    bool bootAnalyze = false;

    rv = lenUnits = 0;

    assert(*socketFd != -1);

    //Get bootAnalyze
    bootAnalyze = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[ANALYZE_OPT].name);
    if (!bootAnalyze) {
        fillUnitsDisplayList(&UNITD_DATA->units, &unitsDisplay);
        if (!USER_INSTANCE) {
            /* Loading all the units */
            loadUnits(&unitsDisplay, UNITS_PATH, NULL, NO_STATE,
                      false, NULL, PARSE_SOCK_RESPONSE_UNITLIST, true);
        }
        else {
            /* Loading all the units */
            loadUnits(&unitsDisplay, UNITS_USER_PATH, NULL, NO_STATE,
                      false, NULL, PARSE_SOCK_RESPONSE_UNITLIST, true);
            loadUnits(&unitsDisplay, UNITS_USER_LOCAL_PATH, NULL, NO_STATE,
                      false, NULL, PARSE_SOCK_RESPONSE_UNITLIST, true);
        }
    }
    else {
        fillUnitsDisplayList(&UNITD_DATA->bootUnits, &unitsDisplay);
        fillUnitsDisplayList(&UNITD_DATA->initUnits, &unitsDisplay);
        /* Adding "boot and system execution time like messages" */
        char *diffBooTime, *diffExecTime;
        diffBooTime = diffExecTime = NULL;
        Time *current = timeNew(NULL);
        /* Computing the duration */
        stringSetDiffTime(&diffBooTime, BOOT_STOP, BOOT_START);
        stringSetDiffTime(&diffExecTime, current, BOOT_START);
        /* Adding the messages */
        if (!(*messages))
            *messages = arrayNew(objectRelease);
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[TIME_MSG].desc, "Boot", diffBooTime));
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[TIME_MSG].desc, "Execution", diffExecTime));
        /* Release resources */
        objectRelease(&diffBooTime);
        objectRelease(&diffExecTime);
        timeRelease(&current);
    }
    /* unitsDisplay could empty or null */
    if (!bootAnalyze && (!unitsDisplay || unitsDisplay->size == 0)) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNITS_LIST_EMPTY_ERR].desc));
        goto out;
    }
    /* Sorting the array by name */
    if (unitsDisplay) {
        qsort(unitsDisplay->arr, unitsDisplay->size, sizeof(Unit *), sortUnitsByName);
        /* Adding the sorted array */
        (*sockMessageOut)->unitsDisplay = unitsDisplay;
    }

    out:
        /* Marshall response */
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE_UNITLIST);
        if (UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "getUnitsListServer::Buffer sent (%lu): \n%s",
                                            strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getUnitListServer."
                                         "Send func has returned %d = %s", errno, strerror(errno));
        }

        objectRelease(&buffer);
        return rv;
}

int
getUnitStatusServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv = 0;
    Array **unitsDisplay, **errors, **messages;
    char *buffer, *unitName;
    Unit *unit = NULL;

    buffer = unitName = NULL;
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    messages = &(*sockMessageOut)->messages;

    assert(sockMessageIn);
    assert(*socketFd != -1);
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

    /* Try to get the unit from memory */
    unit = getUnitByName(UNITD_DATA->units, unitName);
    if (unit) {
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
        arrayAdd(*unitsDisplay, unitNew(unit, PARSE_SOCK_RESPONSE));
    }
    else {
        /* Check and parse unitName. We don't consider the units into memory
        * because we show only syntax errors, not logic errors.
        */
        loadAndCheckUnit(unitsDisplay, true, unitName, true, errors, NULL);
    }

    out:
        /* Marshall response */
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitStatusServer::Buffer sent (%lu): \n%s",
                                            strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getUnitStatusServer."
                                         "Send func has returned %d = %s", errno, strerror(errno));
        }

        objectRelease(&buffer);
        return rv;
}

int
stopUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut,
               bool sendResponse)
{
    int rv = 0;
    Array **units, **unitsDisplay, **errors, **unitErrors;
    char *buffer, *unitName;
    Unit *unit = NULL;
    ProcessData **pData = NULL;
    PState *pState = NULL;
    PType *pType = NULL;
    bool isDead = false;

    buffer = unitName = NULL;
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    units = &UNITD_DATA->units;

    assert(sockMessageIn);
    assert(*socketFd != -1);
    /* Unit name could contain ".unit" suffix */
    unitName = getUnitName(sockMessageIn->arg);
    if (!unitName)
        unitName = sockMessageIn->arg;
    else {
        objectRelease(&sockMessageIn->arg);
        sockMessageIn->arg = unitName;
    }
    /* Create the array */
    if (!(*unitsDisplay))
        *unitsDisplay = arrayNew(unitRelease);

    /* Try to get the unit from memory */
    unit = getUnitByName(*units, unitName);
    if (unit) {
        /* Close eventual pipe */
        if (unit->pipe)
            closePipes(NULL, unit);

        /* Get unit data */
        pData = &unit->processData;
        pState = &(*pData)->pStateData->pState;
        pType = &unit->type;
        unitErrors = &unit->errors;

        /* Waiting for notifier. Basically, it should never happen. Extreme case */
        while (NOTIFIER_WORKING)
            msleep(200);

        if (*pState == DEAD) {
            if (unit->isChanged || (*unitErrors && (*unitErrors)->size > 0)) {
                /* Release the unit and load "dead" data */
                arrayRemove(*units, unit);
                rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors, NULL);
                if (rv != 0)
                    goto out;
            }
            else
                isDead = true;
        }
    }

    if (unit && !isDead) {
        /* Stop the process */
        if (*pType == DAEMON && *pState == RUNNING) {
            /* We don't show the result on the console and don't catch it by signal handler */
            unit->showResult = false;
            unit->isStopping = true;
            stopProcesses(NULL, unit);
        }
        /* Waiting for notifier. Basically, it should never happen. Extreme case */
        while (NOTIFIER_WORKING)
            msleep(200);

        if (unit->isChanged || *pType == ONESHOT ||
           (*pType == DAEMON && (*pState == EXITED || *pState == KILLED))) {
            /* Release the unit and load "dead" data */
            arrayRemove(*units, unit);
            if (sendResponse) {
                rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors, NULL);
                if (rv != 0)
                    goto out;
            }
        }
        else if (sendResponse)
            /* Create a copy for client */
            arrayAdd(*unitsDisplay, unitNew(unit, PARSE_SOCK_RESPONSE));
    }
    else {
        /* When we come from disabledUnitFunc unitsDisplay is already here */
        if ((*unitsDisplay)->size == 0) {
            /* We don't parse this unit. We only check the unitname */
            rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors, NULL);
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
            if (UNITD_DEBUG)
                syslog(LOG_DAEMON | LOG_DEBUG, "StopUnitServer::Buffer sent (%lu): \n%s",
                                                strlen(buffer), buffer);
            /* Sending the response */
            if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
                syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::stopUnitServer."
                                             "Send func has returned %d = %s", errno, strerror(errno));
            }
            objectRelease(&buffer);
        }

        return rv;
}

int
startUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut,
                bool sendResponse)
{
    int rv, len;
    Array **unitsDisplay, **errors, **units, *conflicts, *stopConflictsArr,
          **messages, *requires;
    char *buffer, *unitName, *unitPath;
    Unit *unit, *unitConflict, *unitDep;
    bool force, restart, hasError;
    const char *dep, *conflict, *unitPathSearch;
    PState *pState, *pStateConflict;
    ProcessData *pData, *pDataConflict;

    unit = unitConflict = unitDep = NULL;
    force = restart = hasError = false;
    rv = len = 0;
    buffer = unitName = unitPath = NULL;
    dep = conflict = NULL;
    unitPathSearch = "";
    pData = pDataConflict = NULL;
    pState = pStateConflict = NULL;
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    messages = &(*sockMessageOut)->messages;
    units = &UNITD_DATA->units;
    stopConflictsArr = NULL;

    assert(sockMessageIn);
    assert(*socketFd != -1);
    /* Unit name could contain ".unit" suffix */
    unitName = getUnitName(sockMessageIn->arg);
    if (!unitName)
        unitName = sockMessageIn->arg;
    else {
        objectRelease(&sockMessageIn->arg);
        sockMessageIn->arg = unitName;
    }
    force = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[FORCE_OPT].name);
    restart = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RESTART_OPT].name);

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
            /* If we are come from enableUnitServer func then we don't add the following msg */
            if (sendResponse) {
                if (!(*messages))
                    *messages = arrayNew(objectRelease);
                arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_START_MSG].desc,
                                           !USER_INSTANCE ? "" : "--user ", unitName));
            }
            goto out;
        }
        else {
            if (*pState != DEAD)
                stopUnitServer(socketFd, sockMessageIn, sockMessageOut, false);
            /* stopUnitServer function could not removed it but it must be always removed */
            unit = getUnitByName(*units, unitName);
            if (unit) {
                /* We always remove the unit */
                arrayRemove(*units, unit);
                unit = getUnitByName(*units, unitName);
            }
            assert(unit == NULL);
        }
    }

    /* If we are come from enableUnitServer then we can release unitsDisplay to satisfy loadAndCheckUnit.
     * We need of unitPathSearch to parse the unit
    */
    if ((*unitsDisplay)->size > 0) {
        arrayRelease(unitsDisplay);
        *unitsDisplay = arrayNew(unitRelease);
    }

    rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors, &unitPathSearch);
    if (rv == 0) {
        assert(strlen(unitPathSearch) > 0);
        /* We only start the dead unit */
        unit = unitNew(NULL, PARSE_UNIT);
        unit->name = stringNew(unitName);
        unitPath = stringNew(unitPathSearch);
        stringAppendChr(&unitPath, '/');
        stringAppendStr(&unitPath, unit->name);
        stringAppendStr(&unitPath, ".unit");
        unit->path = unitPath;
        unit->enabled = isEnabledUnit(unitName, NO_STATE);
        /* Aggregate all the errors */
        parseUnit(unitsDisplay, &unit, true, NO_STATE);
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
                *unitDep->processData->finalStatus != FINAL_STATUS_SUCCESS ||
                (unitDep->errors->size > 0)) {
                if (!(*errors))
                    *errors = arrayNew(objectRelease);
                arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNSATISFIED_DEP_ERR].desc,
                                         dep, unitName));
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
                   (*pStateConflict == DEAD && *pDataConflict->finalStatus != FINAL_STATUS_NOT_READY)) {
                    if (!force) {
                        if (!(*errors))
                            *errors = arrayNew(objectRelease);
                        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_EXEC_ERROR].desc,
                                                 unitName, conflict));
                        hasError = true;
                    }
                    else {
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
            arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_FORCE_START_CONFLICT_MSG].desc, unitName));
            unitRelease(&unit);
            goto out;
        }
        else if (stopConflictsArr) {
            /* Stopping all conflicts (parallelized) */
            closePipes(&stopConflictsArr, NULL);
            stopProcesses(&stopConflictsArr, NULL);
            len = stopConflictsArr->size;
            for (int i = 0; i < len; i++) {
                unitConflict = arrayGet(stopConflictsArr, i);
                /* Waiting for notifier. Basically, it should never happen. Extreme case */
                while (NOTIFIER_WORKING)
                    msleep(200);

                pDataConflict = unitConflict->processData;
                pStateConflict = &pDataConflict->pStateData->pState;

                /* Release */
                if (unitConflict->isChanged || unitConflict->type == ONESHOT ||
                   (unitConflict->type == DAEMON && (*pStateConflict == EXITED || *pStateConflict == KILLED)))
                    arrayRemove(*units, unitConflict);
            }
            arrayRelease(&stopConflictsArr);
        }
        /* Adding the unit into memory */
        arrayAdd(*units, unit);
        /* Creating eventual pipe */
        if (hasPipe(unit)) {
            unit->pipe = pipeNew();
            /* Create process data history array accordingly */
            unit->processDataHistory = arrayNew(processDataRelease);
            openPipes(NULL, unit);
        }
        startProcesses(units, unit);
    }

    out:
        if (sendResponse) {
            /* Marshall response */
            buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
            if (UNITD_DEBUG)
                syslog(LOG_DAEMON | LOG_DEBUG, "StartUnitServer::Buffer sent (%lu): \n%s",
                                                strlen(buffer), buffer);
            /* Sending the response */
            if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
                syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::startUnitServer."
                                             "Send func has returned %d = %s", errno, strerror(errno));
            }
            objectRelease(&buffer);
        }
        return rv;
}

int
disableUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut,
                  const char *unitNameArg, bool sendResponse)
{
    Array **unitsDisplay, **errors, **messages, *scriptParams, *statesData;
    Unit *unit, *unitDisplay;
    char *unitName, *buffer, *stateStr, *from, *to;
    const char *unitPathSearch = "";
    bool run = false;
    int rv, len;
    StateData *stateData = NULL;
    State state = NO_STATE;

    unitName = buffer = stateStr = from = to = NULL;
    unit = unitDisplay = NULL;
    scriptParams = statesData = NULL;
    rv = len = 0;

    assert(sockMessageIn);
    assert(*socketFd != -1);
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    messages = &(*sockMessageOut)->messages;

    /* UnitNameArg is comes from enableUnitServer */
    if (unitNameArg)
        unitName = stringNew(unitNameArg);
    else {
        /* Unit name could contain ".unit" suffix */
        unitName = getUnitName(sockMessageIn->arg);
        if (!unitName)
            unitName = sockMessageIn->arg;
        else {
            objectRelease(&sockMessageIn->arg);
            sockMessageIn->arg = unitName;
        }
    }
    run = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RUN_OPT].name);

    /* Create the array */
    if (!(*unitsDisplay))
        *unitsDisplay = arrayNew(unitRelease);

    /* Try to get the unit from memory */
    unit = getUnitByName(UNITD_DATA->units, unitName);
    if (unit) {
        if (!unit->enabled) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ALREADY_ERR].desc, "disabled"));
            if (run) {
                if (!(*messages))
                    *messages = arrayNew(objectRelease);
                arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_STOP_MSG].desc,
                                           !USER_INSTANCE ? "" : "--user ", unitName));
            }
            goto out;
        }
        arrayAdd(*unitsDisplay, unitNew(unit, PARSE_SOCK_RESPONSE));
    }
    else {
//FIXME Test the following if statement
        /* If we are come from enableUnitServer func then unitsDisplay already contains
         * our unit which has been already parsed and checked.
        */
        if ((*unitsDisplay)->size == 0) {
            /* If the unit is not in memory then check that unitname is valid.
             * No need to parse unit because 'unitDisplay->enabled' (required later) is already
             * available with 'false' parameter.
             * Additionally, we only have to remove a symlink regardless by
             * its eventual configuration errors.
            */
            rv = loadAndCheckUnit(unitsDisplay, false, unitName, false, errors, &unitPathSearch);
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
    }
    else {
        assert((*unitsDisplay)->size == 1);
        unitDisplay = arrayGet(*unitsDisplay, 0);
    }
    if (!unit && !unitDisplay->enabled) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
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
        arrayAdd(statesData,
                 (void *)&STATE_DATA_ITEMS[STATE_CMDLINE != NO_STATE ? STATE_CMDLINE : STATE_DEFAULT]);
    }
    else
        arrayAdd(statesData, (void *)&STATE_DATA_ITEMS[USER]);

    len = statesData->size;
    assert(len >= 1);
    for (int i = 0; i < len; ++i) {
        stateData = arrayGet(statesData, i);
        stateStr = stringNew(stateData->desc);
        stringAppendStr(&stateStr, ".state");
        state = stateData->state;

        if (isEnabledUnit(unitName, state)) {
            /* Get script parameter array
             * In this case 'unitPathSearch' is useless because we are removing the symlink
            */
            scriptParams = getScriptParams(unitName, stateStr, SYML_REMOVE_OP, unitPathSearch);
            from = arrayGet(scriptParams, 2);
            to = arrayGet(scriptParams, 3);
            /* Execute the script */
            rv = execScript(UNITD_DATA_PATH, "/scripts/symlink-handle.sh", scriptParams->arr, NULL);
            if (rv != 0) {
                /* We don't put this error into response because it should never occurred */
                syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::disableUnitServer."
                                             "ExecScript func has returned %d = %s", rv, strerror(rv));
            }
            else {
                if (unit)
                    unit->enabled = false;
                /* Put the result into response */
                if (!(*messages))
                    *messages = arrayNew(objectRelease);
                arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_REMOVED_SYML_MSG].desc,
                                           to, from));
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
            if (UNITD_DEBUG)
                syslog(LOG_DAEMON | LOG_DEBUG, "DisableUnitServer::Buffer sent (%lu): \n%s",
                       strlen(buffer), buffer);
            /* Sending the response */
            if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
                syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::disableUnitServer."
                                             "Send func has returned %d = %s", errno, strerror(errno));
            }
            objectRelease(&buffer);
        }
        if (unitNameArg) {
            arrayRemove(*unitsDisplay, unitDisplay);
            objectRelease(&unitName);
        }
        arrayRelease(&statesData);
        return rv;
}

int
enableUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv, len;
    Array **units, **unitsDisplay, **errors, **messages, **unitDisplayErrors, *conflicts,
          *conflictNames, *unitsConflicts, *wantedBy, *scriptParams, *requires;
    Unit *unit, *unitDisplay, *unitConflict;
    char *unitName, *buffer, *stateStr, *from, *to;
    const char *conflictName, *unitPathSearch;
    bool force, run, hasError, reEnable, isAlreadyDisabled;
    ProcessData *pDataConflict = NULL;
    PState *pStateConflict = NULL;

    rv = len = 0;
    unitName = buffer = stateStr = from = to = NULL;
    conflictName = unitPathSearch = NULL;
    force = run = hasError = isAlreadyDisabled = false;
    unit = unitDisplay = unitConflict = NULL;
    conflicts = conflictNames = unitsConflicts = requires = NULL;

    assert(sockMessageIn);
    assert(*socketFd != -1);
    units = &UNITD_DATA->units;
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;
    messages = &(*sockMessageOut)->messages;

    /* Unit name could contain ".unit" suffix */
    unitName = getUnitName(sockMessageIn->arg);
    if (!unitName)
        unitName = sockMessageIn->arg;
    else {
        objectRelease(&sockMessageIn->arg);
        sockMessageIn->arg = unitName;
    }
    force = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[FORCE_OPT].name);
    run = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RUN_OPT].name);
    reEnable = arrayContainsStr(sockMessageIn->options, OPTIONS_DATA[RE_ENABLE_OPT].name);

    /* Create the array */
    *unitsDisplay = arrayNew(unitRelease);

    /* try to get the unit from memory */
    unit = getUnitByName(*units, unitName);
    if (unit) {
        if (reEnable) {
            if (!run && unit->isChanged) {
                if (!(*errors))
                    *errors = arrayNew(objectRelease);
                arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_CHANGED_ERR].desc));
                if (!(*messages))
                    *messages = arrayNew(objectRelease);
                arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CHANGED_RE_ENABLE_MSG].desc));
                goto out;
            }
            else {
                disableUnitServer(socketFd, sockMessageIn, sockMessageOut, NULL, false);
                isAlreadyDisabled = true;
                /* disableUnitServer func could fully release the unit thus retry to get it */
                unit = getUnitByName(*units, unitName);
                /* Release and create unitsDisplay array to satisfy loadAndCheckUnit */
                arrayRelease(unitsDisplay);
                *unitsDisplay = arrayNew(unitRelease);
            }
        }

        if (unit && unit->enabled) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ALREADY_ERR].desc, "enabled"));
//FIXME show re-enabling message. See disableServer func!
            goto out;
        }
        /* Waiting for notifier. Basically, it should never happen. Extreme case */
        while (NOTIFIER_WORKING)
            msleep(200);

        if (unit && unit->isChanged) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_CHANGED_ERR].desc));
            if (!(*messages))
                *messages = arrayNew(objectRelease);
            arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CHANGED_MSG].desc,
                                       !USER_INSTANCE ? "" : "--user ", unitName));
            goto out;
        }
    }

//FIXME run loadAndCheckUnit if the unit is not in memory
    // in this case we can retrieve unitPathSearch from unit->path
    // checking UNITS_USER_PATH, UNITS_PATH and UNITS_USER_LOCAL_PATH because
    // UNITS_PATH is included in UNITS_USER_PATH and thus stringStartWith func could fail.

    /* Check the unitName and parse the unit */
    unitPathSearch = "";
    rv = loadAndCheckUnit(unitsDisplay, true, unitName, true, errors, &unitPathSearch);
    if (rv != 0)
        goto out;

    /* The unit could have some configuration errors */
    unitDisplayErrors = &unitDisplay->errors;
    if (!(*unitDisplayErrors))
        *unitDisplayErrors = arrayNew(objectRelease);
    if ((*unitDisplayErrors)->size > 0) {
        *errors = arrayStrCopy(*unitDisplayErrors);
        goto out;
    }

    assert((*unitsDisplay)->size == 1);
    unitDisplay = arrayGet(*unitsDisplay, 0);
    /* Check if it is already enabled */
    if (!unit && !reEnable && unitDisplay->enabled) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ALREADY_ERR].desc, "enabled"));
        goto out;
    }
    else if (!unit && reEnable && unitDisplay->enabled && !isAlreadyDisabled) {
//FIXME probably, in disableUnitFunc we can avoid to call loadAndCheckUnit because unitsDisplay is already here
        //and the unit is already parsed
        disableUnitServer(socketFd, sockMessageIn, sockMessageOut, NULL, false);
    }

    /* Before to perform the checks and enabling, we test that at least a valid state is here */
    wantedBy = unitDisplay->wantedBy;
    if (!USER_INSTANCE) {
        if (!arrayContainsStr(wantedBy, STATE_DATA_ITEMS[REBOOT].desc) &&
            !arrayContainsStr(wantedBy, STATE_DATA_ITEMS[POWEROFF].desc) &&
            !arrayContainsStr(wantedBy, STATE_DATA_ITEMS[STATE_CMDLINE].desc) &&
            !arrayContainsStr(wantedBy, STATE_DATA_ITEMS[STATE_DEFAULT].desc))
            hasError = true;
    }
    else {
        if (!arrayContainsStr(wantedBy, STATE_DATA_ITEMS[USER].desc))
            hasError = true;
    }
    if (hasError) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ENABLE_STATE_ERR].desc));
        if (!(*messages))
            *messages = arrayNew(objectRelease);
        /* Building message */
        char *msg = NULL;
        if (!USER_INSTANCE) {
            msg = getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_ENABLE_STATE_MSG].desc,
                            STATE_DATA_ITEMS[STATE_CMDLINE != NO_STATE ? STATE_CMDLINE : STATE_DEFAULT].desc);
            stringAppendStr(&msg, "/n");
            stringAppendStr(&msg, STATE_DATA_ITEMS[REBOOT].desc);
            stringAppendStr(&msg, "/n");
            stringAppendStr(&msg, STATE_DATA_ITEMS[POWEROFF].desc);
        }
        else
            msg = getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_ENABLE_STATE_MSG].desc, STATE_DATA_ITEMS[USER].desc);
        arrayAdd(*messages, msg);
        goto out;
    }

    /* Check dependencies */
    requires = unitDisplay->requires;
    len = (requires ? requires->size : 0);
    for (int i = 0; i < len; i++) {
        const char *depName = arrayGet(requires, i);
        if (!isEnabledUnit(depName, NO_STATE)) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNSATISFIED_DEP_ERR].desc,
                                     depName, unitName));
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
                if (!(*errors))
                    *errors = arrayNew(objectRelease);
                arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_EXEC_ERROR].desc,
                                         unitName, conflictName));
                hasError = true;
            }
            else {
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
        if (!(*messages))
            *messages = arrayNew(objectRelease);
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_FORCE_START_CONFLICT_MSG].desc, unitName));
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

            /* Waiting for notifier. Basically, it should never happen. Extreme case */
            while (NOTIFIER_WORKING)
                msleep(200);

            pDataConflict = unitConflict->processData;
            pStateConflict = &pDataConflict->pStateData->pState;

            /* Release */
            if (unitConflict->isChanged || unitConflict->type == ONESHOT ||
                (unitConflict->type == DAEMON && (*pStateConflict == EXITED || *pStateConflict == KILLED)))
                arrayRemove(*units, unitConflict);
        }
        arrayRelease(&unitsConflicts);
    }

    /* Call the script to add symlinks.
     * We always consider the default state (or cmdline ), reboot and poweroff (system instance)
     * or user state (user instance)
    */
//FIXME wantedBy has been already taken
//    wantedBy = unitDisplay->wantedBy;
    len = wantedBy->size;
    for (int i = 0; i < len; ++i) {
        /* Get wanted state */
        stateStr = stringNew(arrayGet(wantedBy, i));
        stringAppendStr(&stateStr, ".state");
        State state = getStateByStr(stateStr);
        assert(state != NO_STATE);
        if (state == STATE_DEFAULT || state == STATE_CMDLINE ||
            state == REBOOT || state == POWEROFF || state == USER) {
            /* Get script parameter array */
            scriptParams = getScriptParams(unitName, stateStr, SYML_ADD_OP, unitPathSearch);
            from = arrayGet(scriptParams, 2);
            to = arrayGet(scriptParams, 3);
            /* Execute the script */
            rv = execScript(UNITD_DATA_PATH, "/scripts/symlink-handle.sh", scriptParams->arr, NULL);
            if (rv != 0) {
                /* We don't put this error into response because it should never occurred */
                syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::enableUnitServer."
                                             "ExecScript func has returned %d = %s", rv, strerror(rv));
            }
            else {
                if (unit)
                    unit->enabled = true;
                /* Put the result into response */
                if (!(*messages))
                    *messages = arrayNew(objectRelease);
                arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CREATED_SYML_MSG].desc,
                                           to, from));
            }
            arrayRelease(&scriptParams);
        }
        objectRelease(&stateStr);
    }
    /* If 'run' = true then start it */
    if (run)
        startUnitServer(socketFd, sockMessageIn, sockMessageOut, false);

    out:
        /* Marshall response */
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "EnableUnitServer::Buffer sent (%lu): \n%s",
                   strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::enableUnitServer."
                                         "Send func has returned %d = %s", errno, strerror(errno));
        }

        objectRelease(&buffer);
        return rv;
}

int
getUnitDataServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv = 0;
    char *unitName, *buffer;
    bool requires, conflicts, states;
    Array **unitsDisplay, **units, **errors, **messages, **unitDisplayErrors;
    Unit *unit, *unitDisplay;

    unitName = buffer = NULL;
    requires = conflicts = states = false;
    unit = unitDisplay = NULL;

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

    /* We always read the data from the disk.
     * If the unit is into memory and its content is changed then show the error.
     * That means the data are not synchronized between memory and disk.
     */
    unit = getUnitByName(*units, unitName);
    if (unit) {
        /* Waiting for notifier. Basically, it should never happen. Extreme case */
        while (NOTIFIER_WORKING)
            msleep(200);
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
    }

    rv = loadAndCheckUnit(unitsDisplay, true, unitName, true, errors, NULL);
    if (rv != 0)
        goto out;

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

    out:
        /* Marshall response */
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitDataServer::Buffer sent (%lu): \n%s",
                   strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getUnitDataServer."
                                         "Send func has returned %d = %s", errno, strerror(errno));
        }

        objectRelease(&buffer);
        return rv;
}

int
getDefaultStateServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
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
    arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[STATE_MSG].desc,
                               "Default", STATE_DATA_ITEMS[STATE_DEFAULT].desc));
    /* Current state */
    if (STATE_CMDLINE != NO_STATE) {
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[STATE_MSG].desc,
                                   "Current", STATE_DATA_ITEMS[STATE_CMDLINE].desc));
    }
    /* New default state */
    if (STATE_NEW_DEFAULT != NO_STATE) {
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[STATE_MSG].desc,
                                   "New default", STATE_DATA_ITEMS[STATE_NEW_DEFAULT].desc));
    }

    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
    if (UNITD_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetDefaultStateServer::Buffer sent (%lu): \n%s",
               strlen(buffer), buffer);
    /* Sending the response */
    if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
        syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getDefaultStateServer."
                                     "Send func has returned %d = %s", errno, strerror(errno));
    }

    objectRelease(&buffer);
    return rv;
}

int
setDefaultStateServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv = 0;
    char *buffer, *newDefaultStateStr;
    Array **messages, **errors;
    State newDefaultState = NO_STATE;
    State defaultState = STATE_DEFAULT;

    messages = errors = NULL;
    buffer = newDefaultStateStr = NULL;

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
            setNewDefaultStateSyml(defaultState, messages);
            STATE_NEW_DEFAULT = NO_STATE;
            arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[DEFAULT_STATE_SYML_RESTORED_MSG].desc,
                                       STATE_DATA_ITEMS[defaultState].desc));
        }
        else {
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[DEFAULT_SYML_SET_ERR].desc,
                                     STATE_DATA_ITEMS[defaultState].desc));
        }
    }
    else {
        STATE_NEW_DEFAULT = newDefaultState;
        /* Create symlink */
        setNewDefaultStateSyml(STATE_NEW_DEFAULT, messages);
    }

    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
    if (UNITD_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetDefaultStateServer::Buffer sent (%lu): \n%s",
               strlen(buffer), buffer);
    /* Sending the response */
    if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
        syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getDefaultStateServer."
                                     "Send func has returned %d = %s", errno, strerror(errno));
    }

    objectRelease(&buffer);
    return rv;
}
