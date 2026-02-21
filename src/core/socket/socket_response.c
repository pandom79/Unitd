/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

/* COMMUNICATION PROTOCOL (RESPONSE) ACCORDING THE PARSER FUNCTIONALITY */

/* PARSE_SOCK_RESPONSE_UNITLIST functionality
Message=value1          (optional and repeatable)
Message=value2
.....
Message=valueN
Error=value1            (optional and repeatable)
Error=value2
......
Error=valueN

[Unit]                  (optional and repeatable)
Name=value              (optional and repeatable)
Enabled=value           (optional and repeatable)
Pid=value               (optional and repeatable)
PState=value            (optional and repeatable)
FinalStatus=value       (optional and repeatable)
Desc=value              (optional and repeatable)
Duration=value          (optional and repeatable)
RestartNum=value        (optional and repeatable)
Restartable=value       (optional and repeatable)
Type=value              (optional and repeatable)
NextTimeDate=value      (optional and repeatable)
LeftTimeDuration=value  (optional and repeatable)
SignalNum=value         (optional and repeatable)
*/

/* PARSE_SOCK_RESPONSE functionality
  (it will include the initial part of the unit section of the PARSE_SOCK_RESPONSE_UNITLIST functionality)

Message=value1          (optional and repeatable)
Message=value2
.....
Message=valueN
Error=value1            (optional and repeatable)
Error=value2
......
Error=valueN

[Unit]                  (optional and repeatable)
Name=value              (optional and repeatable)
Enabled=value           (optional and repeatable)
Pid=value               (optional and repeatable)
PState=value            (optional and repeatable)
FinalStatus=value       (optional and repeatable)
Desc=value              (optional and repeatable)
Duration=value          (optional and repeatable)
RestartNum=value        (optional and repeatable)
Restartable=value       (optional and repeatable)
Type=value              (optional and repeatable)
NextTimeDate=value      (optional and repeatable)
LeftTimeDuration=value  (optional and repeatable)
TimerName=value         (optional and repeatable)
TimerPState=value       (optional and repeatable)
PathUnitName=value      (optional and repeatable)
PathUnitPState=value    (optional and repeatable)
Path=value              (optional and repeatable)
RestartMax=value        (optional and repeatable)
UnitError=value1        (optional and repeatable)
UnitError=value2        (optional and repeatable)
....
UnitError=valueN        (optional and repeatable)
ExitCode=value          (optional and repeatable)
DateTimeStart=value     (optional and repeatable)
DateTimeStop=value      (optional and repeatable)
Interval=value          (optional and repeatable)
[PDataHistory]          (optional and repeatable)
PidH=value              (optional and repeatable)
ExitCodeH=value         (optional and repeatable)
PStateH=value           (optional and repeatable)
SignalNumH=value        (optional and repeatable)
FinalStatusH=value      (optional and repeatable)
DateTimeStartH=value    (optional and repeatable)
DateTimeStopH=value     (optional and repeatable)
DurationH=value         (optional and repeatable)

*/

/* Properties */
typedef enum {
    UNIT_SEC = 0,
    MESSAGE = 1,
    ERROR = 2,
    NAME = 3,
    ENABLED = 4,
    PID = 5,
    PSTATE = 6,
    FINALSTATUS = 7,
    DESC = 8,
    DURATION = 9,
    RESTARTNUM = 10,
    RESTARTABLE = 11,
    TYPE = 12,
    NEXTTIMEDATE = 13,
    LEFTTIMEDURATION = 14,
    SIGNALNUM = 15,
    IS_CHANGED = 16,
    TIMERNAME = 17,
    TIMERPSTATE = 18,
    PATHUNITNAME = 19,
    PATHUNITPSTATE = 20,
    PATH = 21,
    RESTARTMAX = 22,
    UNITERROR = 23,
    EXITCODE = 24,
    DATETIMESTART = 25,
    DATETIMESTOP = 26,
    INTERVAL = 27,
    PDATAHISTORY_SEC = 28,
    PIDH = 29,
    EXITCODEH = 30,
    PSTATEH = 31,
    SIGNALNUMH = 32,
    FINALSTATUSH = 33,
    DATETIMESTARTH = 34,
    DATETIMESTOPH = 35,
    DURATIONH = 36
} Keys;

