/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

Command
getCommand(const char *command)
{
    assert(command);
    for (int i = 0; i < COMMANDS_LEN; i++) {
        if (stringEquals(command, COMMANDS_DATA[i].name))
            return i;
    }
    return NO_COMMAND;
}

SockMessageIn*
sockMessageInNew()
{
    SockMessageIn *sockMessageIn = calloc(1, sizeof(SockMessageIn));
    assert(sockMessageIn);
    sockMessageIn->command = NO_COMMAND;
    sockMessageIn->options = NULL;
    sockMessageIn->arg = NULL;
    return sockMessageIn;
}

void
sockMessageInRelease(SockMessageIn **sockMessageIn)
{
    if (*sockMessageIn) {
        objectRelease(&(*sockMessageIn)->arg);
        arrayRelease(&(*sockMessageIn)->options);
        objectRelease(sockMessageIn);
    }
}

SockMessageOut*
sockMessageOutNew()
{
    SockMessageOut *sockMessageOut = calloc(1, sizeof(SockMessageOut));
    assert(sockMessageOut);
    sockMessageOut->errors = NULL;
    sockMessageOut->unitsDisplay = NULL;
    sockMessageOut->messages = NULL;
    return sockMessageOut;
}

void
sockMessageOutRelease(SockMessageOut **sockMessageOut)
{
    if (*sockMessageOut) {
        arrayRelease(&(*sockMessageOut)->unitsDisplay);
        arrayRelease(&(*sockMessageOut)->errors);
        arrayRelease(&(*sockMessageOut)->messages);
        objectRelease(sockMessageOut);
    }
}

int
initSocket(struct sockaddr_un *name)
{
    int socketConnection = -1;

    /* Connection */
    if ((socketConnection = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
        logError(CONSOLE, "src/core/socket/socket_server.c", "initSocket",
                      errno, strerror(errno), "Socket error");
    }
    else {
        /* Initialize sockaddr_un */
        memset(name, 0, sizeof(struct sockaddr_un));
        name->sun_family = AF_UNIX;
        strncpy(name->sun_path,
                !USER_INSTANCE ? SOCKET_PATH : SOCKET_USER_PATH,
                sizeof(name->sun_path) - 1);
    }

    return socketConnection;
}

int
unitdSockConn(int *socketConnection, struct sockaddr_un *name)
{
    int rv = 0;
    if ((rv = connect(*socketConnection, (const struct sockaddr *)name,
                      sizeof(struct sockaddr_un))) == -1) {
        logErrorStr(SYSTEM,
                         !USER_INSTANCE ?
                         "Unitd system instance could be not running!" :
                         "Unitd user instance could be not running!");
        rv = EUIDOWN;
    }
    return rv;
}

int
readMessage(int *socketFd, char **buffer, int *bufferSize)
{
    int rv = 0;

    assert(*buffer);
    assert(*socketFd != -1);
    assert(*bufferSize > 0);

    while (1) {
        if ((rv = uRecv(*socketFd, *buffer, *bufferSize, MSG_PEEK | MSG_TRUNC)) == -1) {
            logError(CONSOLE, "src/core/socket/socket_common.c",
                          "readMessage", errno, strerror(errno), "Recv error");
            goto out;
        }
        assert(rv > 0);
        /* If the data are more large than buffer then we re-allocate it */
        if (rv > *bufferSize) {
            *bufferSize = rv + 1;
            *buffer = realloc(*buffer, *bufferSize * sizeof(char));
            assert(*buffer);
        }
        else
            break;
    }

    out:
        (*buffer)[(*bufferSize) - 1] = '\0';
        return rv;
}

int
sortUnitsByName(const void *unitDisplayA, const void *unitDisplayB)
{
    return strcasecmp((*(Unit **)unitDisplayA)->name, (*(Unit **)unitDisplayB)->name);
}

void
setValueForBuffer(char **buffer, int value)
{
    assert(*buffer);
    if (value == -1 || value == -2)
        stringAppendStr(buffer, NONE);
    else {
        char valueStr[10] = {0};
        sprintf(valueStr, "%d", value);
        stringAppendStr(buffer, valueStr);
    }
}

Array*
getScriptParams(const char *unitName, const char *stateStr,
                const char *symlOperation, const char *unitPath)
{
    Array *scriptParams = NULL;
    char *command, *from, *to;
    command = from = to = NULL;

    /* Building 'to' parameter */
    from = stringNew(unitPath);
    /* Building 'from' parameter */
    to = !USER_INSTANCE ? stringNew(UNITS_ENAB_PATH) : stringNew(UNITS_USER_ENAB_PATH);
    stringAppendChr(&to, '/');
    stringAppendStr(&to, stateStr);
    stringAppendChr(&to, '/');
    stringAppendStr(&to, unitName);
    /* Create the script parameters array */
    scriptParams = arrayNew(objectRelease);
    /* Building the command which must be the first element */
    command = stringNew(UNITD_DATA_PATH);
    stringAppendStr(&command, "/scripts/symlink-handle.sh");
    arrayAdd(scriptParams, command); //0
    arrayAdd(scriptParams, stringNew(symlOperation)); //1
    arrayAdd(scriptParams, from); //2
    arrayAdd(scriptParams, to); //3
    /* Must be null terminated */
    arrayAdd(scriptParams, NULL); //4

    return scriptParams;
}

int
sendWallMsg(Command command)
{
    int rv = 0;
    char *msg, *cmd;

    msg = cmd = NULL;

    switch (command) {
        case REBOOT_COMMAND:
            msg = stringNew("System reboot in 5 seconds ...");
           break;
        case POWEROFF_COMMAND:
            msg = stringNew("System power off in 5 seconds ...");
            break;
        case HALT_COMMAND:
            msg = stringNew("System halt in 5 seconds ...");
            break;
        case KEXEC_COMMAND:
            msg = stringNew("System reboot with kexec in 5 seconds ...");
            break;
        default:
            break;
    }

    assert(msg);
    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "MSG", msg);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);
    /* Execute the script */
    rv = execUScript(&envVars, "send-wallmsg");
    arrayRelease(&envVars);
    objectRelease(&msg);
    return rv;
}

