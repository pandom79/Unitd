/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

//INIT PARSER CONFIGURATION
enum SectionNameEnum { UNIT = 0, COMMAND = 1, STATE = 2 };
/* Properties */
enum PropertyNameEnum {
    DESCRIPTION = 0,
    REQUIRES = 1,
    TYPE = 2,
    RESTART = 3,
    RESTART_MAX = 4,
    CONFLICTS = 5,
    RUN = 6,
    STOP = 7,
    FAILURE = 8,
    WANTEDBY = 9
};
/* Sections */
int UNITS_SECTIONS_ITEMS_LEN = 3;
SectionData UNITS_SECTIONS_ITEMS[] = { { { UNIT, "[Unit]" }, false, true, 0 },
                                       { { COMMAND, "[Command]" }, false, true, 0 },
                                       { { STATE, "[State]" }, false, true, 0 } };
/* The accepted values for the properties */
static const char *TYPE_VALUES[] = { PTYPE_DATA_ITEMS[DAEMON].desc, PTYPE_DATA_ITEMS[ONESHOT].desc,
                                     NULL };
static const char *BOOL_VALUES[] = { "false", "true", NULL };
static const char *WANTEDBY_VALUES[] = { STATE_DATA_ITEMS[INIT].desc,
                                         STATE_DATA_ITEMS[POWEROFF].desc,
                                         STATE_DATA_ITEMS[SINGLE_USER].desc,
                                         STATE_DATA_ITEMS[MULTI_USER].desc,
                                         STATE_DATA_ITEMS[MULTI_USER_NET].desc,
                                         STATE_DATA_ITEMS[CUSTOM].desc,
                                         STATE_DATA_ITEMS[GRAPHICAL].desc,
                                         STATE_DATA_ITEMS[REBOOT].desc,
                                         STATE_DATA_ITEMS[FINAL].desc,
                                         STATE_DATA_ITEMS[USER].desc,
                                         NULL };
int UNITS_PROPERTIES_ITEMS_LEN = 10;
PropertyData UNITS_PROPERTIES_ITEMS[] = {
    { UNIT, { DESCRIPTION, "Description" }, false, true, false, 0, NULL, NULL },
    { UNIT, { REQUIRES, "Requires" }, true, false, false, 0, NULL, NULL },
    { UNIT, { TYPE, "Type" }, false, false, false, 0, TYPE_VALUES, NULL },
    { UNIT, { RESTART, "Restart" }, false, false, false, 0, BOOL_VALUES, NULL },
    { UNIT, { RESTART_MAX, "RestartMax" }, false, false, true, 0, NULL, NULL },
    { UNIT, { CONFLICTS, "Conflict" }, true, false, false, 0, NULL, NULL },
    { COMMAND, { RUN, "Run" }, false, true, false, 0, NULL, NULL },
    { COMMAND, { STOP, "Stop" }, false, false, false, 0, NULL, NULL },
    { COMMAND, { FAILURE, "Failure" }, false, false, false, 0, NULL, NULL },
    { STATE, { WANTEDBY, "WantedBy" }, true, true, false, 0, WANTEDBY_VALUES, NULL }
};
//END PARSER CONFIGURATION

const UnitsErrorsData UNITS_ERRORS_ITEMS[] = {
    { UNABLE_OPEN_UNIT_ERR, "Unable to open '%s'!" },
    { REQUIRE_ITSELF_ERR, "'%s' cannot require itself!" },
    { CONFLICT_ITSELF_ERR, "'%s' cannot have a conflict with itself!" },
    { UNSATISFIED_DEP_ERR, "'%s' dependency is not satisfied for '%s'!" },
    { BIDIRECTIONAL_DEP_ERR, "Bidirectional dependency detected between '%s' and '%s'!" },
    { DEPS_ERR, "The '%s' property is not properly configured for '%s', '%s' and '%s'!" },
    { CONFLICT_DEP_ERROR, "'%s' cannot have a dependency and a conflict with '%s'!" },
    { CONFLICT_EXEC_ERROR, "'%s' has a conflict with '%s'!" },
    { WANTEDBY_ERR, "'%s' is not wanted by '%s' state!" },
    { WANTEDBY_INIT_FINAL_ERR, "The '%s' and '%s' states are not allowed!" },
    { UNIT_PATH_INIT_FINAL_ERR, "The '%s' and '%s' states can't contain symbolic links!" },
    { UNIT_PATH_ERR, "'%s' is not a valid symbolic link!" },
    { UNIT_NOT_EXIST_ERR, "Unable to find '%s' file!" },
    { UNIT_TIMEOUT_ERR, "Timeout expired for '%s'!" },
    { UNIT_ALREADY_ERR, "The unit is already %s!" },
    { DEFAULT_SYML_MISSING_ERR, "The default state symlink is missing!" },
    { DEFAULT_SYML_TYPE_ERR, "The default state doesn't look like a symlink!" },
    { DEFAULT_SYML_BAD_DEST_ERR, "The default state symlink points to a bad destination : %s" },
    { DEFAULT_SYML_SET_ERR, "The default state is already set to '%s'!" },
    { UNIT_CHANGED_ERR, "The unit content is changed!" },
    { UNIT_ENABLE_STATE_ERR, "Unable to perform the enabling!" },
    { UNIT_EXIST_ERR, "'%s' already exists!" },
    { UTIMER_INTERVAL_ERR, "At least one criterion must be defined for the interval!" },
    { UPATH_WELL_FORMED_PATH_ERR, "The '%s' property path is not well formed!" },
    { UPATH_PATH_SEC_ERR, "At least one path to be monitored must be defined!" },
    { UPATH_ACCESS_ERR, "Unable to access to '%s' property path!" },
    { UPATH_PATH_RESOURCE_ERR, "The '%s' property path doesn't look like a %s!" }
};

