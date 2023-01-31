/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../unitd_impl.h"

char *UNITD_USER_TIMER_DATA_PATH;

//INIT PARSER CONFIGURATION
enum SectionNameEnum  {
    UNIT = 0,
    INTERVAL = 1,
    STATE = 2
};

/* Properties */
enum PropertyNameEnum  {
    DESCRIPTION = 0,
    REQUIRES = 1,
    CONFLICTS = 2,
    WAKESYSTEM = 3,
    SECONDS = 4,
    MINUTES = 5,
    HOURS = 6,
    DAYS = 7,
    WEEKS = 8,
    MONTHS = 9,
    WANTEDBY = 10
};

/* Sections */
int UTIMERS_SECTIONS_ITEMS_LEN = 3;
SectionData UTIMERS_SECTIONS_ITEMS[] = {
    { { UNIT, "[Unit]" }, false, true, 0 },
    { { INTERVAL, "[Interval]" }, false, true, 0 },
    { { STATE, "[State]" }, false, true, 0 }
};

/* The accepted values for the properties */
static const char *BOOL_VALUES[] = { "false", "true", NULL };
static const char *WANTEDBY_VALUES[] = {
                                        STATE_DATA_ITEMS[SINGLE_USER].desc,
                                        STATE_DATA_ITEMS[MULTI_USER].desc,
                                        STATE_DATA_ITEMS[MULTI_USER_NET].desc,
                                        STATE_DATA_ITEMS[CUSTOM].desc,
                                        STATE_DATA_ITEMS[GRAPHICAL].desc,
                                        STATE_DATA_ITEMS[USER].desc,
                                        NULL
                                       };

