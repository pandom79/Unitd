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
SignalNum=value         (optional and repeatable)
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

//CONFIGURING THE PARSER ACCORDING THE COMMUNICATION PROTOCOL

enum SectionNameEnum  {
  UNIT = 0,
  PDATAHISTORY = 1,
};

/* Properties */
enum PropertyNameEnum  {
    MESSAGE = 0,
    ERROR = 1,
    NAME = 2,
    ENABLED = 3,
    PID = 4,
    PSTATE = 5,
    FINALSTATUS = 6,
    DESC = 7,
    DURATION = 8,
    RESTARTNUM = 9,
    RESTARTABLE = 10,
    TYPE = 11,
    NEXTTIMEDATE = 12,
    LEFTTIMEDURATION = 13,
    TIMERNAME = 14,
    TIMERPSTATE = 15,
    PATHUNITNAME = 16,
    PATHUNITPSTATE = 17,
    PATH = 18,
    RESTARTMAX = 19,
    UNITERROR = 20,
    EXITCODE = 21,
    SIGNALNUM = 22,
    DATETIMESTART = 23,
    DATETIMESTOP = 24,
    INTERVAL = 25,
    PIDH = 26,
    EXITCODEH = 27,
    PSTATEH = 28,
    SIGNALNUMH = 29,
    FINALSTATUSH = 30,
    DATETIMESTARTH = 31,
    DATETIMESTOPH = 32,
    DURATIONH = 33
};

/* Sections */
int SOCKRES_SECTIONS_ITEMS_LEN = 2;
SectionData SOCKRES_SECTIONS_ITEMS[] = {
    { { UNIT, "[Unit]" }, true, false, 0 },
    { { PDATAHISTORY, "[PDataHistory]" }, true, false, 0 },
};

/* Properties */
int SOCKRES_PROPERTIES_ITEMS_LEN = 34;
PropertyData SOCKRES_PROPERTIES_ITEMS[] = {
    { NO_SECTION, { MESSAGE, "Message" }, true, false, false, 0, NULL, NULL },
    { NO_SECTION, { ERROR, "Error" }, true, false, false, 0, NULL, NULL },
    { UNIT, { NAME, "Name" }, true, false, false, 0, NULL, NULL },
    { UNIT, { ENABLED, "Enabled" }, true, false, false, 0, NULL, NULL },
    { UNIT, { PID, "Pid" }, true, false, false, 0, NULL, NULL },
    { UNIT, { PSTATE, "PState" }, true, false, false, 0, NULL, NULL },
    { UNIT, { FINALSTATUS, "FinalStatus" }, true, false, false, 0, NULL, NULL },
    { UNIT, { DESC, "Desc" }, true, false, false, 0, NULL, NULL },
    { UNIT, { DURATION, "Duration" }, true, false, false, 0, NULL, NULL },
    { UNIT, { RESTARTNUM, "RestartNum" }, true, false, false, 0, NULL, NULL },
    { UNIT, { RESTARTABLE, "Restartable" }, true, false, false, 0, NULL, NULL },
    { UNIT, { TYPE, "Type" }, true, false, false, 0, NULL, NULL },
    { UNIT, { NEXTTIMEDATE, "NextTimeDate" }, true, false, false, 0, NULL, NULL },
    { UNIT, { LEFTTIMEDURATION, "LeftTimeDuration" }, true, false, false, 0, NULL, NULL },
    { UNIT, { TIMERNAME, "TimerName" }, true, false, false, 0, NULL, NULL },
    { UNIT, { TIMERPSTATE, "TimerPState" }, true, false, false, 0, NULL, NULL },
    { UNIT, { PATHUNITNAME, "PathUnitName" }, true, false, false, 0, NULL, NULL },
    { UNIT, { PATHUNITPSTATE, "PathUnitPState" }, true, false, false, 0, NULL, NULL },
    { UNIT, { PATH, "Path" }, true, false, false, 0, NULL, NULL },
    { UNIT, { RESTARTMAX, "RestartMax" }, true, false, false, 0, NULL, NULL },
    { UNIT, { UNITERROR, "UnitError" }, true, false, false, 0, NULL, NULL },
    { UNIT, { EXITCODE, "ExitCode" }, true, false, false, 0, NULL, NULL },
    { UNIT, { SIGNALNUM, "SignalNum" }, true, false, false, 0, NULL, NULL },
    { UNIT, { DATETIMESTART, "DateTimeStart" }, true, false, false, 0, NULL, NULL },
    { UNIT, { DATETIMESTOP, "DateTimeStop" }, true, false, false, 0, NULL, NULL },
    { UNIT, { INTERVAL, "Interval" }, true, false, false, 0, NULL, NULL },
    { PDATAHISTORY, { PIDH, "PidH" }, true, false, false, 0, NULL, NULL },
    { PDATAHISTORY, { EXITCODEH, "ExitCodeH" }, true, false, false, 0, NULL, NULL },
    { PDATAHISTORY, { PSTATEH, "PStateH" }, true, false, false, 0, NULL, NULL },
    { PDATAHISTORY, { SIGNALNUMH, "SignalNumH" }, true, false, false, 0, NULL, NULL },
    { PDATAHISTORY, { FINALSTATUSH, "FinalStatusH" }, true, false, false, 0, NULL, NULL },
    { PDATAHISTORY, { DATETIMESTARTH, "DateTimeStartH" }, true, false, false, 0, NULL, NULL },
    { PDATAHISTORY, { DATETIMESTOPH, "DateTimeStopH" }, true, false, false, 0, NULL, NULL },
    { PDATAHISTORY, { DURATIONH, "DurationH" }, true, false, false, 0, NULL, NULL }
};