const UnitsMessagesData UNITS_MESSAGES_ITEMS[] = {
    { UNIT_START_MSG, "Please, run 'unitctl restart %s%s' to restart it." },
    { UNIT_FORCE_START_CONFLICT_MSG, "Please, use '--force' or '-f' option to force it." },
    { UNIT_REMOVED_SYML_MSG, "Removed symlink '%s' -> '%s'." },
    { UNIT_CREATED_SYML_MSG, "Created symlink '%s' -> '%s'." },
    { STATE_MSG, "%s state : %s" },
    { DEFAULT_STATE_SYML_RESTORED_MSG, "The default state has been restored to '%s'." },
    { TIME_MSG, "%s time : \033[1;37m%s\033[0m" },
    { UNIT_CHANGED_MSG, "Please, run 'unitctl stop %s%s' to continue working with this unit." },
    { UNIT_ENABLE_STATE_MSG,
      "The 'wantedBy' property should contain at least one of the following states :\n%s" },
    { UNIT_CHANGED_RE_ENABLE_MSG, "Please, use '--run' or '-r' option to run this operation." },
    { UNIT_RE_ENABLE_MSG, "Please, run 'unitctl re-enable %s%s' to re-enable it." },
    { UNIT_NO_DATA_FOUND_MSG, "No data found." }
};

/* Return the unit name by unit path */
char *getUnitName(const char *unitPath)
{
    char *unitName = NULL;
    int startIdx = -1;

    startIdx = stringLastIndexOfChr(unitPath, '/');
    if (startIdx != -1)
        unitName = stringSub(unitPath, startIdx + 1, strlen(unitPath) - 1);
    else {
        PType unitType = getPTypeByUnitName(unitPath);
        if (unitType == DAEMON && !stringEndsWithStr(unitPath, ".unit")) {
            unitName = stringNew(unitPath);
            stringAppendStr(&unitName, ".unit");
        } else
            unitName = stringNew(unitPath);
    }

    return unitName;
}

PType getPTypeByPTypeStr(const char *typeStr)
{
    for (PType i = DAEMON; i <= ONESHOT; i++) {
        if (stringEquals(typeStr, PTYPE_DATA_ITEMS[i].desc))
            return i;
    }

    return NO_PROCESS_TYPE;
}

PType getPTypeByUnitName(const char *unitFile)
{
    if (unitFile) {
        if (stringEndsWithStr(unitFile, ".utimer"))
            return TIMER;
        else if (stringEndsWithStr(unitFile, ".upath"))
            return UPATH;
        else
            return DAEMON;
    }

    return NO_PROCESS_TYPE;
}

Unit *getUnitByName(Array *units, const char *unitName)
{
    Unit *unit = NULL;
    int len = (units ? units->size : 0);
    for (int i = 0; i < len; i++) {
        unit = arrayGet(units, i);
        if (stringEquals(unit->name, unitName))
            return unit;
    }

    return NULL;
}

Unit *getUnitByPid(Array *units, pid_t pid)
{
    Unit *unit = NULL;
    int len = (units ? units->size : 0);
    for (int i = 0; i < len; i++) {
        unit = arrayGet(units, i);
        if (*unit->processData->pid == pid)
            return unit;
    }

    return NULL;
}

Unit *getUnitByFailurePid(Array *units, pid_t pid)
{
    Unit *unit = NULL;
    pid_t *failurePid = NULL;
    int len = (units ? units->size : 0);
    for (int i = 0; i < len; i++) {
        unit = arrayGet(units, i);
        failurePid = unit->failurePid;
        if (failurePid && *failurePid == pid)
            return unit;
    }

    return NULL;
}