int UTIMERS_PROPERTIES_ITEMS_LEN = 11;
PropertyData UTIMERS_PROPERTIES_ITEMS[] = {
    { UNIT,     { DESCRIPTION, "Description" }, false, true, false, 0, NULL, NULL },
    { UNIT,     { REQUIRES, "Requires" }, true, false, false, 0, NULL, NULL },
    { UNIT,     { CONFLICTS, "Conflict" }, true, false, false, 0, NULL, NULL },
    { UNIT,     { WAKESYSTEM, "WakeSystem" }, false, false, false, 0, BOOL_VALUES, NULL },
    { INTERVAL, { SECONDS, "Seconds" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { MINUTES, "Minutes" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { HOURS, "Hours" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { DAYS, "Days" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { WEEKS, "Weeks" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { MONTHS, "Months" }, false, false, true, 0, NULL, NULL },
    { STATE,    { WANTEDBY, "WantedBy" }, true, true, false, 0, WANTEDBY_VALUES, NULL }
};
//END PARSER CONFIGURATION

Timer*
timerNew()
{
    Timer *timer = calloc(1, sizeof(Timer));
    assert(timer);

    //Timer id
    timer_t *timerId = calloc(1, sizeof(timer_t));
    assert(timerId);
    timer->timerId = timerId;

    //Event data
    struct eventData *eventData = calloc(1, sizeof(struct eventData));
    assert(eventData);
    timer->eventData = eventData;

    //Signal event
    struct sigevent *sev = calloc(1, sizeof(struct sigevent));
    assert(sev);
    sev->sigev_value.sival_ptr = eventData;
    sev->sigev_notify = SIGEV_THREAD;
    sev->sigev_notify_function = &expired;
    timer->sev = sev;

    //Itimerspec
    struct itimerspec *its =  calloc(1, sizeof(struct itimerspec));
    assert(its);
    its->it_value.tv_sec = -1;
    timer->its = its;

    return timer;
}

void
timerRelease(Timer **timer)
{
    if (*timer) {
        objectRelease(&(*timer)->timerId);
        objectRelease(&(*timer)->eventData);
        objectRelease(&(*timer)->sev);
        objectRelease(&(*timer)->its);
        objectRelease(timer);
    }
}

void
setNextTimeDate(Unit **unit)
{
    Time *nextTime = NULL;
    char *unitName = NULL;

    assert(*unit);

    unitName = (*unit)->name;
    nextTime = (*unit)->nextTime;

    /* Set next time (Date) */
    char *nextTimeDate = stringGetTimeStamp(nextTime, false, "%d-%m-%Y %H:%M:%S");
    strcpy((*unit)->nextTimeDate, nextTimeDate);
    assert(strlen((*unit)->nextTimeDate) > 0);

    if (UNITD_DEBUG)
        logInfo(SYSTEM, "%s: next time in seconds = %lu, Date = %s\n",
                unitName, *nextTime->sec, nextTimeDate);

    objectRelease(&nextTimeDate);
}

void
setLeftTimeAndDuration(Unit **unit)
{
    Time *nextTime = NULL;
    long *leftTime = NULL;
    Time *current = timeNew(NULL);
    char *unitName = NULL;

    assert(*unit);
    unitName = (*unit)->name;
    nextTime = (*unit)->nextTime;
    leftTime = (*unit)->leftTime;
    *current->durationSec = 0;
    *current->durationMillisec = 0;

    /* Set the left time in seconds */
    *leftTime = *nextTime->sec - *current->sec;

    /* Calculating the left time as duration between next and current */
    *nextTime->durationSec = *leftTime;
    *nextTime->durationMillisec = 0;
    char *leftTimeDuration = *leftTime <= 0 ? stringNew("expired") :
                                              stringGetDiffTime(nextTime, current);
    strcpy((*unit)->leftTimeDuration, leftTimeDuration);
    assert(strlen((*unit)->leftTimeDuration) > 0);

    if (UNITD_DEBUG)
        logInfo(SYSTEM, "%s: Left time in seconds = %lu, Duration = %s\n",
                unitName, *leftTime, leftTimeDuration);

    /* Release resources */
    objectRelease(&leftTimeDuration);
    timeRelease(&current);
}

int
saveTime(Unit *unit, const char *timerUnitName, Time *currentTime, int finalStatus)
{
    char *filePath, *pattern;
    int rv = 0;

    filePath = pattern = NULL;

    /* Building file path ...*/
    if (!USER_INSTANCE)
        filePath = stringNew(UNITD_TIMER_DATA_PATH);
    else
        filePath = stringNew(UNITD_USER_TIMER_DATA_PATH);
    stringAppendChr(&filePath, '/');
    stringAppendStr(&filePath, unit ? unit->name : timerUnitName);
    stringAppendStr(&filePath, unit ? "|next|" : "|last|");

    /* Create pattern. */
    pattern = stringNew(filePath);
    stringAppendChr(&pattern, '*');

    if (unit) {
        char timeStr[50] = {0};
        sprintf(timeStr, "%lu", *unit->nextTime->sec);
        assert(strlen(timeStr) > 0);
        stringAppendStr(&filePath, timeStr);
    }
    else {
        /* Current time */
        char currentTimeStr[50] = {0};
        sprintf(currentTimeStr, "%lu", *currentTime->sec);
        assert(strlen(currentTimeStr) > 0);
        stringAppendStr(&filePath, currentTimeStr);
        stringAppendChr(&filePath, '|');
        /* Final status */
        char finalStatusStr[3] = {0};
        sprintf(finalStatusStr, "%d", finalStatus);
        assert(strlen(finalStatusStr) > 0);
        stringAppendStr(&filePath, finalStatusStr);
    }

    /* Preparing environment variables */
    Array *envVars = NULL;
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "PATTERN", pattern);
    addEnvVar(&envVars, "FILE_PATH", filePath);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);

    /* Execute the script */
    rv = execUScript(&envVars, "save-time");

    arrayRelease(&envVars);
    objectRelease(&filePath);
    objectRelease(&pattern);
    return rv;
}

int
setNextTimeFromInterval(Unit **unit)
{
    Time *nextTime = NULL;
    int rv = 0;

    assert(*unit);
    nextTime = (*unit)->nextTime;

    time_t now = time(NULL);
    struct tm *tmTime = localtime(&now);

    int seconds = (*unit)->intSeconds ? *(*unit)->intSeconds : 0;
    if (seconds > 0)
        tmTime->tm_sec += seconds;

    int minutes = (*unit)->intMinutes ? *(*unit)->intMinutes : 0;
    if (minutes > 0)
        tmTime->tm_min += minutes;

    int hours = (*unit)->intHours ? *(*unit)->intHours : 0;
    if (hours > 0)
        tmTime->tm_hour += hours;

    int days = (*unit)->intDays ? *(*unit)->intDays : 0;
    if (days > 0)
        tmTime->tm_mday += days;

    int weeks = (*unit)->intWeeks ? *(*unit)->intWeeks : 0;
    if (weeks > 0)
        tmTime->tm_mday += weeks * 7;

    int months = (*unit)->intMonths ? *(*unit)->intMonths : 0;
    if (months > 0)
        tmTime->tm_mon += months;

    time_t start = mktime(tmTime);

    /* Set the next time */
    *nextTime->sec = start;

    /* Set next time (Date) */
    setNextTimeDate(unit);

    /* Set left time and duration */
    setLeftTimeAndDuration(unit);

    return rv;
}

int
setNextTimeFromDisk(Unit **unit)
{
    glob_t results;
    char *pattern, *timeStr;
    int rv = 0;
    Time *nextTime = NULL;
    const char *unitName = NULL;

    assert(*unit);
    pattern = timeStr = NULL;
    unitName = (*unit)->name;
    nextTime = (*unit)->nextTime;

    /* Building the pattern */
    if (!USER_INSTANCE)
        pattern = stringNew(UNITD_TIMER_DATA_PATH);
    else
        pattern = stringNew(UNITD_USER_TIMER_DATA_PATH);
    stringAppendChr(&pattern, '/');
    stringAppendStr(&pattern, unitName);
    stringAppendStr(&pattern, "|next*");

    /* Executing the glob func */
    if ((rv = glob(pattern, 0, NULL, &results)) == 0) {
        size_t lenResults = results.gl_pathc;
        if (lenResults > 0) {
            /* Should be there only one file with this pattern. */
            if (lenResults > 1) {
                rv = 1;
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "setNextTimeFromDisk",
                         rv, strerror(rv),
                         "Have been found '%d' files with '%s' pattern! Regenerate the next file.\n",
                         lenResults, pattern);
                goto out;
            }
            const char *fileName = results.gl_pathv[0];
            assert(fileName);
            /* Get the next time */
            int index = stringLastIndexOfChr(fileName, '|');
            timeStr = stringSub(fileName, index + 1, strlen(fileName) - 1);
            if (!isValidNumber(timeStr, false)) {
                rv = 1;
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "setNextTimeFromDisk", rv,
                         strerror(rv), "The time from '%s' file is not valid!\n", fileName);
                goto out;
            }
            else {
                /* Set the next time */
                *nextTime->sec = atol(timeStr);

                /* Set next time (Date) */
                setNextTimeDate(unit);

                /* Set left time and duration */
                setLeftTimeAndDuration(unit);
            }
        }
    }

    out:
        objectRelease(&pattern);
        objectRelease(&timeStr);
        globfree(&results);
        return rv;
}

int
checkInterval(Unit **unit)
{
    int rv = 0;
    char *interval, *intervalStr;
    int *intSeconds, *intMinutes, *intHours, *intDays, *intWeeks, *intMonths;

    interval = intervalStr = NULL;
    intSeconds = intMinutes = intHours = intDays = intWeeks = intMonths = NULL;

    assert(*unit);

    intervalStr = (*unit)->intervalStr;
    intSeconds = (*unit)->intSeconds;
    intMinutes = (*unit)->intMinutes;
    intHours = (*unit)->intHours;
    intDays = (*unit)->intDays;
    intWeeks = (*unit)->intWeeks;
    intMonths = (*unit)->intMonths;

    if ((!intSeconds || *intSeconds == -1 ) &&
        (!intMinutes  || *intMinutes == -1) &&
        (!intHours || *intHours == -1) &&
        (!intDays || *intDays == -1) &&
        (!intWeeks || *intWeeks == -1) &&
        (!intMonths || *intMonths == -1)) {
        rv = 1;
        arrayAdd((*unit)->errors,
                 getMsg(-1, UNITS_ERRORS_ITEMS[UTIMER_INTERVAL_ERR].desc));
    }
    else {
        /* Building the interval like an unique string. */
        interval = stringNew("");
        /* Check months */
        if (intMonths && *intMonths > 0) {
            char months[10] = {0};
            sprintf(months, "%d", *intMonths);
            assert(strlen(months) > 0);
            stringAppendStr(&interval, months);
            stringAppendStr(&interval, "M ");
        }
        /* Check weeks */
        if (intWeeks && *intWeeks > 0) {
            char weeks[10] = {0};
            sprintf(weeks, "%d", *intWeeks);
            assert(strlen(weeks) > 0);
            stringAppendStr(&interval, weeks);
            stringAppendStr(&interval, "w ");
        }
        /* Check days */
        if (intDays && *intDays > 0) {
            char days[10] = {0};
            sprintf(days, "%d", *intDays);
            assert(strlen(days) > 0);
            stringAppendStr(&interval, days);
            stringAppendStr(&interval, "d ");
        }
        /* Check hours */
        if (intHours && *intHours > 0) {
            char hours[10] = {0};
            sprintf(hours, "%d", *intHours);
            assert(strlen(hours) > 0);
            stringAppendStr(&interval, hours);
            stringAppendStr(&interval, "h ");
        }
        /* Check minutes */
        if (intMinutes && *intMinutes > 0) {
            char minutes[10] = {0};
            sprintf(minutes, "%d", *intMinutes);
            assert(strlen(minutes) > 0);
            stringAppendStr(&interval, minutes);
            stringAppendStr(&interval, "m ");
        }
        /* Check seconds */
        if (intSeconds && *intSeconds > 0) {
            char seconds[10] = {0};
            sprintf(seconds, "%d", *intSeconds);
            assert(strlen(seconds) > 0);
            stringAppendStr(&interval, seconds);
            stringAppendStr(&interval, "s ");
        }

        /* Trim and copy */
        stringTrim(interval, NULL);
        strcpy(intervalStr, interval);
        assert(strlen(intervalStr) > 0);
    }

    objectRelease(&interval);
    return rv;
}

int
parseTimerUnit(Array **units, Unit **unit, bool isAggregate) {

    FILE *fp = NULL;
    int rv, numLine, sizeErrs;
    size_t len = 0;
    char *line, *error, *value, *unitPath, *dep, *conflict;
    Array *lineData, **errors, *requires, *conflicts, *wantedBy;
    PropertyData *propertyData = NULL;

    numLine = rv = sizeErrs = 0;
    line = error = value = unitPath = dep = conflict = NULL;
    lineData = requires = conflicts = wantedBy = NULL;

    assert(*unit);
    /* Initialize the parser */
    parserInit(PARSE_TIMER_UNIT);
    /* Initialize the Unit */
    errors = &(*unit)->errors;
    if (!(*errors))
        *errors = arrayNew(objectRelease);
    requires = arrayNew(objectRelease);
    conflicts = arrayNew(objectRelease);
    wantedBy = arrayNew(objectRelease);
    (*unit)->requires = requires;
    (*unit)->conflicts = conflicts;
    (*unit)->wantedBy = wantedBy;
    unitPath = (*unit)->path;

    /* Initialize the specific timer values */
    (*unit)->timer = timerNew();
    (*unit)->nextTime = timeNew(NULL);
    (*unit)->wakeSystem = NULL;

    long *leftTime = calloc(1, sizeof(long));
    assert(leftTime);
    *leftTime = -1;
    (*unit)->leftTime = leftTime;

    char *nextTimeDate = calloc(50, sizeof(char));
    assert(nextTimeDate);
    (*unit)->nextTimeDate = nextTimeDate;

    char *leftTimeDuration = calloc(50, sizeof(char));
    assert(leftTimeDuration);
    (*unit)->leftTimeDuration = leftTimeDuration;

    char *intervalStr = calloc(50, sizeof(char));
    assert(intervalStr);
    (*unit)->intervalStr = intervalStr;

    /* Some repeatable properties require the duplicate value check.
     * Just set their pointers in the PROPERTIES_ITEM array.
     * Optional.
    */
    UTIMERS_PROPERTIES_ITEMS[REQUIRES].notDupValues = requires;
    UTIMERS_PROPERTIES_ITEMS[CONFLICTS].notDupValues = conflicts;
    UTIMERS_PROPERTIES_ITEMS[WANTEDBY].notDupValues = wantedBy;

    /* Open the file */
    if ((fp = fopen(unitPath, "r")) == NULL) {
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNABLE_OPEN_UNIT_ERR].desc, unitPath));
        rv = 1;
        return rv;
    }

    while (getline(&line, &len, fp) != -1) {
        numLine++;
        /* Parsing the line */
        rv = parseLine(line, numLine, &lineData, &propertyData);
        /* lineData[0] -> Key   (Required)
         * lineData[1] -> Value (Optional: NULL in section case)
         * lineData[2] -> Error (Optional)
        */
        if (lineData) {
            if ((error = arrayGet(lineData, 2))) {
                assert(rv != 0);
                arrayAdd(*errors, stringNew(error));
                /* The error string is allocated even if the stringSplit
                 * has been called with 'false' argument
                */
                if (!isAggregate)
                    break;
                else {
                    arrayRelease(&lineData);
                    continue;
                }
            }
            else {
                if ((value = arrayGet(lineData, 1))) {
                    switch (propertyData->propertyName.propertyNameEnum) {
                    case DESCRIPTION:
                        (*unit)->desc = stringNew(value);
                        break;
                    case REQUIRES:
                        dep = stringNew(value);
                        arrayAdd(requires, dep);
                        if ((*errors)->size == 0 || isAggregate)
                             checkRequires(units, unit, isAggregate);
                        break;
                    case CONFLICTS:
                        conflict = stringNew(value);
                        arrayAdd(conflicts, conflict);
                        if ((*errors)->size == 0 || isAggregate)
                            checkConflicts(unit, value, isAggregate);
                        break;
                    case WAKESYSTEM:
                        if (strcmp(value, BOOL_VALUES[true]) == 0) {
                            (*unit)->wakeSystem = calloc(1, sizeof(bool));
                            assert((*unit)->wakeSystem);
                            *(*unit)->wakeSystem = true;
                        }
                        break;
                    case SECONDS:
                        (*unit)->intSeconds = calloc(1, sizeof(int));
                        assert((*unit)->intSeconds);
                        *(*unit)->intSeconds = atoi(value);
                        break;
                    case MINUTES:
                        (*unit)->intMinutes = calloc(1, sizeof(int));
                        assert((*unit)->intMinutes);
                        *(*unit)->intMinutes = atoi(value);
                        break;
                    case HOURS:
                        (*unit)->intHours = calloc(1, sizeof(int));
                        assert((*unit)->intHours);
                        *(*unit)->intHours = atoi(value);
                        break;
                    case DAYS:
                        (*unit)->intDays = calloc(1, sizeof(int));
                        assert((*unit)->intDays);
                        *(*unit)->intDays = atoi(value);
                        break;
                    case WEEKS:
                        (*unit)->intWeeks = calloc(1, sizeof(int));
                        assert((*unit)->intWeeks);
                        *(*unit)->intWeeks = atoi(value);
                        break;
                    case MONTHS:
                        (*unit)->intMonths = calloc(1, sizeof(int));
                        assert((*unit)->intMonths);
                        *(*unit)->intMonths = atoi(value);
                        break;
                    case WANTEDBY:
                        arrayAdd(wantedBy, stringNew(value));
                        break;
                    }
                }
            }
            arrayRelease(&lineData);
        }
    }
    /* Parser end */
    parserEnd(errors, isAggregate);
    /* Check the error's size */
    if ((sizeErrs = (*errors)->size) > 0) {
        if (!isAggregate) {
            /* At most we can have two errors because the not valid symlink error is always aggregated.
             * See loadUnits func
            */
            assert(sizeErrs == 1 || sizeErrs == 2);
        }
        else
            assert(sizeErrs > 0);
        rv = 1;
    }

    arrayRelease(&lineData);
    objectRelease(&line);
    fclose(fp);
    fp = NULL;
    return rv;
}

