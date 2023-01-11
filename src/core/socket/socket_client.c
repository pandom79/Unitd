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
#define WIDTH_LAST_TIME      9
#define WIDTH_DATE          19
#define MAX_LEN_KEY         13

bool UNITCTL_DEBUG;

static int
writeUnitContent(State defaultState, const char *unitPath, const char *unitName)
{
    int rv = 0;
    FILE *fp = NULL;
    const char *propertyName = NULL;

    /* Open unit file in write mode */
    if ((fp = fopen(unitPath, "w")) == NULL) {
        logError(CONSOLE, "src/core/socket/socket_client.c",
                          "writeUnitContent", errno, strerror(errno),
                          "Unable to open (write mode) %s unit", unitPath);
        rv = 1;
        goto out;
    }

    /* UNIT SECTION */
    fprintf(fp, "%s\n", UNITS_SECTIONS_ITEMS[0].sectionName.desc);
    /* Description property */
    fprintf(fp, "%s = set the description for %s ...\n\n", UNITS_PROPERTIES_ITEMS[0].propertyName.desc,
            unitName);
    /* Requires property */
    propertyName = UNITS_PROPERTIES_ITEMS[1].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and repeatable).\n", propertyName);
    fprintf(fp, "# %s = unit name 1\n", propertyName);
    fprintf(fp, "# %s = ...\n", propertyName);
    fprintf(fp, "# %s = unit name n\n\n", propertyName);
    /* Type property */
    propertyName = UNITS_PROPERTIES_ITEMS[2].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Available values : oneshot, daemon (default).\n");
    fprintf(fp, "# %s = set the type ...\n\n", propertyName);
    /* Restart property */
    propertyName = UNITS_PROPERTIES_ITEMS[3].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Available values : true, false (default).\n");
    fprintf(fp, "# %s = set the value ...\n\n", propertyName);
    /* RestartMax property */
    propertyName = UNITS_PROPERTIES_ITEMS[4].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Accept a numeric value greater than zero.\n");
    fprintf(fp, "# If it is set then Restart property will be ignored.\n");
    fprintf(fp, "# %s = set the number ...\n\n", propertyName);
    /* Conflicts property */
    propertyName = UNITS_PROPERTIES_ITEMS[5].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and repeatable).\n", propertyName);
    fprintf(fp, "# %s = unit name 1\n", propertyName);
    fprintf(fp, "# %s = ...\n", propertyName);
    fprintf(fp, "# %s = unit name n\n\n", propertyName);

    /* COMMAND SECTION */
    fprintf(fp, "%s\n", UNITS_SECTIONS_ITEMS[1].sectionName.desc);
    /* Run property */
    fprintf(fp, "%s = set the command to run ...\n\n", UNITS_PROPERTIES_ITEMS[6].propertyName.desc);
    /* Stop property */
    propertyName = UNITS_PROPERTIES_ITEMS[7].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# %s = set the command to stop ...\n\n", propertyName);
    /* Failure property */
    propertyName = UNITS_PROPERTIES_ITEMS[8].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# %s = set the command to run on failure ...\n\n", propertyName);

    /* STATE SECTION */
    fprintf(fp, "%s\n", UNITS_SECTIONS_ITEMS[2].sectionName.desc);
    /* WantedBy property */
    propertyName = UNITS_PROPERTIES_ITEMS[9].propertyName.desc;
    if (!USER_INSTANCE) {
        fprintf(fp, "# '%s' property (required and repeatable).\n", propertyName);
        /* In this case, we don't consider the cmdline state which is an exception
         * but we set the default state instead.
         * Anyway, the user can change in whatever way wants.
        */
        for (State state = POWEROFF; state <= REBOOT; state++) {
            if (state == defaultState)
                fprintf(fp, "%s = %s\n", propertyName, STATE_DATA_ITEMS[state].desc);
            else
                fprintf(fp, "# %s = %s\n", propertyName, STATE_DATA_ITEMS[state].desc);
        }
    }
    else
        fprintf(fp, "%s = %s\n", propertyName, STATE_DATA_ITEMS[USER].desc);
    fprintf(fp, "\n");

    out:
        /* Close file */
        fclose(fp);
        fp = NULL;
        return rv;
}

