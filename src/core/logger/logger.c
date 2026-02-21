/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

FILE *UNITLOGD_INDEX_FILE;
FILE *UNITLOGD_LOG_FILE;

static void logSystem(int priority, const char *color, const char *format, va_list *args)
{
    if (format) {
        char *colorStr = color ? stringNew(format) : NULL;
        if (colorStr) {
            stringPrependStr(&colorStr, color);
            stringAppendStr(&colorStr, DEFAULT_COLOR);
        }
        /* To 'va_start' function we pass 'format' to avoid compiler warning.
         * We can do that because the "args" present in "format", are present in "colorStr" as well.
         */
        vsyslog(priority, colorStr ? colorStr : format, *args);
        objectRelease(&colorStr);
    }
}

void logInfo(int options, const char *format, ...)
{
    va_list args;
    if (options & CONSOLE) {
        fflush(stdout);
        va_start(args, format);
        printf(LIGHT_WHITE_COLOR);
        vprintf(format, args);
        printf(DEFAULT_COLOR);
        va_end(args);
        fflush(stdout);
    }
    if (options & SYSTEM) {
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_INFO, NULL, format, &args);
        va_end(args);
    }
}

void logWarning(int options, const char *format, ...)
{
    va_list args;
    if (options & CONSOLE) {
        fflush(stdout);
        va_start(args, format);
        printf(YELLOW_COLOR);
        vprintf(format, args);
        printf(DEFAULT_COLOR);
        va_end(args);
        fflush(stdout);
    }
    if (options & SYSTEM) {
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_WARNING, YELLOW_COLOR, format, &args);
        va_end(args);
    }
}

void logErrorStr(int options, const char *format, ...)
{
    va_list args;
    if (options & CONSOLE) {
        fflush(stdout);
        va_start(args, format);
        printf(RED_COLOR);
        vprintf(format, args);
        printf(DEFAULT_COLOR);
        va_end(args);
        fflush(stdout);
    }
    if (options & SYSTEM) {
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_ERR, RED_COLOR, format, &args);
        va_end(args);
    }
}

void logSuccess(int options, const char *format, ...)
{
    va_list args;
    if (options & CONSOLE) {
        fflush(stdout);
        va_start(args, format);
        printf(GREEN_COLOR);
        vprintf(format, args);
        printf(DEFAULT_COLOR);
        va_end(args);
        fflush(stdout);
    }
    if (options & SYSTEM) {
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_INFO, GREEN_COLOR, format, &args);
        va_end(args);
    }
}

void logError(int options, const char *transUnit, const char *funcName, int returnValue,
              const char *errDesc, const char *format, ...)
{
    va_list args;
    char returnValueStr[100] = { 0 };

    if (options & CONSOLE) {
        fflush(stdout);
        va_start(args, format);
        sprintf(returnValueStr, "%d", returnValue);
        printf(RED_COLOR);
        printf("\nAn error has occurred\n");
        printf("File: ");
        printf(transUnit);
        printf("\nFunction: ");
        printf(funcName);
        printf("\nReturn value: ");
        printf(returnValueStr);
        printf("\nDescription: ");
        printf(errDesc);
        printf("\n");
        vprintf(format, args);
        printf("\n");
        printf(DEFAULT_COLOR);
        va_end(args);
        fflush(stdout);
    }
    if (options & SYSTEM) {
        if (!(*returnValueStr))
            sprintf(returnValueStr, "%d", returnValue);
        char *all = stringNew("An error has occurred! File = ");
        stringAppendStr(&all, transUnit);
        stringAppendStr(&all, ", Function = ");
        stringAppendStr(&all, funcName);
        stringAppendStr(&all, ", Return value = ");
        stringAppendStr(&all, returnValueStr);
        stringAppendStr(&all, ", Description = ");
        stringAppendStr(&all, errDesc);
        stringAppendStr(&all, ". ");
        stringAppendStr(&all, format);
        /* To 'va_start' function we pass 'format' to avoid compiler warning.
         * We can do that because the same "args" present in "format", are present in "all" as well.
         */
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_ERR, RED_COLOR, all, &args);
        va_end(args);
        objectRelease(&all);
    }
}
