/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

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
    unlink(SOCKET_NAME);
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
            if ((socketData = accept(socketConnection, NULL, NULL)) == -1) {
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
                if (FD_ISSET(*currentFd, &readFds)) {
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
        unlink(SOCKET_NAME);
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

    sockMessageIn = sockMessageInCreate();
    sockMessageOut = sockMessageOutCreate();

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
                getUnitListServer(socketFd, &sockMessageOut);
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
getUnitListServer(int *socketFd, SockMessageOut **sockMessageOut)
{
    char *buffer = NULL;
    Array *units, *unitsDisplay;
    int rv, lenUnits;

    units = unitsDisplay = NULL;
    rv = lenUnits = 0;

    assert(*socketFd != -1);

    /* Building the unitsDisplay array */
    units = UNITD_DATA->units;
    lenUnits = (units ? units->size : 0);
    if (lenUnits > 0)
        unitsDisplay = arrayNew(unitRelease);
    for (int i = 0; i < lenUnits; i++)
        arrayAdd(unitsDisplay, unitCreate(arrayGet(units, i), PARSE_SOCK_RESPONSE_UNITLIST));

    /* Loading all the units */
    loadUnits(&unitsDisplay, UNITS_PATH, NULL, NO_STATE,
              false, NULL, PARSE_SOCK_RESPONSE_UNITLIST, true);
    /* Sorting the array by name */
    qsort(unitsDisplay->arr, unitsDisplay->size, sizeof(Unit *), sortUnitsByName);
    /* Adding the sorted array */
    (*sockMessageOut)->unitsDisplay = unitsDisplay;
    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE_UNITLIST);
    if (UNITD_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "getUnitsListServer::Buffer sent (%lu): \n%s",
                                        strlen(buffer), buffer);
    /* Sending the response */
    if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
        syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getUnitListServer."
                                     "Send func has returned %d = %s", rv, strerror(rv));
    }

    objectRelease(&buffer);
    return rv;
}

int
getUnitStatusServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv, lenUnitsDisplay;
    Array **unitsDisplay, **errors;
    char *buffer, *unitName;
    Unit *unit = NULL;

    rv = lenUnitsDisplay = 0;
    buffer = unitName = NULL;
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    errors = &(*sockMessageOut)->errors;

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
    if (unit)
        arrayAdd(*unitsDisplay, unitCreate(unit, PARSE_SOCK_RESPONSE));
    else {
        /* Check and parse unitName. We don't consider the units into memory
        * because we show only syntax errors, not logic errors.
        */
        loadUnits(unitsDisplay, UNITS_PATH, NULL, NO_STATE, true, unitName, PARSE_SOCK_RESPONSE, true);
        if ((*unitsDisplay)->size == 0) {
            *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_NOT_EXIST].desc, unitName));
        }
    }
    /* Marshall response */
    buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
    if (UNITD_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitStatusServer::Buffer sent (%lu): \n%s",
                                        strlen(buffer), buffer);
    /* Sending the response */
    if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
        syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getUnitStatusServer."
                                     "Send func has returned %d = %s", rv, strerror(rv));
    }

    objectRelease(&buffer);
    return rv;
}

