/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

#define PADDING              3
#define WIDTH               11
#define WIDTH_UNIT_NAME      9
#define WIDTH_ENABLED        6
#define WIDTH_PID            3
#define WIDTH_STATUS         6
#define WIDTH_DESCRIPTION   11
#define MAX_LEN_KEY         13

bool UNITCTL_DEBUG;

static void
printBar(int len)
{
    printf("%s%*s%s\n", WHITE_UNDERLINE_COLOR, len, "", DEFAULT_COLOR);
}

static void
printStatus(PState pState, const char *status, int finalStatus, bool newline)
{
    switch (finalStatus) {
        case FINAL_STATUS_SUCCESS:
            unitdLogSuccess(LOG_UNITD_CONSOLE, "%s", status);
            break;
        case FINAL_STATUS_FAILURE:
            if (pState == STOPPED)
                unitdLogWarning(LOG_UNITD_CONSOLE, "%s", status);
            else
                unitdLogErrorStr(LOG_UNITD_CONSOLE, "%s", "Failed");
            break;
        case FINAL_STATUS_READY:
            if (pState == RESTARTING)
                unitdLogWarning(LOG_UNITD_CONSOLE, "%s", status);
            else
                printf("%s%s%s", GREY_COLOR, status, DEFAULT_COLOR);
            break;
        default:
            break;
    }
    if (newline)
        printf("\n");
}

static void
printExitCode(int exitCode)
{
    if (exitCode != -1) {
        printf("%*s ", MAX_LEN_KEY, "Exit code :");
        if (exitCode == EXIT_SUCCESS)
            unitdLogSuccess(LOG_UNITD_CONSOLE, "%d (%s)\n", exitCode, "Success");
        else if (exitCode != EXIT_SUCCESS)
            unitdLogErrorStr(LOG_UNITD_CONSOLE, "%d (%s)\n", exitCode, strerror(exitCode));
    }
}

static void
printSignalNum(int signalNum)
{
    if (signalNum != -1) {
        printf("%*s ", MAX_LEN_KEY, "Signal :");
        if (signalNum != SIGSTOP && signalNum != SIGTSTP && signalNum != SIGCONT)
            unitdLogErrorStr(LOG_UNITD_CONSOLE, "%d (%s)\n", signalNum, strsignal(signalNum));
        else if (signalNum == SIGSTOP || signalNum == SIGTSTP)
            unitdLogWarning(LOG_UNITD_CONSOLE, "%d (%s)\n", signalNum, strsignal(signalNum));
        else if (signalNum == SIGCONT)
            unitdLogSuccess(LOG_UNITD_CONSOLE, "%d (%s)\n", signalNum, strsignal(signalNum));
    }
}

static int
getMaxUnitName(Array *unitsDisplay)
{
    int rv , lenUnits, lenName;
    rv = lenUnits = lenName = 0;

    lenUnits = (unitsDisplay ? unitsDisplay->size : 0);
    for (int i = 0; i < lenUnits; i++) {
        if ((lenName = strlen(((Unit *)arrayGet(unitsDisplay, i))->name)) > rv)
            rv = lenName;
    }
    return rv;
}

static int
getSockMessageIn(SockMessageIn **sockMessageIn, int *socketConnection, Command command,
                 const char *unitName, Array *options)
{
    int rv = 0;
    struct sockaddr_un name;

    assert(command != NO_COMMAND);

    /* Socket initialize */
    if ((*socketConnection = initSocket(&name)) == -1) {
        rv = 1;
        goto out;
    }

    /* Connect */
    if (unitdSockConn(socketConnection, &name) != 0) {
        rv = 1;
        goto out;
    }

    *sockMessageIn = sockMessageInCreate();
    (*sockMessageIn)->command = command;
    if (unitName)
        (*sockMessageIn)->unitName = stringNew(unitName);
    if (options)
        (*sockMessageIn)->options = options;

    out:
        return rv;
}