Unit *unitNew(Unit *unitFrom, ParserFuncType funcType)
{
    int rv = 0;
    Unit *unit = calloc(1, sizeof(Unit));
    assert(unit);

    /* These variables are in common between the parser functionalities */
    unit->name = (unitFrom ? stringNew(unitFrom->name) : NULL);
    unit->enabled = (unitFrom ? unitFrom->enabled : false);
    unit->path = (unitFrom ? stringNew(unitFrom->path) : NULL);
    unit->processData = (unitFrom ? processDataNew(unitFrom->processData, funcType) :
                                    processDataNew(NULL, funcType));
    unit->desc = (unitFrom ? stringNew(unitFrom->desc) : NULL);
    unit->restartNum = (unitFrom ? unitFrom->restartNum : 0);
    unit->restart = (unitFrom ? unitFrom->restart : false);
    unit->restartMax = (unitFrom ? unitFrom->restartMax : -1);
    unit->type = (unitFrom ? unitFrom->type : DAEMON);
    //TIMER DATA
    /* The following data are only initialized here but they are allocated by parseUnitTimer() func
     * to avoid to allocate them uselessly when the unit type is different by TIMER except
     * when we make copies for the communication between client and server.
    */
    /* Seconds */
    int *seconds = NULL;
    if (unitFrom && unitFrom->intSeconds) {
        seconds = calloc(1, sizeof(int));
        assert(seconds);
        *seconds = *unitFrom->intSeconds;
    }
    unit->intSeconds = seconds;
    /* Minutes */
    int *minutes = NULL;
    if (unitFrom && unitFrom->intMinutes) {
        minutes = calloc(1, sizeof(int));
        assert(minutes);
        *minutes = *unitFrom->intMinutes;
    }
    unit->intMinutes = minutes;
    /* Hours */
    int *hours = NULL;
    if (unitFrom && unitFrom->intHours) {
        hours = calloc(1, sizeof(int));
        assert(hours);
        *hours = *unitFrom->intHours;
    }
    unit->intHours = hours;
    /* Days */
    int *days = NULL;
    if (unitFrom && unitFrom->intDays) {
        days = calloc(1, sizeof(int));
        assert(days);
        *days = *unitFrom->intDays;
    }
    unit->intDays = days;
    /* Weeks */
    int *weeks = NULL;
    if (unitFrom && unitFrom->intWeeks) {
        weeks = calloc(1, sizeof(int));
        assert(weeks);
        *weeks = *unitFrom->intWeeks;
    }
    unit->intWeeks = weeks;
    /* Months */
    int *months = NULL;
    if (unitFrom && unitFrom->intMonths) {
        months = calloc(1, sizeof(int));
        assert(months);
        *months = *unitFrom->intMonths;
    }
    unit->intMonths = months;
    /* Left time */
    long *leftTime = NULL;
    if (unitFrom && unitFrom->leftTime) {
        leftTime = calloc(1, sizeof(long));
        assert(leftTime);
        *leftTime = *unitFrom->leftTime;
    }
    unit->leftTime = leftTime;
    /* Left time (duration) */
    if (unitFrom && unitFrom->leftTimeDuration) {
        unit->leftTimeDuration = calloc(50, sizeof(char));
        assert(unit->leftTimeDuration);
        assert(stringCopy(unit->leftTimeDuration, unitFrom->leftTimeDuration));
    } else
        unit->leftTimeDuration = NULL;
    /* Next time */
    unit->nextTime = (unitFrom && unitFrom->nextTime ? timeNew(unitFrom->nextTime) : NULL);
    /* Next time (date) */
    if (unitFrom && unitFrom->nextTimeDate) {
        unit->nextTimeDate = calloc(50, sizeof(char));
        assert(unit->nextTimeDate);
        assert(stringCopy(unit->nextTimeDate, unitFrom->nextTimeDate));
    } else
        unit->nextTimeDate = NULL;
    //END TIMER DATA
    if (funcType == PARSE_SOCK_RESPONSE || funcType == PARSE_UNIT) {
        /* Requires */
        unit->requires = (unitFrom ? arrayStrCopy(unitFrom->requires) : NULL);
        /* Conflicts */
        unit->conflicts = (unitFrom ? arrayStrCopy(unitFrom->conflicts) : NULL);
        /* Errors */
        unit->errors = (unitFrom ? arrayStrCopy(unitFrom->errors) : NULL);
        /* Wanted by */
        unit->wantedBy = (unitFrom ? arrayStrCopy(unitFrom->wantedBy) : NULL);
        /* Process Data history */
        Array *pDataHistory = NULL;
        Array *pDataHistoryFrom = (unitFrom ? unitFrom->processDataHistory : NULL);
        int len = (pDataHistoryFrom ? pDataHistoryFrom->size : 0);
        if (len > 0) {
            pDataHistory = arrayNew(processDataRelease);
            for (int i = 0; i < len; i++)
                arrayAdd(pDataHistory, processDataNew(arrayGet(pDataHistoryFrom, i), funcType));
        }
        unit->processDataHistory = pDataHistory;
        /* Eventual timer name for the unit */
        unit->timerName = (unitFrom && unitFrom->timerName ? stringNew(unitFrom->timerName) : NULL);
        /* Eventual timer process data for the unit */
        PState *timerPState = NULL;
        if (unitFrom && unitFrom->timerPState) {
            timerPState = calloc(1, sizeof(PState));
            assert(timerPState);
            *timerPState = *unitFrom->timerPState;
        }
        unit->timerPState = timerPState;
        /* Eventual path unit name */
        unit->pathUnitName =
            (unitFrom && unitFrom->pathUnitName ? stringNew(unitFrom->pathUnitName) : NULL);
        /* Eventual path unit pstate */
        PState *pathUnitPState = NULL;
        if (unitFrom && unitFrom->pathUnitPState) {
            pathUnitPState = calloc(1, sizeof(PState));
            assert(pathUnitPState);
            *pathUnitPState = *unitFrom->pathUnitPState;
        }
        unit->pathUnitPState = pathUnitPState;
        // TIMER DATA
        /* Interval as string */
        unit->intervalStr =
            (unitFrom && unitFrom->intervalStr ? stringNew(unitFrom->intervalStr) : NULL);
        // END TIMER DATA
    }
    if (funcType == PARSE_UNIT) {
        /* Set the default values */
        unit->showResult = true;
        unit->isStopping = false;
        unit->isChanged = false;
        /* Path unit data. */
        unit->pathExists = NULL;
        unit->pathExistsMonitor = NULL;
        unit->pathExistsGlob = NULL;
        unit->pathExistsGlobMonitor = NULL;
        unit->pathResourceChanged = NULL;
        unit->pathResourceChangedMonitor = NULL;
        unit->pathDirectoryNotEmpty = NULL;
        unit->pathDirectoryNotEmptyMonitor = NULL;
        /* Initialize mutex */
        pthread_mutex_t *mutex = NULL;
        mutex = calloc(1, sizeof(pthread_mutex_t));
        assert(mutex);
        unit->mutex = mutex;
        if ((rv = pthread_mutex_init(mutex, NULL)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/units/units.c", "unitNew", rv, strerror(rv),
                     "Unable to run pthread_mutex_init");
            kill(UNITD_PID, SIGTERM);
        }
        /* Initialize condition variable */
        pthread_cond_t *cv = NULL;
        cv = calloc(1, sizeof(pthread_cond_t));
        assert(cv);
        unit->cv = cv;
        if ((rv = pthread_cond_init(cv, NULL)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/units/units.c", "unitNew", rv, strerror(rv),
                     "Unable to run pthread_cond_init");
            kill(UNITD_PID, SIGTERM);
        }
    }

    return unit;
}

