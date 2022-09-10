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
Path=value              (optional and repeatable)
RestartMax=value        (optional and repeatable)
Type=value              (optional and repeatable)
UnitError=value1        (optional and repeatable)
UnitError=value2        (optional and repeatable)
....
UnitError=valueN        (optional and repeatable)
ExitCode=value          (optional and repeatable)
SignalNum=value         (optional and repeatable)
DateTimeStart=value     (optional and repeatable)
DateTimeStop=value      (optional and repeatable)
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
    PATH = 11,
    RESTARTMAX = 12,
    TYPE = 13,
    UNITERROR = 14,
    EXITCODE = 15,
    SIGNALNUM = 16,
    DATETIMESTART = 17,
    DATETIMESTOP = 18,
    PIDH = 19,
    EXITCODEH = 20,
    PSTATEH = 21,
    SIGNALNUMH = 22,
    FINALSTATUSH = 23,
    DATETIMESTARTH = 24,
    DATETIMESTOPH = 25,
    DURATIONH = 26
};

/* Sections */
int SOCKRES_SECTIONS_ITEMS_LEN = 2;
SectionData SOCKRES_SECTIONS_ITEMS[] = {
    { { UNIT, "[Unit]" }, true, false, 0 },
    { { PDATAHISTORY, "[PDataHistory]" }, true, false, 0 },
};