int
unitdShutdown(Command command, bool force)
{
    assert(command != NO_COMMAND);
    if (force) {
        sync();
        switch (command) {
            case REBOOT_COMMAND:
                reboot(RB_AUTOBOOT);
                break;
            case POWEROFF_COMMAND:
                reboot(RB_POWER_OFF);
                break;
            case HALT_COMMAND:
                reboot(RB_HALT_SYSTEM);
                break;
            default:
                break;
        }
    }
    /* Not reached if we are forcing */

    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection;
    char *buffer = NULL;

    rv = socketConnection = -1;

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, command, NULL, NULL)) != 0)
        goto out;

    /* Marshalling */
    buffer = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "UnitdShutdown::Buffer sent (%lu): \n%s", strlen(buffer), buffer);

    /* Sending the message */
    if ((rv = send(socketConnection, buffer, strlen(buffer), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "unitdShutdown",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* No need the response */
    out:
        objectRelease(&buffer);
        sockMessageInRelease(&sockMessageIn);
        if (socketConnection != -1)
            close(socketConnection);
        return rv;
}

int
getUnitList(SockMessageOut **sockMessageOut)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize, lenUnits, maxLen, len;
    char *bufferReq, *bufferRes;

    rv = socketConnection = lenUnits = maxLen = len = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, LIST_COMMAND, NULL, NULL)) != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitsList::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "getUnitsList",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* Read the response message */
    bufferRes = calloc(bufferSize, sizeof(char));
    assert(bufferRes);
    if ((rv = readMessage(&socketConnection, &bufferRes, &bufferSize)) == -1)
        goto out;

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitsList::Buffer received (%lu): \n%s",
                                        strlen(bufferRes), bufferRes);

    /* Unmarshall the response */
    if (!(*sockMessageOut))
        *sockMessageOut = sockMessageOutCreate();
    rv = unmarshallResponse(bufferRes, sockMessageOut);

    out:
        objectRelease(&bufferReq);
        objectRelease(&bufferRes);
        sockMessageInRelease(&sockMessageIn);
        if (socketConnection != -1)
            close(socketConnection);
        return rv;
}

int
showUnitList(SockMessageOut **sockMessageOut)
{
    int rv, lenUnits, maxLen, len, *finalStatus;
    Array *unitsDisplay = NULL;
    Unit *unitDisplay = NULL;
    const char *unitName, *status;
    bool enabled = false;
    pid_t *pid;

    unitName = status = NULL;
    rv = lenUnits = maxLen = len = -1;

    /* Filling sockMessageOut */
    if ((rv = getUnitList(sockMessageOut)) == 0) {
        unitsDisplay = (*sockMessageOut)->unitsDisplay;
        maxLen = getMaxUnitName(unitsDisplay);
        lenUnits = (unitsDisplay ? unitsDisplay->size : 0);
        if (maxLen < WIDTH_UNIT_NAME)
            maxLen = WIDTH_UNIT_NAME;
        /* HEADER */
        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "UNIT NAME", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, maxLen - WIDTH_UNIT_NAME + PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "ENABLED", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "PID", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, 8 - WIDTH_PID + PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "STATUS", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, 10 - WIDTH_STATUS + PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "DESCRIPTION", DEFAULT_COLOR);
        printf("\n");

        /* CELLS */
        for (int i = 0; i < lenUnits; i++) {
            unitDisplay = arrayGet(unitsDisplay, i);
            ProcessData *pData = unitDisplay->processData;
            PStateData *pStateData = pData->pStateData;
            PState pState = pStateData->pState;

            /* Unit name */
            unitName = unitDisplay->name;
            printf("%s", unitName);
            len = strlen(unitName);
            if (maxLen < len)
                maxLen = len;
            printf("%*s", maxLen - len + PADDING, "");

            /* Enabled */
            enabled = unitDisplay->enabled;
            printf("%s", (enabled ? "true" : "false"));
            if (enabled)
                printf("%*s", WIDTH_ENABLED - 3 + PADDING, "");
            else
                printf("%*s", WIDTH_ENABLED - 4 + PADDING, "");

            /* PID */
            pid = pData->pid;
            char pidStr[10];
            if (*pid == -1) {
                pidStr[0] = '-';
                pidStr[1] = '\0';
            }
            else
                sprintf(pidStr, "%d", *pid);
            printf("%s", pidStr);
            printf("%*s", 8 - (int)strlen(pidStr) + PADDING, "");

            /* STATUS */
            finalStatus = pData->finalStatus;
            status = pStateData->desc;
            printStatus(pState, status, *finalStatus, false);
            if (*finalStatus == FINAL_STATUS_FAILURE && pState != STOPPED)
                printf("%*s", 10 - 6 + PADDING, ""); //Failed str
            else
                printf("%*s", 10 - ((int)strlen(status)) + PADDING, ""); //Status str
            /* Description */
            printf("%s", unitDisplay->desc);
            printf("\n");
        }
        printf("\n%d units found\n", lenUnits);
    }

    sockMessageOutRelease(sockMessageOut);
    return rv;
}

