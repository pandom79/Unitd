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
#define WIDTH_DURATION       8
#define MAX_LEN_KEY         13

bool UNITCTL_DEBUG;

static char*
getUnitPathByName(const char *arg)
{
    FILE *fp = NULL;
    char *unitPath = stringNew(UNITS_PATH);
    stringAppendChr(&unitPath, '/');
    stringAppendStr(&unitPath, arg);
    if (!stringEndsWithStr(arg, ".unit"))
        stringAppendStr(&unitPath, ".unit");

    if ((fp = fopen(unitPath, "r")) == NULL) {
        unitdLogErrorStr(LOG_UNITD_CONSOLE, UNITS_ERRORS_ITEMS[UNIT_NOT_EXIST].desc, arg);
        printf("\n");
        objectRelease(&unitPath);
    }
    else {
        /* Close file */
        fclose(fp);
        fp = NULL;
    }

    return unitPath;
}

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
getMaxLen(Array *unitsDisplay, const char *param)
{
    int rv , lenUnits, len;
    rv = lenUnits = len = 0;
    Unit *unit = NULL;
    char *duration, *desc;

    duration = desc = NULL;

    lenUnits = (unitsDisplay ? unitsDisplay->size : 0);
    for (int i = 0; i < lenUnits; i++) {
        unit = arrayGet(unitsDisplay, i);
        if (strcmp(param, "name") == 0) {
            if ((len = strlen(unit->name)) > rv)
                rv = len;
        }
        else if (strcmp(param, "duration") == 0) {
            duration = unit->processData->duration;
            if (duration && (len = strlen(duration)))
                rv = len;
        }
        else if (strcmp(param, "desc") == 0) {
            desc = unit->desc;
            if (desc && (len = strlen(unit->desc)) > rv)
                rv = len;
        }
    }
    return rv;
}

static int
getSockMessageIn(SockMessageIn **sockMessageIn, int *socketConnection, Command command,
                 const char *arg, Array *options)
{
    int rv = 0;
    struct sockaddr_un name;

    assert(command != NO_COMMAND);

    *sockMessageIn = sockMessageInNew();
    (*sockMessageIn)->command = command;
    if (arg)
        (*sockMessageIn)->arg = stringNew(arg);
    if (options)
        (*sockMessageIn)->options = options;

    /* Socket initialize */
    if ((*socketConnection = initSocket(&name)) == -1) {
        rv = 1;
        goto out;
    }

    /* Connect */
    if (unitdSockConn(socketConnection, &name) != 0)
        rv = 1;

    out:
        return rv;
}

