/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../unitd_impl.h"

char *UNITD_USER_TIMER_DATA_PATH;

//INIT PARSER CONFIGURATION
enum SectionNameEnum { UNIT = 0, INTERVAL = 1, STATE = 2 };
enum PropertyNameEnum {
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
int UTIMERS_SECTIONS_ITEMS_LEN = 3;
SectionData UTIMERS_SECTIONS_ITEMS[] = { { { UNIT, "[Unit]" }, false, true, 0 },
                                         { { INTERVAL, "[Interval]" }, false, true, 0 },
                                         { { STATE, "[State]" }, false, true, 0 } };
static const char *BOOL_VALUES[] = { "false", "true", NULL };
static const char *WANTEDBY_VALUES[] = { STATE_DATA_ITEMS[SINGLE_USER].desc,
                                         STATE_DATA_ITEMS[MULTI_USER].desc,
                                         STATE_DATA_ITEMS[MULTI_USER_NET].desc,
                                         STATE_DATA_ITEMS[CUSTOM].desc,
                                         STATE_DATA_ITEMS[GRAPHICAL].desc,
                                         STATE_DATA_ITEMS[USER].desc,
                                         NULL };
int UTIMERS_PROPERTIES_ITEMS_LEN = 11;
PropertyData UTIMERS_PROPERTIES_ITEMS[] = {
    { UNIT, { DESCRIPTION, "Description" }, false, true, false, 0, NULL, NULL },
    { UNIT, { REQUIRES, "Requires" }, true, false, false, 0, NULL, NULL },
    { UNIT, { CONFLICTS, "Conflict" }, true, false, false, 0, NULL, NULL },
    { UNIT, { WAKESYSTEM, "WakeSystem" }, false, false, false, 0, BOOL_VALUES, NULL },
    { INTERVAL, { SECONDS, "Seconds" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { MINUTES, "Minutes" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { HOURS, "Hours" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { DAYS, "Days" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { WEEKS, "Weeks" }, false, false, true, 0, NULL, NULL },
    { INTERVAL, { MONTHS, "Months" }, false, false, true, 0, NULL, NULL },
    { STATE, { WANTEDBY, "WantedBy" }, true, true, false, 0, WANTEDBY_VALUES, NULL }
};
//END PARSER CONFIGURATION

Timer *timerNew()
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
    struct itimerspec *its = calloc(1, sizeof(struct itimerspec));
    assert(its);
    its->it_value.tv_sec = -1;
    timer->its = its;

    return timer;
}

void timerRelease(Timer **timer)
{
    if (*timer) {
        objectRelease(&(*timer)->timerId);
        objectRelease(&(*timer)->eventData);
        objectRelease(&(*timer)->sev);
        objectRelease(&(*timer)->its);
        objectRelease(timer);
    }
}

void setNextTimeDate(Unit **unit)
{
    Time *nextTime = NULL;
    char *unitName = NULL;

    assert(*unit);

    unitName = (*unit)->name;
    nextTime = (*unit)->nextTime;
    /* Set next time (Date) */
    char *nextTimeDate = stringGetTimeStamp(nextTime, false, "%d-%m-%Y %H:%M:%S");
    stringCopy((*unit)->nextTimeDate, nextTimeDate);
    if (DEBUG)
        logInfo(SYSTEM, "%s: next time in seconds = %lu, Date = %s\n", unitName, *nextTime->sec,
                nextTimeDate);

    objectRelease(&nextTimeDate);
}

void setLeftTimeAndDuration(Unit **unit)
{
    Time *nextTime = NULL, *current = timeNew(NULL);
    long *leftTime = NULL;
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
    stringCopy((*unit)->leftTimeDuration, leftTimeDuration);
    if (DEBUG)
        logInfo(SYSTEM, "%s: Left time in seconds = %lu, Duration = %s\n", unitName, *leftTime,
                leftTimeDuration);

    objectRelease(&leftTimeDuration);
    timeRelease(&current);
}

int saveTime(Unit *unit, const char *timerUnitName, Time *currentTime, int finalStatus)
{
    char *filePath = NULL, *pattern = NULL;
    int rv = 0;

    if (!USER_INSTANCE)
        filePath = stringNew(UNITD_TIMER_DATA_PATH);
    else
        filePath = stringNew(UNITD_USER_TIMER_DATA_PATH);
    stringAppendChr(&filePath, '/');
    stringAppendStr(&filePath, unit ? unit->name : timerUnitName);
    stringAppendStr(&filePath, unit ? "|next|" : "|last|");
    pattern = stringNew(filePath);
    stringAppendChr(&pattern, '*');
    if (unit) {
        char timeStr[50] = { 0 };
        sprintf(timeStr, "%lu", *unit->nextTime->sec);
        stringAppendStr(&filePath, timeStr);
    } else {
        /* Current time */
        char currentTimeStr[50] = { 0 };
        sprintf(currentTimeStr, "%lu", *currentTime->sec);
        stringAppendStr(&filePath, currentTimeStr);
        stringAppendChr(&filePath, '|');
        /* Final status */
        char finalStatusStr[3] = { 0 };
        sprintf(finalStatusStr, "%d", finalStatus);
        stringAppendStr(&filePath, finalStatusStr);
    }
    Array *envVars = NULL;
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "PATTERN", pattern);
    addEnvVar(&envVars, "FILE_PATH", filePath);
    arrayAdd(envVars, NULL);
    rv = execUScript(&envVars, "save-time");

    arrayRelease(&envVars);
    objectRelease(&filePath);
    objectRelease(&pattern);
    return rv;
}

int setNextTimeFromInterval(Unit **unit)
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
    *nextTime->sec = start;
    setNextTimeDate(unit);
    setLeftTimeAndDuration(unit);

    return rv;
}

int setNextTimeFromDisk(Unit **unit)
{
    glob_t results;
    char *pattern = NULL, *timeStr = NULL;
    int rv = 0;
    Time *nextTime = NULL;
    const char *unitName = NULL;

    assert(*unit);

    unitName = (*unit)->name;
    nextTime = (*unit)->nextTime;
    if (!USER_INSTANCE)
        pattern = stringNew(UNITD_TIMER_DATA_PATH);
    else
        pattern = stringNew(UNITD_USER_TIMER_DATA_PATH);
    stringAppendChr(&pattern, '/');
    stringAppendStr(&pattern, unitName);
    stringAppendStr(&pattern, "|next*");
    if ((rv = glob(pattern, 0, NULL, &results)) == 0) {
        size_t lenResults = results.gl_pathc;
        if (lenResults > 0) {
            /* There should be only one file with this pattern. */
            if (lenResults > 1) {
                rv = 1;
                logError(
                    CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "setNextTimeFromDisk", rv,
                    strerror(rv),
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
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c",
                         "setNextTimeFromDisk", rv, strerror(rv),
                         "The time from '%s' file is not valid!\n", fileName);
                goto out;
            } else {
                /* Set the next time */
                *nextTime->sec = atol(timeStr);
                setNextTimeDate(unit);
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

int checkInterval(Unit **unit)
{
    int rv = 0;
    char *interval = NULL, *intervalStr = NULL;
    int *intSeconds = NULL, *intMinutes = NULL, *intHours = NULL, *intDays = NULL, *intWeeks = NULL,
        *intMonths = NULL;

    assert(*unit);

    intervalStr = (*unit)->intervalStr;
    intSeconds = (*unit)->intSeconds;
    intMinutes = (*unit)->intMinutes;
    intHours = (*unit)->intHours;
    intDays = (*unit)->intDays;
    intWeeks = (*unit)->intWeeks;
    intMonths = (*unit)->intMonths;
    if ((!intSeconds || *intSeconds == -1) && (!intMinutes || *intMinutes == -1) &&
        (!intHours || *intHours == -1) && (!intDays || *intDays == -1) &&
        (!intWeeks || *intWeeks == -1) && (!intMonths || *intMonths == -1)) {
        rv = 1;
        arrayAdd((*unit)->errors, getMsg(-1, UNITS_ERRORS_ITEMS[UTIMER_INTERVAL_ERR].desc));
    } else {
        /* Building the interval like an unique string. */
        interval = stringNew("");
        /* Check months */
        if (intMonths && *intMonths > 0) {
            char months[10] = { 0 };
            sprintf(months, "%d", *intMonths);
            stringAppendStr(&interval, months);
            stringAppendStr(&interval, "M ");
        }
        /* Check weeks */
        if (intWeeks && *intWeeks > 0) {
            char weeks[10] = { 0 };
            sprintf(weeks, "%d", *intWeeks);
            stringAppendStr(&interval, weeks);
            stringAppendStr(&interval, "w ");
        }
        /* Check days */
        if (intDays && *intDays > 0) {
            char days[10] = { 0 };
            sprintf(days, "%d", *intDays);
            stringAppendStr(&interval, days);
            stringAppendStr(&interval, "d ");
        }
        /* Check hours */
        if (intHours && *intHours > 0) {
            char hours[10] = { 0 };
            sprintf(hours, "%d", *intHours);
            stringAppendStr(&interval, hours);
            stringAppendStr(&interval, "h ");
        }
        /* Check minutes */
        if (intMinutes && *intMinutes > 0) {
            char minutes[10] = { 0 };
            sprintf(minutes, "%d", *intMinutes);
            stringAppendStr(&interval, minutes);
            stringAppendStr(&interval, "m ");
        }
        /* Check seconds */
        if (intSeconds && *intSeconds > 0) {
            char seconds[10] = { 0 };
            sprintf(seconds, "%d", *intSeconds);
            stringAppendStr(&interval, seconds);
            stringAppendStr(&interval, "s ");
        }
        /* Trim and copy */
        stringTrim(interval, NULL);
        stringCopy(intervalStr, interval);
    }

    objectRelease(&interval);
    return rv;
}

int parseTimerUnit(Array **units, Unit **unit, bool isAggregate)
{
    FILE *fp = NULL;
    int rv = 0, numLine = 0, sizeErrs = 0;
    size_t len = 0;
    char *line = NULL, *error = NULL, *value = NULL, *unitPath = NULL, *dep = NULL,
         *conflict = NULL;
    Array *lineData = NULL, **errors, *requires = NULL, *conflicts = NULL, *wantedBy = NULL;
    PropertyData *propertyData = NULL;
    SectionData *sectionData = NULL;

    assert(*unit);

    parserInit(UTIMERS_SECTIONS_ITEMS_LEN, UTIMERS_SECTIONS_ITEMS, UTIMERS_PROPERTIES_ITEMS_LEN,
               UTIMERS_PROPERTIES_ITEMS);
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
    // Left time
    long *leftTime = calloc(1, sizeof(long));
    assert(leftTime);
    *leftTime = -1;
    (*unit)->leftTime = leftTime;
    // Next time
    char *nextTimeDate = calloc(50, sizeof(char));
    assert(nextTimeDate);
    (*unit)->nextTimeDate = nextTimeDate;
    // Left time (duration)
    char *leftTimeDuration = calloc(50, sizeof(char));
    assert(leftTimeDuration);
    (*unit)->leftTimeDuration = leftTimeDuration;
    // Interval as string
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
    if ((fp = fopen(unitPath, "r")) == NULL) {
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNABLE_OPEN_UNIT_ERR].desc, unitPath));
        rv = 1;
        return rv;
    }
    while (getline(&line, &len, fp) != -1) {
        numLine++;
        rv = parseLine(line, numLine, &lineData, &sectionData, &propertyData);
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
            } else {
                if ((value = arrayGet(lineData, 1))) {
                    switch (propertyData->property.id) {
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
                        if (stringEquals(value, BOOL_VALUES[true])) {
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
    parserEnd(errors, isAggregate);
    /* Check the error's size */
    if ((sizeErrs = (*errors)->size) > 0) {
        if (!isAggregate) {
            /* At most we can have two errors because the not valid symlink error is always aggregated.
             * See loadUnits func
            */
            assert(sizeErrs == 1 || sizeErrs == 2);
        } else
            assert(sizeErrs > 0);
        rv = 1;
    }

    arrayRelease(&lineData);
    objectRelease(&line);
    fclose(fp);
    fp = NULL;
    return rv;
}

void expired(union sigval timerData)
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
            logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "expired", errno,
                     strerror(errno), "Unable to write into pipe for the %s unit", timerUnitName);
            kill(UNITD_PID, SIGTERM);
        }
}

void armTimer(Unit *unit)
{
    int rv = 0;
    Timer *timer = NULL;
    const char *unitName = NULL;
    bool wakeSystem = false;

    assert(unit);

    unitName = unit->name;
    wakeSystem = unit->wakeSystem && *unit->wakeSystem ? true : false;
    timer = unit->timer;
    timer->eventData->timerUnitName = unitName;
    timer->its->it_value.tv_sec = *unit->leftTime;
    while (true) {
        rv = timer_create(wakeSystem ? CLOCK_BOOTTIME_ALARM : CLOCK_BOOTTIME, timer->sev,
                          timer->timerId);
        if (rv == -1) {
            if (errno == EAGAIN)
                continue;
            else {
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "armTimer", errno,
                         strerror(errno), "Unable to create the timer for '%s'", unitName);
                kill(UNITD_PID, SIGTERM);
                break;
            }
        }
        break;
    }
    rv = timer_settime(*timer->timerId, 0, timer->its, NULL);
    if (rv == -1) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "armTimer", errno,
                 strerror(errno), "Unable to start the timer for '%s'", unitName);
        kill(UNITD_PID, SIGTERM);
    }
}

void disarmTimer(Unit *unit)
{
    Timer *timer = NULL;
    int rv = 0;

    assert(unit);

    timer = unit->timer;
    if (*timer->timerId != 0 && (rv = timer_delete(*timer->timerId)) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "disarmTimer", errno,
                 strerror(errno), "Unable to delete the timer for '%s'", unit->name);
        kill(UNITD_PID, SIGTERM);
    }
    *timer->timerId = 0;
}