//END PARSER CONFIGURATION

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
        msgKey = SOCKRES_PROPERTIES_ITEMS[MESSAGE].propertyName.desc;
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
        errKey = SOCKRES_PROPERTIES_ITEMS[ERROR].propertyName.desc;
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
            buffer = stringNew(SOCKRES_SECTIONS_ITEMS[UNIT].sectionName.desc);
        else
            stringAppendStr(&buffer, SOCKRES_SECTIONS_ITEMS[UNIT].sectionName.desc);

        stringAppendStr(&buffer, TOKEN);

        /* The following data are in common between
         * PARSE_SOCK_RESPONSE_UNITLIST and PARSE_SOCK_RESPONSE
        */

        /* Name */
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[NAME].propertyName.desc);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, unit->name);
        stringAppendStr(&buffer, TOKEN);
        /* Enabled */
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[ENABLED].propertyName.desc);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, (unit->enabled ? "1" : "0"));
        stringAppendStr(&buffer, TOKEN);
        /* Pid */
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[PID].propertyName.desc);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->pid);
        stringAppendStr(&buffer, TOKEN);
        /* Process State */
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[PSTATE].propertyName.desc);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, pData->pStateData->pState);
        stringAppendStr(&buffer, TOKEN);
        /* Final Status */
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[FINALSTATUS].propertyName.desc);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->finalStatus);
        stringAppendStr(&buffer, TOKEN);
        /* Description */
        unitDesc = unit->desc;
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[DESC].propertyName.desc);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, (unitDesc ? unitDesc : NONE));
        stringAppendStr(&buffer, TOKEN);
        /* Duration */
        duration = pData->duration;
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[DURATION].propertyName.desc);
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
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[RESTARTNUM].propertyName.desc);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, unit->restartNum);
        stringAppendStr(&buffer, TOKEN);
        /* Restartable */
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[RESTARTABLE].propertyName.desc);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, ((unit->restart || unit->restartMax > 0) ? "1" : "0"));
        stringAppendStr(&buffer, TOKEN);
        /* Type */
        stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[TYPE].propertyName.desc);
        stringAppendStr(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, unit->type);
        stringAppendStr(&buffer, TOKEN);
        /* Next time (date) */
        char *nextTimeDate = unit->nextTimeDate;
        if (nextTimeDate && strlen(nextTimeDate) > 0) {
            stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[NEXTTIMEDATE].propertyName.desc);
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, nextTimeDate);
            stringAppendStr(&buffer, TOKEN);
        }
        /* Left time (duration) */
        char *leftTimeDuration = unit->leftTimeDuration;
        if (leftTimeDuration && strlen(leftTimeDuration) > 0) {
            setLeftTimeAndDuration(&unit);
            stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[LEFTTIMEDURATION].propertyName.desc);
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, leftTimeDuration);
            stringAppendStr(&buffer, TOKEN);
        }

        if (funcType == PARSE_SOCK_RESPONSE) {
            /* Timer name */
            char *timerName = unit->timerName;
            if (timerName) {
                stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[TIMERNAME].propertyName.desc);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, timerName);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Timer process state */
            PState *timerPState = unit->timerPState;
            if (timerPState) {
                stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[TIMERPSTATE].propertyName.desc);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *timerPState);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Path unit name */
            char *pathUnitName = unit->pathUnitName;
            if (pathUnitName) {
                stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[PATHUNITNAME].propertyName.desc);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pathUnitName);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Path unit process state */
            PState *pathUnitPState = unit->pathUnitPState;
            if (pathUnitPState) {
                stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[PATHUNITPSTATE].propertyName.desc);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pathUnitPState);
                stringAppendStr(&buffer, TOKEN);
            }
            /* Path */
            unitPath = unit->path;
            stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[PATH].propertyName.desc);
            stringAppendStr(&buffer, ASSIGNER);
            stringAppendStr(&buffer, (unitPath ? unitPath : NONE));
            stringAppendStr(&buffer, TOKEN);
            /* RestartMax */
            stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[RESTARTMAX].propertyName.desc);
            stringAppendStr(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, unit->restartMax);
            stringAppendStr(&buffer, TOKEN);
            /* Unit errors */
            unitErrors = unit->errors;
            lenUnitErrors = (unitErrors ? unitErrors->size : 0);
            for (int j = 0; j < lenUnitErrors; j++) {
                if (!unitErrorKey)
                    unitErrorKey = SOCKRES_PROPERTIES_ITEMS[UNITERROR].propertyName.desc;
                stringAppendStr(&buffer, unitErrorKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, arrayGet(unitErrors, j));
                stringAppendStr(&buffer, TOKEN);
            }
            /* Exit code */
            stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[EXITCODE].propertyName.desc);
            stringAppendStr(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, *pData->exitCode);
            stringAppendStr(&buffer, TOKEN);
            /* Signal Num */
            stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[SIGNALNUM].propertyName.desc);
            stringAppendStr(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, *pData->signalNum);
            stringAppendStr(&buffer, TOKEN);
            /* Date Time Start */
            dateTimeStart = pData->dateTimeStartStr;
            stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[DATETIMESTART].propertyName.desc);
            stringAppendStr(&buffer, ASSIGNER);
            if (dateTimeStart)
                stringAppendStr(&buffer, dateTimeStart);
            else
                stringAppendStr(&buffer, NONE);
            stringAppendStr(&buffer, TOKEN);
            /* Date Time Stop */
            dateTimeStop = pData->dateTimeStopStr;
            stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[DATETIMESTOP].propertyName.desc);
            stringAppendStr(&buffer, ASSIGNER);
            if (dateTimeStop)
                stringAppendStr(&buffer, dateTimeStop);
            else
                stringAppendStr(&buffer, NONE);
            stringAppendStr(&buffer, TOKEN);
            /* Interval */
            char *intervalStr = unit->intervalStr;
            if (intervalStr && strlen(intervalStr) > 0) {
                stringAppendStr(&buffer, SOCKRES_PROPERTIES_ITEMS[INTERVAL].propertyName.desc);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, intervalStr);
            }
            /* Process Data history */
            pDataHistory = unit->processDataHistory;
            lenPdataHistory = (pDataHistory ? pDataHistory->size : 0);
            if (lenPdataHistory > 0)
                pDataHistorySecKey = SOCKRES_SECTIONS_ITEMS[PDATAHISTORY].sectionName.desc;
            for (int j = 0; j < lenPdataHistory; j++) {
                pData = arrayGet(pDataHistory, j);
                stringAppendStr(&buffer, pDataHistorySecKey);
                stringAppendStr(&buffer, TOKEN);
                /* Pid history */
                if (!pidHKey)
                    pidHKey = SOCKRES_PROPERTIES_ITEMS[PIDH].propertyName.desc;
                stringAppendStr(&buffer, pidHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->pid);
                stringAppendStr(&buffer, TOKEN);
                /* Exit code history */
                if (!exitCodeHKey)
                    exitCodeHKey = SOCKRES_PROPERTIES_ITEMS[EXITCODEH].propertyName.desc;
                stringAppendStr(&buffer, exitCodeHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->exitCode);
                stringAppendStr(&buffer, TOKEN);
                /* Process State History */
                if (!pStateHKey)
                    pStateHKey = SOCKRES_PROPERTIES_ITEMS[PSTATEH].propertyName.desc;
                stringAppendStr(&buffer, pStateHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, pData->pStateData->pState);
                stringAppendStr(&buffer, TOKEN);
                /* Signal number History */
                if (!signalNumHKey)
                    signalNumHKey = SOCKRES_PROPERTIES_ITEMS[SIGNALNUMH].propertyName.desc;
                stringAppendStr(&buffer, signalNumHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->signalNum);
                stringAppendStr(&buffer, TOKEN);
                /* Final status History */
                if (!finalStatusHKey)
                    finalStatusHKey = SOCKRES_PROPERTIES_ITEMS[FINALSTATUSH].propertyName.desc;
                stringAppendStr(&buffer, finalStatusHKey);
                stringAppendStr(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->finalStatus);
                stringAppendStr(&buffer, TOKEN);
                /* Date time start history */
                if (!datetimeStartHKey)
                    datetimeStartHKey = SOCKRES_PROPERTIES_ITEMS[DATETIMESTARTH].propertyName.desc;
                stringAppendStr(&buffer, datetimeStartHKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pData->dateTimeStartStr);
                stringAppendStr(&buffer, TOKEN);
                /* Date time stop history */
                if (!datetimeStopHKey)
                    datetimeStopHKey = SOCKRES_PROPERTIES_ITEMS[DATETIMESTOPH].propertyName.desc;
                stringAppendStr(&buffer, datetimeStopHKey);
                stringAppendStr(&buffer, ASSIGNER);
                stringAppendStr(&buffer, pData->dateTimeStopStr);
                stringAppendStr(&buffer, TOKEN);
                /* Duration history */
                if (!durationKey)
                    durationKey = SOCKRES_PROPERTIES_ITEMS[DURATIONH].propertyName.desc;
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

    int rv, errorsSize, len, unitSectionCount, pDataHistorySecCount;
    Array *errors, *entries, *lineData, **unitsDisplay, **unitErrors, **messages,
          **sockErrors, **pDatasHistory;
    PropertyData *propertyData = NULL;
    char *value, *error;
    Unit *unitDisplay = NULL;
    ProcessData *pData, *pDataHistory;
    SectionData *unitSecData = &SOCKRES_SECTIONS_ITEMS[UNIT];
    SectionData *pDataHSecData = &SOCKRES_SECTIONS_ITEMS[PDATAHISTORY];

    rv = errorsSize = len = unitSectionCount = pDataHistorySecCount = 0;
    entries = lineData = NULL;
    pData = pDataHistory = NULL;
    errors = NULL;

    assert(buffer);
    assert(*sockMessageOut);
    unitsDisplay = &(*sockMessageOut)->unitsDisplay;
    messages = &(*sockMessageOut)->messages;
    sockErrors = &(*sockMessageOut)->errors;

    /* Parser init */
    parserInit(PARSE_SOCK_RESPONSE);

    /* Get the entries (simulating the file lines) */
    entries = stringSplit(buffer, TOKEN, true);
    len = (entries ? entries->size : 0);
    for (int i = 0; i < len; i++) {
        rv = parseLine(arrayGet(entries, i), i + 1, &lineData, &propertyData);
        if (lineData) {
            if (rv != 0) {
                error = arrayGet(lineData, 2);
                assert(error);
                logErrorStr(SYSTEM, "An error has occurred in unmarshallResponse! Error = %s", error);
                logErrorStr(SYSTEM, "Buffer =  %s", buffer);
                goto out;
            }
            if ((value = arrayGet(lineData, 1))) {
                /* Check unit section */
                if (unitSectionCount < unitSecData->sectionCount) {
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
                    unitSectionCount++;
                }
                /* Check Process data history section */
                if (pDataHistorySecCount < pDataHSecData->sectionCount) {
                    if (!(*pDatasHistory))
                        *pDatasHistory = arrayNew(processDataRelease);

                    pDataHistory = processDataNew(NULL, PARSE_SOCK_RESPONSE);
                    arrayAdd(*pDatasHistory, pDataHistory);
                    pDataHistorySecCount++;
                }
                switch (propertyData->propertyName.propertyNameEnum) {
                    case MESSAGE:
                        if (!(*messages))
                            *messages = arrayNew(objectRelease);
                        arrayAdd(*messages, stringNew(value));
                        break;
                    case ERROR:
                        if (!(*sockErrors))
                            *sockErrors = arrayNew(objectRelease);
                        arrayAdd(*sockErrors, stringNew(value));
                        break;
                    case NAME:
                        unitDisplay->name = stringNew(value);
                        break;
                    case DESC:
                        if (stringEquals(value, NONE))
                            unitDisplay->desc = NULL;
                        else
                            unitDisplay->desc = stringNew(value);
                        break;
                    case PATH:
                        unitDisplay->path = stringNew(value);
                        break;
                    case ENABLED:
                        unitDisplay->enabled = atoi(value);
                        break;
                    case RESTARTABLE:
                        unitDisplay->restart = atoi(value);
                        break;
                    case NEXTTIMEDATE:
                        unitDisplay->nextTimeDate = stringNew(value);
                        break;
                    case LEFTTIMEDURATION:
                        unitDisplay->leftTimeDuration = stringNew(value);
                        break;
                    case TIMERNAME:
                        unitDisplay->timerName = stringNew(value);
                        break;
                    case TIMERPSTATE:
                        unitDisplay->timerPState = calloc(1, sizeof (PState));
                        assert(unitDisplay->timerPState);
                        *unitDisplay->timerPState = atoi(value);
                        break;
                    case PATHUNITNAME:
                        unitDisplay->pathUnitName = stringNew(value);
                        break;
                    case PATHUNITPSTATE:
                        unitDisplay->pathUnitPState = calloc(1, sizeof (PState));
                        assert(unitDisplay->pathUnitPState);
                        *unitDisplay->pathUnitPState = atoi(value);
                        break;
                    case RESTARTNUM:
                        unitDisplay->restartNum = atoi(value);
                        break;
                    case RESTARTMAX:
                        if (stringEquals(value, NONE))
                            unitDisplay->restartMax = -1;
                        else
                            unitDisplay->restartMax = atoi(value);
                        break;
                    case TYPE:
                        unitDisplay->type = atoi(value);
                        break;
                    case UNITERROR:
                        if (!(*unitErrors))
                            *unitErrors = arrayNew(objectRelease);
                        arrayAdd(*unitErrors, stringNew(value));
                        break;
                    case PID:
                        if (stringEquals(value, NONE))
                            *pData->pid = -1;
                        else
                            *pData->pid = atoi(value);
                        break;
                    case EXITCODE:
                        if (stringEquals(value, NONE))
                            *pData->exitCode = -1;
                        else
                            *pData->exitCode = atoi(value);
                        break;
                    case PSTATE:
                        *pData->pStateData = PSTATE_DATA_ITEMS[atoi(value)];
                        break;
                    case SIGNALNUM:
                        if (stringEquals(value, NONE))
                            *pData->signalNum = -1;
                        else
                            *pData->signalNum = atoi(value);
                        break;
                    case FINALSTATUS:
                        if (stringEquals(value, NONE))
                            *pData->finalStatus = FINAL_STATUS_READY;
                        else
                            *pData->finalStatus = atoi(value);
                        break;
                    case DATETIMESTART:
                        if (stringEquals(value, NONE))
                            pData->dateTimeStartStr = NULL;
                        else
                            pData->dateTimeStartStr = stringNew(value);
                        break;
                    case DATETIMESTOP:
                        if (stringEquals(value, NONE))
                            pData->dateTimeStopStr = NULL;
                        else
                            pData->dateTimeStopStr = stringNew(value);
                        break;
                    case DURATION:
                        if (stringEquals(value, NONE))
                            pData->duration = NULL;
                        else
                            pData->duration = stringNew(value);
                        break;
                    case INTERVAL:
                        unitDisplay->intervalStr = stringNew(value);
                        break;
                    case PIDH:
                        if (stringEquals(value, NONE))
                            *pDataHistory->pid = -1;
                        else
                            *pDataHistory->pid = atoi(value);
                        break;
                    case EXITCODEH:
                        if (stringEquals(value, NONE))
                            *pDataHistory->exitCode = -1;
                        else
                            *pDataHistory->exitCode = atoi(value);
                        break;
                    case PSTATEH:
                        *pDataHistory->pStateData = PSTATE_DATA_ITEMS[atoi(value)];
                        break;
                    case SIGNALNUMH:
                        if (stringEquals(value, NONE))
                            *pDataHistory->signalNum = -1;
                        else
                            *pDataHistory->signalNum = atoi(value);
                        break;
                    case FINALSTATUSH:
                        if (stringEquals(value, NONE))
                            *pDataHistory->finalStatus = FINAL_STATUS_READY;
                        else
                            *pDataHistory->finalStatus = atoi(value);
                        break;
                    case DATETIMESTARTH:
                        pDataHistory->dateTimeStartStr = stringNew(value);
                        break;
                    case DATETIMESTOPH:
                        if (stringEquals(value, NONE))
                            pDataHistory->dateTimeStopStr = NULL;
                        else
                            pDataHistory->dateTimeStopStr = stringNew(value);
                        break;
                    case DURATIONH:
                        if (stringEquals(value, NONE))
                            pDataHistory->duration = NULL;
                        else
                            pDataHistory->duration = stringNew(value);
                        break;
                    default:
                        break;
                }
            }
        }
        arrayRelease(&lineData);
    }

    /* Parser end */
    parserEnd(&errors, true);
    /* These errors should never occurred */
    errorsSize = (errors ? errors->size : 0);
    if (errorsSize > 0) {
        syslog(LOG_DAEMON | LOG_ERR, "The parserEnd func in unmarshallResponse func has returned "
                                     "the following errors:\n");
        for (int i = 0; i < errorsSize; i++) {
            syslog(LOG_DAEMON | LOG_ERR, "Error %d = %s", i, (char *)arrayGet(errors, i));
        }
        rv = 1;
    }

    out:
        arrayRelease(&errors);
        arrayRelease(&lineData);
        arrayRelease(&entries);
        return rv;
}