int
unitdShutdown(Command command, bool force, bool noWtmp, bool noWall)
{
    assert(command != NO_COMMAND);

    /* Write a broadcast message to all users */
    if (!noWall)
        sendWallMsg(command);

    if (force) {
         /* Write a wtmp 'shutdown' record */
        if (!noWtmp && writeWtmp(false) != 0)
            unitdLogErrorStr(LOG_UNITD_CONSOLE, "An error has occurred in writeWtmp!\n");

#ifndef UNITD_TEST
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
            case KEXEC_COMMAND:
                reboot(RB_KEXEC);
                break;
            default:
                break;
        }
#endif
    }
    /* Not reached if we are forcing */

    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection;
    char *buffer = NULL;
    Array *options = NULL;

    rv = socketConnection = -1;

    if (noWtmp) {
        options = arrayNew(objectRelease);
        arrayAdd(options, stringNew(OPTIONS_DATA[NO_WTMP_OPT].name));
    }

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, command, NULL, options)) != 0)
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
getUnitList(SockMessageOut **sockMessageOut, bool bootAnalyze)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize, lenUnits, maxLen, len;
    char *bufferReq, *bufferRes;
    Array *options = arrayNew(objectRelease);

    rv = socketConnection = lenUnits = maxLen = len = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    if (bootAnalyze)
        arrayAdd(options, stringNew(OPTIONS_DATA[ANALYZE_OPT].name));

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, LIST_COMMAND, NULL, options)) != 0)
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
        *sockMessageOut = sockMessageOutNew();
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
    int rv, lenUnits, maxLenName, maxLenDesc, len, *finalStatus;
    Array *unitsDisplay = NULL;
    Unit *unitDisplay = NULL;
    const char *unitName, *status;
    bool enabled = false;
    pid_t *pid;

    unitName = status = NULL;
    rv = lenUnits = maxLenName = maxLenDesc = len = -1;

    /* Filling sockMessageOut */
    if ((rv = getUnitList(sockMessageOut, false)) == 0) {
        unitsDisplay = (*sockMessageOut)->unitsDisplay;
        maxLenName = getMaxLen(unitsDisplay, "name");
        maxLenDesc = getMaxLen(unitsDisplay, "desc");
        lenUnits = (unitsDisplay ? unitsDisplay->size : 0);
        if (maxLenName < WIDTH_UNIT_NAME)
            maxLenName = WIDTH_UNIT_NAME;
        if (maxLenDesc < WIDTH_DESCRIPTION)
            maxLenDesc = WIDTH_DESCRIPTION;
        /* HEADER */
        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "UNIT NAME", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, maxLenName - WIDTH_UNIT_NAME + PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "ENABLED", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "PID", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, 8 - WIDTH_PID + PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "STATUS", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, 10 - WIDTH_STATUS + PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "DESCRIPTION", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, maxLenDesc - WIDTH_DESCRIPTION, "", DEFAULT_COLOR);
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
            if (maxLenName < len)
                maxLenName = len;
            printf("%*s", maxLenName - len + PADDING, "");

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
        *sockMessageOut = sockMessageOutNew();
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
    const char *status, *desc, *dateTimeStart, *dateTimeStop, *duration;
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
            dateTimeStart = pData->dateTimeStartStr;
            dateTimeStop = pData->dateTimeStopStr;
            duration = pData->duration;

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
            /* Pid */
            pid = pData->pid;
            if (*pid != -1)
                printf("%*s %d\n", MAX_LEN_KEY, "Pid :", *pid);
            /* Status */
            finalStatus = pData->finalStatus;
            status = pStateData->desc;
            printf("%*s ", MAX_LEN_KEY, "Status :");
            printStatus(pState, status, *finalStatus, true);
            /* Date time start */
            if (strcmp(dateTimeStart, NONE) != 0)
                printf("%*s %s\n", MAX_LEN_KEY, "Started at :", dateTimeStart);
            /* Date time stop */
            if (dateTimeStop)
                printf("%*s %s\n", MAX_LEN_KEY, "Finished at :", dateTimeStop);
            if (pState != DEAD) {
                /* Exit code */
                exitCode = pData->exitCode;
                printExitCode(*exitCode);
                /* Signal num */
                signalNum = pData->signalNum;
                printSignalNum(*signalNum);
            }

            lenUnitErrors = (unitErrors ? unitErrors->size : 0);
            /* Duration */
            if (lenUnitErrors == 0 && duration)
                printf("%*s %s\n", MAX_LEN_KEY, "Duration :", duration);
            /* Unit errors */
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
                    printf("%*s %s\n", MAX_LEN_KEY, "Started at :", pDataHistory->dateTimeStartStr);
                    printf("%*s %s\n", MAX_LEN_KEY, "Finished at :", pDataHistory->dateTimeStopStr);
                    printf("%*s %s\n", MAX_LEN_KEY, "Duration :", pDataHistory->duration);
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
        *sockMessageOut = sockMessageOutNew();
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
        *sockMessageOut = sockMessageOutNew();
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
        *sockMessageOut = sockMessageOutNew();
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
        *sockMessageOut = sockMessageOutNew();
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
        rv = getSockMessageIn(&sockMessageIn, &socketConnection, LIST_REQUIRES_COMMAND, unitName, options);
    }
    else if (conflicts) {
        arrayAdd(options, stringNew(OPTIONS_DATA[CONFLICTS_OPT].name));
        /* Get SockMessageIn struct */
        rv = getSockMessageIn(&sockMessageIn, &socketConnection, LIST_CONFLICTS_COMMAND, unitName, options);
    }
    else if (states) {
        /* Get SockMessageIn struct */
        rv = getSockMessageIn(&sockMessageIn, &socketConnection, LIST_STATES_COMMAND, unitName, options);
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
        *sockMessageOut = sockMessageOutNew();
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
        *sockMessageOut = sockMessageOutNew();
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
setDefaultState(SockMessageOut **sockMessageOut, const char *stateStr)
{
    SockMessageIn *sockMessageIn = NULL;
    int rv, socketConnection, bufferSize;
    char *bufferReq, *bufferRes, *defaultStateStr;
    State defaultState = NO_STATE;

    rv = socketConnection = -1;
    bufferReq = bufferRes = NULL;
    bufferSize = INITIAL_SIZE;

    defaultStateStr = stringNew(stateStr);
    stringReplaceAllStr(&defaultStateStr, ".state", "");
    defaultState = getStateByStr(defaultStateStr);
    switch (defaultState) {
        case NO_STATE:
        case INIT:
        case POWEROFF:
        case REBOOT:
        case FINAL:
            unitdLogErrorStr(LOG_UNITD_CONSOLE, "The '%s' argument is not valid!\n", stateStr);
            unitdLogInfo(LOG_UNITD_CONSOLE, "Please, use one of the following values :\n"
                                            "single-user\nmulti-user\nmulti-user-net\ncustom\ngraphical\n");
            rv = 1;
            goto out;
        default:
            break;
    }

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, SET_DEFAULT_STATE_COMMAND, defaultStateStr, NULL)) != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "SetDefaultState::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_client.c", "setDefaultState",
                      errno, strerror(errno), "Send error");
        goto out;
    }

    /* Read the response message */
    bufferRes = calloc(bufferSize, sizeof(char));
    assert(bufferRes);
    if ((rv = readMessage(&socketConnection, &bufferRes, &bufferSize)) == -1)
        goto out;

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "SetDefaultState::Buffer received (%lu): \n%s",
               strlen(bufferRes), bufferRes);

    /* Unmarshall the response */
    if (!(*sockMessageOut))
        *sockMessageOut = sockMessageOutNew();
    rv = unmarshallResponse(bufferRes, sockMessageOut);

    out:
        objectRelease(&bufferReq);
        objectRelease(&bufferRes);
        objectRelease(&defaultStateStr);
        sockMessageInRelease(&sockMessageIn);
        if (socketConnection != -1)
            close(socketConnection);
        return rv;
}

