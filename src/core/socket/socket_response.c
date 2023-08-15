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
    TIMERNAME = 16,
    TIMERPSTATE = 17,
    PATHUNITNAME = 18,
    PATHUNITPSTATE = 19,
    PATH = 20,
    RESTARTMAX = 21,
    UNITERROR = 22,
    EXITCODE = 23,
    DATETIMESTART = 24,
    DATETIMESTOP = 25,
    INTERVAL = 26,
    PDATAHISTORY_SEC = 27,
    PIDH = 28,
    EXITCODEH = 29,
    PSTATEH = 30,
    SIGNALNUMH = 31,
    FINALSTATUSH = 32,
    DATETIMESTARTH = 33,
    DATETIMESTOPH = 34,
    DURATIONH = 35
} Keys;

static const char*
asStr(Keys key)
{
    assert(key >= UNIT_SEC);
    switch (key) {
        case UNIT_SEC:
            return "[Unit]";
        case PDATAHISTORY_SEC:
            return "[PDataHistory]";
        case MESSAGE:
            return "Message";
        case ERROR:
            return "Error";
        case NAME:
            return "Name";
        case ENABLED:
            return "Enabled";
        case PID:
            return "Pid";
        case PSTATE:
            return "PState";
        case FINALSTATUS:
            return "FinalStatus";
        case DESC:
            return "Desc";
        case DURATION:
            return "Duration";
        case RESTARTNUM:
            return "RestartNum";
        case RESTARTABLE:
            return "Restartable";
        case TYPE:
            return "Type";
        case NEXTTIMEDATE:
            return "NextTimeDate";
        case LEFTTIMEDURATION:
            return "LeftTimeDuration";
        case TIMERNAME:
            return "TimerName";
        case TIMERPSTATE:
            return "TimerPState";
        case PATHUNITNAME:
            return "PathUnitName";
        case PATHUNITPSTATE:
            return "PathUnitPState";
        case PATH:
            return "Path";
        case RESTARTMAX:
            return "RestartMax";
        case UNITERROR:
            return "UnitError";
        case EXITCODE:
            return "ExitCode";
        case SIGNALNUM:
            return "SignalNum";
        case DATETIMESTART:
            return "DateTimeStart";
        case DATETIMESTOP:
            return "DateTimeStop";
        case INTERVAL:
            return "Interval";
        case PIDH:
            return "PidH";
        case EXITCODEH:
            return "ExitCodeH";
        case PSTATEH:
            return "PStateH";
        case SIGNALNUMH:
            return "SignalNumH";
        case FINALSTATUSH:
            return "FinalStatusH";
        case DATETIMESTARTH:
            return "DateTimeStartH";
        case DATETIMESTOPH:
            return "DateTimeStopH";
        case DURATIONH:
            return "DurationH";
        default:
            return "";
    }

}