void
fillUnitsDisplayList(Array **units, Array **unitsDisplay, ListFilter listFilter)
{
    Unit *unit = NULL;
    int lenUnits = (*units ? (*units)->size : 0);
    assert(*unitsDisplay);
    bool add = false;
    PType pType = NO_PROCESS_TYPE;

    for (int i = 0; i < lenUnits; i++) {
        add = false;
        unit = arrayGet(*units, i);
        pType = unit->type;
        switch (listFilter) {
            case TIMERS_FILTER:
                if (pType == TIMER)
                    add = true;
                break;
            case UPATH_FILTER:
                if (pType == UPATH)
                    add = true;
                break;
            default:
                add = true;
                break;
        }
        if (add)
            arrayAdd(*unitsDisplay, unitNew(unit, PARSE_SOCK_RESPONSE_UNITLIST));
    }
}

int
loadAndCheckUnit(Array **unitsDisplay, bool isAggregate, const char *unitName,
                 bool parse, Array **errors)
{
    int rv = 0, *lenUnitsDisplay, oldLen;

    assert(*unitsDisplay);
    assert(*unitName);
    oldLen = (*unitsDisplay)->size;
    lenUnitsDisplay = &(*unitsDisplay)->size;

    if (!USER_INSTANCE) {
        loadUnits(unitsDisplay, UNITS_PATH, NULL, NO_STATE,
                  isAggregate, unitName, PARSE_SOCK_RESPONSE, parse);
    }
    else {
        loadUnits(unitsDisplay, UNITS_USER_PATH, NULL, NO_STATE,
                  isAggregate, unitName, PARSE_SOCK_RESPONSE, parse);
        if (*lenUnitsDisplay == oldLen) {
            loadUnits(unitsDisplay, UNITS_USER_LOCAL_PATH, NULL, NO_STATE,
                      isAggregate, unitName, PARSE_SOCK_RESPONSE, parse);
        }
    }
    if (*lenUnitsDisplay == oldLen) {
        if (!(*errors))
            *errors = arrayNew(objectRelease);
        arrayAdd(*errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_NOT_EXIST_ERR].desc, unitName));
        rv = 1;
    }

    return rv;
}