bool isEnabledUnit(const char *unitName, State currentState)
{
    char *pattern = NULL;
    int rv = 0, len = 0;
    glob_t results;
    bool found = false;
    Array *statesData = NULL;
    StateData *stateData = NULL;

    assert(unitName);

    /* The check must be done only for reboot, poweroff and (default or cmdline state).
     * We can never have reboot or poweroff state as default.
    */
    statesData = arrayNew(NULL);
    if (currentState != NO_STATE)
        arrayAdd(statesData, (void *)&STATE_DATA_ITEMS[currentState]);
    else {
        if (!USER_INSTANCE) {
            arrayAdd(statesData, (void *)&STATE_DATA_ITEMS[REBOOT]);
            arrayAdd(statesData, (void *)&STATE_DATA_ITEMS[POWEROFF]);
            arrayAdd(statesData,
                     (void *)&STATE_DATA_ITEMS[STATE_CMDLINE != NO_STATE ? STATE_CMDLINE :
                                                                           STATE_DEFAULT]);
        } else
            arrayAdd(statesData, (void *)&STATE_DATA_ITEMS[USER]);
    }
    len = statesData->size;
    assert(len >= 1);
    for (int i = 0; i < len; i++) {
        stateData = arrayGet(statesData, i);
        pattern = !USER_INSTANCE ? stringNew(UNITS_ENAB_PATH) : stringNew(UNITS_USER_ENAB_PATH);
        stringAppendChr(&pattern, '/');
        stringAppendStr(&pattern, stateData->desc);
        stringAppendStr(&pattern, ".state/");
        stringAppendStr(&pattern, unitName);
        if ((rv = glob(pattern, 0, NULL, &results)) == 0 && results.gl_pathc > 0)
            found = true;
        objectRelease(&pattern);
        globfree(&results);
        if (found)
            break;
    }

    arrayRelease(&statesData);
    return found;
}

int checkAndSetUnitPath(Unit **currentUnit, State state)
{
    int rv = 0;
    char *wherePoints = NULL, *path = NULL;
    Array **errors = NULL;
    bool hasError = false;

    assert(*currentUnit);
    assert(state != NO_STATE);

    errors = &(*currentUnit)->errors;
    path = (*currentUnit)->path;
    if (state == INIT || state == FINAL) {
        /* The init and final states can't contain sym links */
        if ((rv = readSymLink(path, &wherePoints)) == 0) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_PATH_INIT_FINAL_ERR].desc,
                                     STATE_DATA_ITEMS[INIT].desc, STATE_DATA_ITEMS[FINAL].desc));
        } else if (rv == 2)
            rv = 0;
    } else {
        /* Check where the unit is pointing */
        rv = readSymLink(path, &wherePoints);
        if (rv == 2)
            hasError = true;
        else {
            /**
             * Relative symlinks don't start with UNITS_PATH, UNITS_USER_PATH ...
             * At most, they can contain these paths.
            */
            if (!USER_INSTANCE) {
                if (!stringContainsStr(wherePoints, UNITS_PATH))
                    hasError = true;
            } else {
                if (!stringContainsStr(wherePoints, UNITS_USER_PATH) &&
                    !stringContainsStr(wherePoints, UNITS_USER_LOCAL_PATH))
                    hasError = true;
            }
        }
        if (hasError) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_PATH_ERR].desc, path));
        } else
            stringSet(&(*currentUnit)->path, wherePoints);
    }

    objectRelease(&wherePoints);
    return rv;
}

int checkWantedBy(Unit **currentUnit, State currentState, bool isAggregate)
{
    int rv = 0;
    Array *wantedBy = NULL, *errors = NULL;
    const char *currentStateStr = NULL, *initStateDesc = STATE_DATA_ITEMS[INIT].desc,
               *finalStateDesc = STATE_DATA_ITEMS[FINAL].desc;

    assert(*currentUnit);
    assert(currentState != NO_STATE);

    wantedBy = (*currentUnit)->wantedBy;
    errors = (*currentUnit)->errors;
    /* The units for the 'init' or 'final' state are excluded by the units handling */
    if (currentState != INIT && currentState != FINAL) {
        if (arrayContainsStr(wantedBy, initStateDesc) ||
            arrayContainsStr(wantedBy, finalStateDesc)) {
            rv = 1;
            arrayAdd(errors, getMsg(-1, UNITS_ERRORS_ITEMS[WANTEDBY_INIT_FINAL_ERR].desc,
                                    initStateDesc, finalStateDesc));
            if (!isAggregate)
                return rv;
        }
    }
    /* We check if the unit is wanted by the current state */
    currentStateStr = STATE_DATA_ITEMS[currentState].desc;
    if (!arrayContainsStr(wantedBy, currentStateStr)) {
        rv = 1;
        arrayAdd(errors, getMsg(-1, UNITS_ERRORS_ITEMS[WANTEDBY_ERR].desc, (*currentUnit)->name,
                                currentStateStr));
    }

    return rv;
}

int checkConflicts(Unit **currentUnit, const char *conflictName, bool isAggregate)
{
    Array *errors = NULL;
    char *currentUnitName = NULL;
    int rv = 0;

    assert(*currentUnit);

    /* Get the unit data */
    currentUnitName = (*currentUnit)->name;
    errors = (*currentUnit)->errors;
    /* The unit cannot be in conflict with itself */
    if (stringEquals(conflictName, currentUnitName)) {
        rv = 1;
        arrayAdd(errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_ITSELF_ERR].desc, currentUnitName));
        if (!isAggregate)
            return rv;
    } else {
        /* We cannot have a dependency and a conflict with the same unit! */
        if (arrayContainsStr((*currentUnit)->requires, conflictName)) {
            rv = 1;
            arrayAdd(errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_DEP_ERROR].desc,
                                    currentUnitName, conflictName));
            if (!isAggregate)
                return rv;
        }
    }

    return rv;
}

