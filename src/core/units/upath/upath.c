/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../unitd_impl.h"

//INIT PARSER CONFIGURATION
enum SectionNameEnum { UNIT = 0, PATH = 1, STATE = 2 };
enum PropertyNameEnum {
    DESCRIPTION = 0,
    REQUIRES = 1,
    CONFLICTS = 2,
    PATH_EXISTS = 3,
    PATH_EXISTS_GLOB = 4,
    PATH_RESOURCE_CHANGED = 5,
    PATH_DIRECTORY_NOT_EMPTY = 6,
    WANTEDBY = 7
};
int UPATH_SECTIONS_ITEMS_LEN = 3;
SectionData UPATH_SECTIONS_ITEMS[] = { { { UNIT, "[Unit]" }, false, true, 0 },
                                       { { PATH, "[Path]" }, false, true, 0 },
                                       { { STATE, "[State]" }, false, true, 0 } };
static const char *WANTEDBY_VALUES[] = { STATE_DATA_ITEMS[SINGLE_USER].desc,
                                         STATE_DATA_ITEMS[MULTI_USER].desc,
                                         STATE_DATA_ITEMS[MULTI_USER_NET].desc,
                                         STATE_DATA_ITEMS[CUSTOM].desc,
                                         STATE_DATA_ITEMS[GRAPHICAL].desc,
                                         STATE_DATA_ITEMS[USER].desc,
                                         NULL };
int UPATH_PROPERTIES_ITEMS_LEN = 8;
// clang-format off
PropertyData UPATH_PROPERTIES_ITEMS[] = {
    { UNIT,  { DESCRIPTION, "Description" }, false, true, false, 0, NULL, NULL },
    { UNIT,  { REQUIRES, "Requires" }, true, false, false, 0, NULL, NULL },
    { UNIT,  { CONFLICTS, "Conflict" }, true, false, false, 0, NULL, NULL },
    { PATH,  { PATH_EXISTS, "PathExists" }, false, false, false, 0, NULL, NULL },
    { PATH,  { PATH_EXISTS_GLOB, "PathExistsGlob" }, false, false, false, 0, NULL, NULL },
    { PATH,  { PATH_RESOURCE_CHANGED, "PathResourceChanged" }, false, false, false, 0, NULL, NULL },
    { PATH,  { PATH_DIRECTORY_NOT_EMPTY, "PathDirectoryNotEmpty" }, false, false, false, 0, NULL, NULL },
    { STATE, { WANTEDBY, "WantedBy" }, true, true, false, 0, WANTEDBY_VALUES, NULL }
};
// clang-format on
//END PARSER CONFIGURATION

int checkWatchers(Unit **unit, bool isAggregate)
{
    int rv = 0;
    bool watcherFound = false;
    enum PropertyNameEnum propertyName;
    const char *watchPath = NULL;
    char **watchMonitorPath = NULL;

    assert(*unit);

    for (WatcherType watcherType = PATH_EXISTS_WATCHER;
         watcherType <= PATH_DIRECTORY_NOT_EMPTY_WATCHER; watcherType++) {
        watchPath = NULL;
        watchMonitorPath = NULL;
        switch (watcherType) {
        case PATH_EXISTS_WATCHER:
            watchPath = (*unit)->pathExists;
            watchMonitorPath = &(*unit)->pathExistsMonitor;
            propertyName = PATH_EXISTS;
            break;
        case PATH_EXISTS_GLOB_WATCHER:
            watchPath = (*unit)->pathExistsGlob;
            watchMonitorPath = &(*unit)->pathExistsGlobMonitor;
            propertyName = PATH_EXISTS_GLOB;
            break;
        case PATH_RESOURCE_CHANGED_WATCHER:
            watchPath = (*unit)->pathResourceChanged;
            watchMonitorPath = &(*unit)->pathResourceChangedMonitor;
            propertyName = PATH_RESOURCE_CHANGED;
            break;
        case PATH_DIRECTORY_NOT_EMPTY_WATCHER:
            watchPath = (*unit)->pathDirectoryNotEmpty;
            watchMonitorPath = &(*unit)->pathDirectoryNotEmptyMonitor;
            /* In this case watchMonitorPath is equal to watchPath. */
            *watchMonitorPath = stringNew(watchPath);
            propertyName = PATH_DIRECTORY_NOT_EMPTY;
            break;
        default:
            break;
        }
        if (watchPath) {
            if (!watcherFound)
                watcherFound = true;
            if (watchPath[0] == '/') {
                /* Extract the level up folder */
                if (!(*watchMonitorPath))
                    *watchMonitorPath =
                        stringSub(watchPath, 0, stringLastIndexOfChr(watchPath, '/'));
                /* should never happen */
                if (!(*watchMonitorPath)) {
                    logError(CONSOLE | SYSTEM, "src/core/units/upath/upath.c", "checkWatchers",
                             EPERM, strerror(EPERM), "The monitor path is null for %s property",
                             UPATH_PROPERTIES_ITEMS[propertyName].property.desc);
                    kill(UNITD_PID, SIGTERM);
                }
                /* Check if we can access there in read mode because
                 * inotify_add_watch func might return an error.
                */
                if ((rv = access(*watchMonitorPath, R_OK)) != 0) {
                    arrayAdd((*unit)->errors,
                             getMsg(-1, UNITS_ERRORS_ITEMS[UPATH_ACCESS_ERR].desc,
                                    UPATH_PROPERTIES_ITEMS[propertyName].property.desc));
                    if (!isAggregate)
                        goto out;
                }
            } else {
                rv = 1;
                arrayAdd((*unit)->errors,
                         getMsg(-1, UNITS_ERRORS_ITEMS[UPATH_WELL_FORMED_PATH_ERR].desc,
                                UPATH_PROPERTIES_ITEMS[propertyName].property.desc));
                if (!isAggregate)
                    goto out;
            }
        }
    }
    if (!watcherFound) {
        arrayAdd((*unit)->errors, getMsg(-1, UNITS_ERRORS_ITEMS[UPATH_PATH_SEC_ERR].desc));
        rv = 1;
    }

out:
    return rv;
}