static int
writeTimerContent(State defaultState, const char *unitPath, const char *unitName)
{
    int rv = 0;
    FILE *fp = NULL;
    const char *propertyName = NULL;

    /* Open unit file in write mode */
    if ((fp = fopen(unitPath, "w")) == NULL) {
        logError(CONSOLE, "src/core/socket/socket_client.c",
                          "writeTimerContent", errno, strerror(errno),
                          "Unable to open (write mode) %s unit", unitPath);
        rv = 1;
        goto out;
    }

    /* UNIT SECTION */
    fprintf(fp, "%s\n", UTIMERS_SECTIONS_ITEMS[0].sectionName.desc);
    /* Description property */
    fprintf(fp, "%s = set the description for %s ...\n\n", UTIMERS_PROPERTIES_ITEMS[0].propertyName.desc,
            unitName);
    /* Requires property */
    propertyName = UTIMERS_PROPERTIES_ITEMS[1].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and repeatable).\n", propertyName);
    fprintf(fp, "# %s = unit name 1\n", propertyName);
    fprintf(fp, "# %s = ...\n", propertyName);
    fprintf(fp, "# %s = unit name n\n\n", propertyName);
    /* Conflicts property */
    propertyName = UTIMERS_PROPERTIES_ITEMS[2].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and repeatable).\n", propertyName);
    fprintf(fp, "# %s = unit name 1\n", propertyName);
    fprintf(fp, "# %s = ...\n", propertyName);
    fprintf(fp, "# %s = unit name n\n\n", propertyName);
    /* Wake system */
    propertyName = UTIMERS_PROPERTIES_ITEMS[3].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Available values : true, false (default).\n");
    fprintf(fp, "# %s = set the value ...\n\n", propertyName);

    /* INTERVAL SECTION */
    fprintf(fp, "%s\n", UTIMERS_SECTIONS_ITEMS[1].sectionName.desc);
    /* Seconds property */
    propertyName = UTIMERS_PROPERTIES_ITEMS[4].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Accept a numeric value greater than zero.\n");
    fprintf(fp, "%s = set the seconds number ...\n\n", propertyName);
    /* Minutes property */
    propertyName = UTIMERS_PROPERTIES_ITEMS[5].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Accept a numeric value greater than zero.\n");
    fprintf(fp, "%s = set the minutes number ...\n\n", propertyName);
    /* Hours property */
    propertyName = UTIMERS_PROPERTIES_ITEMS[6].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Accept a numeric value greater than zero.\n");
    fprintf(fp, "%s = set the hours number ...\n\n", propertyName);
    /* Days property */
    propertyName = UTIMERS_PROPERTIES_ITEMS[7].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Accept a numeric value greater than zero.\n");
    fprintf(fp, "%s = set the days number ...\n\n", propertyName);
    /* Weeks property */
    propertyName = UTIMERS_PROPERTIES_ITEMS[8].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Accept a numeric value greater than zero.\n");
    fprintf(fp, "%s = set the weeks number ...\n\n", propertyName);
    /* Months property */
    propertyName = UTIMERS_PROPERTIES_ITEMS[9].propertyName.desc;
    fprintf(fp, "# '%s' property (optional and not repeatable).\n", propertyName);
    fprintf(fp, "# Accept a numeric value greater than zero.\n");
    fprintf(fp, "%s = set the months number ...\n\n", propertyName);

    /* STATE SECTION */
    fprintf(fp, "%s\n", UNITS_SECTIONS_ITEMS[2].sectionName.desc);
    /* WantedBy property */
    propertyName = UNITS_PROPERTIES_ITEMS[9].propertyName.desc;
    if (!USER_INSTANCE) {
        fprintf(fp, "# '%s' property (required and repeatable).\n", propertyName);
        /* In this case, we don't consider the cmdline state which is an exception
         * but we set the default state instead.
         * Anyway, the user can change in whatever way wants.
        */
        for (State state = POWEROFF; state <= REBOOT; state++) {
            if (state == defaultState)
                fprintf(fp, "%s = %s\n", propertyName, STATE_DATA_ITEMS[state].desc);
            else
                fprintf(fp, "# %s = %s\n", propertyName, STATE_DATA_ITEMS[state].desc);
        }
    }
    else
        fprintf(fp, "%s = %s\n", propertyName, STATE_DATA_ITEMS[USER].desc);
    fprintf(fp, "\n");

    out:
        /* Close file */
        fclose(fp);
        fp = NULL;
        return rv;
}

static char*
getLastTime(Unit *unit)
{
    char *pattern, *lastTimeStr;
    int rv = 0, finalStatus = -1;
    const char *timeStr, *finalStatusStr;
    glob_t results;
    Time *lastTime = timeNew(NULL);
    Array *entries = NULL;

    pattern = lastTimeStr = NULL;
    timeStr = finalStatusStr = NULL;

    assert(unit);

    /* Building the pattern */
    if (!USER_INSTANCE)
        pattern = stringNew(UNITD_TIMER_DATA_PATH);
    else
        pattern = stringNew(UNITD_USER_TIMER_DATA_PATH);
    stringAppendChr(&pattern, '/');
    stringAppendStr(&pattern, unit->name);
    stringAppendStr(&pattern, "|last|*");

    /* Executing the glob func */
    if ((rv = glob(pattern, 0, NULL, &results)) == 0) {
        size_t lenResults = results.gl_pathc;
        if (lenResults > 0) {
            /* Should be there only one file with this pattern. */
            if (lenResults > 1) {
                rv = 1;
                logError(UNITD_BOOT_LOG | SYSTEM, "src/core/socket/socket_client.c", "getLastTime",
                         rv, strerror(rv),
                         "Have been found '%d' files with '%s' pattern!\n",
                         lenResults, pattern);
                goto out;
            }
            const char *fileName = results.gl_pathv[0];
            assert(fileName);

            /* Split the data */
            entries = stringSplit((char *)fileName, "|", false);
            assert(entries);
            int lenEntries = entries->size;
            assert(lenEntries == 4);
            /* The third and fourth represents the last time and final status.*/
            timeStr = arrayGet(entries, 2);
            finalStatusStr = arrayGet(entries, 3);

            /* Check values */
            if (!isValidNumber(timeStr, false)) {
                rv = 1;
                logError(UNITD_BOOT_LOG | SYSTEM, "src/core/socket/socket_client.c", "getLastTime", rv,
                         strerror(rv), "The time from '%s' file is not valid!\n", fileName);
                goto out;
            }
            if (!isValidNumber(finalStatusStr, true)) {
                rv = 1;
                logError(UNITD_BOOT_LOG | SYSTEM, "src/core/socket/socket_client.c", "getLastTime", rv,
                         strerror(rv), "The final status from '%s' file is not valid!\n", fileName);
                goto out;
            }

            /* Set final status as int */
            finalStatus = atoi(finalStatusStr);

            /* Set the seconds. */
            *lastTime->sec = atol(timeStr);

            /* Set last Time */
            lastTimeStr = stringGetTimeStamp(lastTime, false, "%d-%m-%Y %H:%M:%S");
            assert(lastTimeStr);
            stringPrependStr(&lastTimeStr, finalStatus == 0 ? GREEN_COLOR : RED_COLOR);
            stringAppendStr(&lastTimeStr, DEFAULT_COLOR);
        }
    }

    out:
        globfree(&results);
        timeRelease(&lastTime);
        objectRelease(&pattern);
        arrayRelease(&entries);
        return lastTimeStr;
}