// clang-format off
static const key_value KEY_VALUE[] = {
    { UNIT_SEC, "[Unit]" },
    { MESSAGE, "Message" },
    { ERROR, "Error" },
    { NAME, "Name" },
    { ENABLED, "Enabled" },
    { PID, "Pid" },
    { PSTATE, "PState" },
    { FINALSTATUS, "FinalStatus" },
    { DESC, "Desc" },
    { DURATION, "Duration" },
    { RESTARTNUM, "RestartNum" },
    { RESTARTABLE, "Restartable" },
    { TYPE, "Type" },
    { NEXTTIMEDATE, "NextTimeDate" },
    { LEFTTIMEDURATION, "LeftTimeDuration" },
    { SIGNALNUM, "SignalNum" },
    { IS_CHANGED, "IsChanged" },
    { TIMERNAME, "TimerName" },
    { TIMERPSTATE, "TimerPState" },
    { PATHUNITNAME, "PathUnitName" },
    { PATHUNITPSTATE, "PathUnitPState" },
    { PATH, "Path" },
    { RESTARTMAX, "RestartMax" },
    { UNITERROR, "UnitError" },
    { EXITCODE, "ExitCode" },
    { DATETIMESTART, "DateTimeStart" },
    { DATETIMESTOP, "DateTimeStop" },
    { INTERVAL, "Interval" },
    { PDATAHISTORY_SEC, "[PDataHistory]" },
    { PIDH, "PidH" },
    { EXITCODEH, "ExitCodeH" },
    { PSTATEH, "PStateH" },
    { SIGNALNUMH, "SignalNumH" },
    { FINALSTATUSH, "FinalStatusH" },
    { DATETIMESTARTH, "DateTimeStartH" },
    { DATETIMESTOPH, "DateTimeStopH" },
    { DURATIONH, "DurationH" },
};
// clang-format on