int checkRequires(Array **units, Unit **currentUnit, bool isAggregate)
{
    Array *deps = NULL, *errors = NULL;
    int rv = 0, lenRequires = 0, lenRequiresDep = 0;
    char *depName = NULL, *currentUnitName = NULL, *depDepName = NULL;
    Unit *unitDep = NULL, *unitDepDep = NULL;

    assert(*units);
    assert(*currentUnit);

    /* Get the unit data */
    currentUnitName = (*currentUnit)->name;
    deps = (*currentUnit)->requires;
    errors = (*currentUnit)->errors;
    lenRequires = (deps ? deps->size : 0);
    for (int i = 0; i < lenRequires; i++) {
        depName = arrayGet(deps, i);
        assert(depName);
        /* The unit cannot depend by itself */
        if (stringEquals(depName, currentUnitName)) {
            rv = 1;
            arrayAdd(errors,
                     getMsg(-1, UNITS_ERRORS_ITEMS[REQUIRE_ITSELF_ERR].desc, currentUnitName));
            if (!isAggregate)
                return rv;
        } else {
            /* The dependency cannot be bidirectional (A depends B, B depends A).
             * That will cause a block to the starting of the threads because everyone will
             * wait for the other.
             * That must not happen!!
            */
            unitDep = getUnitByName(*units, depName);
            if (unitDep && arrayContainsStr(unitDep->requires, currentUnitName)) {
                rv = 1;
                arrayAdd(errors, getMsg(-1, UNITS_ERRORS_ITEMS[BIDIRECTIONAL_DEP_ERR].desc,
                                        currentUnitName, unitDep->name));
                if (!isAggregate)
                    return rv;
            }
            /* The dependencies must be properly configured.
             * A depends C, B depends A, C depends B or
             * A depends B, B depends C, C depends A.
             * That will cause a block to the starting of the threads because everyone will
             * wait for the other.
             * That must not happen!!
            */
            lenRequiresDep = (unitDep && unitDep->requires ? unitDep->requires->size : 0);
            for (int j = 0; j < lenRequiresDep; j++) {
                depDepName = arrayGet(unitDep->requires, j);
                unitDepDep = getUnitByName(*units, depDepName);
                if (unitDepDep && arrayContainsStr(unitDepDep->requires, currentUnitName)) {
                    rv = 1;
                    arrayAdd(errors, getMsg(-1, UNITS_ERRORS_ITEMS[DEPS_ERR].desc,
                                            UNITS_PROPERTIES_ITEMS[REQUIRES].property.desc,
                                            currentUnitName, unitDep->name, unitDepDep->name));
                    if (!isAggregate)
                        return rv;
                }
            }
        }
    }

    return rv;
}

int loadUnits(Array **units, const char *path, const char *dirName, State currentState,
              bool isAggregate, const char *unitNameArg, ParserFuncType funcType, bool parse)
{
    glob_t results;
    char *pattern = NULL, *patternTimer = NULL, *patternPath = NULL, *unitName = NULL,
         *unitPath = NULL;
    int rv = 0, resultInitFinal = 0;
    Unit *unit = NULL;
    size_t lenResults = 0;

    assert(path);

    /* Building the pattern */
    pattern = stringNew(path);
    patternTimer = stringNew(path);
    patternPath = stringNew(path);
    if (currentState != NO_STATE) {
        stringAppendChr(&pattern, '/');
        stringAppendStr(&pattern, dirName);
        stringAppendStr(&pattern, "/*.unit");
        stringAppendChr(&patternTimer, '/');
        stringAppendStr(&patternTimer, dirName);
        stringAppendStr(&patternTimer, "/*.utimer");
        stringAppendChr(&patternPath, '/');
        stringAppendStr(&patternPath, dirName);
        stringAppendStr(&patternPath, "/*.upath");
    } else {
        if (!unitNameArg) {
            stringAppendStr(&pattern, "/*.unit");
            stringAppendStr(&patternTimer, "/*.utimer");
            stringAppendStr(&patternPath, "/*.upath");
        } else {
            stringAppendChr(&pattern, '/');
            stringAppendStr(&pattern, unitNameArg);
        }
    }
    /* Executing the glob function */
    if (DEBUG && currentState != NO_STATE)
        logWarning(UNITD_BOOT_LOG, "\n[*] SEARCHING THE UNITS in %s/%s ...\n", path, dirName);
    if ((rv = glob(pattern, 0, NULL, &results)) == 0) {
        /* Aggregate the patterns if unitname is null */
        if (!unitNameArg) {
            if ((rv = glob(patternTimer, GLOB_APPEND, NULL, &results)) != 0)
                logWarning(UNITD_BOOT_LOG, "No timers found for %s state.\n",
                           STATE_DATA_ITEMS[currentState].desc);
            if ((rv = glob(patternPath, GLOB_APPEND, NULL, &results)) != 0)
                logWarning(UNITD_BOOT_LOG, "No path units found for %s state.\n",
                           STATE_DATA_ITEMS[currentState].desc);
        }
        lenResults = results.gl_pathc;
        assert(lenResults > 0);
        if (!(*units))
            *units = arrayNew(unitRelease);
        if (DEBUG && currentState != NO_STATE)
            logInfo(UNITD_BOOT_LOG, "Found %d units in %s/%s!\n", lenResults, path, dirName);
        for (size_t i = 0; i < lenResults; i++) {
            /* Get the unit path */
            unitPath = results.gl_pathv[i];
            /* Set unit name */
            unitName = getUnitName(unitPath);
            /* If the unit is already in memory then skip it. (to avoid duplicates in unit list function) */
            if (currentState != NO_STATE || getUnitByName(*units, unitName) == NULL) {
                /* Create the unit */
                unit = unitNew(NULL, funcType);
                unit->name = unitName;
                unit->type = getPTypeByUnitName(unitName);
                assert(unit->type != NO_PROCESS_TYPE);
                unit->path = stringNew(unitPath);
                /* Check unit path */
                if (currentState != NO_STATE) {
                    /* We always aggregate this eventual error regardless isAggregate value to allow
                     * unitctl status command to display the correct data
                    */
                    checkAndSetUnitPath(&unit, currentState);
                }
                /* Set enabled/disabled */
                unit->enabled =
                    (currentState != NO_STATE ? true : isEnabledUnit(unitName, NO_STATE));
                if (DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Unit name = '%s', path = '%s'. Parsing it ...\n",
                            unitName, unitPath);
                /* Parse the Unit file */
                if (parse) {
                    switch (unit->type) {
                    case DAEMON:
                    case ONESHOT:
                        rv = parseUnit(units, &unit, isAggregate, currentState);
                        if (rv != 0 && (currentState == INIT || currentState == FINAL))
                            resultInitFinal = 1;
                        break;
                    case TIMER:
                        rv = parseTimerUnit(units, &unit, isAggregate);
                        if (rv == 0 || isAggregate)
                            checkInterval(&unit);
                        break;
                    case UPATH:
                        rv = parsePathUnit(units, &unit, isAggregate);
                        if (rv == 0 || isAggregate)
                            checkWatchers(&unit, isAggregate);
                        break;
                    default:
                        break;
                    }
                    if ((rv == 0 || isAggregate) && currentState != NO_STATE)
                        checkWantedBy(&unit, currentState, isAggregate);
                }
                /* Create the pipe */
                if (currentState != INIT && currentState != FINAL && currentState != REBOOT &&
                    currentState != POWEROFF && funcType == PARSE_UNIT && unit->errors &&
                    unit->errors->size == 0) {
                    switch (unit->type) {
                    case DAEMON:
                    case ONESHOT:
                        if (hasPipe(unit)) {
                            unit->pipe = pipeNew();
                            /* Create process data history array accordingly */
                            unit->processDataHistory = arrayNew(processDataRelease);
                        }
                        break;
                    case TIMER:
                        /* We always need of the pipe. No need of a processDataHistory instead. */
                        unit->pipe = pipeNew();
                        break;
                    case UPATH:
                        addWatchers(&unit);
                        break;
                    default:
                        break;
                    }
                }
                /* Adding the unit to the array */
                arrayAdd(*units, unit);
            } else
                objectRelease(&unitName);
        }
        if (currentState != NO_STATE) {
            /* If we are in the init or final state then show the configuration error and emergency shell */
            if (currentState == INIT || currentState == FINAL) {
                if (resultInitFinal == 1) {
                    logWarning(CONSOLE, "[*] CONFIGURATION ERRORS\n");
                    for (size_t i = 0; i < lenResults; i++) {
                        unit = arrayGet(*units, i);
                        logInfo(CONSOLE, "Unit name = '%s'\n", unit->name);
                        arrayPrint(CONSOLE, &((Unit *)arrayGet(*units, i))->errors, true);
                    }
                }
            } else {
                if (DEBUG) {
                    logWarning(UNITD_BOOT_LOG, "\n[*] CONFIGURATION ERRORS\n");
                    for (size_t i = 0; i < lenResults; i++) {
                        unit = arrayGet(*units, i);
                        logInfo(UNITD_BOOT_LOG, "Unit name = '%s'\n", unit->name);
                        arrayPrint(UNITD_BOOT_LOG, &((Unit *)arrayGet(*units, i))->errors, true);
                    }
                }
            }
        }
    } else if (rv == GLOB_NOMATCH && currentState != NO_STATE) {
        /* Zero units are allowed only for Reboot and Poweroff State otherwise we show the errors */
        if (currentState != REBOOT && currentState != POWEROFF && currentState != USER)
            logError(CONSOLE, "src/core/units/units.c", "loadUnits", GLOB_NOMATCH, "GLOB_NOMATCH",
                     "Zero units found for %s state", STATE_DATA_ITEMS[currentState].desc);
        else {
            if (DEBUG)
                logWarning(UNITD_BOOT_LOG, "Zero units found for %s state\n",
                           STATE_DATA_ITEMS[currentState].desc);
        }
    }

    objectRelease(&pattern);
    objectRelease(&patternTimer);
    objectRelease(&patternPath);
    globfree(&results);
    return rv;
}