int
getUnitStatus(SockMessageOut **sockMessageOut, const char *unitName)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize;
    char *bufferReq, *bufferRes;

    rv = socketConnection = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    assert(unitName);

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, STATUS_COMMAND, unitName, NULL)) != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitStatus::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "getUnitStatus",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* Read the response message */
    bufferRes = calloc(bufferSize, sizeof(char));
    assert(bufferRes);
    if ((rv = readMessage(&socketConnection, &bufferRes, &bufferSize)) == -1)
        goto out;

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitStatus::Buffer received (%lu): \n%s",
                                        strlen(bufferRes), bufferRes);

    /* Unmarshall the response */
    if (!(*sockMessageOut))
        *sockMessageOut = sockMessageOutCreate();
    rv = unmarshallResponse(bufferRes, sockMessageOut);

    out:
        objectRelease(&bufferReq);
        objectRelease(&bufferRes);
        sockMessageInRelease(&sockMessageIn);
        if (socketConnection != -1)
            close(socketConnection);
        return rv;
}

int
showUnitStatus(SockMessageOut **sockMessageOut, const char *unitName)
{
    int rv, len, *finalStatus, *exitCode, *signalNum, *pid, restartNum, lenPDataHistory,
        lenUnitErrors;
    Array *sockErrors, *units, *unitErrors, *pDatasHistory;
    Unit *unit = NULL;
    ProcessData *pData, *pDataHistory;
    PStateData *pStateData, *pStateDataHistory;
    const char *status, *desc, *dateTimeStart, *dateTimeStop;
    PState pState;

    rv = len = restartNum = lenPDataHistory = lenUnitErrors = 0;
    sockErrors = units = unitErrors = pDatasHistory = NULL;
    status = desc = dateTimeStart = dateTimeStop = NULL;
    pData = pDataHistory = NULL;
    pStateData = pStateDataHistory = NULL;

    assert(unitName);

    if ((rv = getUnitStatus(sockMessageOut, unitName)) == 0) {

        /* Display the errors */
        sockErrors = (*sockMessageOut)->errors;
        len = (sockErrors ? sockErrors->size : 0);
        for (int i = 0; i < len; i++) {
            unitdLogErrorStr(LOG_UNITD_CONSOLE, arrayGet(sockErrors, i));
            printf("\n");
        }

        /* Display the unit detail */
        units = (*sockMessageOut)->unitsDisplay;
        len = (units ? units->size : 0);
        for (int i = 0; i < len; i++) {
            unit = arrayGet(units, i);
            desc = unit->desc;
            pData = unit->processData;
            pStateData = pData->pStateData;
            pState = pStateData->pState;
            unitErrors = unit->errors;
            dateTimeStart = pData->dateTimeStart;
            dateTimeStop = pData->dateTimeStop;

            printf("%s%s%s", WHITE_UNDERLINE_COLOR, "UNIT DATA", DEFAULT_COLOR);
            /* Name */
            printf("\n%*s %s (%s)\n", MAX_LEN_KEY, "Name :", unit->name, unit->path);

            /* Description */
            if (desc)
                printf("%*s %s\n", MAX_LEN_KEY, "Description :", desc);
            /* Enabled */
            printf("%*s %s\n", MAX_LEN_KEY, "Enabled :", (unit->enabled ? "true" : "false"));
            /* Restartable */
            printf("%*s %s", MAX_LEN_KEY, "Restartable :", (unit->restart ? "true" : "false"));
            if (unit->restartMax != -1)
                printf(" (Max %d)", unit->restartMax);
            printf("\n");

            printf("\n%s%s%s\n", WHITE_UNDERLINE_COLOR, "PROCESS DATA", DEFAULT_COLOR);
            /* Process Type */
            printf("%*s %s\n", MAX_LEN_KEY, "Type :", PTYPE_DATA_ITEMS[unit->type].desc);
            if (pState != DIED) {
                /* Pid */
                pid = pData->pid;
                if (*pid != -1)
                    printf("%*s %d\n", MAX_LEN_KEY, "Pid :", *pid);
            }
            /* Status */
            finalStatus = pData->finalStatus;
            status = pStateData->desc;
            printf("%*s ", MAX_LEN_KEY, "Status :");
            printStatus(pState, status, *finalStatus, true);
            /* Date time start */
            if (strcmp(dateTimeStart, NONE) != 0)
                printf("%*s %s\n", MAX_LEN_KEY, "Started at :", dateTimeStart);
            if (pState != DIED) {
                /* Date time stop */
                if (dateTimeStop)
                    printf("%*s %s\n", MAX_LEN_KEY, "Finished at :", dateTimeStop);
                /* Exit code */
                exitCode = pData->exitCode;
                printExitCode(*exitCode);
                /* Signal num */
                signalNum = pData->signalNum;
                printSignalNum(*signalNum);
            }

            /* Unit errors */
            lenUnitErrors = (unitErrors ? unitErrors->size : 0);
            if (lenUnitErrors > 0) {
                printf("\n%s%s%s\n", WHITE_UNDERLINE_COLOR, "UNIT ERRORS", DEFAULT_COLOR);
                for (int j = 0; j < lenUnitErrors; j++) {
                    unitdLogErrorStr(LOG_UNITD_CONSOLE, "[%d] %s", j + 1, arrayGet(unitErrors, j));
                    printf("\n");
                }
            }

            /* Process data history */
            restartNum = unit->restartNum;
            if (restartNum > 0) {
                printf("\n%s%s%s", WHITE_UNDERLINE_COLOR, "PROCESS DATA HISTORY", DEFAULT_COLOR);
                if (restartNum == 1)
                    unitdLogWarning(LOG_UNITD_CONSOLE, "\nThe unit has been restarted %d time.\n", restartNum);
                else
                    unitdLogWarning(LOG_UNITD_CONSOLE, "\nThe unit has been restarted %d times.\n", restartNum);

                if (restartNum > SHOW_MAX_RESULTS)
                    unitdLogWarning(LOG_UNITD_CONSOLE, "Following are shown the last %d results.\n",
                                                        SHOW_MAX_RESULTS);
                pDatasHistory = unit->processDataHistory;
                lenPDataHistory = (pDatasHistory ? pDatasHistory->size : 0 );
                for (int j = 0; j < lenPDataHistory; j++) {
                    pDataHistory = arrayGet(pDatasHistory, j);
                    pStateDataHistory = pDataHistory->pStateData;
                    /* Number */
                    printf("[%d]\n", j + 1);
                    /* Pid */
                    pid = pDataHistory->pid;
                    if (*pid != -1)
                        printf("%*s %d\n", MAX_LEN_KEY, "Pid :", *pid);
                    /* Status */
                    finalStatus = pDataHistory->finalStatus;
                    status = pStateDataHistory->desc;
                    printf("%*s ", MAX_LEN_KEY, "Status :");
                    printStatus(pState, status, *finalStatus, true);
                    /* Date time start/stop */
                    printf("%*s %s\n", MAX_LEN_KEY, "Started at :", pDataHistory->dateTimeStart);
                    printf("%*s %s\n", MAX_LEN_KEY, "Finished at :", pDataHistory->dateTimeStop);
                    /* Exit code */
                    exitCode = pDataHistory->exitCode;
                    printExitCode(*exitCode);
                    /* Signal num */
                    signalNum = pDataHistory->signalNum;
                    printSignalNum(*signalNum);
                    printBar(40);
                }
            }
       }
    }

    sockMessageOutRelease(sockMessageOut);
    return 0;
}