static char*
getUnitPathByName(const char *arg, bool showErr)
{
    Array *unitsDisplay = arrayNew(unitRelease);
    Array *errors = arrayNew(objectRelease);
    char *unitName, *unitPath;
    int rv = 0;

    unitName = unitPath = NULL;

    /* Unit name could contain ".unit" suffix */
    unitName = getUnitName(arg);
    if (!unitName)
        unitName = stringNew(arg);

    rv = loadAndCheckUnit(&unitsDisplay, false, unitName, false, &errors);
    if (rv != 0) {
        assert(errors->size == 1);
        if (showErr)
            logErrorStr(CONSOLE, "%s\n", (char *)arrayGet(errors, 0));
        goto out;
    }
    else {
        assert(unitsDisplay->size == 1);
        unitPath = stringNew(((Unit *)arrayGet(unitsDisplay, 0))->path);
    }

    out:
        objectRelease(&unitName);
        arrayRelease(&unitsDisplay);
        arrayRelease(&errors);

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
            if (pState == DEAD)
                printf("%s%s%s", GREY_COLOR, status, DEFAULT_COLOR);
            /* The timers can have a restarting state and a final status equal to success.
             * Read the comment in startUnitTimerThread. (Restart case).
            */
            else if (pState == RESTARTING)
                logWarning(CONSOLE, "%s", status);
            else
                logSuccess(CONSOLE, "%s", status);
            break;
        case FINAL_STATUS_FAILURE:
                logErrorStr(CONSOLE, "%s", "Failed");
            break;
        case FINAL_STATUS_READY:
            if (pState == RESTARTING)
                logWarning(CONSOLE, "%s", status);
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
            logSuccess(CONSOLE, "%d (%s)\n", exitCode, "Success");
        else if (exitCode != EXIT_SUCCESS)
            logErrorStr(CONSOLE, "%d (%s)\n", exitCode, strerror(exitCode));
    }
}

static void
printSignalNum(int signalNum)
{
    if (signalNum != -1) {
        printf("%*s ", MAX_LEN_KEY, "Signal :");
        if (signalNum != SIGSTOP && signalNum != SIGTSTP && signalNum != SIGCONT)
            logErrorStr(CONSOLE, "%d (%s)\n", signalNum, strsignal(signalNum));
        else if (signalNum == SIGSTOP || signalNum == SIGTSTP)
            logInfo(CONSOLE, "%d (%s)\n", signalNum, strsignal(signalNum));
        else if (signalNum == SIGCONT)
            logSuccess(CONSOLE, "%d (%s)\n", signalNum, strsignal(signalNum));
    }
}

static int
getMaxLen(Array *unitsDisplay, const char *param)
{
    int rv , lenUnits, len;
    Unit *unit = NULL;
    char *duration, *desc, *leftTime;

    duration = desc = leftTime = NULL;
    rv = lenUnits = len = 0;

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
            if (desc && (len = strlen(desc)) > rv)
                rv = len;
        }
        else if (strcmp(param, "leftTime") == 0) {
            leftTime = unit->leftTimeDuration;
            if (leftTime && (len = strlen(leftTime)) > rv)
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
    rv = unitdSockConn(socketConnection, &name);

    out:
        return rv;
}