int
stopUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut,
               bool sendResponse)
{
    int rv = 0;
    Array **units, **unitsDisplay, **errors;
    char *buffer, *unitName;
    Unit *unit = NULL;

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
        /* Create a copy for client */
        arrayAdd(*unitsDisplay, unitCreate(unit, PARSE_SOCK_RESPONSE));
        /* Close eventual pipe */
        if (unit->pipe)
            closePipes(NULL, unit);
        /* Stop the process */
        if (unit->type == DAEMON && unit->processData->pStateData->pState == RUNNING) {
            /* We don't show the result on the console and don't catch it by signal handler */
            unit->showResult = false;
            unit->isStopping = true;
            stopProcesses(NULL, unit);
        }
        /* Release the unit */
        arrayRemove(*units, unit);
    }
    else {
        loadUnits(unitsDisplay, UNITS_PATH, NULL, NO_STATE, false, unitName, PARSE_SOCK_RESPONSE, false);
        if ((*unitsDisplay)->size == 0) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_NOT_EXIST].desc, unitName));
            rv = 1;
        }
        else {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_DIED_ERR].desc));
            rv = 1;
        }
    }
    /* Marshall response */
    if (sendResponse) {
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "StopUnitServer::Buffer sent (%lu): \n%s",
                                            strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::stopUnitServer."
                                         "Send func has returned %d = %s", rv, strerror(rv));
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
    const char *dep, *conflict;

    unit = unitConflict = unitDep = NULL;
    force = restart = hasError = false;
    rv = len = 0;
    buffer = unitName = unitPath = NULL;
    dep = conflict = NULL;
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
        if (!restart) {
            /* Avoid to show the unit detail if the unit is already into memory */
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_START_ERR].desc, unitName));
            /* If we are come from enableUnitServer func then we don't add the following msg */
            if (sendResponse) {
                if (!(*messages))
                    *messages = arrayNew(objectRelease);
                arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_START_MSG].desc, unitName));
            }
            goto out;
        }
        else if (restart) {
            assert(stopUnitServer(socketFd, sockMessageIn, sockMessageOut, false) == 0);
            unit = NULL;
        }
    }
    loadUnits(unitsDisplay, UNITS_PATH, NULL, NO_STATE, false, unitName, PARSE_SOCK_RESPONSE, false);

    if ((*unitsDisplay)->size == 0) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_NOT_EXIST].desc, unitName));
    }
    else {
        /* We only start the died unit */
        unit = unitCreate(NULL, PARSE_UNIT);
        unit->name = stringNew(unitName);
        unitPath = stringNew(UNITS_PATH);
        stringAppendChr(&unitPath, '/');
        stringAppendStr(&unitPath, unit->name);
        stringAppendStr(&unitPath, ".unit");
        unit->path = unitPath;
        unit->enabled = isEnabledUnit(unitName);
        /* Aggregate all the syntax errors.
        * Check and parse unitName. We don't consider the units into memory
        * because we show only syntax errors, not logic errors.
        */
        parseUnit(unitsDisplay, &unit, true);
        /* Check wanted by */
        if (STATE_CMDLINE != NO_STATE && STATE_DEFAULT == NO_STATE)
            checkWantedBy(&unit, STATE_CMDLINE, true);
        else if (STATE_CMDLINE == NO_STATE && STATE_DEFAULT != NO_STATE)
            checkWantedBy(&unit, STATE_DEFAULT, true);
        if (unit->errors->size > 0) {
            /* The errors are here thus we show the unit errors */
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
            if ((unitConflict = getUnitByName(*units, conflict))) {
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
            for (int i = 0; i < len; i++)
                arrayRemove(*units, arrayGet(stopConflictsArr, i));
            arrayRelease(&stopConflictsArr);
        }
        /* Adding the unit into memory */
        arrayAdd(*units, unit);
        /* Creating eventual pipe */
        if (hasPipe(unit))
            openPipes(NULL, unit);
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
                                             "Send func has returned %d = %s", rv, strerror(rv));
            }
            objectRelease(&buffer);
        }
        return rv;
}

