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

static const char *asStr(Keys key)
{
    assert(key >= COMMAND);
    switch (key) {
    case COMMAND:
        return "Command";
    case ARG:
        return "Arg";
    case OPTION:
        return "Option";
    default:
        return "";
    }
}

char *marshallRequest(SockMessageIn *sockMessageIn)
{
    char *buffer = NULL, commandStr[10];
    const char *arg = NULL, *optionKey = NULL;
    Array *options = NULL;
    int len = 0;

    assert(sockMessageIn);
    assert(sockMessageIn->command != NO_COMMAND);

    /* Command */
    buffer = stringNew(asStr(COMMAND));
    stringAppendStr(&buffer, ASSIGNER);
    sprintf(commandStr, "%d", sockMessageIn->command);
    stringAppendStr(&buffer, commandStr);
    stringAppendStr(&buffer, TOKEN);
    /* Unit name */
    arg = sockMessageIn->arg;
    if (sockMessageIn->arg) {
        stringAppendStr(&buffer, asStr(ARG));
        stringAppendStr(&buffer, ASSIGNER);
        stringAppendStr(&buffer, arg);
        stringAppendStr(&buffer, TOKEN);
    }
    /* Options */
    options = sockMessageIn->options;
    len = (options ? options->size : 0);
    if (len > 0)
        optionKey = asStr(OPTION);
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
    char key[BUFSIZ] = { 0 }, *value = NULL, entries[BUFSIZ] = { 0 }, c = 0;

    assert(buffer);
    assert(sockMessageIn);

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
            memmove(key, entries, strlen(entries) - strlen(value) - 1);
            if (stringEquals(asStr(COMMAND), key)) {
                (*sockMessageIn)->command = atoi(value);
                goto next;
            }
            if (stringEquals(asStr(ARG), key)) {
                (*sockMessageIn)->arg = stringNew(value);
                goto next;
            }
            if (stringEquals(asStr(OPTION), key)) {
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
        memset(entries, 0, BUFSIZ);
        memset(key, 0, BUFSIZ);
        continue;
    }

out:
    return rv;
}