int
showUnit(Command command, SockMessageOut **sockMessageOut, const char *arg,
         bool force, bool restart, bool run)
{
    int rv, len, lenErrors;
    char *message, *unitPath;
    Array *sockErrors, *unitsDisplay, *unitErrors, *messages;
    rv = len = 0;
    sockErrors = unitsDisplay = unitErrors = messages = NULL;
    message = unitPath = NULL;

    switch (command) {
        case STOP_COMMAND:
            if ((unitPath = getUnitPathByName(arg)))
                rv = stopUnit(sockMessageOut, arg);
            else rv = 1;
            break;
        case START_COMMAND:
        case RESTART_COMMAND:
            if ((unitPath = getUnitPathByName(arg)))
                rv = startUnit(sockMessageOut, arg, force, restart);
            else rv = 1;
            break;
        case DISABLE_COMMAND:
            if ((unitPath = getUnitPathByName(arg)))
                rv = disableUnit(sockMessageOut, arg, run);
            else rv = 1;
            break;
        case ENABLE_COMMAND:
            if ((unitPath = getUnitPathByName(arg)))
                rv = enableUnit(sockMessageOut, arg, force, run);
            else rv = 1;
            break;
        case LIST_REQUIRES_COMMAND:
        case LIST_CONFLICTS_COMMAND:
        case LIST_STATES_COMMAND:
            if ((unitPath = getUnitPathByName(arg))) {
                if (command == LIST_REQUIRES_COMMAND)
                    rv = getUnitData(sockMessageOut, arg, true, false, false);
                else if (command == LIST_CONFLICTS_COMMAND)
                    rv = getUnitData(sockMessageOut, arg, false, true, false);
                else if (command == LIST_STATES_COMMAND)
                    rv = getUnitData(sockMessageOut, arg, false, false, true);
            }
            else rv = 1;
            break;
        case GET_DEFAULT_STATE_COMMAND:
            rv = getDefaultState(sockMessageOut);
            break;
        case SET_DEFAULT_STATE_COMMAND:
            rv = setDefaultState(sockMessageOut, arg);
            break;
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
            message = arrayGet(messages, i);
            if (stringStartsWithStr(message, "Warning"))
                unitdLogWarning(LOG_UNITD_CONSOLE, message);
            else
                unitdLogInfo(LOG_UNITD_CONSOLE, message);
            printf("\n");
        }
        /* If there are not errors then show the unit detail */
        if (lenErrors == 0) {
            if (command == START_COMMAND || command == RESTART_COMMAND ||
                command == STOP_COMMAND || run) {
                /* Redirect to showUnitStatus to show the unit detail */
                sockMessageOutRelease(sockMessageOut);
                showUnitStatus(sockMessageOut, arg);
            }
        }
    }

    objectRelease(&unitPath);
    sockMessageOutRelease(sockMessageOut);
    return 0;
}