/* We load the units which have a suffix different by ".unit". */
int loadOtherUnits(Array **units, const char *path, const char *dirName, bool isAggregate,
                   bool parse, ListFilter listFilter)
{
    glob_t results;
    char *pattern = NULL, *unitName = NULL, *unitPath = NULL;
    int rv = 0;
    Unit *unit = NULL;
    size_t lenResults = 0;

    assert(path);
    assert(listFilter == TIMERS_FILTER || listFilter == UPATH_FILTER);

    /* Building the pattern */
    pattern = stringNew(path);
    stringAppendChr(&pattern, '/');
    stringAppendStr(&pattern, dirName);
    switch (listFilter) {
    case TIMERS_FILTER:
        stringAppendStr(&pattern, "/*.utimer");
        break;
    case UPATH_FILTER:
        stringAppendStr(&pattern, "/*.upath");
        break;
    default:
        break;
    }
    if ((rv = glob(pattern, 0, NULL, &results)) == 0) {
        lenResults = results.gl_pathc;
        assert(lenResults > 0);
        if (!(*units))
            *units = arrayNew(unitRelease);
        for (size_t i = 0; i < lenResults; i++) {
            /* Get the unit path */
            unitPath = results.gl_pathv[i];
            /* Set unit name */
            unitName = getUnitName(unitPath);
            /* If the unit is already in memory then skip it. (to avoid duplicates in unit list function) */
            if (getUnitByName(*units, unitName) == NULL) {
                /* Create the unit */
                unit = unitNew(NULL, PARSE_SOCK_RESPONSE_UNITLIST);
                unit->name = unitName;
                unit->type = getPTypeByUnitName(unitName);
                assert(unit->type != NO_PROCESS_TYPE);
                unit->path = stringNew(unitPath);
                /* Set enabled/disabled */
                unit->enabled = isEnabledUnit(unitName, NO_STATE);
                /* Parse the Unit file */
                if (parse) {
                    switch (unit->type) {
                    case TIMER:
                        rv = parseTimerUnit(units, &unit, isAggregate);
                        break;
                    case UPATH:
                        rv = parsePathUnit(units, &unit, isAggregate);
                        break;
                    default:
                        break;
                    }
                }
                /* Adding the unit to the array */
                arrayAdd(*units, unit);
            } else
                objectRelease(&unitName);
        }
    }

    objectRelease(&pattern);
    globfree(&results);
    return rv;
}