ListFilter
getListFilterByCommand(Command command)
{
    ListFilter listFilter = NO_FILTER;
    if (command != NO_COMMAND && command != LIST_COMMAND) {
        switch (command) {
            case LIST_ENABLED_COMMAND:
                listFilter = ENABLED_FILTER;
                break;
            case LIST_DISABLED_COMMAND:
                listFilter = DISABLED_FILTER;
                break;
            case LIST_STARTED_COMMAND:
                listFilter = STARTED_FILTER;
                break;
            case LIST_DEAD_COMMAND:
                listFilter = DEAD_FILTER;
                break;
            case LIST_FAILED_COMMAND:
                listFilter = FAILED_FILTER;
                break;
            case LIST_RESTARTABLE_COMMAND:
                listFilter = RESTARTABLE_FILTER;
                break;
            case LIST_RESTARTED_COMMAND:
                listFilter = RESTARTED_FILTER;
                break;
            case LIST_TIMERS_COMMAND:
                listFilter = TIMERS_FILTER;
                break;
            case LIST_UPATH_COMMAND:
                listFilter = UPATH_FILTER;
                break;
            default:
                break;
        }
    }
    return listFilter;
}

ListFilter
getListFilterByOpt(Array *options)
{
    const char *listFilterOpt = NULL;
    int len = options ? options->size : 0;
    for (int i = 0; i < len; i++) {
        listFilterOpt = arrayGet(options, i);
        for (ListFilter listFilter = ENABLED_FILTER; listFilter <= UPATH_FILTER; listFilter++) {
            if (stringEquals(listFilterOpt, LIST_FILTER_DATA[listFilter].desc)) {
                return listFilter;
            }
        }
    }
    return NO_FILTER;
}

void
applyListFilter(Array **unitsDisplay, ListFilter listFilter)
{
    Unit *unitDisplay = NULL;
    ProcessData *pData = NULL;
    bool match, remove;
    assert(*unitsDisplay);
    assert(listFilter != NO_FILTER);
    int *len = &(*unitsDisplay)->size;

    for (int i = 0; i < *len; i++) {
        unitDisplay = arrayGet(*unitsDisplay, i);
        pData = unitDisplay->processData;
        match = remove = false;
        switch (listFilter) {
            case ENABLED_FILTER:
                if (!unitDisplay->enabled)
                    remove = true;
                break;
            case DISABLED_FILTER:
                if (unitDisplay->enabled)
                    remove = true;
                break;
            case STARTED_FILTER:
                match = ((pData->pStateData->pState == RUNNING || pData->pStateData->pState == EXITED) &&
                        *pData->finalStatus == FINAL_STATUS_SUCCESS);
                if (!match)
                    remove = true;
                break;
            case DEAD_FILTER:
                match = (pData->pStateData->pState == DEAD &&
                        (*pData->finalStatus == FINAL_STATUS_READY || *pData->finalStatus == FINAL_STATUS_SUCCESS));
                if (!match)
                    remove = true;
                break;
            case FAILED_FILTER:
                if (*pData->finalStatus != FINAL_STATUS_FAILURE)
                    remove = true;
                break;
            case RESTARTABLE_FILTER:
                if (!unitDisplay->restart && unitDisplay->restartMax <= 0)
                    remove = true;
                break;
            case RESTARTED_FILTER:
                if (unitDisplay->restartNum <= 0 && *unitDisplay->processData->signalNum != SIGCONT)
                    remove = true;
                break;
            default:
                break;
        }
        if (remove) {
            arrayRemove(*unitsDisplay, unitDisplay);
            i--;
        }
    }

}
