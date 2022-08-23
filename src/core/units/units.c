/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

//INIT PARSER CONFIGURATION
enum SectionNameEnum  {
  UNIT = 0,
  COMMAND = 1,
  STATE = 2
};

/* Properties */
enum PropertyNameEnum  {
    DESCRIPTION = 0,
    REQUIRES = 1,
    TYPE = 2,
    RESTART = 3,
    RESTART_MAX = 4,
    CONFLICTS = 5,
    RUN = 6,
    STOP = 7,
    WANTEDBY = 8
};

/* Sections */
int UNITS_SECTIONS_ITEMS_LEN = 3;
SectionData UNITS_SECTIONS_ITEMS[] = {
    { { UNIT, "[Unit]" }, false, true, 0 },
    { { COMMAND, "[Command]" }, false, true, 0 },
    { { STATE, "[State]" }, false, true, 0 }
};

/* The accepted values for the properties */
static const char *TYPE_VALUES[] = { PTYPE_DATA_ITEMS[DAEMON].desc , PTYPE_DATA_ITEMS[ONESHOT].desc , NULL };
static const char *BOOL_VALUES[] = { "false", "true", NULL };
static const char *WANTEDBY_VALUES[] = {
                                        STATE_DATA_ITEMS[INIT].desc,
                                        STATE_DATA_ITEMS[POWEROFF].desc,
                                        STATE_DATA_ITEMS[SINGLE_USER].desc,
                                        STATE_DATA_ITEMS[MULTI_USER].desc,
                                        STATE_DATA_ITEMS[MULTI_USER_NET].desc,
                                        STATE_DATA_ITEMS[CUSTOM].desc,
                                        STATE_DATA_ITEMS[GRAPHICAL].desc,
                                        STATE_DATA_ITEMS[REBOOT].desc,
                                        STATE_DATA_ITEMS[FINAL].desc,
                                        STATE_DATA_ITEMS[USER].desc,
                                        NULL
                                       };

int UNITS_PROPERTIES_ITEMS_LEN = 9;
PropertyData UNITS_PROPERTIES_ITEMS[] = {
    { UNIT, { DESCRIPTION, "Description" }, false, true, false, 0, NULL, NULL },
    { UNIT, { REQUIRES, "Requires" }, true, false, false, 0, NULL, NULL },
    { UNIT, { TYPE, "Type" }, false, false, false, 0, TYPE_VALUES, NULL },
    { UNIT, { RESTART, "Restart" }, false, false, false, 0, BOOL_VALUES, NULL },
    { UNIT, { RESTART_MAX, "RestartMax" }, false, false, true, 0, NULL, NULL },
    { UNIT, { CONFLICTS, "Conflict" }, true, false, false, 0, NULL, NULL },
    { COMMAND, { RUN, "Run" }, false, true, false, 0, NULL, NULL },
    { COMMAND, { STOP, "Stop" }, false, false, false, 0, NULL, NULL },
    { STATE, { WANTEDBY, "WantedBy" }, true, true, false, 0, WANTEDBY_VALUES, NULL }
};

//END PARSER CONFIGURATION