char *marshallResponse(SockMessageOut *sockMessageOut, ParserFuncType funcType)
{
    char *buffer = NULL;
    Array *messages = NULL, *errors = NULL, *units = NULL, *unitErrors = NULL, *pDataHistory = NULL;
    int len = 0, lenUnitErrors = 0, lenPdataHistory = 0;
    const char *msgKey = NULL, *errKey = NULL, *unitDesc = NULL, *unitPath, *dateTimeStart,
               *dateTimeStop, *unitErrorKey = NULL, *pDataHistorySecKey = NULL, *pidHKey = NULL,
               *exitCodeHKey = NULL, *pStateHKey = NULL, *signalNumHKey = NULL,
               *finalStatusHKey = NULL, *datetimeStartHKey = NULL, *datetimeStopHKey = NULL,
               *duration, *durationKey = NULL;
    Unit *unit = NULL;
    ProcessData *pData = NULL;

    assert(sockMessageOut);

    /* The following data (messages and errors) are in common between
    * PARSE_SOCK_RESPONSE_UNITLIST and PARSE_SOCK_RESPONSE
    */
    /* Messages */
    messages = sockMessageOut->messages;
    len = (messages ? messages->size : 0);
    if (len > 0)
        msgKey = KEY_VALUE[MESSAGE].value;
    for (int i = 0; i < len; i++) {
        if (i == 0 && !buffer)
            buffer = stringNew(msgKey);
        else
            stringAppendStr(&buffer, msgKey);

        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, arrayGet(messages, i));
        stringAppendStr(&buffer, TOKEN);
    }
    /* Errors */
    errors = sockMessageOut->errors;
    len = (errors ? errors->size : 0);
    if (len > 0)
        errKey = KEY_VALUE[ERROR].value;
    for (int i = 0; i < len; i++) {
        if (i == 0 && !buffer)
            buffer = stringNew(errKey);
        else
            stringAppendStr(&buffer, errKey);

        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, arrayGet(errors, i));
        stringAppendStr(&buffer, TOKEN);
    }
    /* Units */
    units = sockMessageOut->unitsDisplay;
    len = (units ? units->size : 0);
    for (int i = 0; i < len; i++) {
        unit = arrayGet(units, i);
        pData = unit->processData;
        /* Unit section */
        if (i == 0 && !buffer)
            buffer = stringNew(KEY_VALUE[UNIT_SEC].value);
        else
            stringAppendStr(&buffer, KEY_VALUE[UNIT_SEC].value);
        stringAppendStr(&buffer, TOKEN);
        /* The following data are in common between
         * PARSE_SOCK_RESPONSE_UNITLIST and PARSE_SOCK_RESPONSE
        */
        /* Name */
        stringAppendStr(&buffer, KEY_VALUE[NAME].value);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, unit->name);
        stringAppendStr(&buffer, TOKEN);
        /* Enabled */
        stringAppendStr(&buffer, KEY_VALUE[ENABLED].value);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, (unit->enabled ? "1" : "0"));
        stringAppendStr(&buffer, TOKEN);
        /* Pid */
        stringAppendStr(&buffer, KEY_VALUE[PID].value);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->pid);
        stringAppendStr(&buffer, TOKEN);
        /* Process State */
        stringAppendStr(&buffer, KEY_VALUE[PSTATE].value);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, pData->pStateData->pState);
        stringAppendStr(&buffer, TOKEN);
        /* Final Status */
        stringAppendStr(&buffer, KEY_VALUE[FINALSTATUS].value);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->finalStatus);
        stringAppendStr(&buffer, TOKEN);
        /* Description */
        unitDesc = unit->desc;
        stringAppendStr(&buffer, KEY_VALUE[DESC].value);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, (unitDesc ? unitDesc : NONE));
        stringAppendStr(&buffer, TOKEN);
        /* Duration */
        duration = pData->duration;
        stringAppendStr(&buffer, KEY_VALUE[DURATION].value);
        stringAppendStr(&buffer, ASSIGNER);
        if (duration)
            stringAppendStr(&buffer, duration);
        else {
            if (pData->timeStart) {
                Time *currentTimeStop = timeNew(NULL);
                char *diff = stringGetDiffTime(currentTimeStop, pData->timeStart);
                stringAppendStr(&buffer, diff);
                timeRelease(&currentTimeStop);
                objectRelease(&diff);
            } else
                stringAppendStr(&buffer, NONE);
        }
        stringAppendStr(&buffer, TOKEN);
        /* RestartNum */
        stringAppendStr(&buffer, KEY_VALUE[RESTARTNUM].value);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, unit->restartNum);
        stringAppendStr(&buffer, TOKEN);
        /* Restartable */
        stringAppendStr(&buffer, KEY_VALUE[RESTARTABLE].value);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, ((unit->restart || unit->restartMax > 0) ? "1" : "0"));
        stringAppendStr(&buffer, TOKEN);
        /* Type */
        stringAppendStr(&buffer, KEY_VALUE[TYPE].value);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, unit->type);
        stringAppendStr(&buffer, TOKEN);
        /* Next time (date) */
        char *nextTimeDate = unit->nextTimeDate;
        if (nextTimeDate && strlen(nextTimeDate) > 0) {
            stringAppendStr(&buffer, KEY_VALUE[NEXTTIMEDATE].value);
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, nextTimeDate);
            stringAppendStr(&buffer, TOKEN);
        }
        /* Left time (duration) */
        char *leftTimeDuration = unit->leftTimeDuration;
        if (leftTimeDuration && strlen(leftTimeDuration) > 0) {
            setLeftTimeAndDuration(&unit);
            stringAppendStr(&buffer, KEY_VALUE[LEFTTIMEDURATION].value);
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, leftTimeDuration);
            stringAppendStr(&buffer, TOKEN);
        }
        /* Signal Num */
        stringAppendStr(&buffer, KEY_VALUE[SIGNALNUM].value);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->signalNum);
        stringAppendStr(&buffer, TOKEN);
        /* Unit content is changed */
        if (unit->isChanged) {
            stringAppendStr(&buffer, KEY_VALUE[IS_CHANGED].value);
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, "1");
            stringAppendStr(&buffer, TOKEN);
        }
        if (funcType == PARSE_SOCK_RESPONSE) {
            /* Timer name */
            char *timerName = unit->timerName;
            if (timerName) {
                stringAppendStr(&buffer, KEY_VALUE[TIMERNAME].value);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, timerName);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Timer process state */
            PState *timerPState = unit->timerPState;
            if (timerPState) {
                stringAppendStr(&buffer, KEY_VALUE[TIMERPSTATE].value);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *timerPState);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Path unit name */
            char *pathUnitName = unit->pathUnitName;
            if (pathUnitName) {
                stringAppendStr(&buffer, KEY_VALUE[PATHUNITNAME].value);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pathUnitName);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Path unit process state */
            PState *pathUnitPState = unit->pathUnitPState;
            if (pathUnitPState) {
                stringAppendStr(&buffer, KEY_VALUE[PATHUNITPSTATE].value);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pathUnitPState);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Path */
            unitPath = unit->path;
            stringAppendStr(&buffer, KEY_VALUE[PATH].value);
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, (unitPath ? unitPath : NONE));
            stringAppendStr(&buffer, TOKEN);
            /* RestartMax */
            stringAppendStr(&buffer, KEY_VALUE[RESTARTMAX].value);
            stringAppendStr(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, unit->restartMax);
            stringAppendStr(&buffer, TOKEN);
            /* Unit errors */
            unitErrors = unit->errors;
            lenUnitErrors = (unitErrors ? unitErrors->size : 0);
            for (int j = 0; j < lenUnitErrors; j++) {
                if (!unitErrorKey)
                    unitErrorKey = KEY_VALUE[UNITERROR].value;
                stringAppendStr(&buffer, unitErrorKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, arrayGet(unitErrors, j));
                stringAppendStr(&buffer, TOKEN);
            }
            /* Exit code */
            stringAppendStr(&buffer, KEY_VALUE[EXITCODE].value);
            stringAppendStr(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, *pData->exitCode);
            stringAppendStr(&buffer, TOKEN);
            /* Date Time Start */
            dateTimeStart = pData->dateTimeStartStr;
            stringAppendStr(&buffer, KEY_VALUE[DATETIMESTART].value);
            stringAppendStr(&buffer, ASSIGNER);
            if (dateTimeStart)
                stringAppendStr(&buffer, dateTimeStart);
            else
                stringAppendStr(&buffer, NONE);
            stringAppendStr(&buffer, TOKEN);
            /* Date Time Stop */
            dateTimeStop = pData->dateTimeStopStr;
            stringAppendStr(&buffer, KEY_VALUE[DATETIMESTOP].value);
            stringAppendStr(&buffer, ASSIGNER);
            if (dateTimeStop)
                stringAppendStr(&buffer, dateTimeStop);
            else
                stringAppendStr(&buffer, NONE);
            stringAppendStr(&buffer, TOKEN);
            /* Interval */
            char *intervalStr = unit->intervalStr;
            if (intervalStr && strlen(intervalStr) > 0) {
                stringAppendStr(&buffer, KEY_VALUE[INTERVAL].value);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, intervalStr);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Process Data history */
            pDataHistory = unit->processDataHistory;
            lenPdataHistory = (pDataHistory ? pDataHistory->size : 0);
            if (lenPdataHistory > 0)
                pDataHistorySecKey = KEY_VALUE[PDATAHISTORY_SEC].value;
            for (int j = 0; j < lenPdataHistory; j++) {
                pData = arrayGet(pDataHistory, j);
                stringAppendStr(&buffer, pDataHistorySecKey);
                stringAppendStr(&buffer, TOKEN);
                /* Pid history */
                if (!pidHKey)
                    pidHKey = KEY_VALUE[PIDH].value;
                stringAppendStr(&buffer, pidHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->pid);
                stringAppendStr(&buffer, TOKEN);
                /* Exit code history */
                if (!exitCodeHKey)
                    exitCodeHKey = KEY_VALUE[EXITCODEH].value;
                stringAppendStr(&buffer, exitCodeHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->exitCode);
                stringAppendStr(&buffer, TOKEN);
                /* Process State History */
                if (!pStateHKey)
                    pStateHKey = KEY_VALUE[PSTATEH].value;
                stringAppendStr(&buffer, pStateHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, pData->pStateData->pState);
                stringAppendStr(&buffer, TOKEN);
                /* Signal number History */
                if (!signalNumHKey)
                    signalNumHKey = KEY_VALUE[SIGNALNUMH].value;
                stringAppendStr(&buffer, signalNumHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->signalNum);
                stringAppendStr(&buffer, TOKEN);
                /* Final status History */
                if (!finalStatusHKey)
                    finalStatusHKey = KEY_VALUE[FINALSTATUSH].value;
                stringAppendStr(&buffer, finalStatusHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->finalStatus);
                stringAppendStr(&buffer, TOKEN);
                /* Date time start history */
                if (!datetimeStartHKey)
                    datetimeStartHKey = KEY_VALUE[DATETIMESTARTH].value;
                stringAppendStr(&buffer, datetimeStartHKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pData->dateTimeStartStr);
                stringAppendStr(&buffer, TOKEN);
                /* Date time stop history */
                if (!datetimeStopHKey)
                    datetimeStopHKey = KEY_VALUE[DATETIMESTOPH].value;
                stringAppendStr(&buffer, datetimeStopHKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pData->dateTimeStopStr);
                stringAppendStr(&buffer, TOKEN);
                /* Duration history */
                if (!durationKey)
                    durationKey = KEY_VALUE[DURATIONH].value;
                stringAppendStr(&buffer, durationKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pData->duration);
                stringAppendStr(&buffer, TOKEN);
            }
        }
    }

    return buffer;
}