int parseUnit(Array **units, Unit **unit, bool isAggregate, State currentState)
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

    /* Initialize the parser */
    parserInit(UNITS_SECTIONS_ITEMS_LEN, UNITS_SECTIONS_ITEMS, UNITS_PROPERTIES_ITEMS_LEN,
               UNITS_PROPERTIES_ITEMS);
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
    /* Some repeatable properties require the duplicate value check.
     * Just set their pointers in the PROPERTIES_ITEM array.
     * Optional.
    */
    UNITS_PROPERTIES_ITEMS[REQUIRES].notDupValues = requires;
    UNITS_PROPERTIES_ITEMS[CONFLICTS].notDupValues = conflicts;
    UNITS_PROPERTIES_ITEMS[WANTEDBY].notDupValues = wantedBy;
    /* Open the file */
    if ((fp = fopen(unitPath, "r")) == NULL) {
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNABLE_OPEN_UNIT_ERR].desc, unitPath));
        rv = 1;
        return rv;
    }
    while (getline(&line, &len, fp) != -1) {
        numLine++;
        /* Parsing the line */
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
                    case TYPE:
                        /* If the type is different than default */
                        if (!stringEquals(value, PTYPE_DATA_ITEMS[DAEMON].desc))
                            (*unit)->type = getPTypeByPTypeStr(value);
                        break;
                    case RESTART:
                        if (stringEquals(value, BOOL_VALUES[true]))
                            (*unit)->restart = true;
                        else if (stringEquals(value, BOOL_VALUES[false]))
                            (*unit)->restart = false;
                        break;
                    case RESTART_MAX:
                        (*unit)->restartMax = atoi(value);
                        break;
                    case CONFLICTS:
                        conflict = stringNew(value);
                        arrayAdd(conflicts, conflict);
                        if ((*errors)->size == 0 || isAggregate)
                            checkConflicts(unit, value, isAggregate);
                        break;
                    case RUN:
                        (*unit)->runCmd = stringNew(value);
                        if (currentState == INIT || currentState == FINAL)
                            stringReplaceStr(&(*unit)->runCmd, UNITD_DATA_PATH_CMD_VAR,
                                             UNITD_DATA_PATH);
                        break;
                    case STOP:
                        (*unit)->stopCmd = stringNew(value);
                        if (currentState == INIT || currentState == FINAL)
                            stringReplaceStr(&(*unit)->stopCmd, UNITD_DATA_PATH_CMD_VAR,
                                             UNITD_DATA_PATH);
                        break;
                    case FAILURE:
                        (*unit)->failureCmd = stringNew(value);
                        /* Failure Pid */
                        pid_t *failurePid = calloc(1, sizeof(pid_t));
                        assert(failurePid);
                        *failurePid = -1;
                        (*unit)->failurePid = failurePid;
                        /* Failure exit code */
                        int *failureExitCode = calloc(1, sizeof(int));
                        assert(failureExitCode);
                        *failureExitCode = -1;
                        (*unit)->failureExitCode = failureExitCode;
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

void unitRelease(Unit **unit)
{
    Unit *unitTemp = *unit;
    pthread_cond_t *cv = NULL;
    pthread_mutex_t *mutex = NULL;
    int rv = 0;

    if (unitTemp) {
        objectRelease(&unitTemp->name);
        objectRelease(&unitTemp->path);
        arrayRelease(&unitTemp->conflicts);
        objectRelease(&unitTemp->desc);
        arrayRelease(&unitTemp->errors);
        arrayRelease(&unitTemp->requires);
        objectRelease(&unitTemp->runCmd);
        objectRelease(&unitTemp->stopCmd);
        objectRelease(&unitTemp->failureCmd);
        objectRelease(&unitTemp->failurePid);
        objectRelease(&unitTemp->failureExitCode);
        arrayRelease(&unitTemp->wantedBy);
        /* Destroy and free the condition variable */
        if ((cv = unitTemp->cv)) {
            if ((rv = pthread_cond_destroy(cv)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/units/units.c", "unitRelease", rv,
                         strerror(rv), "Unable to run pthread_cond_destroy");
            }
            objectRelease(&cv);
        }
        /* Destroy and free the mutex */
        if ((mutex = unitTemp->mutex)) {
            /* Destroy and free the mutex */
            if ((rv = pthread_mutex_destroy(mutex)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/units/units.c", "unitRelease", rv,
                         strerror(rv), "Unable to run pthread_mutex_destroy");
            }
            objectRelease(&mutex);
        }
        /* Process Data History */
        arrayRelease(&(unitTemp->processDataHistory));
        /* Process Data */
        processDataRelease(&(unitTemp->processData));
        /* Pipe */
        pipeRelease(&unitTemp->pipe);
        /* Eventual timer data for the unit */
        objectRelease(&unitTemp->timerName);
        objectRelease(&unitTemp->timerPState);
        /* Eventual path unit data for the unit */
        objectRelease(&unitTemp->pathUnitName);
        objectRelease(&unitTemp->pathUnitPState);
        /* Unit timer data */
        objectRelease(&unitTemp->wakeSystem);
        objectRelease(&unitTemp->intSeconds);
        objectRelease(&unitTemp->intMinutes);
        objectRelease(&unitTemp->intHours);
        objectRelease(&unitTemp->intDays);
        objectRelease(&unitTemp->intWeeks);
        objectRelease(&unitTemp->intMonths);
        objectRelease(&unitTemp->leftTimeDuration);
        objectRelease(&unitTemp->nextTimeDate);
        objectRelease(&unitTemp->leftTime);
        timeRelease(&unitTemp->nextTime);
        objectRelease(&unitTemp->intervalStr);
        timerRelease(&unitTemp->timer);
        /* Path unit */
        objectRelease(&unitTemp->pathExists);
        objectRelease(&unitTemp->pathExistsMonitor);
        objectRelease(&unitTemp->pathExistsGlob);
        objectRelease(&unitTemp->pathExistsGlobMonitor);
        objectRelease(&unitTemp->pathResourceChanged);
        objectRelease(&unitTemp->pathResourceChangedMonitor);
        objectRelease(&unitTemp->pathDirectoryNotEmpty);
        objectRelease(&unitTemp->pathDirectoryNotEmptyMonitor);
        notifierRelease(&unitTemp->notifier);
        /* Unit */
        objectRelease(unit);
    }
}

ProcessData *processDataNew(ProcessData *pDataFrom, ParserFuncType funcType)
{
    ProcessData *pDataRet = NULL;
    pid_t *pid = NULL;
    int *finalStatus = NULL;
    PStateData *pStateData = NULL;

    assert(funcType != NO_FUNC);

    /* These variables are in common between UNIT_FUNC, SOCKET_RESPONSE_FUNC and
     * SOCKET_RESPONSE_UNITSLIST_FUNC
     */

    /* Initialize Process Data */
    pDataRet = calloc(1, sizeof(ProcessData));
    assert(pDataRet);
    //Pid
    pid = calloc(1, sizeof(pid_t));
    assert(pid);
    if (!pDataFrom)
        *pid = -1;
    else
        *pid = *pDataFrom->pid;
    pDataRet->pid = pid;
    //Process State Data
    pStateData = calloc(1, sizeof(PStateData));
    assert(pStateData);
    if (!pDataFrom)
        *pStateData = PSTATE_DATA_ITEMS[DEAD];
    else
        *pStateData = *pDataFrom->pStateData;
    pDataRet->pStateData = pStateData;
    //Final Status
    finalStatus = calloc(1, sizeof(int));
    assert(finalStatus);
    if (!pDataFrom)
        *finalStatus = FINAL_STATUS_READY;
    else
        *finalStatus = *pDataFrom->finalStatus;
    pDataRet->finalStatus = finalStatus;
    //Duration
    char *durationFrom = (pDataFrom ? pDataFrom->duration : NULL);
    if (durationFrom)
        pDataRet->duration = stringNew(durationFrom);
    //Signal number
    int *signalNum = calloc(1, sizeof(int));
    assert(signalNum);
    if (!pDataFrom)
        *signalNum = -1;
    else
        *signalNum = *pDataFrom->signalNum;
    pDataRet->signalNum = signalNum;
    /* If we are not showing the list then we add more data */
    if (funcType == PARSE_UNIT || funcType == PARSE_SOCK_RESPONSE) {
        //Exit code
        int *exitCode = calloc(1, sizeof(int));
        assert(exitCode);
        if (!pDataFrom)
            *exitCode = -1;
        else
            *exitCode = *pDataFrom->exitCode;
        pDataRet->exitCode = exitCode;
        //Date start
        char *dateTimeStartFrom = (pDataFrom ? pDataFrom->dateTimeStartStr : NULL);
        if (dateTimeStartFrom)
            pDataRet->dateTimeStartStr = stringNew(dateTimeStartFrom);
        //Date stop
        char *dateTimeStopFrom = (pDataFrom ? pDataFrom->dateTimeStopStr : NULL);
        if (dateTimeStopFrom)
            pDataRet->dateTimeStopStr = stringNew(dateTimeStopFrom);
        //Time start
        Time *timeStartFrom = (pDataFrom ? pDataFrom->timeStart : NULL);
        if (timeStartFrom)
            pDataRet->timeStart = timeNew(timeStartFrom);
        //Time stop
        Time *timeStopFrom = (pDataFrom ? pDataFrom->timeStop : NULL);
        if (timeStopFrom)
            pDataRet->timeStop = timeNew(timeStopFrom);
    }

    return pDataRet;
}

void resetPDataForRestart(ProcessData **pData)
{
    assert(*pData);

    *(*pData)->pid = -1;
    *(*pData)->pStateData = PSTATE_DATA_ITEMS[RESTARTING];
    *(*pData)->finalStatus = FINAL_STATUS_READY;
    objectRelease(&(*pData)->duration);
    *(*pData)->exitCode = -1;
    *(*pData)->signalNum = -1;
    objectRelease(&(*pData)->dateTimeStartStr);
    objectRelease(&(*pData)->dateTimeStopStr);
    timeRelease(&(*pData)->timeStart);
    timeRelease(&(*pData)->timeStop);
}

void processDataRelease(ProcessData **pData)
{
    ProcessData *pDataTemp = *pData;
    if (pDataTemp) {
        objectRelease(&(pDataTemp->pid));
        objectRelease(&(pDataTemp->exitCode));
        objectRelease(&(pDataTemp->pStateData));
        objectRelease(&(pDataTemp->signalNum));
        objectRelease(&(pDataTemp->finalStatus));
        objectRelease(&(pDataTemp->dateTimeStartStr));
        objectRelease(&(pDataTemp->dateTimeStopStr));
        objectRelease(&(pDataTemp->duration));
        timeRelease(&pDataTemp->timeStart);
        timeRelease(&pDataTemp->timeStop);
        objectRelease(pData);
    }
}

Pipe *pipeNew()
{
    int rv = 0;
    Pipe *pipeObj = NULL;
    pthread_mutex_t *mutex = NULL;

    /* Pipe */
    pipeObj = calloc(1, sizeof(Pipe));
    assert(pipeObj);
    /* Mutex */
    mutex = calloc(1, sizeof(pthread_mutex_t));
    assert(mutex);
    pipeObj->mutex = mutex;
    if ((rv = pthread_mutex_init(mutex, NULL)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/units/units.c", "pipeNew", rv, strerror(rv),
                 "Unable to run pthread_mutex_init");
        kill(UNITD_PID, SIGTERM);
    }
    if ((rv = pipe(pipeObj->fds)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/units/units.c", "pipeNew", errno, strerror(errno),
                 "Unable to run pipe");
        kill(UNITD_PID, SIGTERM);
    }

    return pipeObj;
}

void pipeRelease(Pipe **pipe)
{
    Pipe *pipeTemp = *pipe;
    pthread_mutex_t *mutex = NULL;
    int rv = 0;

    if (pipeTemp) {
        /* Destroy and free the mutex */
        if ((mutex = pipeTemp->mutex)) {
            if ((rv = pthread_mutex_destroy(mutex)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/units/units.c", "pipeRelease", rv,
                         strerror(rv), "Unable to run pthread_mutex_destroy");
            }
            objectRelease(&mutex);
        }
        /* Close fds */
        close(pipeTemp->fds[0]);
        close(pipeTemp->fds[1]);
        /* Release pipe */
        objectRelease(pipe);
    }
}