int
stopUnit(SockMessageOut **sockMessageOut, const char *unitName)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize;
    char *bufferReq, *bufferRes;

    rv = socketConnection = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    assert(unitName);

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, STOP_COMMAND, unitName, NULL)) != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "StopUnit::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "stopUnit",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* Read the response message */
    bufferRes = calloc(bufferSize, sizeof(char));
    assert(bufferRes);
    if ((rv = readMessage(&socketConnection, &bufferRes, &bufferSize)) == -1)
        goto out;

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "StopUnit::Buffer received (%lu): \n%s",
                                        strlen(bufferRes), bufferRes);

    /* Unmarshall the response */
    if (!(*sockMessageOut))
        *sockMessageOut = sockMessageOutCreate();
    rv = unmarshallResponse(bufferRes, sockMessageOut);

    out:
        objectRelease(&bufferReq);
        objectRelease(&bufferRes);
        sockMessageInRelease(&sockMessageIn);
        if (socketConnection != -1)
            close(socketConnection);
        return rv;
}

int
startUnit(SockMessageOut **sockMessageOut, const char *unitName,
          bool force, bool restart)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize;
    char *bufferReq, *bufferRes;
    Array *options = NULL;

    rv = socketConnection = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    assert(unitName);

    if (force) {
        options = arrayNew(objectRelease);
        arrayAdd(options, stringNew(OPTIONS_DATA[FORCE_OPT].name));
    }
    if (restart) {
        if (!options)
            options = arrayNew(objectRelease);
        arrayAdd(options, stringNew(OPTIONS_DATA[RESTART_OPT].name));
    }

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, START_COMMAND, unitName, options)) != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "StartUnit::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "startUnit",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* Read the response message */
    bufferRes = calloc(bufferSize, sizeof(char));
    assert(bufferRes);
    if ((rv = readMessage(&socketConnection, &bufferRes, &bufferSize)) == -1)
        goto out;

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "StartUnit::Buffer received (%lu): \n%s",
                                        strlen(bufferRes), bufferRes);

    /* Unmarshall the response */
    if (!(*sockMessageOut))
        *sockMessageOut = sockMessageOutCreate();
    rv = unmarshallResponse(bufferRes, sockMessageOut);

    out:
        objectRelease(&bufferReq);
        objectRelease(&bufferRes);
        sockMessageInRelease(&sockMessageIn);
        if (socketConnection != -1)
            close(socketConnection);
        return rv;
}