/* Properties */
int SOCKRES_PROPERTIES_ITEMS_LEN = 27;
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
    { UNIT, { PATH, "Path" }, true, false, false, 0, NULL, NULL },
    { UNIT, { RESTARTMAX, "RestartMax" }, true, false, false, 0, NULL, NULL },
    { UNIT, { TYPE, "Type" }, true, false, false, 0, NULL, NULL },
    { UNIT, { UNITERROR, "UnitError" }, true, false, false, 0, NULL, NULL },
    { UNIT, { EXITCODE, "ExitCode" }, true, false, false, 0, NULL, NULL },
    { UNIT, { SIGNALNUM, "SignalNum" }, true, false, false, 0, NULL, NULL },
    { UNIT, { DATETIMESTART, "DateTimeStart" }, true, false, false, 0, NULL, NULL },
    { UNIT, { DATETIMESTOP, "DateTimeStop" }, true, false, false, 0, NULL, NULL },
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
            stringConcat(&buffer, msgKey);

        stringConcat(&buffer, ASSIGNER);
        stringConcat(&buffer, arrayGet(messages, i));
        stringConcat(&buffer, TOKEN);
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
            stringConcat(&buffer, errKey);

        stringConcat(&buffer, ASSIGNER);
        stringConcat(&buffer, arrayGet(errors, i));
        stringConcat(&buffer, TOKEN);
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
            stringConcat(&buffer, SOCKRES_SECTIONS_ITEMS[UNIT].sectionName.desc);

        stringConcat(&buffer, TOKEN);

        /* The following data are in common between
         * PARSE_SOCK_RESPONSE_UNITLIST and PARSE_SOCK_RESPONSE
        */

        /* Name */
        stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[NAME].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        stringConcat(&buffer, unit->name);
        stringConcat(&buffer, TOKEN);
        /* Enabled */
        stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[ENABLED].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        stringConcat(&buffer, (unit->enabled ? "1" : "0"));
        stringConcat(&buffer, TOKEN);
        /* Pid */
        stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[PID].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->pid);
        stringConcat(&buffer, TOKEN);
        /* Process State */
        stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[PSTATE].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, pData->pStateData->pState);
        stringConcat(&buffer, TOKEN);
        /* Final Status */
        stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[FINALSTATUS].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, *pData->finalStatus);
        stringConcat(&buffer, TOKEN);
        /* Description */
        unitDesc = unit->desc;
        stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[DESC].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        stringConcat(&buffer, (unitDesc ? unitDesc : NONE));
        stringConcat(&buffer, TOKEN);
        /* Duration */
        duration = pData->duration;
        stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[DURATION].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        if (duration)
            stringConcat(&buffer, duration);
        else {
            if (pData->timeStart) {
                Time *currentTimeStop = timeNew(NULL);
                char *diff = stringGetDiffTime(currentTimeStop, pData->timeStart);
                stringConcat(&buffer, diff);
                timeRelease(&currentTimeStop);
                objectRelease(&diff);
            }
            else
                stringConcat(&buffer, NONE);
        }
        stringConcat(&buffer, TOKEN);
        /* RestartNum */
        stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[RESTARTNUM].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        setValueForBuffer(&buffer, unit->restartNum);
        stringConcat(&buffer, TOKEN);
        /* Restartable */
        stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[RESTARTABLE].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        stringConcat(&buffer, ((unit->restart || unit->restartMax > 0) ? "1" : "0"));
        stringConcat(&buffer, TOKEN);

        if (funcType == PARSE_SOCK_RESPONSE) {
            /* Path */
            unitPath = unit->path;
            stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[PATH].propertyName.desc);
            stringConcat(&buffer, ASSIGNER);
            stringConcat(&buffer, (unitPath ? unitPath : NONE));
            stringConcat(&buffer, TOKEN);
            /* RestartMax */
            stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[RESTARTMAX].propertyName.desc);
            stringConcat(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, unit->restartMax);
            stringConcat(&buffer, TOKEN);
            /* Type */
            stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[TYPE].propertyName.desc);
            stringConcat(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, unit->type);
            stringConcat(&buffer, TOKEN);
            /* Unit errors */
            unitErrors = unit->errors;
            lenUnitErrors = (unitErrors ? unitErrors->size : 0);
            for (int j = 0; j < lenUnitErrors; j++) {
                if (!unitErrorKey)
                    unitErrorKey = SOCKRES_PROPERTIES_ITEMS[UNITERROR].propertyName.desc;
                stringConcat(&buffer, unitErrorKey);
                stringConcat(&buffer, ASSIGNER);
                stringConcat(&buffer, arrayGet(unitErrors, j));
                stringConcat(&buffer, TOKEN);
            }
            /* Exit code */
            stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[EXITCODE].propertyName.desc);
            stringConcat(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, *pData->exitCode);
            stringConcat(&buffer, TOKEN);
            /* Signal Num */
            stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[SIGNALNUM].propertyName.desc);
            stringConcat(&buffer, ASSIGNER);
            setValueForBuffer(&buffer, *pData->signalNum);
            stringConcat(&buffer, TOKEN);
            /* Date Time Start */
            dateTimeStart = pData->dateTimeStartStr;
            stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[DATETIMESTART].propertyName.desc);
            stringConcat(&buffer, ASSIGNER);
            if (dateTimeStart)
                stringConcat(&buffer, dateTimeStart);
            else
                stringConcat(&buffer, NONE);
            stringConcat(&buffer, TOKEN);
            /* Date Time Stop */
            dateTimeStop = pData->dateTimeStopStr;
            stringConcat(&buffer, SOCKRES_PROPERTIES_ITEMS[DATETIMESTOP].propertyName.desc);
            stringConcat(&buffer, ASSIGNER);
            if (dateTimeStop)
                stringConcat(&buffer, dateTimeStop);
            else
                stringConcat(&buffer, NONE);
            stringConcat(&buffer, TOKEN);
            /* Process Data history */
            pDataHistory = unit->processDataHistory;
            lenPdataHistory = (pDataHistory ? pDataHistory->size : 0);
            if (lenPdataHistory > 0)
                pDataHistorySecKey = SOCKRES_SECTIONS_ITEMS[PDATAHISTORY].sectionName.desc;
            for (int j = 0; j < lenPdataHistory; j++) {
                pData = arrayGet(pDataHistory, j);
                stringConcat(&buffer, pDataHistorySecKey);
                stringConcat(&buffer, TOKEN);
                /* Pid history */
                if (!pidHKey)
                    pidHKey = SOCKRES_PROPERTIES_ITEMS[PIDH].propertyName.desc;
                stringConcat(&buffer, pidHKey);
                stringConcat(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->pid);
                stringConcat(&buffer, TOKEN);
                /* Exit code history */
                if (!exitCodeHKey)
                    exitCodeHKey = SOCKRES_PROPERTIES_ITEMS[EXITCODEH].propertyName.desc;
                stringConcat(&buffer, exitCodeHKey);
                stringConcat(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->exitCode);
                stringConcat(&buffer, TOKEN);
                /* Process State History */
                if (!pStateHKey)
                    pStateHKey = SOCKRES_PROPERTIES_ITEMS[PSTATEH].propertyName.desc;
                stringConcat(&buffer, pStateHKey);
                stringConcat(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, pData->pStateData->pState);
                stringConcat(&buffer, TOKEN);
                /* Signal number History */
                if (!signalNumHKey)
                    signalNumHKey = SOCKRES_PROPERTIES_ITEMS[SIGNALNUMH].propertyName.desc;
                stringConcat(&buffer, signalNumHKey);
                stringConcat(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->signalNum);
                stringConcat(&buffer, TOKEN);
                /* Final status History */
                if (!finalStatusHKey)
                    finalStatusHKey = SOCKRES_PROPERTIES_ITEMS[FINALSTATUSH].propertyName.desc;
                stringConcat(&buffer, finalStatusHKey);
                stringConcat(&buffer, ASSIGNER);
                setValueForBuffer(&buffer, *pData->finalStatus);
                stringConcat(&buffer, TOKEN);
                /* Date time start history */
                if (!datetimeStartHKey)
                    datetimeStartHKey = SOCKRES_PROPERTIES_ITEMS[DATETIMESTARTH].propertyName.desc;
                stringConcat(&buffer, datetimeStartHKey);
                stringConcat(&buffer, ASSIGNER);
                stringConcat(&buffer, pData->dateTimeStartStr);
                stringConcat(&buffer, TOKEN);
                /* Date time stop history */
                if (!datetimeStopHKey)
                    datetimeStopHKey = SOCKRES_PROPERTIES_ITEMS[DATETIMESTOPH].propertyName.desc;
                stringConcat(&buffer, datetimeStopHKey);
                stringConcat(&buffer, ASSIGNER);
                stringConcat(&buffer, pData->dateTimeStopStr);
                stringConcat(&buffer, TOKEN);
                /* Duration history */
                if (!durationKey)
                    durationKey = SOCKRES_PROPERTIES_ITEMS[DURATIONH].propertyName.desc;
                stringConcat(&buffer, durationKey);
                stringConcat(&buffer, ASSIGNER);
                stringConcat(&buffer, pData->duration);
                stringConcat(&buffer, TOKEN);
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
                syslog(LOG_DAEMON | LOG_ERR, "An error occurred in unmarshallResponse = %s\n",
                                             error);
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
                        if (strcmp(value, NONE) == 0)
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
                    case RESTARTNUM:
                        unitDisplay->restartNum = atoi(value);
                        break;
                    case RESTARTMAX:
                        if (strcmp(value, NONE) == 0)
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
                        if (strcmp(value, NONE) == 0)
                            *pData->pid = -1;
                        else
                            *pData->pid = atoi(value);
                        break;
                    case EXITCODE:
                        if (strcmp(value, NONE) == 0)
                            *pData->exitCode = -1;
                        else
                            *pData->exitCode = atoi(value);
                        break;
                    case PSTATE:
                        *pData->pStateData = PSTATE_DATA_ITEMS[atoi(value)];
                        break;
                    case SIGNALNUM:
                        if (strcmp(value, NONE) == 0)
                            *pData->signalNum = -1;
                        else
                            *pData->signalNum = atoi(value);
                        break;
                    case FINALSTATUS:
                        if (strcmp(value, NONE) == 0)
                            *pData->finalStatus = FINAL_STATUS_READY;
                        else
                            *pData->finalStatus = atoi(value);
                        break;
                    case DATETIMESTART:
                        if (strcmp(value, NONE) == 0)
                            pData->dateTimeStartStr = NULL;
                        else
                            pData->dateTimeStartStr = stringNew(value);
                        break;
                    case DATETIMESTOP:
                        if (strcmp(value, NONE) == 0)
                            pData->dateTimeStopStr = NULL;
                        else
                            pData->dateTimeStopStr = stringNew(value);
                        break;
                    case DURATION:
                        if (strcmp(value, NONE) == 0)
                            pData->duration = NULL;
                        else
                            pData->duration = stringNew(value);
                        break;
                    case PIDH:
                        if (strcmp(value, NONE) == 0)
                            *pDataHistory->pid = -1;
                        else
                            *pDataHistory->pid = atoi(value);
                        break;
                    case EXITCODEH:
                        if (strcmp(value, NONE) == 0)
                            *pDataHistory->exitCode = -1;
                        else
                            *pDataHistory->exitCode = atoi(value);
                        break;
                    case PSTATEH:
                        *pDataHistory->pStateData = PSTATE_DATA_ITEMS[atoi(value)];
                        break;
                    case SIGNALNUMH:
                        if (strcmp(value, NONE) == 0)
                            *pDataHistory->signalNum = -1;
                        else
                            *pDataHistory->signalNum = atoi(value);
                        break;
                    case FINALSTATUSH:
                        if (strcmp(value, NONE) == 0)
                            *pDataHistory->finalStatus = FINAL_STATUS_READY;
                        else
                            *pDataHistory->finalStatus = atoi(value);
                        break;
                    case DATETIMESTARTH:
                        pDataHistory->dateTimeStartStr = stringNew(value);
                        break;
                    case DATETIMESTOPH:
                        if (strcmp(value, NONE) == 0)
                            pDataHistory->dateTimeStopStr = NULL;
                        else
                            pDataHistory->dateTimeStopStr = stringNew(value);
                        break;
                    case DURATIONH:
                        if (strcmp(value, NONE) == 0)
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