char*
marshallResponse(SockMessageOut *sockMessageOut, ParserFuncType funcType)
{
    char *buffer = NULL;
    Array *messages, *errors, *units, *unitErrors, *pDataHistory;
    int len, lenUnitErrors, lenPdataHistory;
    const char *msgKey, *errKey, *unitDesc, *unitPath, *dateTimeStart, *dateTimeStop,
               *unitErrorKey, *pDataHistorySecKey, *pidHKey, *exitCodeHKey,
               *pStateHKey, *signalNumHKey, *finalStatusHKey, *datetimeStartHKey,
               *datetimeStopHKey, *duration, *durationKey;
    Unit *unit = NULL;
    ProcessData *pData = NULL;

    messages = errors = units = unitErrors = pDataHistory = NULL;
    len = lenUnitErrors = lenPdataHistory = 0;
    msgKey = errKey = unitDesc = unitErrorKey = pDataHistorySecKey =
    pidHKey = exitCodeHKey = pStateHKey = signalNumHKey =
    finalStatusHKey = datetimeStartHKey = datetimeStopHKey = durationKey = NULL;

    assert(sockMessageOut);

    /* The following data (messages and errors) are in common between
    * PARSE_SOCK_RESPONSE_UNITLIST and PARSE_SOCK_RESPONSE
    */

    /* Messages */
    messages = sockMessageOut->messages;
    len = (messages ? messages->size : 0);
    if (len > 0)
        msgKey = asStr(MESSAGE);
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
        errKey = asStr(ERROR);
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
        /* Process Data */
        pData = unit->processData;

        /* Unit section */
        if (i == 0 && !buffer)
            buffer = stringNew(asStr(UNIT_SEC));
        else
            stringAppendStr(&buffer, asStr(UNIT_SEC));

        stringAppendStr(&buffer, TOKEN);

        /* The following data are in common between
         * PARSE_SOCK_RESPONSE_UNITLIST and PARSE_SOCK_RESPONSE
        */

        /* Name */
        stringAppendStr(&buffer, asStr(NAME));
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, unit->name);
        stringAppendStr(&buffer, TOKEN);
        /* Enabled */
        stringAppendStr(&buffer, asStr(ENABLED));
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, (unit->enabled ? "1" : "0"));
        stringAppendStr(&buffer, TOKEN);
        /* Pid */
        stringAppendStr(&buffer, asStr(PID));
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->pid);
        stringAppendStr(&buffer, TOKEN);
        /* Process State */
        stringAppendStr(&buffer, asStr(PSTATE));
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, pData->pStateData->pState);
        stringAppendStr(&buffer, TOKEN);
        /* Final Status */
        stringAppendStr(&buffer, asStr(FINALSTATUS));
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->finalStatus);
        stringAppendStr(&buffer, TOKEN);
        /* Description */
        unitDesc = unit->desc;
        stringAppendStr(&buffer, asStr(DESC));
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, (unitDesc ? unitDesc : NONE));
        stringAppendStr(&buffer, TOKEN);
        /* Duration */
        duration = pData->duration;
        stringAppendStr(&buffer, asStr(DURATION));
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
            }
            else
                stringAppendStr(&buffer, NONE);
        }
        stringAppendStr(&buffer, TOKEN);
        /* RestartNum */
        stringAppendStr(&buffer, asStr(RESTARTNUM));
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, unit->restartNum);
        stringAppendStr(&buffer, TOKEN);
        /* Restartable */
        stringAppendStr(&buffer, asStr(RESTARTABLE));
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, ((unit->restart || unit->restartMax > 0) ? "1" : "0"));
        stringAppendStr(&buffer, TOKEN);
        /* Type */
        stringAppendStr(&buffer, asStr(TYPE));
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, unit->type);
        stringAppendStr(&buffer, TOKEN);
        /* Next time (date) */
        char *nextTimeDate = unit->nextTimeDate;
        if (nextTimeDate && strlen(nextTimeDate) > 0) {
            stringAppendStr(&buffer, asStr(NEXTTIMEDATE));
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, nextTimeDate);
            stringAppendStr(&buffer, TOKEN);
        }
        /* Left time (duration) */
        char *leftTimeDuration = unit->leftTimeDuration;
        if (leftTimeDuration && strlen(leftTimeDuration) > 0) {
            setLeftTimeAndDuration(&unit);
            stringAppendStr(&buffer, asStr(LEFTTIMEDURATION));
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, leftTimeDuration);
            stringAppendStr(&buffer, TOKEN);
        }
        /* Signal Num */
        stringAppendStr(&buffer, asStr(SIGNALNUM));
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->signalNum);
        stringAppendStr(&buffer, TOKEN);

        if (funcType == PARSE_SOCK_RESPONSE) {
            /* Timer name */
            char *timerName = unit->timerName;
            if (timerName) {
                stringAppendStr(&buffer, asStr(TIMERNAME));
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, timerName);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Timer process state */
            PState *timerPState = unit->timerPState;
            if (timerPState) {
                stringAppendStr(&buffer, asStr(TIMERPSTATE));
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *timerPState);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Path unit name */
            char *pathUnitName = unit->pathUnitName;
            if (pathUnitName) {
                stringAppendStr(&buffer, asStr(PATHUNITNAME));
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pathUnitName);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Path unit process state */
            PState *pathUnitPState = unit->pathUnitPState;
            if (pathUnitPState) {
                stringAppendStr(&buffer, asStr(PATHUNITPSTATE));
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pathUnitPState);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Path */
            unitPath = unit->path;
            stringAppendStr(&buffer, asStr(PATH));
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, (unitPath ? unitPath : NONE));
            stringAppendStr(&buffer, TOKEN);
            /* RestartMax */
            stringAppendStr(&buffer, asStr(RESTARTMAX));
            stringAppendStr(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, unit->restartMax);
            stringAppendStr(&buffer, TOKEN);
            /* Unit errors */
            unitErrors = unit->errors;
            lenUnitErrors = (unitErrors ? unitErrors->size : 0);
            for (int j = 0; j < lenUnitErrors; j++) {
                if (!unitErrorKey)
                    unitErrorKey = asStr(UNITERROR);
                stringAppendStr(&buffer, unitErrorKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, arrayGet(unitErrors, j));
                stringAppendStr(&buffer, TOKEN);
            }
            /* Exit code */
            stringAppendStr(&buffer, asStr(EXITCODE));
            stringAppendStr(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, *pData->exitCode);
            stringAppendStr(&buffer, TOKEN);
            /* Date Time Start */
            dateTimeStart = pData->dateTimeStartStr;
            stringAppendStr(&buffer, asStr(DATETIMESTART));
            stringAppendStr(&buffer, ASSIGNER);
            if (dateTimeStart)
                stringAppendStr(&buffer, dateTimeStart);
            else
                stringAppendStr(&buffer, NONE);
            stringAppendStr(&buffer, TOKEN);
            /* Date Time Stop */
            dateTimeStop = pData->dateTimeStopStr;
            stringAppendStr(&buffer, asStr(DATETIMESTOP));
            stringAppendStr(&buffer, ASSIGNER);
            if (dateTimeStop)
                stringAppendStr(&buffer, dateTimeStop);
            else
                stringAppendStr(&buffer, NONE);
            stringAppendStr(&buffer, TOKEN);
            /* Interval */
            char *intervalStr = unit->intervalStr;
            if (intervalStr && strlen(intervalStr) > 0) {
                stringAppendStr(&buffer, asStr(INTERVAL));
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, intervalStr);
            }
            /* Process Data history */
            pDataHistory = unit->processDataHistory;
            lenPdataHistory = (pDataHistory ? pDataHistory->size : 0);
            if (lenPdataHistory > 0)
                pDataHistorySecKey = asStr(PDATAHISTORY_SEC);
            for (int j = 0; j < lenPdataHistory; j++) {
                pData = arrayGet(pDataHistory, j);
                stringAppendStr(&buffer, pDataHistorySecKey);
                stringAppendStr(&buffer, TOKEN);
                /* Pid history */
                if (!pidHKey)
                    pidHKey = asStr(PIDH);
                stringAppendStr(&buffer, pidHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->pid);
                stringAppendStr(&buffer, TOKEN);
                /* Exit code history */
                if (!exitCodeHKey)
                    exitCodeHKey = asStr(EXITCODEH);
                stringAppendStr(&buffer, exitCodeHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->exitCode);
                stringAppendStr(&buffer, TOKEN);
                /* Process State History */
                if (!pStateHKey)
                    pStateHKey = asStr(PSTATEH);
                stringAppendStr(&buffer, pStateHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, pData->pStateData->pState);
                stringAppendStr(&buffer, TOKEN);
                /* Signal number History */
                if (!signalNumHKey)
                    signalNumHKey = asStr(SIGNALNUMH);
                stringAppendStr(&buffer, signalNumHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->signalNum);
                stringAppendStr(&buffer, TOKEN);
                /* Final status History */
                if (!finalStatusHKey)
                    finalStatusHKey = asStr(FINALSTATUSH);
                stringAppendStr(&buffer, finalStatusHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->finalStatus);
                stringAppendStr(&buffer, TOKEN);
                /* Date time start history */
                if (!datetimeStartHKey)
                    datetimeStartHKey = asStr(DATETIMESTARTH);
                stringAppendStr(&buffer, datetimeStartHKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pData->dateTimeStartStr);
                stringAppendStr(&buffer, TOKEN);
                /* Date time stop history */
                if (!datetimeStopHKey)
                    datetimeStopHKey = asStr(DATETIMESTOPH);
                stringAppendStr(&buffer, datetimeStopHKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pData->dateTimeStopStr);
                stringAppendStr(&buffer, TOKEN);
                /* Duration history */
                if (!durationKey)
                    durationKey = asStr(DURATIONH);
                stringAppendStr(&buffer, durationKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pData->duration);
                stringAppendStr(&buffer, TOKEN);
            }
        }
    }

    return buffer;
}