int
disableUnit(SockMessageOut **sockMessageOut, const char *unitName, bool run)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize;
    char *bufferReq, *bufferRes;
    Array *options = NULL;

    rv = socketConnection = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    assert(unitName);

    if (run) {
        options = arrayNew(objectRelease);
        arrayAdd(options, stringNew(OPTIONS_DATA[RUN_OPT].name));
    }

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, DISABLE_COMMAND, unitName, options)) != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "DisableUnit::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "disableUnit",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* Read the response message */
    bufferRes = calloc(bufferSize, sizeof(char));
    assert(bufferRes);
    if ((rv = readMessage(&socketConnection, &bufferRes, &bufferSize)) == -1)
        goto out;

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "DisableUnit::Buffer received (%lu): \n%s",
                                        strlen(bufferRes), bufferRes);

    /* Unmarshall the response */
    if (!(*sockMessageOut))
        *sockMessageOut = sockMessageOutCreate();
    rv = unmarshallResponse(bufferRes, sockMessageOut);

    out:
        objectRelease(&bufferReq);
        objectRelease(&bufferRes);
        sockMessageInRelease(&sockMessageIn);
        if (socketConnection != -1)
            close(socketConnection);
        return rv;
}

int
enableUnit(SockMessageOut **sockMessageOut, const char *unitName, bool force, bool run)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize;
    char *bufferReq, *bufferRes;
    Array *options = NULL;

    rv = socketConnection = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    assert(unitName);

    if (run) {
        options = arrayNew(objectRelease);
        arrayAdd(options, stringNew(OPTIONS_DATA[RUN_OPT].name));
    }
    if (force) {
        if (!options)
            options = arrayNew(objectRelease);
        arrayAdd(options, stringNew(OPTIONS_DATA[FORCE_OPT].name));
    }

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, ENABLE_COMMAND, unitName, options)) != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "EnableUnit::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "enableUnit",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* Read the response message */
    bufferRes = calloc(bufferSize, sizeof(char));
    assert(bufferRes);
    if ((rv = readMessage(&socketConnection, &bufferRes, &bufferSize)) == -1)
        goto out;

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "EnableUnit::Buffer received (%lu): \n%s",
               strlen(bufferRes), bufferRes);

    /* Unmarshall the response */
    if (!(*sockMessageOut))
        *sockMessageOut = sockMessageOutCreate();
    rv = unmarshallResponse(bufferRes, sockMessageOut);