int
disableUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut,
                  const char *unitNameArg, bool sendResponse)
{
    Array **unitsDisplay, **errors, **messages, *wantedBy, *scriptParams, **unitDisplayErrors;
    Unit *unit, *unitDisplay;
    char *unitName, *buffer, *stateStr, *from, *to;
    bool run = false;
    int rv, len;

    unitName = buffer = stateStr = from = to = NULL;
    unit = unitDisplay = NULL;
    wantedBy = scriptParams = NULL;
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
    if (unit && !unit->enabled) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_DISABLED_ERR].desc, unitName));
        goto out;
    }
    if (unit) {
        unitDisplay = unitCreate(unit, PARSE_SOCK_RESPONSE);
        arrayAdd(*unitsDisplay, unitDisplay);
    }
    else
        loadUnits(unitsDisplay, UNITS_PATH, NULL, NO_STATE, true, unitName, PARSE_SOCK_RESPONSE, true);

    if ((*unitsDisplay)->size == 0) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_NOT_EXIST].desc, unitName));
        goto out;
    }

    /* If we are come from enableUnitServer func (conflict case) then unitdisplay has index = 1.
     * To avoid to accumulate unit we always release this unit.
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
    if (!unitDisplay->enabled) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_DISABLED_ERR].desc, unitName));
        goto out;
    }
    unitDisplayErrors = &unitDisplay->errors;
    if (*unitDisplayErrors && (*unitDisplayErrors)->size > 0) {
        *errors = arrayStrCopy(*unitDisplayErrors);
        goto out;
    }
    /* Call the script to remove symlinks from the states
     * We always consider the default state (or cmdline ), reboot and poweroff
    */
    wantedBy = unitDisplay->wantedBy;
    len = wantedBy->size;
    for (int i = 0; i < len; ++i) {
        /* Get wanted state */
        stateStr = stringNew(arrayGet(wantedBy, i));
        stringAppendStr(&stateStr, ".state");
        State state = getStateByStr(stateStr);
        assert(state != NO_STATE);
        if (state == STATE_DEFAULT || state == STATE_CMDLINE || state == REBOOT || state == POWEROFF) {
            /* Get script parameter array */
            scriptParams = getScriptParams(unitName, stateStr, SYML_REMOVE_OP);
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
                                             "Send func has returned %d = %s", rv, strerror(rv));
            }
            objectRelease(&buffer);
        }
        if (unitNameArg) {
            arrayRemove(*unitsDisplay, unitDisplay);
            objectRelease(&unitName);
        }
        return rv;
}

int
enableUnitServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv, len;
    Array **units, **unitsDisplay, **errors, **messages, **unitDisplayErrors, *conflicts,
          *conflictNames, *unitsConflicts, *wantedBy, *scriptParams;
    Unit *unit, *unitDisplay, *unitConflict;
    char *unitName, *buffer, *stateStr, *from, *to;
    const char *conflictName = NULL;
    bool force, run, hasError;

    rv = len = 0;
    unitName = buffer = stateStr = from = to = NULL;
    force = run = hasError = false;
    unit = unitDisplay = unitConflict = NULL;
    conflicts = conflictNames = unitsConflicts = NULL;

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

    /* Create the array */
    *unitsDisplay = arrayNew(unitRelease);

    /* try to get the unit from memory */
    unit = getUnitByName(*units, unitName);
    if (unit) {
        if (unit->enabled) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ENABLED_ERR].desc, unitName));
            goto out;
        }
        unitDisplay = unitCreate(unit, PARSE_SOCK_RESPONSE);
        arrayAdd(*unitsDisplay, unitDisplay);
    }
    else
        loadUnits(unitsDisplay, UNITS_PATH, NULL, NO_STATE, true, unitName, PARSE_SOCK_RESPONSE, true);

    /* Check if unitname exists */
    if ((*unitsDisplay)->size == 0) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_NOT_EXIST].desc, unitName));
        goto out;
    }

    assert((*unitsDisplay)->size == 1);
    unitDisplay = arrayGet(*unitsDisplay, 0);
    /* Check if it is already enabled */
    if (unitDisplay->enabled) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_ENABLED_ERR].desc, unitName));
        goto out;
    }
    /* Before the enable it, we perform the "requires" specific check.
    * Actually, this check has been already called but now, we re-execute it considerating all memory data
    * to satisfy all checks (syntax and logic errors)
    */
    unitDisplayErrors = &unitDisplay->errors;
    if (!(*unitDisplayErrors))
        *unitDisplayErrors = arrayNew(objectRelease);
    checkRequires(units, &unitDisplay, true);
    if ((*unitDisplayErrors)->size > 0) {
        *errors = arrayStrCopy(*unitDisplayErrors);
        goto out;
    }
    /* Check conflict */
    conflicts = unitDisplay->conflicts;
    len = (conflicts ? conflicts->size : 0);
    for (int i = 0; i < len; i++) {
        conflictName = arrayGet(conflicts, i);
        unitConflict = getUnitByName(*units, conflictName);
        if ((unitConflict && unitConflict->enabled) ||
            (!unitConflict && isEnabledUnit(conflictName))) {
            if (!force) {
                if (!(*errors))
                    *errors = arrayNew(objectRelease);
                arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_EXEC_ERROR].desc,
                                         unitName, conflictName));
                hasError = true;
            }
            else {
                /* If we are forcing then we must be sure that the conflict have no errors */
                if ((unitConflict && unitConflict->errors && unitConflict->errors->size > 0) ||
                    (!unitConflict && hasUnitError(conflictName))) {
                    if (!(*errors))
                        *errors = arrayNew(objectRelease);
                    arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_CONFLICT_FORCE_ERR].desc,
                                             conflictName));
                    arrayRelease(&conflictNames);
                    arrayRelease(&unitsConflicts);
                    goto out;
                }

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
        for (int i = 0; i < len; i++)
            arrayRemove(*units, arrayGet(unitsConflicts, i));
        arrayRelease(&unitsConflicts);
    }

    /* Call the script to add symlinks.
     * We always consider the default state (or cmdline ), reboot and poweroff
    */
    wantedBy = unitDisplay->wantedBy;
    len = wantedBy->size;
    for (int i = 0; i < len; ++i) {
        /* Get wanted state */
        stateStr = stringNew(arrayGet(wantedBy, i));
        stringAppendStr(&stateStr, ".state");
        State state = getStateByStr(stateStr);
        assert(state != NO_STATE);
        if (state == STATE_DEFAULT || state == STATE_CMDLINE || state == REBOOT || state == POWEROFF) {
            /* Get script parameter array */
            scriptParams = getScriptParams(unitName, stateStr, SYML_ADD_OP);
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
                                         "Send func has returned %d = %s", rv, strerror(rv));
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

    /* try to get the unit from memory */
    unit = getUnitByName(*units, unitName);
    if (unit) {
        unitDisplay = unitCreate(unit, PARSE_SOCK_RESPONSE);
        arrayAdd(*unitsDisplay, unitDisplay);
    }
    else
        loadUnits(unitsDisplay, UNITS_PATH, NULL, NO_STATE, true, unitName, PARSE_SOCK_RESPONSE, true);

    if ((*unitsDisplay)->size == 0) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_NOT_EXIST].desc, unitName));
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

    out:
        /* Marshall response */
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitDataServer::Buffer sent (%lu): \n%s",
                   strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getUnitDataServer."
                                         "Send func has returned %d = %s", rv, strerror(rv));
        }

        objectRelease(&buffer);
        return rv;
}