void
expired(union sigval timerData)
{
    int output = 1, rv = 0;
    const char *timerUnitName = NULL;
    Array *units = UNITD_DATA->units;

    struct eventData *data = timerData.sival_ptr;
    assert(data);

    timerUnitName = data->timerUnitName;
    Unit *timerUnit = getUnitByName(units, timerUnitName);
    assert(timerUnit);

    /* We communicate to the timer unit pipe to restart the timer. */
    if (SHUTDOWN_COMMAND == NO_COMMAND)
    if ((rv = uWrite(timerUnit->pipe->fds[1], &output, sizeof(int))) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "expired",
                      errno, strerror(errno), "Unable to write into pipe for the %s unit", timerUnitName);
        kill(UNITD_PID, SIGTERM);
    }
}

void
armTimer(Unit *unit)
{
    int rv = 0;
    Timer *timer = NULL;
    const char *unitName = NULL;
    bool wakeSystem = false;

    assert(unit);
    unitName = unit->name;

    /* Get timer data */
    wakeSystem = unit->wakeSystem && *unit->wakeSystem ? true : false;
    timer = unit->timer;
    timer->eventData->timerUnitName = unitName;
    timer->its->it_value.tv_sec = *unit->leftTime;

    /* Create timer */
    while (true) {
        rv = timer_create(wakeSystem ? CLOCK_BOOTTIME_ALARM : CLOCK_BOOTTIME,
                          timer->sev, timer->timerId);
        if (rv == -1) {
            if (errno == EAGAIN)
                continue;
            else {
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "armTimer", errno,
                         strerror(errno), "Unable to create the timer for '%s'",
                         unitName);
                kill(UNITD_PID, SIGTERM);
                break;
            }
        }
        break;
    }
    /* Start timer */
    rv = timer_settime(*timer->timerId, 0, timer->its, NULL);
    if (rv == -1) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "armTimer", errno,
                 strerror(errno), "Unable to start the timer for '%s'",
                 unitName);
        kill(UNITD_PID, SIGTERM);
    }
}