int unmarshallResponse(char *buffer, SockMessageOut **sockMessageOut)
{
    int rv = 0, lenBuffer = 0;
    Array **unitsDisplay, **unitErrors, **messages, **sockErrors, **pDatasHistory;
    char *value = NULL, key[BUFSIZ], c = 0, entries[BUFSIZ];
    Unit *unitDisplay = NULL;
    ProcessData *pData = NULL, *pDataHistory = NULL;

    assert(buffer);
    assert(*sockMessageOut);

    stringCopy(entries, "");
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    messages = &(*sockMessageOut)->messages;
    sockErrors = &(*sockMessageOut)->errors;
    lenBuffer = buffer ? strlen(buffer) : 0;
    for (int i = 0; i < lenBuffer; i++) {
        c = buffer[i];
        if (c != TOKEN[0]) {
            char chrStr[] = { c, '\0' };
            strcat(entries, chrStr);
            continue;
        } else {
            value = strstr(entries, ASSIGNER);
            if (value) {
                value++;
                stringCopyN(key, entries, strlen(entries) - strlen(value) - 1);
            } else
                stringCopy(key, entries);
            if (!value) {
                // SECTIONS
                if (stringEquals(key, KEY_VALUE[UNIT_SEC].value)) {
                    if (!(*unitsDisplay))
                        *unitsDisplay = arrayNew(unitRelease);
                    unitDisplay = unitNew(NULL, PARSE_SOCK_RESPONSE);
                    pData = unitDisplay->processData;
                    pDatasHistory = &unitDisplay->processDataHistory;
                    unitErrors = &unitDisplay->errors;
                    arrayAdd(*unitsDisplay, unitDisplay);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[PDATAHISTORY_SEC].value)) {
                    if (!(*pDatasHistory))
                        *pDatasHistory = arrayNew(processDataRelease);
                    pDataHistory = processDataNew(NULL, PARSE_SOCK_RESPONSE);
                    arrayAdd(*pDatasHistory, pDataHistory);
                    goto next;
                }
                // Should never happen
                logError(CONSOLE | SYSTEM, "src/core/socket/socket_response.c",
                         "unmarshallResponse", EPERM, strerror(EPERM), "Section %s not found!",
                         key);
                rv = 1;
                goto out;
            } else {
                // PROPERTIES
                if (stringEquals(key, KEY_VALUE[MESSAGE].value)) {
                    if (!(*messages))
                        *messages = arrayNew(objectRelease);
                    arrayAdd(*messages, stringNew(value));
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[ERROR].value)) {
                    if (!(*sockErrors))
                        *sockErrors = arrayNew(objectRelease);
                    arrayAdd(*sockErrors, stringNew(value));
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[NAME].value)) {
                    unitDisplay->name = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[DESC].value)) {
                    if (stringEquals(value, NONE))
                        unitDisplay->desc = NULL;
                    else
                        unitDisplay->desc = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[PATH].value)) {
                    unitDisplay->path = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[ENABLED].value)) {
                    unitDisplay->enabled = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[RESTARTABLE].value)) {
                    unitDisplay->restart = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[NEXTTIMEDATE].value)) {
                    unitDisplay->nextTimeDate = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[LEFTTIMEDURATION].value)) {
                    unitDisplay->leftTimeDuration = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[TIMERNAME].value)) {
                    unitDisplay->timerName = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[TIMERPSTATE].value)) {
                    unitDisplay->timerPState = calloc(1, sizeof(PState));
                    assert(unitDisplay->timerPState);
                    *unitDisplay->timerPState = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[PATHUNITNAME].value)) {
                    unitDisplay->pathUnitName = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[PATHUNITPSTATE].value)) {
                    unitDisplay->pathUnitPState = calloc(1, sizeof(PState));
                    assert(unitDisplay->pathUnitPState);
                    *unitDisplay->pathUnitPState = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[RESTARTNUM].value)) {
                    unitDisplay->restartNum = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[RESTARTMAX].value)) {
                    if (stringEquals(value, NONE))
                        unitDisplay->restartMax = -1;
                    else
                        unitDisplay->restartMax = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[TYPE].value)) {
                    unitDisplay->type = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[UNITERROR].value)) {
                    if (!(*unitErrors))
                        *unitErrors = arrayNew(objectRelease);
                    arrayAdd(*unitErrors, stringNew(value));
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[PID].value)) {
                    if (stringEquals(value, NONE))
                        *pData->pid = -1;
                    else
                        *pData->pid = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[EXITCODE].value)) {
                    if (stringEquals(value, NONE))
                        *pData->exitCode = -1;
                    else
                        *pData->exitCode = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[PSTATE].value)) {
                    *pData->pStateData = PSTATE_DATA_ITEMS[atoi(value)];
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[SIGNALNUM].value)) {
                    if (stringEquals(value, NONE))
                        *pData->signalNum = -1;
                    else
                        *pData->signalNum = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[IS_CHANGED].value)) {
                    unitDisplay->isChanged = true;
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[FINALSTATUS].value)) {
                    if (stringEquals(value, NONE))
                        *pData->finalStatus = FINAL_STATUS_READY;
                    else
                        *pData->finalStatus = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[DATETIMESTART].value)) {
                    if (stringEquals(value, NONE))
                        pData->dateTimeStartStr = NULL;
                    else
                        pData->dateTimeStartStr = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[DATETIMESTOP].value)) {
                    if (stringEquals(value, NONE))
                        pData->dateTimeStopStr = NULL;
                    else
                        pData->dateTimeStopStr = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[DURATION].value)) {
                    if (stringEquals(value, NONE))
                        pData->duration = NULL;
                    else
                        pData->duration = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[INTERVAL].value)) {
                    unitDisplay->intervalStr = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[PIDH].value)) {
                    if (stringEquals(value, NONE))
                        *pDataHistory->pid = -1;
                    else
                        *pDataHistory->pid = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[EXITCODEH].value)) {
                    if (stringEquals(value, NONE))
                        *pDataHistory->exitCode = -1;
                    else
                        *pDataHistory->exitCode = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[PSTATEH].value)) {
                    *pDataHistory->pStateData = PSTATE_DATA_ITEMS[atoi(value)];
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[SIGNALNUMH].value)) {
                    if (stringEquals(value, NONE))
                        *pDataHistory->signalNum = -1;
                    else
                        *pDataHistory->signalNum = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[FINALSTATUSH].value)) {
                    if (stringEquals(value, NONE))
                        *pDataHistory->finalStatus = FINAL_STATUS_READY;
                    else
                        *pDataHistory->finalStatus = atoi(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[DATETIMESTARTH].value)) {
                    pDataHistory->dateTimeStartStr = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[DATETIMESTOPH].value)) {
                    if (stringEquals(value, NONE))
                        pDataHistory->dateTimeStopStr = NULL;
                    else
                        pDataHistory->dateTimeStopStr = stringNew(value);
                    goto next;
                }
                if (stringEquals(key, KEY_VALUE[DURATIONH].value)) {
                    if (stringEquals(value, NONE))
                        pDataHistory->duration = NULL;
                    else
                        pDataHistory->duration = stringNew(value);
                    goto next;
                }
                // Should never happen
                logError(CONSOLE | SYSTEM, "src/core/socket/socket_response.c",
                         "unmarshallResponse", EPERM, strerror(EPERM), "Property %s not found!",
                         key);
                rv = EPERM;
                goto out;
            }
        }
next:
        stringCopy(entries, "");
        stringCopy(key, "");
        continue;
    }

out:
    return rv;
}