int
getDefaultStateServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv = 0;
    char *buffer, *msg, *destDefStateSyml, *error;
    Array **messages, **errors;
    State defaultState = NO_STATE;

    buffer = msg = destDefStateSyml = error = NULL;
    messages = errors = NULL;

    assert(sockMessageIn);
    assert(*socketFd != -1);
    errors = &(*sockMessageOut)->errors;
    *errors = arrayNew(objectRelease);
    messages = &(*sockMessageOut)->messages;
    *messages = arrayNew(objectRelease);

    if (STATE_CMDLINE == NO_STATE && STATE_DEFAULT != NO_STATE) {
        msg = stringNew("Default state : ");
        stringAppendStr(&msg, STATE_DATA_ITEMS[STATE_DEFAULT].desc);
        arrayAdd(*messages, stringNew(msg));
        objectRelease(&msg);
    }
    else if (STATE_CMDLINE != NO_STATE && STATE_DEFAULT == NO_STATE) {
        rv = getDefaultStateStr(&destDefStateSyml);
        /* Symlink missing */
        if (rv == 1) {
            objectRelease(&destDefStateSyml);
            arrayAdd(*errors, stringNew("The default state symlink is missing!"));
            goto out;
        }
        /* Not a symlink */
        else if (rv == 2) {
            objectRelease(&destDefStateSyml);
            arrayAdd(*errors, stringNew("The default state doesn't look like a symlink!"));
            goto out;
        }
        defaultState = getStateByStr(destDefStateSyml);
        /* Bad destination */
        if (defaultState == NO_STATE) {
            error = stringNew("The default state symlink points to a bad destination: ");
            stringAppendStr(&error, destDefStateSyml);
            arrayAdd(*errors, error);
            objectRelease(&destDefStateSyml);
            goto out;
        }
        objectRelease(&destDefStateSyml);

        /* Default state */
        msg = stringNew("Default state : ");
        stringAppendStr(&msg, STATE_DATA_ITEMS[defaultState].desc);
        arrayAdd(*messages, stringNew(msg));
        objectRelease(&msg);

        /* Current state */
        msg = stringNew("Current state : ");
        stringAppendStr(&msg, STATE_DATA_ITEMS[STATE_CMDLINE].desc);
        arrayAdd(*messages, stringNew(msg));
        objectRelease(&msg);
    }

    if (STATE_NEW_DEFAULT != NO_STATE) {
        msg = stringNew("New default state : ");
        stringAppendStr(&msg, STATE_DATA_ITEMS[STATE_NEW_DEFAULT].desc);
        arrayAdd(*messages, stringNew(msg));
        objectRelease(&msg);

        msg = stringNew("Warning :\n"
                        "Please, reboot or power off/halt the system\nwithout '-f' or '--force' option "
                        "to apply the change.");
        arrayAdd(*messages, stringNew(msg));
        objectRelease(&msg);
    }

    out:
        /* Marshall response */
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "GetDefaultStateServer::Buffer sent (%lu): \n%s",
                   strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getDefaultStateServer."
                                         "Send func has returned %d = %s", rv, strerror(rv));
        }

        objectRelease(&buffer);
        return rv;
}