void
disarmTimer(Unit *unit)
{
    Timer *timer = NULL;
    int rv = 0;

    assert(unit);
    timer = unit->timer;

    if (*timer->timerId != 0 &&
       (rv = timer_delete(*timer->timerId)) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "disarmTimer", errno,
                 strerror(errno), "Unable to delete the timer for '%s'",
                 unit->name);
        kill(UNITD_PID, SIGTERM);
    }
    *timer->timerId = 0;
}

void*
startTimerUnitThread(void *arg)
{
    Unit *unit = NULL;
    const char *unitName = NULL;
    int rv, input;
    Pipe *unitPipe = NULL;
    long *leftTime = NULL;

    rv = input = 0;

    assert(arg);
    unit = (Unit *)arg;
    unitName = unit->name;
    unitPipe = unit->pipe;
    leftTime = unit->leftTime;

    if ((rv = pthread_mutex_lock(unitPipe->mutex)) != 0) {
         logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread",
                  rv, strerror(rv), "Unable to acquire the lock of the pipe mutex for '%s'",
                  unitName);
         kill(UNITD_PID, SIGTERM);
    }

    /* Before to start, we wait for system is up.
     * We check if ctrl+alt+del is pressed as well.
    */
    while (!LISTEN_SOCK_REQUEST && SHUTDOWN_COMMAND == NO_COMMAND)
        msleep(50);
    if (SHUTDOWN_COMMAND != NO_COMMAND)
        goto out;

    /* Try to get the persistent "nextTime" */
    rv = setNextTimeFromDisk(&unit);
    if (rv != 0 || *leftTime <= 0) {
        if (rv == 0) {
            if (UNITD_DEBUG)
                logInfo(SYSTEM, "%s: the persistent 'nextTime' exists but it is expired.",
                                unitName);
            /* Unit execution management. */
            if (SHUTDOWN_COMMAND != NO_COMMAND || (rv = executeUnit(unit, TIMER)) == EUIDOWN) {
                logWarning(SYSTEM, "Shutting down the unitd instance. Skipped '%s' execution.",
                           unitName);
                goto out;
            }
        }

        if (UNITD_DEBUG)
            logInfo(SYSTEM, "%s: generating the 'nextTime' by interval ...", unitName);
        setNextTimeFromInterval(&unit);
        assert(unit->nextTime && *leftTime != -1);
        if ((rv = saveTime(unit, NULL, NULL, -1)) != 0) {
            kill(UNITD_PID, SIGTERM);
            goto out;
        }
    }

    /* Arming the timer */
    armTimer(unit);

    /* Listening the pipe */
    while (true) {
        if ((rv = uRead(unitPipe->fds[0], &input, sizeof(int))) == -1) {
            logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread", errno,
                     strerror(errno), "Unable to read from pipe for '%s'",
                     unitName);
            kill(UNITD_PID, SIGTERM);
            goto out;
        }
        if (input == 1) {
            /* The left time is expired!
             * Lock and unlock the unit mutex to simulate the same
             * behaviour of listenPipeThread() func when the processes restart.
             * An eventual unit status request will show the restarted or completed data.
            */

            disarmTimer(unit);

            if ((rv = pthread_mutex_lock(unit->mutex)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread",
                         rv, strerror(rv), "Unable to lock the mutex (restart timer)",
                         unitName);
                kill(UNITD_PID, SIGTERM);
            }

            /* Reset data for restarting.
             * Unlike the units, the restarting of the timers is considerated like a success.
             * For this reason, we don't set a final status different by SUCCESS.
             * In this way, the requires check is satisfied as well.
            */
            *unit->processData->pStateData = PSTATE_DATA_ITEMS[RESTARTING];
            strcpy(unit->nextTimeDate, "-");
            assert(strlen(unit->nextTimeDate) == 1);
            strcpy(unit->leftTimeDuration, "-");
            assert(strlen(unit->leftTimeDuration) == 1);

            /* Unlock */
            if ((rv = pthread_mutex_unlock(unit->mutex)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread", rv,
                         strerror(rv), "Unable to unlock the mutex (restart timer)",
                         unitName);
                kill(UNITD_PID, SIGTERM);
            }

            /* Lock */
            if ((rv = pthread_mutex_lock(unit->mutex)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread",
                         rv, strerror(rv), "Unable to lock the mutex (restart timer before start)",
                         unitName);
                kill(UNITD_PID, SIGTERM);
            }

            /* Executing the unit ... */
            if (SHUTDOWN_COMMAND != NO_COMMAND || (rv = executeUnit(unit, TIMER)) == EUIDOWN)
                logWarning(SYSTEM, "Shutting down the unitd instance. Skipped '%s' execution.",
                           unitName);
            else {
                setNextTimeFromInterval(&unit);
                assert(unit->nextTime && *leftTime != -1);
                if ((rv = saveTime(unit, NULL, NULL, -1)) != 0)
                    kill(UNITD_PID, SIGTERM);

                /* Now, we arm the timer because the unit execution could be take some seconds. */
                armTimer(unit);
            }

            *unit->processData->pStateData = PSTATE_DATA_ITEMS[RUNNING];

            /* Unlock */
            if ((rv = pthread_mutex_unlock(unit->mutex)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread", rv,
                         strerror(rv), "Unable to unlock the mutex (restart timer after start)",
                         unitName);
                kill(UNITD_PID, SIGTERM);
            }
        }
        else if (input == THREAD_EXIT)
            goto out;
    }

    out:
        disarmTimer(unit);
        if ((rv = pthread_mutex_unlock(unitPipe->mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread",
                      rv, strerror(rv), "Unable to unlock the pipe mutex for the %s unit",
                      unitName);
            kill(UNITD_PID, SIGTERM);
        }
        pthread_exit(0);
}

