/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

FILE *UNITD_BOOT_LOG_FILE = NULL;
char *UNITD_USER_LOG_PATH;
FILE *UNITLOGD_BOOT_LOG_FILE;
FILE *UNITLOGD_INDEX_FILE;
FILE *UNITLOGD_LOG_FILE;

static void
writeFile(FILE **file, const char *color, const char *format, va_list *args)
{
    if (*file && format) {
        fflush(*file);
        fprintf(*file, color);
        vfprintf(*file, format, args);
        fprintf(*file, DEFAULT_COLOR);
        fflush(*file);
    }
}

static void
writeErrorFile(FILE **file, const char *transUnit, const char *funcName, int returnValue,
               char *returnValueStr, const char *errDesc, const char *format, va_list *args)
{
    if (*file && format) {
        fflush(*file);
        if (!(*returnValueStr))
            sprintf(returnValueStr, "%d", returnValue);
        fprintf(*file, RED_COLOR);
        fprintf(*file, "\nAn error has occurred\n");
        fprintf(*file, "File: ");
        fprintf(*file, transUnit);
        fprintf(*file, "\nFunction: ");
        fprintf(*file, funcName);
        fprintf(*file, "\nReturn value: ");
        fprintf(*file, returnValueStr);
        fprintf(*file, "\nDescription: ");
        fprintf(*file, errDesc);
        fprintf(*file, "\n");
        vfprintf(*file, format, args);
        fprintf(*file, "\n");
        fprintf(*file, DEFAULT_COLOR);
        fflush(*file);
    }
}

static void
logSystem(int priority, const char *color, const char *format, va_list *args)
{
    if (format) {
        char *colorStr = color ? stringNew(format) : NULL;
        if (colorStr) {
            stringPrependStr(&colorStr, color);
            stringAppendStr(&colorStr, DEFAULT_COLOR);
        }
        /* To 'va_start' function we pass 'format' to avoid compiler warning.
         * We can do that because the same "args" present in "format", are present in "colorStr" as well.
         */
        vsyslog(priority, colorStr ? colorStr : format, *args);
        objectRelease(&colorStr);
    }
}

void
logInfo(int options, const char *format, ...)
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
    if (UNITD_BOOT_LOG_FILE && (options & UNITD_BOOT_LOG)) {
        va_start(args, format);
        writeFile(&UNITD_BOOT_LOG_FILE, LIGHT_WHITE_COLOR, format, &args);
        va_end(args);
    }
    if (UNITLOGD_BOOT_LOG_FILE && (options & UNITLOGD_BOOT_LOG)) {
        va_start(args, format);
        writeFile(&UNITLOGD_BOOT_LOG_FILE, LIGHT_WHITE_COLOR, format, &args);
        va_end(args);
    }
    if (options & SYSTEM) {
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_INFO, NULL, format, &args);
        va_end(args);
    }
}

void
logWarning(int options, const char *format, ...)
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
    if (UNITD_BOOT_LOG_FILE && (options & UNITD_BOOT_LOG)) {
        va_start(args, format);
        writeFile(&UNITD_BOOT_LOG_FILE, YELLOW_COLOR, format, &args);
        va_end(args);
    }
    if (UNITLOGD_BOOT_LOG_FILE && (options & UNITLOGD_BOOT_LOG)) {
        va_start(args, format);
        writeFile(&UNITLOGD_BOOT_LOG_FILE, YELLOW_COLOR, format, &args);
        va_end(args);
    }
    if (options & SYSTEM) {
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_WARNING, YELLOW_COLOR, format, &args);
        va_end(args);
    }
}

void
logErrorStr(int options, const char *format, ...)
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
    if (UNITD_BOOT_LOG_FILE && (options & UNITD_BOOT_LOG)) {
        va_start(args, format);
        writeFile(&UNITD_BOOT_LOG_FILE, RED_COLOR, format, &args);
        va_end(args);
    }
    if (UNITLOGD_BOOT_LOG_FILE && (options & UNITLOGD_BOOT_LOG)) {
        va_start(args, format);
        writeFile(&UNITLOGD_BOOT_LOG_FILE, RED_COLOR, format, &args);
        va_end(args);
    }
    if (options & SYSTEM) {
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_ERR, RED_COLOR, format, &args);
        va_end(args);
    }
}

void
logSuccess(int options, const char *format, ...)
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
    if (UNITD_BOOT_LOG_FILE && (options & UNITD_BOOT_LOG)) {
        va_start(args, format);
        writeFile(&UNITD_BOOT_LOG_FILE, GREEN_COLOR, format, &args);
        va_end(args);
    }
    if (UNITLOGD_BOOT_LOG_FILE && (options & UNITLOGD_BOOT_LOG)) {
        va_start(args, format);
        writeFile(&UNITLOGD_BOOT_LOG_FILE, GREEN_COLOR, format, &args);
        va_end(args);
    }
    if (options & SYSTEM) {
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_INFO, GREEN_COLOR, format, &args);
        va_end(args);
    }
}

void
logError(int options, const char *transUnit, const char *funcName,
         int returnValue, const char *errDesc, const char *format, ...)
{
    va_list args;
    char returnValueStr[100] = {0};

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
    if (UNITD_BOOT_LOG_FILE && (options & UNITD_BOOT_LOG)) {
        va_start(args, format);
        writeErrorFile(&UNITD_BOOT_LOG_FILE, transUnit, funcName, returnValue,
                       returnValueStr, errDesc, format, &args);
        va_end(args);
    }
    if (UNITLOGD_BOOT_LOG_FILE && (options & UNITLOGD_BOOT_LOG)) {
        va_start(args, format);
        writeErrorFile(&UNITLOGD_BOOT_LOG_FILE, transUnit, funcName, returnValue,
                       returnValueStr, errDesc, format, &args);
        va_end(args);
    }
    if (options & SYSTEM) {
        if (!(*returnValueStr))
            sprintf(returnValueStr, "%d", returnValue);
        char *all = stringNew("An error has occurred! File = ");
        stringConcat(&all, transUnit);
        stringConcat(&all, ", Function = ");
        stringConcat(&all, funcName);
        stringConcat(&all, ", Return value = ");
        stringConcat(&all, returnValueStr);
        stringConcat(&all, ", Description = ");
        stringConcat(&all, errDesc);
        stringConcat(&all, ". ");
        stringConcat(&all, format);
        /* To 'va_start' function we pass 'format' to avoid compiler warning.
         * We can do that because the same "args" present in "format", are present in "all" as well.
         */
        va_start(args, format);
        logSystem(LOG_DAEMON | LOG_ERR, RED_COLOR, all, &args);
        va_end(args);
        objectRelease(&all);
    }
}