int
setDefaultStateServer(int *socketFd, SockMessageIn *sockMessageIn, SockMessageOut **sockMessageOut)
{
    int rv = 0;
    char *buffer, *msg, *newDefaultStateStr, *destDefStateSyml;
    Array **messages, **errors;
    State newDefaultState = NO_STATE;
    State defaultState = STATE_DEFAULT;

    messages = errors = NULL;
    buffer = msg = newDefaultStateStr = NULL;

    assert(sockMessageIn);
    assert(*socketFd != -1);
    errors = &(*sockMessageOut)->errors;
    *errors = arrayNew(objectRelease);
    messages = &(*sockMessageOut)->messages;
    *messages = arrayNew(objectRelease);
    newDefaultStateStr = sockMessageIn->arg;

    newDefaultState = getStateByStr(newDefaultStateStr);
    assert(newDefaultState != NO_STATE);

    if (defaultState == NO_STATE) {
        assert(STATE_CMDLINE != NO_STATE);
        rv = getDefaultStateStr(&destDefStateSyml);
        /* Symlink missing */
        if (rv == 1) {
            objectRelease(&destDefStateSyml);
            arrayAdd(*errors, stringNew("The default state symlink is missing!"));
            goto out;
        }
        /* Not a symlink */
        else if (rv == 2) {
            objectRelease(&destDefStateSyml);
            arrayAdd(*errors, stringNew("The default state doesn't look like a symlink!"));
            goto out;
        }
        defaultState = getStateByStr(destDefStateSyml);
        objectRelease(&destDefStateSyml);
    }

    if (newDefaultState == defaultState) {
        if (STATE_NEW_DEFAULT != NO_STATE) {
            STATE_NEW_DEFAULT = NO_STATE;
            msg = stringNew("The default state has been restored to '");
            stringAppendStr(&msg, STATE_DATA_ITEMS[defaultState].desc);
            stringAppendStr(&msg, "'.");
            arrayAdd(*messages, msg);
        }
        else {
            msg = stringNew("The default state is already set to '");
            stringAppendStr(&msg, STATE_DATA_ITEMS[defaultState].desc);
            stringAppendStr(&msg, "'!");
            arrayAdd(*errors, msg);
        }
    }
    else {
        STATE_NEW_DEFAULT = newDefaultState;
        msg = stringNew("Please, reboot or power off/halt the system\n"
                        "without '-f' or '--force' option "
                        "to apply the change.");
        arrayAdd(*messages, msg);
    }

    out:
        /* Marshall response */
        buffer = marshallResponse(*sockMessageOut, PARSE_SOCK_RESPONSE);
        if (UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "GetDefaultStateServer::Buffer sent (%lu): \n%s",
                   strlen(buffer), buffer);
        /* Sending the response */
        if ((rv = send(*socketFd, buffer, strlen(buffer), 0)) == -1) {
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in socket_server::getDefaultStateServer."
                                         "Send func has returned %d = %s", rv, strerror(rv));
        }

        objectRelease(&buffer);
        return rv;
}