out:
    objectRelease(&bufferReq);
    objectRelease(&bufferRes);
    sockMessageInRelease(&sockMessageIn);
    if (socketConnection != -1)
        close(socketConnection);
    return rv;
}

int
getUnitData(SockMessageOut **sockMessageOut, const char *unitName,
            bool requires, bool conflicts, bool states)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize;
    char *bufferReq, *bufferRes;
    Array *options = arrayNew(objectRelease);

    rv = socketConnection = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    assert(unitName);

    if (requires) {
        arrayAdd(options, stringNew(OPTIONS_DATA[REQUIRES_OPT].name));
        /* Get SockMessageIn struct */
        rv = getSockMessageIn(&sockMessageIn, &socketConnection, GET_REQUIRES_COMMAND, unitName, options);
    }
    else if (conflicts) {
        arrayAdd(options, stringNew(OPTIONS_DATA[CONFLICTS_OPT].name));
        /* Get SockMessageIn struct */
        rv = getSockMessageIn(&sockMessageIn, &socketConnection, GET_CONFLICTS_COMMAND, unitName, options);
    }
    else if (states) {
        /* Get SockMessageIn struct */
        rv = getSockMessageIn(&sockMessageIn, &socketConnection, GET_STATES_COMMAND, unitName, options);
        arrayAdd(options, stringNew(OPTIONS_DATA[STATES_OPT].name));
    }
    if (rv != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitData::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "getUnitData",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* Read the response message */
    bufferRes = calloc(bufferSize, sizeof(char));
    assert(bufferRes);
    if ((rv = readMessage(&socketConnection, &bufferRes, &bufferSize)) == -1)
        goto out;

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitData::Buffer received (%lu): \n%s",
               strlen(bufferRes), bufferRes);

    /* Unmarshall the response */
    if (!(*sockMessageOut))
        *sockMessageOut = sockMessageOutCreate();
    rv = unmarshallResponse(bufferRes, sockMessageOut);