int
unitdShutdown(Command command, bool force, bool noWtmp, bool noWall)
{
    assert(command != NO_COMMAND);

    /* The noWall and force options are only allowed for system instance */
    if (!USER_INSTANCE) {
        /* Write a broadcast message to all users */
        if (!noWall) {
            sendWallMsg(command);
            msleep(5000);
        }

        if (force) {
            /* Write a wtmp 'shutdown' record */
            if (!noWtmp)
                writeWtmp(false);

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
    }
    /* Not reached if we are forcing (system instance) */

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
        logError(CONSOLE, "src/core/socket/socket_client.c", "unitdShutdown",
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
getUnitList(SockMessageOut **sockMessageOut, bool bootAnalyze, ListFilter listFilter)
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
    else if (listFilter != NO_FILTER)
        arrayAdd(options, stringNew(LIST_FILTER_DATA[listFilter].desc));

    /* Get SockMessageIn struct */
    if ((rv = getSockMessageIn(&sockMessageIn, &socketConnection, LIST_COMMAND, NULL, options)) != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitsList::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        logError(CONSOLE, "src/core/socket/socket_client.c", "getUnitsList",
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
showUnitList(SockMessageOut **sockMessageOut, ListFilter listFilter)
{
    int rv, lenUnits, maxLenName, maxLenDesc, len, *finalStatus;
    Array *unitsDisplay = NULL;
    Unit *unitDisplay = NULL;
    const char *unitName, *status;
    bool enabled = false;
    pid_t *pid;
    int pfds[2];
    pid_t pidFork;

    unitName = status = NULL;
    rv = lenUnits = maxLenName = maxLenDesc = len = -1;

    /* Pipe */
    if ((rv = pipe(pfds)) < 0) {
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showUnitList", errno, strerror(errno),
                 "Pipe function returned a bad exit code");
        goto out;
    }
    /* Fork */
    pidFork = fork();
    if (pidFork < 0) {
        rv = errno;
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showUnitList", errno, strerror(errno),
                 "Fork function returned a bad exit code");
        goto out;
    }
    else if (pidFork == 0) { /* child */
        close(pfds[0]);
        dup2(pfds[1], STDOUT_FILENO);
        close(pfds[1]);

        /* Filling sockMessageOut */
        if ((rv = getUnitList(sockMessageOut, false, listFilter)) == 0) {
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

                /* Unit name
                 * Warning: don't color unit name because the bash completion fails
                */
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
                /* The restarted units require attention */
                if (unitDisplay->restartNum > 0)
                    logWarning(CONSOLE, "%s", pidStr);
                else
                    printf("%s", pidStr);
                printf("%*s", 8 - (int)strlen(pidStr) + PADDING, "");

                /* STATUS */
                finalStatus = pData->finalStatus;
                status = pStateData->desc;
                printStatus(pState, status, *finalStatus, false);
                if (*finalStatus == FINAL_STATUS_FAILURE)
                    printf("%*s", 10 - 6 + PADDING, ""); //Failed str
                else
                    printf("%*s", 10 - ((int)strlen(status)) + PADDING, ""); //Status str
                /* Description */
                printf("%s", unitDisplay->desc);
                printf("\n");
            }
            printf("\n%d units found\n", lenUnits);
        }
    }
    else { /* parent */
        char *args[] = { "less", "-dFGMXr", NULL };
        close(pfds[1]);
        dup2(pfds[0], STDIN_FILENO);
        close(pfds[0]);
        execvp(args[0], args);
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showUnitList", errno, strerror(errno),
                 "Execvp error");
        rv = 1;
        goto out;
    }

    out:
        sockMessageOutRelease(sockMessageOut);
        return rv;
}

int
showTimersList(SockMessageOut **sockMessageOut, ListFilter listFilter)
{
    int rv, lenUnits, maxLenName, maxLeftTime, len, *finalStatus;
    Array *unitsDisplay = NULL;
    Unit *unitDisplay = NULL;
    const char *unitName, *leftTime, *nextTime, *status;
    char *lasTime = NULL;
    int pfds[2];
    pid_t pidFork;

    unitName = leftTime = nextTime = status = NULL;
    rv = lenUnits = maxLenName = len = -1;

    /* Pipe */
    if ((rv = pipe(pfds)) < 0) {
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showTimersList", errno, strerror(errno),
                 "Pipe function returned a bad exit code");
        goto out;
    }
    /* Fork */
    pidFork = fork();
    if (pidFork < 0) {
        rv = errno;
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showTimersList", errno, strerror(errno),
                 "Fork function returned a bad exit code");
        goto out;
    }
    else if (pidFork == 0) { /* child */
        close(pfds[0]);
        dup2(pfds[1], STDOUT_FILENO);
        close(pfds[1]);

        /* Filling sockMessageOut */
        if ((rv = getUnitList(sockMessageOut, false, listFilter)) == 0) {
            unitsDisplay = (*sockMessageOut)->unitsDisplay;
            lenUnits = (unitsDisplay ? unitsDisplay->size : 0);

            /* Get max len */
            maxLenName = getMaxLen(unitsDisplay, "name");
            maxLeftTime = getMaxLen(unitsDisplay, "leftTime");

            if (maxLenName < WIDTH_UNIT_NAME)
                maxLenName = WIDTH_UNIT_NAME;
            if (maxLeftTime < WIDTH_LAST_TIME)
                maxLeftTime = WIDTH_LAST_TIME;

            /* HEADER */
            printf("%s%s%s", WHITE_UNDERLINE_COLOR, "UNIT NAME", DEFAULT_COLOR);
            printf("%s%*s%s", WHITE_UNDERLINE_COLOR, maxLenName - WIDTH_UNIT_NAME + PADDING, "", DEFAULT_COLOR);

            printf("%s%s%s", WHITE_UNDERLINE_COLOR, "ENABLED", DEFAULT_COLOR);
            printf("%s%*s%s", WHITE_UNDERLINE_COLOR, PADDING, "", DEFAULT_COLOR);

            printf("%s%s%s", WHITE_UNDERLINE_COLOR, "STATUS", DEFAULT_COLOR);
            printf("%s%*s%s", WHITE_UNDERLINE_COLOR, 10 - WIDTH_STATUS + PADDING, "", DEFAULT_COLOR);

            printf("%s%s%s", WHITE_UNDERLINE_COLOR, "LAST TIME", DEFAULT_COLOR);
            printf("%s%*s%s", WHITE_UNDERLINE_COLOR, WIDTH_DATE - WIDTH_LAST_TIME + PADDING, "", DEFAULT_COLOR);

            /* "next time" width is the same of "last time" */
            printf("%s%s%s", WHITE_UNDERLINE_COLOR, "NEXT TIME", DEFAULT_COLOR);
            printf("%s%*s%s", WHITE_UNDERLINE_COLOR, WIDTH_DATE - WIDTH_LAST_TIME + PADDING, "", DEFAULT_COLOR);

            /* "left time" width is the same of "last time" */
            printf("%s%s%s", WHITE_UNDERLINE_COLOR, "LEFT TIME", DEFAULT_COLOR);
            printf("%s%*s%s", WHITE_UNDERLINE_COLOR, maxLeftTime - WIDTH_LAST_TIME, "", DEFAULT_COLOR);

            printf("\n");

            /* CELLS */
            for (int i = 0; i < lenUnits; i++) {
                unitDisplay = arrayGet(unitsDisplay, i);
                ProcessData *pData = unitDisplay->processData;
                PStateData *pStateData = pData->pStateData;
                PState pState = pStateData->pState;

                /* Unit name
                 * Warning: don't color unit name because the bash completion fails
                */
                unitName = unitDisplay->name;
                printf("%s", unitName);
                len = strlen(unitName);
                if (maxLenName < len)
                    maxLenName = len;
                printf("%*s", maxLenName - len + PADDING, "");

                /* Enabled */
                bool enabled = unitDisplay->enabled;
                printf("%s", (enabled ? "true" : "false"));
                if (enabled)
                    printf("%*s", WIDTH_ENABLED - 3 + PADDING, "");
                else
                    printf("%*s", WIDTH_ENABLED - 4 + PADDING, "");

                /* Status */
                finalStatus = pData->finalStatus;
                status = pStateData->desc;
                printStatus(pState, status, *finalStatus, false);
                if (*finalStatus == FINAL_STATUS_FAILURE)
                    printf("%*s", 10 - 6 + PADDING, ""); //Failed str
                else
                    printf("%*s", 10 - ((int)strlen(status)) + PADDING, ""); //Status str

                /* Last time */
                lasTime = getLastTime(unitDisplay);
                if (lasTime) {
                    printf("%s", lasTime);
                    printf("%*s", PADDING, "");
                }
                else {
                    lasTime = stringNew("-");
                    printf("%s", lasTime);
                    printf("%*s", WIDTH_DATE - 1 + PADDING, "");
                }
                objectRelease(&lasTime);

                /* Next time */
                nextTime = unitDisplay->nextTimeDate;
                if (nextTime) {
                    printf("%s", nextTime);
                    len = strlen(nextTime);
                    printf("%*s", WIDTH_DATE - len + PADDING, "");
                }
                else {
                    printf("-");
                    printf("%*s", WIDTH_DATE - 1 + PADDING, "");
                }

                /* Left time */
                leftTime = unitDisplay->leftTimeDuration;
                if (leftTime) {
                    printf("%s", leftTime);
                    len = strlen(leftTime);
                    if (maxLeftTime < len)
                        maxLeftTime = len;
                    printf("%*s", maxLeftTime - len, "");
                }
                else {
                    printf("-");
                    printf("%*s", maxLeftTime - 1, "");
                }

                printf("\n");

            }
            printf("\n%d units found\n", lenUnits);
        }
    }
    else { /* parent */
        char *args[] = { "less", "-dFGMXr", NULL };
        close(pfds[1]);
        dup2(pfds[0], STDIN_FILENO);
        close(pfds[0]);
        execvp(args[0], args);
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showTimersList", errno, strerror(errno),
                 "Execvp error");
        rv = 1;
        goto out;
    }

    out:
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
        logError(CONSOLE, "src/core/socket/socket_client.c", "getUnitStatus",
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
    Array *sockErrors, *units, *unitErrors, *pDatasHistory, *messages;
    Unit *unit = NULL;
    ProcessData *pData, *pDataHistory;
    PStateData *pStateData, *pStateDataHistory;
    const char *status, *desc, *dateTimeStart, *dateTimeStop, *duration, *message, *interval,
               *nextTimeDate, *leftTimeDuration;
    PState pState;
    char *lastTime = NULL;
    int pfds[2];
    pid_t pidFork;

    rv = len = restartNum = lenPDataHistory = lenUnitErrors = 0;
    sockErrors = units = unitErrors = pDatasHistory = messages = NULL;
    status = desc = dateTimeStart = dateTimeStop = message = interval = nextTimeDate =
    leftTimeDuration = NULL;
    pData = pDataHistory = NULL;
    pStateData = pStateDataHistory = NULL;

    assert(unitName);

    /* Pipe */
    if ((rv = pipe(pfds)) < 0) {
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showUnitStatus", errno, strerror(errno),
                 "Pipe function returned a bad exit code");
        goto out;
    }
    /* Fork */
    pidFork = fork();
    if (pidFork < 0) {
        rv = errno;
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showUnitStatus", errno, strerror(errno),
                 "Fork function returned a bad exit code");
        goto out;
    }
    else if (pidFork == 0) { /* child */
        close(pfds[0]);
        dup2(pfds[1], STDOUT_FILENO);
        close(pfds[1]);

        if ((rv = getUnitStatus(sockMessageOut, unitName)) == 0) {

            /* Display the errors */
            sockErrors = (*sockMessageOut)->errors;
            len = (sockErrors ? sockErrors->size : 0);
            for (int i = 0; i < len; i++) {
                logErrorStr(CONSOLE, arrayGet(sockErrors, i));
                printf("\n");
            }

            /* Display the messages */
            messages = (*sockMessageOut)->messages;
            len = (messages ? messages->size : 0);
            for (int i = 0; i < len; i++) {
                message = arrayGet(messages, i);
                if (stringStartsWithStr(message, "Warning"))
                    logWarning(CONSOLE, message);
                else
                    logInfo(CONSOLE, message);
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

                /* Restartable.
                 * We show this property only if the unit is not a timer.
                */
                if (unit->type != TIMER) {
                    printf("%*s %s", MAX_LEN_KEY, "Restartable :", (unit->restart ? "true" : "false"));
                    if (unit->restartMax != -1)
                        printf(" (Max %d)", unit->restartMax);
                    printf("\n");
                }

                /* Evetual timer data for the unit */
                char *timerName = unit->timerName ? stringNew(unit->timerName) : NULL;
                if (timerName) {
                    PState *timerPState = unit->timerPState;
                    switch (*timerPState) {
                        case DEAD:
                            stringPrependStr(&timerName, GREY_COLOR);
                            break;
                        case RESTARTING:
                            stringPrependStr(&timerName, YELLOW_COLOR);
                            break;
                        case RUNNING:
                            stringPrependStr(&timerName, GREEN_COLOR);
                            break;
                        default:
                            break;
                    }
                    stringAppendStr(&timerName, DEFAULT_COLOR);
                    printf("%*s %s", MAX_LEN_KEY, "Timer :", timerName);
                    printf("\n");
                }
                objectRelease(&timerName);

                /* Timer Unit Data */
                interval = unit->intervalStr;
                if (interval && strlen(interval) > 0) {
                    printf("\n%s%s%s\n", WHITE_UNDERLINE_COLOR, "TIMER DATA", DEFAULT_COLOR);
                    /* Interval as string */
                    printf("%*s %s\n", MAX_LEN_KEY, "Interval :", interval);
                    /* Last Time */
                    lastTime = getLastTime(unit);
                    if (lastTime) {
                        printf("%*s %s \n", MAX_LEN_KEY, "Last Time :", lastTime);
                        objectRelease(&lastTime);
                    }
                    /* Next time */
                    nextTimeDate = unit->nextTimeDate;
                    if (nextTimeDate && strlen(nextTimeDate) > 0)
                        printf("%*s %s\n", MAX_LEN_KEY, "Next Time :", unit->nextTimeDate);
                    /* Left time */
                    leftTimeDuration = unit->leftTimeDuration;
                    if (leftTimeDuration && strlen(leftTimeDuration) > 0)
                        printf("%*s %s\n", MAX_LEN_KEY, "Left Time :", unit->leftTimeDuration);
                }

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
                if (dateTimeStart && strcmp(dateTimeStart, NONE) != 0)
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
                        logErrorStr(CONSOLE, "[%d] %s", j + 1, arrayGet(unitErrors, j));
                        printf("\n");
                    }
                }

                /* Process data history */
                restartNum = unit->restartNum;
                if (restartNum > 0) {
                    printf("\n%s%s%s\n", WHITE_UNDERLINE_COLOR, "PROCESS DATA HISTORY", DEFAULT_COLOR);

                    if (restartNum == 1)
                        logWarning(CONSOLE, "The unit has been restarted %d time.\n", restartNum);
                    else
                        logWarning(CONSOLE, "The unit has been restarted %d times.\n", restartNum);

                    if (restartNum > SHOW_MAX_RESULTS)
                        logWarning(CONSOLE, "Following are shown the last %d results.\n",
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
    }
    else { /* parent */
        char *args[] = { "less", "-dFGMXr", NULL };
        close(pfds[1]);
        dup2(pfds[0], STDIN_FILENO);
        close(pfds[0]);
        execvp(args[0], args);
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showUnitStatus", errno, strerror(errno),
                 "Execvp error");
        rv = 1;
        goto out;
    }

    out:
        sockMessageOutRelease(sockMessageOut);
        return rv;
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
        logError(CONSOLE, "src/core/socket/socket_client.c", "stopUnit",
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
          bool force, bool restart, bool reset)
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
    if (reset) {
        if (!options)
            options = arrayNew(objectRelease);
        arrayAdd(options, stringNew(OPTIONS_DATA[RESET_OPT].name));
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
        logError(CONSOLE, "src/core/socket/socket_client.c", "startUnit",
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
        logError(CONSOLE, "src/core/socket/socket_client.c", "disableUnit",
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
enableUnit(SockMessageOut **sockMessageOut, const char *unitName, bool force, bool run, bool reEnable)
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
    if (reEnable) {
        if (!options)
            options = arrayNew(objectRelease);
        arrayAdd(options, stringNew(OPTIONS_DATA[RE_ENABLE_OPT].name));
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
        logError(CONSOLE, "src/core/socket/socket_client.c", "enableUnit",
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
        arrayAdd(options, stringNew(OPTIONS_DATA[STATES_OPT].name));
        /* Get SockMessageIn struct */
        rv = getSockMessageIn(&sockMessageIn, &socketConnection, LIST_STATES_COMMAND, unitName, options);
    }
    if (rv != 0)
        goto out;

    /* Marshalling the request */
    bufferReq = marshallRequest(sockMessageIn);

    if (UNITCTL_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "GetUnitData::Buffer sent (%lu): \n%s", strlen(bufferReq), bufferReq);

    /* Sending the request */
    if ((rv = send(socketConnection, bufferReq, strlen(bufferReq), 0)) == -1) {
        logError(CONSOLE, "src/core/socket/socket_client.c", "getUnitData",
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
        logError(CONSOLE, "src/core/socket/socket_client.c", "getDefaultState",
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
            logErrorStr(CONSOLE, "The '%s' argument is not valid!\n", stateStr);
            logInfo(CONSOLE, "Please, use one of the following values :\n%s\n%s\n%s\n%s\n%s\n",
                                            STATE_DATA_ITEMS[SINGLE_USER].desc, STATE_DATA_ITEMS[MULTI_USER].desc,
                                            STATE_DATA_ITEMS[MULTI_USER_NET].desc, STATE_DATA_ITEMS[CUSTOM].desc,
                                            STATE_DATA_ITEMS[GRAPHICAL].desc);
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
        logError(CONSOLE, "src/core/socket/socket_client.c", "setDefaultState",
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
showData(Command command, SockMessageOut **sockMessageOut, const char *arg,
         bool force, bool restart, bool run, bool reEnable, bool reset)
{
    int rv, len, lenErrors;
    char *message, *unitPath;
    Array *sockErrors, *unitsDisplay, *unitErrors, *messages;
    rv = len = 0;
    sockErrors = unitsDisplay = unitErrors = messages = NULL;
    message = unitPath = NULL;

    switch (command) {
        case STOP_COMMAND:
            rv = stopUnit(sockMessageOut, arg);
            break;
        case START_COMMAND:
        case RESTART_COMMAND:
            rv = startUnit(sockMessageOut, arg, force, restart, reset);
            break;
        case DISABLE_COMMAND:
            rv = disableUnit(sockMessageOut, arg, run);
            break;
        case RE_ENABLE_COMMAND:
        case ENABLE_COMMAND:
            rv = enableUnit(sockMessageOut, arg, force, run, reEnable);
            break;
        case LIST_REQUIRES_COMMAND:
        case LIST_CONFLICTS_COMMAND:
        case LIST_STATES_COMMAND:
            if (command == LIST_REQUIRES_COMMAND)
                rv = getUnitData(sockMessageOut, arg, true, false, false);
            else if (command == LIST_CONFLICTS_COMMAND)
                rv = getUnitData(sockMessageOut, arg, false, true, false);
            else if (command == LIST_STATES_COMMAND)
                rv = getUnitData(sockMessageOut, arg, false, false, true);
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
            logErrorStr(CONSOLE, arrayGet(sockErrors, i));
            printf("\n");
        }
        /* Display the messages */
        messages = (*sockMessageOut)->messages;
        len = (messages ? messages->size : 0);
        for (int i = 0; i < len; i++) {
            message = arrayGet(messages, i);
            if (stringStartsWithStr(message, "Warning"))
                logWarning(CONSOLE, message);
            else
                logInfo(CONSOLE, message);
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

    unitPath = getUnitPathByName(arg, true);
    if (unitPath) {
        /* Env vars */
        Array *envVars = arrayNew(objectRelease);
        addEnvVar(&envVars, "UNITD_DATA_PATH", UNITD_DATA_PATH);
        addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
        addEnvVar(&envVars, "UNIT_PATH", unitPath);
        /* Must be null terminated */
        arrayAdd(envVars, NULL);
        /* Execute the script */
        rv = execUScript(&envVars, command == CAT_COMMAND ? "cat-unit" : "edit-unit");
        /* Release resources */
        arrayRelease(&envVars);
        objectRelease(&unitPath);
    }

    return rv;
}


int
createUnit(const char *arg)
{
    int rv = 0;
    char *unitPath, *unitName, *err, *destDefStateSyml = NULL;
    State defaultState = NO_STATE;

    unitPath = unitName = err = destDefStateSyml = NULL;

    assert(arg);

    /* Check the unit name not exists */
    unitPath = getUnitPathByName(arg, false);
    if (unitPath) {
        err = getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_EXIST_ERR].desc, arg);
        logErrorStr(CONSOLE, "%s\n", err);
        rv = 1;
        goto out;
    }

    /* Get the type */
    PType pType = getPTypeByUnitName(arg);
    assert(pType != NO_PROCESS_TYPE);

    /* Building unit path */
    unitPath = stringNew(!USER_INSTANCE ? UNITS_PATH : UNITS_USER_LOCAL_PATH);
    stringAppendChr(&unitPath, '/');
    stringAppendStr(&unitPath, arg);
    if (pType == DAEMON && !stringEndsWithStr(arg, ".unit"))
        stringAppendStr(&unitPath, ".unit");
    assert(unitPath);

    /* Get unit name */
    unitName = getUnitName(unitPath);
    assert(unitName);

    /* Get the default state for system instance */
    if (!USER_INSTANCE) {
        if (getDefaultStateStr(&destDefStateSyml) != 0) {
            rv = 1;
            goto out;
        }
        defaultState = getStateByStr(destDefStateSyml);
        if (defaultState == NO_STATE) {
            /* If we are here then the symlink points to a bad destination */
            logErrorStr(CONSOLE, "The default state symlink points to a bad destination (%s)\n",
                                 destDefStateSyml);
            rv = 1;
            goto out;
        }
    }

    switch (pType) {
        case TIMER:
            writeTimerContent(defaultState, unitPath, unitName);
            break;
        case DAEMON:
        case ONESHOT:
            writeUnitContent(defaultState, unitPath, unitName);
            break;
        default:
            break;
    }

    out:
        /* Release resources */
        objectRelease(&unitPath);
        objectRelease(&unitName);
        objectRelease(&err);
        objectRelease(&destDefStateSyml);
        return rv;
}

int
showBootAnalyze(SockMessageOut **sockMessageOut)
{
    int rv, lenUnits, maxLenName, maxLenDuration, maxLenDesc, len;
    Array *unitsDisplay, *messages;
    char *unitName, *duration, *desc;
    Unit *unitDisplay = NULL;
    int pfds[2];
    pid_t pid;

    rv = lenUnits = maxLenName = maxLenDesc = len = 0;
    unitName = duration = desc = NULL;
    unitsDisplay = messages = NULL;

    /* Pipe */
    if ((rv = pipe(pfds)) < 0) {
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showBootAnalyze", errno, strerror(errno),
                 "Pipe function returned a bad exit code");
        goto out;
    }
    /* Fork */
    pid = fork();
    if (pid < 0) {
        rv = errno;
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showBootAnalyze", errno, strerror(errno),
                 "Fork function returned a bad exit code");
        goto out;
    }
    else if (pid == 0) { /* child */
        close(pfds[0]);
        dup2(pfds[1], STDOUT_FILENO);
        close(pfds[1]);

        /* Filling sockMessageOut */
        if ((rv = getUnitList(sockMessageOut, true, NO_FILTER)) == 0) {
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
                    logSuccess(CONSOLE, "%s", duration);
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

            printf("%s%s%s", WHITE_UNDERLINE_COLOR,
                   !USER_INSTANCE ? "SYSTEM INSTANCE INFO\n" : "USER INSTANCE INFO\n",
                   DEFAULT_COLOR);
            /* Messages */
            len = (messages ? messages->size : 0);
            for (int i = 0; i < len; i++) {
                char *message = arrayGet(messages, i);
                if (stringStartsWithStr(message, "Boot"))
                    logInfo(CONSOLE, "     %s", message);
                else
                    logInfo(CONSOLE, "%s", message);
                printf("\n");
            }
        }
    }
    else { /* parent */
        char *args[] = { "less", "-dFGMXr", NULL };
        close(pfds[1]);
        dup2(pfds[0], STDIN_FILENO);
        close(pfds[0]);
        execvp(args[0], args);
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_client.c", "showBootAnalyze", errno, strerror(errno),
                 "Execvp error");
        rv = 1;
        goto out;
    }

    out:
        sockMessageOutRelease(sockMessageOut);
        return rv;
}

int
checkAdministrator(char **argv)
{
    int rv = 0;
    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "UNITD_DATA_PATH", UNITD_DATA_PATH);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);

    /* Building command */
    char *cmd = stringNew(UNITD_DATA_PATH);
    stringAppendStr(&cmd, "/scripts/administrator-check.sh");

    /* Creating script params */
    Array *scriptParams = arrayNew(objectRelease);
    arrayAdd(scriptParams, cmd); //0
    while (*argv) {
        arrayAdd(scriptParams, stringNew(*argv));
        argv++;
    }
    /* Must be null terminated */
    arrayAdd(scriptParams, NULL); //last

    /* Execute the script */
    rv = execScript(UNITD_DATA_PATH, "/scripts/administrator-check.sh", scriptParams->arr, envVars->arr);
    arrayRelease(&scriptParams);
    arrayRelease(&envVars);

    return rv;
}