void addWatchers(Unit **unit)
{
    Notifier *notifier = NULL;
    const char *watchPathMonitor = NULL;

    assert(*unit);

    notifier = (*unit)->notifier;
    if (!notifier) {
        notifier = notifierNew();
        (*unit)->notifier = notifier;
    }
    assert(notifier);
    for (WatcherType watcherType = PATH_EXISTS_WATCHER;
         watcherType <= PATH_DIRECTORY_NOT_EMPTY_WATCHER; watcherType++) {
        watchPathMonitor = NULL;
        switch (watcherType) {
        case PATH_EXISTS_WATCHER:
            watchPathMonitor = (*unit)->pathExistsMonitor;
            break;
        case PATH_EXISTS_GLOB_WATCHER:
            watchPathMonitor = (*unit)->pathExistsGlobMonitor;
            break;
        case PATH_RESOURCE_CHANGED_WATCHER:
            watchPathMonitor = (*unit)->pathResourceChangedMonitor;
            break;
        case PATH_DIRECTORY_NOT_EMPTY_WATCHER:
            watchPathMonitor = (*unit)->pathDirectoryNotEmptyMonitor;
            break;
        default:
            break;
        }
        if (watchPathMonitor)
            arrayAdd(notifier->watchers, watcherNew(notifier, watchPathMonitor, watcherType));
    }
    if (notifierInit(notifier) != 0)
        arrayAdd((*unit)->errors, getMsg(-1, UNITD_ERRORS_ITEMS[UNITD_GENERIC_ERR].desc));
}

int parsePathUnit(Array **units, Unit **unit, bool isAggregate)
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

    parserInit(UPATH_SECTIONS_ITEMS_LEN, UPATH_SECTIONS_ITEMS, UPATH_PROPERTIES_ITEMS_LEN,
               UPATH_PROPERTIES_ITEMS);
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
    UPATH_PROPERTIES_ITEMS[REQUIRES].notDupValues = requires;
    UPATH_PROPERTIES_ITEMS[CONFLICTS].notDupValues = conflicts;
    UPATH_PROPERTIES_ITEMS[WANTEDBY].notDupValues = wantedBy;
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
                    case CONFLICTS:
                        conflict = stringNew(value);
                        arrayAdd(conflicts, conflict);
                        if ((*errors)->size == 0 || isAggregate)
                            checkConflicts(unit, value, isAggregate);
                        break;
                    case PATH_EXISTS:
                        (*unit)->pathExists = stringNew(value);
                        break;
                    case PATH_EXISTS_GLOB:
                        (*unit)->pathExistsGlob = stringNew(value);
                        break;
                    case PATH_RESOURCE_CHANGED:
                        (*unit)->pathResourceChanged = stringNew(value);
                        break;
                    case PATH_DIRECTORY_NOT_EMPTY:
                        (*unit)->pathDirectoryNotEmpty = stringNew(value);
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