const UnitsErrorsData UNITS_ERRORS_ITEMS[] = {
    { UNABLE_OPEN_UNIT_ERR, "Unable to open the '%s' unit!" },
    { REQUIRE_ITSELF_ERR, "The '%s' unit cannot require itself!" },
    { CONFLICT_ITSELF_ERR, "The '%s' unit cannot have a conflict with itself!" },
    { UNSATISFIED_DEP_ERR, "The '%s' dependency is not satisfied for the '%s' unit!" },
    { BIDIRECTIONAL_DEP_ERR, "The '%s' unit has a bidirectional dependency with the '%s' unit!" },
    { DEPS_ERR, "The '%s' property is not properly configured for '%s', '%s' and '%s' units!" },
    { CONFLICT_DEP_ERROR, "The '%s' unit cannot have a dependency and a conflict with '%s' unit!" },
    { CONFLICT_EXEC_ERROR, "The '%s' unit has a conflict with the '%s' unit!" },
    { WANTEDBY_ERR, "The '%s' unit is not wanted by '%s' state!" },
    { WANTEDBY_INIT_FINAL_ERR, "The '%s' and '%s' states are not allowed!" },
    { UNIT_PATH_INIT_FINAL_ERR, "The '%s' and '%s' states can't contain symbolic links!" },
    { UNIT_PATH_ERR, "The '%s' unit is not a valid symbolic link!" },
    { UNIT_NOT_EXIST_ERR, "The '%s' unit doesn't exist!" },
    { UNIT_TIMEOUT_ERR, "Timeout expired for the '%s' unit !" },
    { UNIT_ALREADY_ERR, "The unit is already %s!" },
    { DEFAULT_SYML_MISSING_ERR, "The default state symlink is missing!" },
    { DEFAULT_SYML_TYPE_ERR, "The default state doesn't look like a symlink!" },
    { DEFAULT_SYML_BAD_DEST_ERR, "The default state symlink points to a bad destination : %s" },
    { DEFAULT_SYML_SET_ERR, "The default state is already set to '%s'!" },
    { UNIT_CHANGED_ERR, "The unit content is changed!" },
    { UNIT_ENABLE_STATE_ERR, "Unable to perform the enabling!" },
    { UNITS_LIST_EMPTY_ERR, "There are no units!" } ,
    { UNIT_EXIST_ERR, "The '%s' unit already exists!"}
};

const UnitsMessagesData UNITS_MESSAGES_ITEMS[] = {
    { UNIT_START_MSG, "Please, run 'unitctl restart %s%s' to restart it." },
    { UNIT_FORCE_START_CONFLICT_MSG, "Please, use '--force' or '-f' option to force it." },
    { UNIT_REMOVED_SYML_MSG, "Removed symlink '%s' -> '%s'." },
    { UNIT_CREATED_SYML_MSG, "Created symlink '%s' -> '%s'." },
    { STATE_MSG, "%s state : %s" },
    { DEFAULT_STATE_SYML_RESTORED_MSG, "The default state has been restored to '%s'." },
    { TIME_MSG, "%s time : \033[1;32m%s\033[0m" },
    { UNIT_CHANGED_MSG, "Please, run 'unitctl stop %s%s' to continue working with this unit." },
    { UNIT_ENABLE_STATE_MSG, "The 'wantedBy' property should contain at least one of the following states :\n%s" },
    { UNIT_CHANGED_RE_ENABLE_MSG, "Please, use '--run' or '-r' option to run this operation." },
    { UNIT_RE_ENABLE_MSG, "Please, run 'unitctl re-enable %s%s' to re-enable it." }
};

/* Return the unit name by unit path or unit name with ".unit" suffix */
char*
getUnitName(const char *unitPath)
{
    char *unitName = NULL;
    int startIdx, endIdx;
    startIdx = endIdx = -1;

    startIdx = stringLastIndexOfChr(unitPath, '/');
    if (startIdx != -1) {
        startIdx += 1;
        endIdx = stringIndexOfStr(unitPath, ".unit");
    }
    else if (startIdx == -1 && stringEndsWithStr(unitPath, ".unit")) {
        startIdx = 0;
        endIdx = stringIndexOfStr(unitPath, ".unit");
    }
    if (startIdx != -1 && endIdx != -1)
        unitName = stringSub(unitPath, startIdx, endIdx - 1);

    return unitName;
}

PType
getPTypeByPTypeStr(const char *typeStr)
{
    for (PType i = DAEMON; i <= ONESHOT; i++) {
        if (strcmp(typeStr, PTYPE_DATA_ITEMS[i].desc) == 0)
            return i;
    }
    return -1;
}