int
startTimerUnit(Unit *unit)
{
    pthread_t thread;
    pthread_attr_t attr;
    const char *unitName = NULL;
    int rv = 0;

    assert(unit);
    unitName = unit->name;

    if ((rv = pthread_attr_init(&attr)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnit", errno,
                      strerror(errno), "pthread_attr_init returned bad exit code %d", rv);
        kill(UNITD_PID, SIGTERM);
    }

    if ((rv = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnit", errno,
                      strerror(errno), "pthread_attr_setdetachstate returned bad exit code %d",
                 rv);
        kill(UNITD_PID, SIGTERM);
    }

    *unit->processData->pStateData = PSTATE_DATA_ITEMS[RUNNING];

    if ((rv = pthread_create(&thread, &attr, startTimerUnitThread, unit)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnit",
                 rv, strerror(rv), "Unable to create the unit timer thread for '%s'", unitName);
        kill(UNITD_PID, SIGTERM);
    }
    else {
        if (UNITD_DEBUG)
            logInfo(SYSTEM, "unit timer thread created successfully for '%s'\n", unitName);
    }

    pthread_attr_destroy(&attr);
    return rv;
}

static int
checkResponse(SockMessageOut *sockMessageOut)
{
    Array *sockErrors, *unitErrors, *unitsDisplay;
    int rv, sockErrSize, unitErrSize;
    Unit *unitDisplay = NULL;

    rv = sockErrSize = unitErrSize = 0;
    sockErrors = unitErrors = unitsDisplay = NULL;

    assert(sockMessageOut);

    sockErrors = sockMessageOut->errors;
    sockErrSize = sockErrors ? sockErrors->size : 0;
    if (sockErrSize > 0) {
        logErrorStr(SYSTEM, "Failed with the following errors:");
        for (int i = 0; i < sockErrSize; i++)
            logErrorStr(SYSTEM, "%s", arrayGet(sockErrors, i));
        rv = 1;
    }
    else {
        unitsDisplay = sockMessageOut->unitsDisplay;
        unitDisplay = arrayGet(unitsDisplay, 0);
        assert(unitDisplay && unitsDisplay->size == 1);
        unitErrors = unitDisplay->errors;
        unitErrSize = unitErrors ? unitErrors->size : 0;
        if (unitErrSize > 0) {
            logErrorStr(SYSTEM, "Failed with the following errors:");
            for (int i = 0; i < unitErrSize; i++)
                logErrorStr(SYSTEM, "%s", arrayGet(unitErrors, i));
            rv = 1;
        }
    }

    return rv;
}

