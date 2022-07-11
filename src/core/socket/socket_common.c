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
        if (strcmp(command, COMMANDS_DATA[i].name) == 0)
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
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_server.c", "initSocket",
                      errno, strerror(errno), "Socket error");
    }
    else {
        /* Initialize sockaddr_un */
        memset(name, 0, sizeof(struct sockaddr_un));
        name->sun_family = AF_UNIX;
        strncpy(name->sun_path, SOCKET_NAME, sizeof(name->sun_path) - 1);
    }

    return socketConnection;
}

int
unitdSockConn(int *socketConnection, struct sockaddr_un *name)
{
    int rv = 0;
    if ((rv = connect(*socketConnection, (const struct sockaddr *)name,
                      sizeof(struct sockaddr_un))) == -1) {
        unitdLogErrorStr(LOG_UNITD_CONSOLE, "The unitd daemon could be not running!\n");
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
        if ((rv = recv(*socketFd, *buffer, *bufferSize, MSG_PEEK | MSG_TRUNC)) == -1) {
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_common.c",
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
        stringConcat(buffer, NONE);
    else {
        char valueStr[10] = {0};
        sprintf(valueStr, "%d", value);
        stringConcat(buffer, valueStr);
    }
}

Array*
getScriptParams(const char *unitName, const char *stateStr, const char *symlOperation)
{
    Array *scriptParams = NULL;
    char *command, *from, *to;
    command = from = to = NULL;

    /* Building 'to' parameter */
    from = stringNew(UNITS_PATH);
    stringAppendChr(&from, '/');
    stringAppendStr(&from, unitName);
    stringAppendStr(&from, ".unit");
    /* Building 'from' parameter */
    to = stringNew(UNITS_ENAB_PATH);
    stringAppendChr(&to, '/');
    stringAppendStr(&to, stateStr);
    stringAppendChr(&to, '/');
    stringAppendStr(&to, unitName);
    stringAppendStr(&to, ".unit");
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
            msg = stringNew("Reboot the system ...");
           break;
        case POWEROFF_COMMAND:
            msg = stringNew("Power off the system ...");
            break;
        case HALT_COMMAND:
            msg = stringNew("Halt the system ...");
            break;
        case KEXEC_COMMAND:
            msg = stringNew("Reboot the system with kexec ...");
            break;
        default:
            break;
    }

    assert(msg);
    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);

    /* Building command */
    cmd = stringNew(UNITD_DATA_PATH);
    stringAppendStr(&cmd, "/scripts/send-wallmsg.sh");

    /* Creating script params */
    Array *scriptParams = arrayNew(objectRelease);
    arrayAdd(scriptParams, cmd); //0
    arrayAdd(scriptParams, msg); //1
    arrayAdd(scriptParams, NULL); //2

    /* Execute the script */
    rv = execScript(UNITD_DATA_PATH, "/scripts/send-wallmsg.sh", scriptParams->arr, envVars->arr);
    if (rv != 0) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/socket/socket_common.c",
                      "sendWallMsg", rv, strerror(rv), "ExecScript error");
    }

    arrayRelease(&envVars);
    arrayRelease(&scriptParams);
    return rv;
}

int
checkAdministrator(char **argv)
{
    int rv = 0;
    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "UNITD_DATA_PATH", UNITD_DATA_PATH);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);

    /* Building command */
    char *cmd = stringNew(UNITD_DATA_PATH);
    stringAppendStr(&cmd, "/scripts/administrator-check.sh");

    /* Creating script params */
    Array *scriptParams = arrayNew(objectRelease);
    arrayAdd(scriptParams, cmd); //0
    while (*argv) {
        arrayAdd(scriptParams, stringNew(*argv));
        argv++;
    }
    /* Must be null terminated */
    arrayAdd(scriptParams, NULL); //last

    /* Execute the script */
    rv = execScript(UNITD_DATA_PATH, "/scripts/administrator-check.sh", scriptParams->arr, envVars->arr);
    arrayRelease(&scriptParams);
    arrayRelease(&envVars);

    return rv;
}

void
fillUnitsDisplayList(Array **units, Array **unitsDisplay)
{
    int lenUnits = (*units ? (*units)->size : 0);
    if (lenUnits > 0 && !(*unitsDisplay))
        *unitsDisplay = arrayNew(unitRelease);
    for (int i = 0; i < lenUnits; i++)
        arrayAdd(*unitsDisplay, unitNew(arrayGet(*units, i), PARSE_SOCK_RESPONSE_UNITLIST));
}
