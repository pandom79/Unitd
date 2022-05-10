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

//CONFIGURING THE PARSER ACCORDING THE COMMUNICATION PROTOCOL

/* Properties */
enum PropertyNameEnum  {
    COMMAND = 0,
    ARG = 1,
    OPTION = 2,
};

/* Properties */
int SOCKREQ_PROPERTIES_ITEMS_LEN = 3;
PropertyData SOCKREQ_PROPERTIES_ITEMS[] = {
    { NO_SECTION, { COMMAND, "Command" }, false, true, false, 0, NULL, NULL },
    { NO_SECTION, { ARG, "Arg" }, false, false, false, 0, NULL, NULL },
    { NO_SECTION, { OPTION, "Option" }, true, false, false, 0, NULL, NULL },
};

//END PARSER CONFIGURATION

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
    buffer = stringNew(SOCKREQ_PROPERTIES_ITEMS[COMMAND].propertyName.desc);
    stringConcat(&buffer, ASSIGNER);
    sprintf(commandStr, "%d", sockMessageIn->command);
    stringConcat(&buffer, commandStr);
    stringConcat(&buffer, TOKEN);
    /* Unit name */
    arg = sockMessageIn->arg;
    if (sockMessageIn->arg) {
        stringConcat(&buffer, SOCKREQ_PROPERTIES_ITEMS[ARG].propertyName.desc);
        stringConcat(&buffer, ASSIGNER);
        stringConcat(&buffer, arg);
        stringConcat(&buffer, TOKEN);
    }
    /* Options */
    options = sockMessageIn->options;
    len = (options ? options->size : 0);
    if (len > 0)
        optionKey = SOCKREQ_PROPERTIES_ITEMS[OPTION].propertyName.desc;
    for (int i = 0; i < len; i++) {
        stringConcat(&buffer, optionKey);
        stringConcat(&buffer, ASSIGNER);
        stringConcat(&buffer, arrayGet(options, i));
        stringConcat(&buffer, TOKEN);
    }

    return buffer;
}

int
unmarshallRequest(char *buffer, SockMessageIn **sockMessageIn)
{
    Array *entries, *lineData, **options, *errors;
    int rv, len, errorsSize;
    PropertyData *propertyData = NULL;
    char *value, *error;

    rv = len = errorsSize = 0;
    entries = lineData = NULL;
    value = error = NULL;
    errors = NULL;

    assert(buffer);
    assert(sockMessageIn);
    options = &(*sockMessageIn)->options;

    /* Parser init */
    parserInit(PARSE_SOCK_REQUEST);

    /* Get the entries (simulating the file lines) */
    entries = stringSplit(buffer, TOKEN, true);
    len = (entries ? entries->size : 0);
    for (int i = 0; i < len; i++) {
        rv = parseLine(arrayGet(entries, i), i + 1, &lineData, &propertyData);
        if (lineData) {
            if (rv != 0) {
                error = arrayGet(lineData, 2);
                assert(error);
                syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in unmarshallRequest = %s\n",
                                             error);
                goto out;
            }
            if ((value = arrayGet(lineData, 1))) {
                switch (propertyData->propertyName.propertyNameEnum) {
                    case COMMAND:
                        (*sockMessageIn)->command = atoi(value);
                        break;
                    case ARG:
                        (*sockMessageIn)->arg = stringNew(value);
                        break;
                    case OPTION:
                        if (!(*options))
                            *options = arrayNew(objectRelease);
                        arrayAdd(*options, stringNew(value));
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
    errorsSize = (errors  ? errors->size : 0);
    if (errorsSize > 0) {
        syslog(LOG_DAEMON | LOG_ERR, "The parserEnd func in unmarshallRequest func has returned "
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