int
executeUnit(Unit *otherUnit, PType pType)
{
    char *unitName = NULL;
    const char *otherUnitName = NULL;
    int rv = 0, socketConnection = 1;
    SockMessageOut *sockMessageOut = sockMessageOutNew();
    SockMessageIn *sockMessageIn = sockMessageInNew();
    Time *now = timeNew(NULL);
    Array *units = UNITD_DATA->units;
    Array *options = arrayNew(objectRelease);

    assert(otherUnit);
    otherUnitName = otherUnit->name;

    /* Get unit name by other */
    unitName = getUnitNameByOther(otherUnitName, pType);

    /* Populate sockMessageIn */
    sockMessageIn->arg = unitName;
    sockMessageIn->command = START_COMMAND;
    sockMessageIn->options = options;
    /* Add restart option. */
    arrayAdd(options, stringNew(OPTIONS_DATA[RESTART_OPT].name));

    /* Try to get unit from memory */
    Unit *unit = getUnitByName(units, unitName);
    if (unit) {
        if (unit->type == DAEMON && unit->processData->pStateData->pState == RUNNING) {
            logInfo(SYSTEM, "%s: '%s' is already running. Skipped!", otherUnitName, unitName);
            goto out;
        }
    }

    logInfo(SYSTEM, "%s: Starting '%s' ...", otherUnitName, unitName);

    /* If we are shutting down the instance then exit with a custom exit code.(EUIDOWN) */
    if (SHUTDOWN_COMMAND != NO_COMMAND) {
        rv = EUIDOWN;
        goto out;
    }

    /* Restart unit */
    rv = startUnitServer(&socketConnection, sockMessageIn, &sockMessageOut, true, true);
    if (rv == 0 && (rv = checkResponse(sockMessageOut)) == 0) {
        /* Get unit from memory */
        Unit *unit = getUnitByName(units, unitName);
        if (unit) {
            int finalStatus = *unit->processData->finalStatus;
            if (UNITD_DEBUG)
                logInfo(SYSTEM, "%s: Final status for '%s' = %d ...", otherUnitName, unitName,
                        finalStatus);
            if (finalStatus == FINAL_STATUS_SUCCESS) {
                logSuccess(SYSTEM, "%s: '%s' executed successfully.", otherUnitName, unitName);
                rv = 0;
            }
            else
                rv = 1;
        }
    }
    if (rv != 0)
        logErrorStr(SYSTEM, "%s: Unable to start '%s'!", otherUnitName, unitName);

    out:
        /* Save the result on disk only if we are not shutting down the instance. */
        if (pType == TIMER && rv != EUIDOWN)
            saveTime(NULL, otherUnitName, now, rv);
        timeRelease(&now);
        sockMessageOutRelease(&sockMessageOut);
        sockMessageInRelease(&sockMessageIn);
        return rv;
}

int
resetNextTime(const char *timerName)
{
    int rv = 0;
    char *pattern = NULL;
    Array *envVars = NULL;

    /* Building pattern ...*/
    if (!USER_INSTANCE)
        pattern = stringNew(UNITD_TIMER_DATA_PATH);
    else
        pattern = stringNew(UNITD_USER_TIMER_DATA_PATH);
    stringAppendChr(&pattern, '/');
    stringAppendStr(&pattern, timerName);
    stringAppendStr(&pattern, "|next|*");

    /* Preparing environment variables */
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "PATTERN", pattern);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);

    /* Execute the script */
    rv = execUScript(&envVars, "remove");

    arrayRelease(&envVars);
    objectRelease(&pattern);
    return rv;
}