Unit*
getUnitByName(Array *units, const char *unitName)
{
    Unit *unit = NULL;
    int len = (units ? units->size : 0);
    for (int i = 0; i < len; i++) {
        unit = arrayGet(units, i);
        if (strcmp(unit->name, unitName) == 0)
            return unit;
    }
    return NULL;
}

Unit*
getUnitByPid(Array *units, pid_t pid)
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

Unit*
unitNew(Unit *unitFrom, ParserFuncType funcType)
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

    if (funcType == PARSE_SOCK_RESPONSE || funcType == PARSE_UNIT) {
        unit->type = (unitFrom ? unitFrom->type : DAEMON);
        unit->restart = (unitFrom ? unitFrom->restart : false);
        unit->restartMax = (unitFrom ? unitFrom->restartMax : -1);
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
    }

    if (funcType == PARSE_UNIT) {

        /* Set the default values */
        unit->showResult = true;
        unit->isStopping = false;
        unit->isChanged = false;

        /* Initialize mutex */
        pthread_mutex_t *mutex = NULL;
        mutex = calloc(1, sizeof(pthread_mutex_t));
        assert(mutex);
        unit->mutex = mutex;
        if ((rv = pthread_mutex_init(mutex, NULL)) != 0) {
            unitdLogError(LOG_UNITD_BOOT, "src/core/units/units.c", "unitCreate", rv, strerror(rv),
                          "Unable to run pthread_mutex_init");
        }
        assert(rv == 0);

        /* Initialize condition variable */
        pthread_cond_t *cv = NULL;
        cv = calloc(1, sizeof(pthread_cond_t));
        assert(cv);
        unit->cv = cv;
        if ((rv = pthread_cond_init(cv, NULL)) != 0) {
            unitdLogError(LOG_UNITD_BOOT, "src/core/units/units.c", "unitCreate", rv, strerror(rv),
                          "Unable to run pthread_cond_init");
        }
        assert(rv == 0);
    }

    return unit;
}

bool
isEnabledUnit(const char *unitName, State currentState)
{
    char *pattern = NULL;
    int rv, len;
    glob_t results;
    bool found = false;
    Array *statesData = NULL;
    StateData *stateData = NULL;

    rv = len = 0;

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
                    (void *)&STATE_DATA_ITEMS[STATE_CMDLINE != NO_STATE ? STATE_CMDLINE : STATE_DEFAULT]);
        }
        else
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
        stringAppendStr(&pattern, ".unit");
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

int
checkAndSetUnitPath(Unit **currentUnit, State state)
{
    int rv = 0;
    char *wherePoints, *path;
    Array **errors = NULL;
    bool hasError = false;

    wherePoints = path = NULL;

    assert(*currentUnit);
    assert(state != NO_STATE);
    errors = &(*currentUnit)->errors;
    path = (*currentUnit)->path;

    if (state == INIT || state == FINAL) {
        /* The init and final states can't contain sym links */
        if ((rv = readSymLink(path, &wherePoints)) == 0) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors,
                     getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_PATH_INIT_FINAL_ERR].desc,
                                  STATE_DATA_ITEMS[INIT].desc, STATE_DATA_ITEMS[FINAL].desc));
        }
        else if (rv == 2)
            rv = 0;
    }
    else {
        /* Check where the unit is pointing */
        rv = readSymLink(path, &wherePoints);
        if (rv == 2)
            hasError = true;
        else {
            if (!USER_INSTANCE) {
                if (!stringStartsWithStr(wherePoints, UNITS_PATH))
                    hasError = true;
            }
            else {
                if (!stringStartsWithStr(wherePoints, UNITS_USER_PATH) &&
                    !stringStartsWithStr(wherePoints, UNITS_USER_LOCAL_PATH))
                    hasError = true;
            }
        }
        if (hasError) {
            if (!(*errors))
                *errors = arrayNew(objectRelease);
            arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_PATH_ERR].desc, path));
        }
        else
            stringSet(&(*currentUnit)->path, wherePoints);
    }

    objectRelease(&wherePoints);
    return rv;
}

