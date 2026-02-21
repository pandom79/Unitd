/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

/* COMMUNICATION PROTOCOL (REQUEST)

Command=value|          (required and not repeatable)
Arg=value|              (optional and not repeatable)
Option=value1|          (optional and repeatable)
Option=value2|
....
Option=valueN|

*/

/* Properties */
typedef enum {
    COMMAND = 0,
    ARG = 1,
    OPTION = 2,
} Keys;

static const key_value KEY_VALUE[] = {
    { COMMAND, "Command" },
    { ARG, "Arg" },
    { OPTION, "Option" },
};

char *marshallRequest(SockMessageIn *sockMessageIn)
{
    char *buffer = NULL, commandStr[10];
    const char *arg = NULL, *optionKey = NULL;
    Array *options = NULL;
    int len = 0;

    assert(sockMessageIn);
    assert(sockMessageIn->command != NO_COMMAND);

    buffer = stringNew(KEY_VALUE[COMMAND].value);
    stringAppendStr(&buffer, ASSIGNER);
    sprintf(commandStr, "%d", sockMessageIn->command);
    stringAppendStr(&buffer, commandStr);
    stringAppendStr(&buffer, TOKEN);
    arg = sockMessageIn->arg;
    if (sockMessageIn->arg) {
        stringAppendStr(&buffer, KEY_VALUE[ARG].value);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, arg);
        stringAppendStr(&buffer, TOKEN);
    }
    options = sockMessageIn->options;
    len = (options ? options->size : 0);
    if (len > 0)
        optionKey = KEY_VALUE[OPTION].value;
    for (int i = 0; i < len; i++) {
        stringAppendStr(&buffer, optionKey);
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, arrayGet(options, i));
        stringAppendStr(&buffer, TOKEN);
    }

    return buffer;
}

int unmarshallRequest(char *buffer, SockMessageIn **sockMessageIn)
{
    Array **options;
    int rv = 0, lenBuffer = 0;
    char key[BUFSIZ], *value = NULL, entries[BUFSIZ], c = 0;

    assert(buffer);
    assert(sockMessageIn);

    stringCopy(entries, "");
    options = &(*sockMessageIn)->options;
    lenBuffer = buffer ? strlen(buffer) : 0;
    for (int i = 0; i < lenBuffer; i++) {
        c = buffer[i];
        if (c != TOKEN[0]) {
            char chrStr[] = { c, '\0' };
            strcat(entries, chrStr);
            continue;
        } else {
            value = strstr(entries, ASSIGNER) + 1;
            stringCopyN(key, entries, strlen(entries) - strlen(value) - 1);
            if (stringEquals(KEY_VALUE[COMMAND].value, key)) {
                (*sockMessageIn)->command = atoi(value);
                goto next;
            }
            if (stringEquals(KEY_VALUE[ARG].value, key)) {
                (*sockMessageIn)->arg = stringNew(value);
                goto next;
            }
            if (stringEquals(KEY_VALUE[OPTION].value, key)) {
                if (!(*options))
                    *options = arrayNew(objectRelease);
                arrayAdd(*options, stringNew(value));
                goto next;
            }
            // Should never happen
            logError(CONSOLE | SYSTEM, "src/core/socket/socket_request.c", "unmarshallRequest",
                     EPERM, strerror(EPERM), "Property %s not found!", key);
            rv = EPERM;
            goto out;
        }
next:
        stringCopy(entries, "");
        stringCopy(key, "");
        continue;
    }

out:
    return rv;
}