int
unmarshallResponse(char *buffer, SockMessageOut **sockMessageOut)
{

    int rv, len;
    Array *entries, **unitsDisplay, **unitErrors, **messages,
          **sockErrors, **pDatasHistory, *keyval;
    char *value, *key, *entry;
    Unit *unitDisplay = NULL;
    ProcessData *pData, *pDataHistory;

    rv = len = 0;
    entries = keyval = NULL;
    pData = pDataHistory = NULL;
    value = key = entry = NULL;

    assert(buffer);
    assert(*sockMessageOut);
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    messages = &(*sockMessageOut)->messages;
    sockErrors = &(*sockMessageOut)->errors;

    /* Get the entries */
    entries = stringSplit(buffer, TOKEN, true);
    len = (entries ? entries->size : 0);
    for (int i = 0; i < len; i++) {
        entry = arrayGet(entries, i);
        /* Each entry has "Key(0)=Value(1)" format. */
        keyval = stringSplit(entry, ASSIGNER, false);
        if (!keyval) {
            // Section
            key = entry;
            value = NULL;
        }
        else {
            // Property
            key = arrayGet(keyval, 0);
            value = arrayGet(keyval, 1);
        }

        if (!value) {
            // SECTIONS
            if (stringEquals(key, asStr(UNIT_SEC))) {
                /* Create the array */
                if (!(*unitsDisplay))
                    *unitsDisplay = arrayNew(unitRelease);
                /* Create the unit */
                unitDisplay = unitNew(NULL, PARSE_SOCK_RESPONSE);
                pData = unitDisplay->processData;
                pDatasHistory = &unitDisplay->processDataHistory;
                unitErrors = &unitDisplay->errors;
                /* Adding the unit to array */
                arrayAdd(*unitsDisplay, unitDisplay);
                goto next;
            }
            if (stringEquals(key, asStr(PDATAHISTORY_SEC))) {
                if (!(*pDatasHistory))
                    *pDatasHistory = arrayNew(processDataRelease);
                pDataHistory = processDataNew(NULL, PARSE_SOCK_RESPONSE);
                arrayAdd(*pDatasHistory, pDataHistory);
                goto next;
            }
            // Should never happen
            logError(CONSOLE | SYSTEM, "src/core/socket/socket_response.c", "unmarshallResponse", EPERM,
                     strerror(EPERM), "Section %s not found!", key);
            arrayRelease(&keyval);
            rv = 1;
            goto out;

        }
        else {
            // PROPERTIES
            if (stringEquals(key, asStr(MESSAGE))) {
                if (!(*messages))
                    *messages = arrayNew(objectRelease);
                arrayAdd(*messages, stringNew(value));
                goto next;
            }
            if (stringEquals(key, asStr(ERROR))) {
                if (!(*sockErrors))
                    *sockErrors = arrayNew(objectRelease);
                arrayAdd(*sockErrors, stringNew(value));
                goto next;
            }
            if (stringEquals(key, asStr(NAME))) {
                unitDisplay->name = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(DESC))) {
                if (stringEquals(value, NONE))
                    unitDisplay->desc = NULL;
                else
                    unitDisplay->desc = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(PATH))) {
                unitDisplay->path = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(ENABLED))) {
                unitDisplay->enabled = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(RESTARTABLE))) {
                unitDisplay->restart = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(NEXTTIMEDATE))) {
                unitDisplay->nextTimeDate = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(LEFTTIMEDURATION))) {
                unitDisplay->leftTimeDuration = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(TIMERNAME))) {
                unitDisplay->timerName = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(TIMERPSTATE))) {
                unitDisplay->timerPState = calloc(1, sizeof (PState));
                assert(unitDisplay->timerPState);
                *unitDisplay->timerPState = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(PATHUNITNAME))) {
                unitDisplay->pathUnitName = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(PATHUNITPSTATE))) {
                unitDisplay->pathUnitPState = calloc(1, sizeof (PState));
                assert(unitDisplay->pathUnitPState);
                *unitDisplay->pathUnitPState = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(RESTARTNUM))) {
                unitDisplay->restartNum = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(RESTARTMAX))) {
                if (stringEquals(value, NONE))
                    unitDisplay->restartMax = -1;
                else
                    unitDisplay->restartMax = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(TYPE))) {
                unitDisplay->type = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(UNITERROR))) {
                if (!(*unitErrors))
                    *unitErrors = arrayNew(objectRelease);
                arrayAdd(*unitErrors, stringNew(value));
                goto next;
            }
            if (stringEquals(key, asStr(PID))) {
                if (stringEquals(value, NONE))
                    *pData->pid = -1;
                else
                    *pData->pid = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(EXITCODE))) {
                if (stringEquals(value, NONE))
                    *pData->exitCode = -1;
                else
                    *pData->exitCode = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(PSTATE))) {
                *pData->pStateData = PSTATE_DATA_ITEMS[atoi(value)];
                goto next;
            }
            if (stringEquals(key, asStr(SIGNALNUM))) {
                if (stringEquals(value, NONE))
                    *pData->signalNum = -1;
                else
                    *pData->signalNum = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(FINALSTATUS))) {
                if (stringEquals(value, NONE))
                    *pData->finalStatus = FINAL_STATUS_READY;
                else
                    *pData->finalStatus = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(DATETIMESTART))) {
                if (stringEquals(value, NONE))
                    pData->dateTimeStartStr = NULL;
                else
                    pData->dateTimeStartStr = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(DATETIMESTOP))) {
                if (stringEquals(value, NONE))
                    pData->dateTimeStopStr = NULL;
                else
                    pData->dateTimeStopStr = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(DURATION))) {
                if (stringEquals(value, NONE))
                    pData->duration = NULL;
                else
                    pData->duration = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(INTERVAL))) {
                unitDisplay->intervalStr = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(PIDH))) {
                if (stringEquals(value, NONE))
                    *pDataHistory->pid = -1;
                else
                    *pDataHistory->pid = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(EXITCODEH))) {
                if (stringEquals(value, NONE))
                    *pDataHistory->exitCode = -1;
                else
                    *pDataHistory->exitCode = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(PSTATEH))) {
                *pDataHistory->pStateData = PSTATE_DATA_ITEMS[atoi(value)];
                goto next;
            }
            if (stringEquals(key, asStr(SIGNALNUMH))) {
                if (stringEquals(value, NONE))
                    *pDataHistory->signalNum = -1;
                else
                    *pDataHistory->signalNum = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(FINALSTATUSH))) {
                if (stringEquals(value, NONE))
                    *pDataHistory->finalStatus = FINAL_STATUS_READY;
                else
                    *pDataHistory->finalStatus = atoi(value);
                goto next;
            }
            if (stringEquals(key, asStr(DATETIMESTARTH))) {
                pDataHistory->dateTimeStartStr = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(DATETIMESTOPH))) {
                if (stringEquals(value, NONE))
                    pDataHistory->dateTimeStopStr = NULL;
                else
                    pDataHistory->dateTimeStopStr = stringNew(value);
                goto next;
            }
            if (stringEquals(key, asStr(DURATIONH))) {
                if (stringEquals(value, NONE))
                    pDataHistory->duration = NULL;
                else
                    pDataHistory->duration = stringNew(value);
                goto next;
            }
            // Should never happen
            logError(CONSOLE | SYSTEM, "src/core/socket/socket_response.c", "unmarshallResponse", EPERM,
                     strerror(EPERM), "Property %s not found!", key);
            arrayRelease(&keyval);
            rv = EPERM;
            goto out;
        }

        next:
            arrayRelease(&keyval);
            continue;
    }

    out:
        arrayRelease(&entries);
        return rv;
}