int
checkWantedBy(Unit **currentUnit, State currentState, bool isAggregate)
{
    int rv = 0;
    const char *currentStateStr = NULL;
    Array *wantedBy, *errors;
    const char *initStateDesc = STATE_DATA_ITEMS[INIT].desc;
    const char *finalStateDesc = STATE_DATA_ITEMS[FINAL].desc;

    wantedBy = errors = NULL;

    assert(*currentUnit);
    assert(currentState != NO_STATE);
    wantedBy = (*currentUnit)->wantedBy;
    errors = (*currentUnit)->errors;

    /* The units for the 'init' or 'final' state are excluded by the units handling */
    if (currentState != INIT && currentState != FINAL) {
        if (arrayContainsStr(wantedBy, initStateDesc) ||
            arrayContainsStr(wantedBy, finalStateDesc)) {
            rv = 1;
            arrayAdd(errors,
                     getMsg(-1, UNITS_ERRORS_ITEMS[WANTEDBY_INIT_FINAL_ERR].desc,
                                  initStateDesc, finalStateDesc));
            if (!isAggregate)
                return rv;
        }
    }
    /* We check if the unit is wanted by the current state */
    currentStateStr = STATE_DATA_ITEMS[currentState].desc;
    if (!arrayContainsStr(wantedBy, currentStateStr)) {
        rv = 1;
        arrayAdd(errors, getMsg(-1, UNITS_ERRORS_ITEMS[WANTEDBY_ERR].desc,
                                      (*currentUnit)->name, currentStateStr));
    }

    return rv;
}

int
checkConflicts(Unit **currentUnit, const char *conflictName, bool isAggregate)
{
    Array *errors = NULL;
    char *currentUnitName = NULL;
    int rv = 0;

    assert(*currentUnit);

    /* Get the unit data */
    currentUnitName = (*currentUnit)->name;
    errors = (*currentUnit)->errors;

    /* The unit cannot be in conflict with itself */
    if (strcmp(conflictName, currentUnitName) == 0) {
        rv = 1;
        arrayAdd(errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_ITSELF_ERR].desc,
                                currentUnitName));
        if (!isAggregate)
            return rv;
    }
    else {
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

int
checkRequires(Array **units, Unit **currentUnit, bool isAggregate)
{
    Array *deps, *errors;
    int lenRequires, lenRequiresDep;
    char *depName, *currentUnitName, *depDepName;
    int rv = 0;
    Unit *unitDep, *unitDepDep;

    assert(*units);
    assert(*currentUnit);

    unitDep = unitDepDep = NULL;
    deps = errors = NULL;
    currentUnitName = depName = depDepName = NULL;
    lenRequires = lenRequiresDep = 0;

    /* Get the unit data */
    currentUnitName = (*currentUnit)->name;
    deps = (*currentUnit)->requires;
    errors = (*currentUnit)->errors;
    lenRequires = (deps ? deps->size : 0);
    for (int i = 0; i < lenRequires; i++) {
        depName = arrayGet(deps, i);
        assert(depName);
        /* The unit cannot depend by itself */
        if (strcmp(depName, currentUnitName) == 0) {
            rv = 1;
            arrayAdd(errors, getMsg(-1, UNITS_ERRORS_ITEMS[REQUIRE_ITSELF_ERR].desc,
                                      currentUnitName));
            if (!isAggregate)
                return rv;
        }
        else {
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
                                            UNITS_PROPERTIES_ITEMS[REQUIRES].propertyName.desc,
                                            currentUnitName, unitDep->name, unitDepDep->name));
                    if (!isAggregate)
                        return rv;
                }
            }
        }
    }

    return rv;
}

