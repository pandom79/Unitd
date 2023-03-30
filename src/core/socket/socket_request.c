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

static const char*
asStr(Keys key)
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

char*
marshallRequest(SockMessageIn *sockMessageIn)
{
    char *buffer = NULL;
    const char *arg, *optionKey;
    char commandStr[10];
    Array *options = NULL;
    int len = 0;

    arg = optionKey = NULL;

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

int
unmarshallRequest(char *buffer, SockMessageIn **sockMessageIn)
{
    Array *entries, **options, *keyval;
    int rv, len;
    char *value, *entry, *key;

    rv = len = 0;
    entries = NULL;
    value = entry = key = NULL;

    assert(buffer);
    assert(sockMessageIn);
    options = &(*sockMessageIn)->options;

    /* Get the entries */
    entries = stringSplit(buffer, TOKEN, true);
    len = (entries ? entries->size : 0);
    for (int i = 0; i < len; i++) {
        entry = arrayGet(entries, i);
        /* Each entry has "Key(0)=Value(1)" format. */
        keyval = stringSplit(entry, ASSIGNER, false);
        key = arrayGet(keyval, 0);
        value = arrayGet(keyval, 1);
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
        logError(CONSOLE | SYSTEM, "src/core/socket/socket_request.c", "unmarshallRequest", EPERM,
                 strerror(EPERM), "Property %s not found!", key);
        arrayRelease(&keyval);
        rv = EPERM;
        goto out;

        next:
            arrayRelease(&keyval);
            continue;
    }

    out:
        arrayRelease(&entries);
        return rv;
}