void *startTimerUnitThread(void *arg)
{
    Unit *unit = NULL;
    const char *unitName = NULL;
    int rv = 0, input = 0;
    Pipe *unitPipe = NULL;
    long *leftTime = NULL;

    assert(arg);

    unit = (Unit *)arg;
    unitName = unit->name;
    unitPipe = unit->pipe;
    leftTime = unit->leftTime;
    if ((rv = pthread_mutex_lock(unitPipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread", rv,
                 strerror(rv), "Unable to acquire the lock of the pipe mutex for '%s'", unitName);
        kill(UNITD_PID, SIGTERM);
    }
    while (!LISTEN_SOCK_REQUEST && SHUTDOWN_COMMAND == NO_COMMAND)
        msleep(50);
    if (SHUTDOWN_COMMAND != NO_COMMAND)
        goto out;
    /* Try to get the persistent "nextTime" */
    rv = setNextTimeFromDisk(&unit);
    if (rv != 0 || *leftTime <= 0) {
        if (rv == 0) {
            if (DEBUG)
                logInfo(SYSTEM, "%s: the persistent 'nextTime' exists but it is expired.",
                        unitName);
            if (SHUTDOWN_COMMAND != NO_COMMAND || (rv = executeUnit(unit, TIMER)) == EUIDOWN) {
                logWarning(SYSTEM, "Shutting down the unitd instance. Skipped '%s' execution.",
                           unitName);
                goto out;
            }
        }
        if (DEBUG)
            logInfo(SYSTEM, "%s: generating the 'nextTime' by interval ...", unitName);
        setNextTimeFromInterval(&unit);
        assert(unit->nextTime && *leftTime != -1);
        if ((rv = saveTime(unit, NULL, NULL, -1)) != 0) {
            kill(UNITD_PID, SIGTERM);
            goto out;
        }
    }
    armTimer(unit);
    while (true) {
        if ((rv = uRead(unitPipe->fds[0], &input, sizeof(int))) == -1) {
            logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread",
                     errno, strerror(errno), "Unable to read from pipe for '%s'", unitName);
            kill(UNITD_PID, SIGTERM);
            goto out;
        }
        if (input == 1) {
            /* The left time is expired!
             * Lock and unlock the unit mutex to simulate the same
             * behaviour of listenPipeThread() func when the processes restart.
             * A possible unit status request will show the restarted or completed data.
            */
            disarmTimer(unit);
            if ((rv = pthread_mutex_lock(unit->mutex)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c",
                         "startTimerUnitThread", rv, strerror(rv),
                         "Unable to lock the mutex (restart timer)", unitName);
                kill(UNITD_PID, SIGTERM);
            }
            /* Reset data for restarting.
             * Unlike the units, the restarting of the timers is considerated like a success.
             * For this reason, we don't set a final status different by SUCCESS.
             * In this way, the requires check is satisfied as well.
            */
            *unit->processData->pStateData = PSTATE_DATA_ITEMS[RESTARTING];
            stringCopy(unit->nextTimeDate, "-");
            stringCopy(unit->leftTimeDuration, "-");
            if ((rv = pthread_mutex_unlock(unit->mutex)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c",
                         "startTimerUnitThread", rv, strerror(rv),
                         "Unable to unlock the mutex (restart timer)", unitName);
                kill(UNITD_PID, SIGTERM);
            }
            if (SHUTDOWN_COMMAND != NO_COMMAND || (rv = executeUnit(unit, TIMER)) == EUIDOWN)
                logWarning(SYSTEM, "Shutting down the unitd instance. Skipped '%s' execution.",
                           unitName);
            else {
                *unit->processData->pStateData = PSTATE_DATA_ITEMS[RUNNING];
                if ((rv = pthread_mutex_lock(unit->mutex)) != 0) {
                    logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c",
                             "startTimerUnitThread", rv, strerror(rv),
                             "Unable to lock the mutex (restart timer before start)", unitName);
                    kill(UNITD_PID, SIGTERM);
                }
                setNextTimeFromInterval(&unit);
                assert(unit->nextTime && *leftTime != -1);
                if ((rv = saveTime(unit, NULL, NULL, -1)) != 0)
                    kill(UNITD_PID, SIGTERM);
                armTimer(unit);
                if ((rv = pthread_mutex_unlock(unit->mutex)) != 0) {
                    logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c",
                             "startTimerUnitThread", rv, strerror(rv),
                             "Unable to unlock the mutex (restart timer after start)", unitName);
                    kill(UNITD_PID, SIGTERM);
                }
            }
        } else if (input == THREAD_EXIT)
            goto out;
    }