int
loadUnits(Array **units, const char *path, const char *dirName,
          State currentState, bool isAggregate, const char *unitNameArg,
          ParserFuncType funcType, bool parse)
{
    glob_t results;
    char *pattern, *unitName, *unitPath;
    int rv, startIdx, endIdx;
    Unit *unit = NULL;
    size_t lenResults = 0;

    pattern = unitName = unitPath = NULL;
    rv = startIdx = endIdx = 0;

    assert(path);

    /* Building the pattern */
    pattern = stringNew(path);

    if (currentState != NO_STATE) {
        stringAppendChr(&pattern, '/');
        stringAppendStr(&pattern, dirName);
        stringAppendStr(&pattern, "/*.unit");
    }
    else {
        if (!unitNameArg)
            stringAppendStr(&pattern, "/*.unit");
        else {
            if (stringEndsWithStr(unitNameArg, ".unit")) {
                stringAppendChr(&pattern, '/');
                stringAppendStr(&pattern, unitNameArg);
            }
            else {
                stringAppendChr(&pattern, '/');
                stringAppendStr(&pattern, unitNameArg);
                stringAppendStr(&pattern, ".unit");
            }
        }
    }

    /* Executing the glob function */
    if (UNITD_DEBUG && currentState != NO_STATE)
        unitdLogWarning(LOG_UNITD_BOOT, "\n[*] SEARCHING THE UNITS in %s/%s ...\n", path, dirName);
    if ((rv = glob(pattern, 0, NULL, &results)) == 0) {
        lenResults = results.gl_pathc;
        assert(lenResults > 0);
        if (!(*units))
            *units = arrayNew(unitRelease);
        if (UNITD_DEBUG && currentState != NO_STATE)
            unitdLogInfo(LOG_UNITD_BOOT, "Found %d units in %s/%s!\n", lenResults, path, dirName);
        for (size_t i = 0; i < lenResults; i++) {
            /* Get the unit path */
            unitPath = results.gl_pathv[i];
            /* Set unit name */
            unitName = getUnitName(unitPath);
            /* If the unit is already in memory then skip it. (to avoid duplicates in unit list function) */
            if (getUnitByName(*units, unitName) == NULL) {
                /* Create the unit */
                unit = unitNew(NULL, funcType);
                unit->name = unitName;
                unit->path = stringNew(unitPath);
                /* Check unit path */
                if (currentState != NO_STATE) {
                    /* We always aggregate this eventual error regardless isAggregate value to allow
                     * unitctl status command to display the correct data
                    */
                    checkAndSetUnitPath(&unit, currentState);
                }
                unit->enabled = (currentState != NO_STATE ? true : isEnabledUnit(unitName, NO_STATE));
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "Unit name = '%s', path = '%s'. Parsing it ...\n", unitName, unitPath);
                /* Parse the Unit file */
                if (parse) {
                    rv = parseUnit(units, &unit, isAggregate, currentState);
                    if ((rv == 0 || isAggregate) && currentState != NO_STATE)
                        checkWantedBy(&unit, currentState, isAggregate);
                }
                /* Create the pipe */
                if (currentState != INIT && currentState != FINAL && currentState != REBOOT && currentState != POWEROFF)
                if (funcType == PARSE_UNIT && unit->errors && unit->errors->size == 0 && hasPipe(unit)) {
                    unit->pipe = pipeNew();
                    /* Create process data history array accordingly */
                    unit->processDataHistory = arrayNew(processDataRelease);
                }

                /* Adding the unit to the array */
                arrayAdd(*units, unit);
            }
            else
                objectRelease(&unitName);
        }

        if (currentState != NO_STATE) {
            /* If we are in the init or final state then show the configuration error and emergency shell */
            if (currentState == INIT || currentState == FINAL) {
                if (rv == 1) {
                    unitdLogWarning(LOG_UNITD_CONSOLE, "[*] CONFIGURATION ERRORS\n");
                    for (size_t i = 0; i < lenResults; i++) {
                        unit = arrayGet(*units, i);
                        unitdLogInfo(LOG_UNITD_CONSOLE, "Unit name = '%s'\n", unit->name);
                        arrayPrint(LOG_UNITD_CONSOLE, &((Unit *)arrayGet(*units, i))->errors, true);
                    }
                }
            }
            else {
                if (UNITD_DEBUG) {
                    unitdLogWarning(LOG_UNITD_BOOT, "\n[*] CONFIGURATION ERRORS\n");
                    for (size_t i = 0; i < lenResults; i++) {
                        unit = arrayGet(*units, i);
                        unitdLogInfo(LOG_UNITD_BOOT, "Unit name = '%s'\n", unit->name);
                        arrayPrint(LOG_UNITD_BOOT, &((Unit *)arrayGet(*units, i))->errors, true);
                    }
                }
            }
        }
    }
    else if (rv == GLOB_NOMATCH && currentState != NO_STATE) {
        /* Zero units are allowed only for Reboot and Poweroff State otherwise we show the errors */
        if (currentState != REBOOT && currentState != POWEROFF && currentState != USER)
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/units/units.c", "loadUnits", GLOB_NOMATCH,
                          "GLOB_NOMATCH", "Zero units found for %s state", STATE_DATA_ITEMS[currentState].desc);
        else {
            if (UNITD_DEBUG)
                unitdLogWarning(LOG_UNITD_BOOT, "Zero units found for %s state\n",
                                STATE_DATA_ITEMS[currentState].desc);
        }
    }

    objectRelease(&pattern);
    globfree(&results);
    return rv;
}