out:
    objectRelease(&bufferReq);
    objectRelease(&bufferRes);
    sockMessageInRelease(&sockMessageIn);
    if (socketConnection != -1)
        close(socketConnection);
    return rv;
}

int
getDefaultState(SockMessageOut **sockMessageOut)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize;
    char *bufferReq, *bufferRes;

    rv = socketConnection = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, GET_DEFAULT_STATE_COMMAND, NULL, NULL)) != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetDefaultState::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "getDefaultState",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* Read the response message */
    bufferRes = calloc(bufferSize, sizeof(char));
    assert(bufferRes);
    if ((rv = readMessage(&socketConnection, &bufferRes, &bufferSize)) == -1)
        goto out;

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetDefaultState::Buffer received (%lu): \n%s",
               strlen(bufferRes), bufferRes);

    /* Unmarshall the response */
    if (!(*sockMessageOut))
        *sockMessageOut = sockMessageOutCreate();
    rv = unmarshallResponse(bufferRes, sockMessageOut);

out:
    objectRelease(&bufferReq);
    objectRelease(&bufferRes);
    sockMessageInRelease(&sockMessageIn);
    if (socketConnection != -1)
        close(socketConnection);
    return rv;
}

int
showUnit(Command command, SockMessageOut **sockMessageOut, const char *unitName,
         bool force, bool restart, bool run)
{
    int rv, len, lenErrors;
    Array *sockErrors, *unitsDisplay, *unitErrors, *messages;
    rv = len = 0;
    sockErrors = unitsDisplay = unitErrors = messages = NULL;

    switch (command) {
        case STOP_COMMAND:
            rv = stopUnit(sockMessageOut, unitName);
            break;
        case START_COMMAND:
        case RESTART_COMMAND:
            rv = startUnit(sockMessageOut, unitName, force, restart);
            break;
        case DISABLE_COMMAND:
            rv = disableUnit(sockMessageOut, unitName, run);
            break;
        case ENABLE_COMMAND:
            rv = enableUnit(sockMessageOut, unitName, force, run);
            break;
        case GET_REQUIRES_COMMAND:
        case GET_CONFLICTS_COMMAND:
        case GET_STATES_COMMAND:
            if (command == GET_REQUIRES_COMMAND)
                rv = getUnitData(sockMessageOut, unitName, true, false, false);
            else if (command == GET_CONFLICTS_COMMAND)
                rv = getUnitData(sockMessageOut, unitName, false, true, false);
            else if (command == GET_STATES_COMMAND)
                rv = getUnitData(sockMessageOut, unitName, false, false, true);
            break;
        case GET_DEFAULT_STATE_COMMAND:
            rv = getDefaultState(sockMessageOut);
        default:
            break;
    }

    if (rv == 0) {
        /* Display the errors */
        sockErrors = (*sockMessageOut)->errors;
        lenErrors = (sockErrors ? sockErrors->size : 0);
        for (int i = 0; i < lenErrors; i++) {
            unitdLogErrorStr(LOG_UNITD_CONSOLE, arrayGet(sockErrors, i));
            printf("\n");
        }
        /* Display the messages */
        messages = (*sockMessageOut)->messages;
        len = (messages ? messages->size : 0);
        for (int i = 0; i < len; i++) {
            unitdLogInfo(LOG_UNITD_CONSOLE, arrayGet(messages, i));
            printf("\n");
        }
        /* If there are not errors then show the unit detail */
        if (lenErrors == 0) {
            if (command != GET_REQUIRES_COMMAND && command != GET_CONFLICTS_COMMAND && command != GET_STATES_COMMAND) {
                /* Redirect to showUnitStatus to show the unit detail */
                sockMessageOutRelease(sockMessageOut);
                showUnitStatus(sockMessageOut, unitName);
            }
        }
    }

    sockMessageOutRelease(sockMessageOut);
    return 0;
}