out:
    disarmTimer(unit);
    if ((rv = pthread_mutex_unlock(unitPipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnitThread", rv,
                 strerror(rv), "Unable to unlock the pipe mutex for the %s unit", unitName);
        kill(UNITD_PID, SIGTERM);
    }
    pthread_exit(0);
}

int startTimerUnit(Unit *unit)
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
                 strerror(errno), "pthread_attr_setdetachstate returned bad exit code %d", rv);
        kill(UNITD_PID, SIGTERM);
    }
    *unit->processData->pStateData = PSTATE_DATA_ITEMS[RUNNING];
    if ((rv = pthread_create(&thread, &attr, startTimerUnitThread, unit)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/units/utimers/utimers.c", "startTimerUnit", rv,
                 strerror(rv), "Unable to create the unit timer thread for '%s'", unitName);
        kill(UNITD_PID, SIGTERM);
    } else {
        if (DEBUG)
            logInfo(SYSTEM, "unit timer thread created successfully for '%s'\n", unitName);
    }

    pthread_attr_destroy(&attr);
    return rv;
}

static int checkResponse(SockMessageOut *sockMessageOut)
{
    Array *sockErrors = NULL, *unitErrors = NULL, *unitsDisplay = NULL;
    int rv = 0, sockErrSize = 0, unitErrSize = 0;
    Unit *unitDisplay = NULL;

    assert(sockMessageOut);

    sockErrors = sockMessageOut->errors;
    sockErrSize = sockErrors ? sockErrors->size : 0;
    if (sockErrSize > 0) {
        logErrorStr(SYSTEM, "Failed with the following errors:");
        for (int i = 0; i < sockErrSize; i++)
            logErrorStr(SYSTEM, "%s", arrayGet(sockErrors, i));
        rv = 1;
    } else {
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

int executeUnit(Unit *otherUnit, PType pType)
{
    char *unitName = NULL;
    const char *otherUnitName = NULL;
    int rv = 0, socketConnection = 1;
    SockMessageOut *sockMessageOut = sockMessageOutNew();
    SockMessageIn *sockMessageIn = sockMessageInNew();
    Time *now = timeNew(NULL);
    Array *units = UNITD_DATA->units, *options = arrayNew(objectRelease);

    assert(otherUnit);

    otherUnitName = otherUnit->name;
    unitName = getUnitNameByOther(otherUnitName, pType);
    sockMessageIn->arg = unitName;
    sockMessageIn->command = START_COMMAND;
    sockMessageIn->options = options;
    arrayAdd(options, stringNew(OPTIONS_DATA[RESTART_OPT].name));
    Unit *unit = getUnitByName(units, unitName);
    if (unit) {
        if (unit->type == DAEMON && unit->processData->pStateData->pState == RUNNING) {
            logWarning(SYSTEM, "%s: '%s' is already running. Skipped!", otherUnitName, unitName);
            goto out;
        }
    }
    logInfo(SYSTEM, "%s: Starting '%s' ...", otherUnitName, unitName);
    /* If we are shutting down the instance then exit with a custom exit code.(EUIDOWN) */
    if (SHUTDOWN_COMMAND != NO_COMMAND) {
        rv = EUIDOWN;
        goto out;
    }
    rv = startUnitServer(&socketConnection, sockMessageIn, &sockMessageOut, true, true);
    if (rv == 0 && (rv = checkResponse(sockMessageOut)) == 0) {
        Unit *unit = getUnitByName(units, unitName);
        if (unit) {
            int finalStatus = *unit->processData->finalStatus;
            if (DEBUG)
                logInfo(SYSTEM, "%s: Final status for '%s' = %d ...", otherUnitName, unitName,
                        finalStatus);
            if (finalStatus == FINAL_STATUS_SUCCESS) {
                logSuccess(SYSTEM, "%s: '%s' executed successfully.", otherUnitName, unitName);
                rv = 0;
            } else
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

int resetNextTime(const char *timerName)
{
    int rv = 0;
    char *pattern = NULL;
    Array *envVars = NULL;

    if (!USER_INSTANCE)
        pattern = stringNew(UNITD_TIMER_DATA_PATH);
    else
        pattern = stringNew(UNITD_USER_TIMER_DATA_PATH);
    stringAppendChr(&pattern, '/');
    stringAppendStr(&pattern, timerName);
    stringAppendStr(&pattern, "|next|*");
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "PATTERN", pattern);
    arrayAdd(envVars, NULL);
    rv = execUScript(&envVars, "remove");

    arrayRelease(&envVars);
    objectRelease(&pattern);
    return rv;
}