int parseUnit(Array **units, Unit **unit, bool isAggregate, State currentState)
{
    FILE *fp = NULL;
    int rv, numLine, sizeErrs;
    size_t len = 0;
    char *line, *error, *value, *unitPath;
    Array *lineData, **errors, *requires, *conflicts, *wantedBy;
    PropertyData *propertyData = NULL;

    numLine = rv = sizeErrs = 0;
    line = error = value = unitPath = NULL;
    lineData = requires = conflicts = wantedBy = NULL;

    assert(*unit);
    /* Initialize the parser */
    parserInit(PARSE_UNIT);
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
                objectRelease(&error);
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
                        arrayAdd(requires, stringNew(value));
                        if ((*errors)->size == 0 || isAggregate)
                             checkRequires(units, unit, isAggregate);
                        break;
                    case TYPE:
                        /* If the type is different than default */
                        if (strcmp(value, PTYPE_DATA_ITEMS[DAEMON].desc) != 0)
                            (*unit)->type = getPTypeByPTypeStr(value);
                        break;
                    case RESTART:
                        if (strcmp(value, BOOL_VALUES[true]) == 0)
                            (*unit)->restart = true;
                        else if (strcmp(value, BOOL_VALUES[false]) == 0)
                            (*unit)->restart = false;
                        break;
                    case RESTART_MAX:
                        (*unit)->restartMax = atoi(value);
                        break;
                    case CONFLICTS:
                        arrayAdd(conflicts, stringNew(value));
                        if ((*errors)->size == 0 || isAggregate)
                            checkConflicts(unit, value, isAggregate);
                        break;
                    case RUN:
                        (*unit)->runCmd = stringNew(value);
                        if (currentState == INIT || currentState == FINAL)
                            stringReplaceStr(&(*unit)->runCmd, "$UNITD_DATA_PATH", UNITD_DATA_PATH);
                        break;
                    case STOP:
                        (*unit)->stopCmd = stringNew(value);
                        if (currentState == INIT || currentState == FINAL)
                            stringReplaceStr(&(*unit)->stopCmd, "$UNITD_DATA_PATH", UNITD_DATA_PATH);
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
unitRelease(Unit **unit)
{
    Unit *unitTemp = *unit;
    char *name, *desc, *runCmd, *stopCmd, *type, *path;
    Array *conflicts, *errors, *requires, *wantedBy;
    pthread_cond_t *cv = NULL;
    pthread_mutex_t *mutex = NULL;
    int rv = 0;

    name = desc = runCmd = stopCmd = type = path = NULL;
    conflicts = errors = requires = wantedBy = NULL;

    if (unitTemp) {
        if ((name = unitTemp->name))
            objectRelease(&name);

        if ((path = unitTemp->path))
            objectRelease(&path);

        if ((conflicts = unitTemp->conflicts))
            arrayRelease(&conflicts);

        if ((desc = unitTemp->desc))
            objectRelease(&desc);

        if ((errors = unitTemp->errors))
            arrayRelease(&errors);

        if ((requires = unitTemp->requires))
            arrayRelease(&requires);

        if ((runCmd = unitTemp->runCmd))
            objectRelease(&runCmd);

        if ((stopCmd = unitTemp->stopCmd))
            objectRelease(&stopCmd);

        if ((wantedBy = unitTemp->wantedBy))
            arrayRelease(&wantedBy);

        /* Destroy and free the condition variable */
        if ((cv = unitTemp->cv)) {
            if ((rv = pthread_cond_destroy(cv)) != 0) {
                unitdLogError(LOG_UNITD_CONSOLE, "src/core/units/units.c", "unitRelease", rv,
                              strerror(rv), "Unable to run pthread_cond_destroy");
            }
            assert(rv == 0);
            objectRelease(&cv);
        }

        /* Destroy and free the mutex */
        if ((mutex = unitTemp->mutex)) {
            /* Destroy and free the mutex */
            if ((rv = pthread_mutex_destroy(mutex)) != 0) {
                unitdLogError(LOG_UNITD_CONSOLE, "src/core/units/units.c", "unitRelease", rv,
                              strerror(rv), "Unable to run pthread_mutex_destroy");
            }
            assert(rv == 0);
            objectRelease(&mutex);
        }

        /* Process Data History */
        arrayRelease(&(unitTemp->processDataHistory));

        /* Process Data */
        processDataRelease(&(unitTemp->processData));

        /* Unit */
        objectRelease(unit);
    }
}

ProcessData*
processDataNew(ProcessData *pDataFrom, ParserFuncType funcType)
{
    ProcessData *pDataRet = NULL;
    pid_t *pid = NULL;
    int *finalStatus = NULL;;
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

        //Signal number
        int *signalNum = calloc(1, sizeof(int));
        assert(signalNum);
        if (!pDataFrom)
            *signalNum = -1;
        else
            *signalNum = *pDataFrom->signalNum;
        pDataRet->signalNum = signalNum;

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

void
resetPDataForRestart(ProcessData **pData)
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

void
processDataRelease(ProcessData **pData)
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

Pipe*
pipeNew()
{
    int rv = 0;
    Pipe *pipe = NULL;
    pthread_mutex_t *mutex = NULL;

    /* Pipe */
    pipe = calloc(1, sizeof(Pipe));
    assert(pipe);

    /* Mutex */
    mutex = calloc(1, sizeof(pthread_mutex_t));
    assert(mutex);
    pipe->mutex = mutex;
    if ((rv = pthread_mutex_init(mutex, NULL)) != 0) {
        unitdLogError(LOG_UNITD_BOOT, "src/core/units/units.c", "pipeNew", rv, strerror(rv),
                      "Unable to run pthread_mutex_init");
    }
    assert(rv == 0);

    return pipe;
}

void
pipeRelease(Pipe **pipe)
{
    Pipe *pipeTemp = *pipe;
    pthread_mutex_t *mutex = NULL;
    int rv = 0;

    if (pipeTemp) {
        /* Destroy and free the mutex */
        if ((mutex = pipeTemp->mutex)) {
            if ((rv = pthread_mutex_destroy(mutex)) != 0) {
                unitdLogError(LOG_UNITD_CONSOLE, "src/core/units/units.c", "pipeRelease", rv,
                              strerror(rv), "Unable to run pthread_mutex_destroy");
            }
            assert(rv == 0);
            objectRelease(&mutex);
        }
        /* Close fds */
        close(pipeTemp->fds[0]);
        close(pipeTemp->fds[1]);
        /* Release pipe */
        objectRelease(pipe);
    }
}