int
catEditUnit(Command command, const char *arg)
{
    int rv = 0;
    char *unitPath = NULL;

    assert(command != NO_COMMAND);
    assert(arg);

    unitPath = getUnitPathByName(arg);
    if (unitPath) {
        /* Env vars */
        Array *envVars = arrayNew(objectRelease);
        addEnvVar(&envVars, "UNITD_DATA_PATH", UNITD_DATA_PATH);
        /* Must be null terminated */
        arrayAdd(envVars, NULL);

        /* Building command */
        Array *scriptParams = arrayNew(objectRelease);
        char *cmd = stringNew(UNITD_DATA_PATH);
        stringAppendStr(&cmd, "/scripts/cat-edit.sh");
        arrayAdd(scriptParams, cmd); //0
        arrayAdd(scriptParams, stringNew(COMMANDS_DATA[command].name)); //1
        arrayAdd(scriptParams, stringNew(unitPath)); //2
        /* Must be null terminated */
        arrayAdd(scriptParams, NULL); //4
        /* Execute the script */
        rv = execScript(UNITD_DATA_PATH, "/scripts/cat-edit.sh", scriptParams->arr, envVars->arr);
        arrayRelease(&scriptParams);
        arrayRelease(&envVars);
        objectRelease(&unitPath);
    }

    return rv;
}

int
showBootAnalyze(SockMessageOut **sockMessageOut)
{
    int rv, lenUnits, maxLenName, maxLenDuration, maxLenDesc, len;
    Array *unitsDisplay, *messages;
    char *unitName, *duration, *desc;
    Unit *unitDisplay = NULL;

    rv = lenUnits = maxLenName = maxLenDesc = len = 0;
    unitName = duration = desc = NULL;
    unitsDisplay = messages = NULL;

    /* Filling sockMessageOut */
    if ((rv = getUnitList(sockMessageOut, true)) == 0) {
        unitsDisplay = (*sockMessageOut)->unitsDisplay;
        messages = (*sockMessageOut)->messages;
        maxLenName = getMaxLen(unitsDisplay, "name");
        maxLenDuration = getMaxLen(unitsDisplay, "duration");
        maxLenDesc = getMaxLen(unitsDisplay, "desc");
        lenUnits = (unitsDisplay ? unitsDisplay->size : 0);
        if (maxLenName < WIDTH_UNIT_NAME)
            maxLenName = WIDTH_UNIT_NAME;
        if (maxLenDuration < WIDTH_DURATION)
            maxLenDuration = WIDTH_DURATION;
        if (maxLenDesc < WIDTH_DESCRIPTION)
            maxLenDesc = WIDTH_DESCRIPTION;
        /* HEADER */
        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "UNIT NAME", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, maxLenName - WIDTH_UNIT_NAME + PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "DURATION", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, maxLenDuration - WIDTH_DURATION + PADDING, "", DEFAULT_COLOR);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "DESCRIPTION", DEFAULT_COLOR);
        printf("%s%*s%s", WHITE_UNDERLINE_COLOR, maxLenDesc - WIDTH_DESCRIPTION, "", DEFAULT_COLOR);

        printf("\n");

        /* CELLS */
        for (int i = 0; i < lenUnits; i++) {
            unitDisplay = arrayGet(unitsDisplay, i);

            //Unit name
            unitName = unitDisplay->name;
            printf("%s", unitName);
            len = strlen(unitName);
            if (maxLenName < len)
                maxLenName = len;
            printf("%*s", maxLenName - len + PADDING, "");

            //Duration
            duration = unitDisplay->processData->duration;
            if (duration)
                unitdLogSuccess(LOG_UNITD_CONSOLE, "%s", duration);
            else
                printf("-");
            len = (duration ? strlen(duration) : 1);
            if (maxLenDuration < len)
                maxLenDuration = len;
            printf("%*s", maxLenDuration - len + PADDING, "");

            /* Description */
            desc = unitDisplay->desc;
            if (desc)
                printf("%s", desc);
            else
                printf("-");
            len = (desc ? strlen(desc) : 1);
            if (maxLenDesc < len)
                maxLenDesc = len;

            printf("\n");
        }
        printf("\n%d units found\n\n", lenUnits);

        printf("%s%s%s", WHITE_UNDERLINE_COLOR, "SYSTEM INFO\n", DEFAULT_COLOR);
        /* Messages */
        len = (messages ? messages->size : 0);
        for (int i = 0; i < len; i++) {
            char *message = arrayGet(messages, i);
            if (stringStartsWithStr(message, "Boot"))
                unitdLogInfo(LOG_UNITD_CONSOLE, "     %s", message);
            else
                unitdLogInfo(LOG_UNITD_CONSOLE, "%s", message);
            printf("\n");
        }

    }

    sockMessageOutRelease(sockMessageOut);
    return rv;
}
