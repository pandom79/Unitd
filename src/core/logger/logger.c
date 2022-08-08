/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

FILE *UNITD_LOG_FILE = NULL;
char *UNITD_USER_LOG_PATH;

int
unitdOpenLog(const char *mode)
{
    if (!UNITD_LOG_FILE) {
        /* Set log path */
        const char *unitdLogPath = NULL;
        if (!USER_INSTANCE)
            unitdLogPath = UNITD_LOG_PATH;
        else
            unitdLogPath = UNITD_USER_LOG_PATH;

        UNITD_LOG_FILE = fopen(unitdLogPath, mode);
        if (!UNITD_LOG_FILE) {
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/logger/logger.c", "unitdOpenLog", errno, strerror(errno),
                          "Unable to open the log %s in mode '%s'", unitdLogPath, mode);
            return -1;
        }
    }

    return 0;
}

int
unitdCloseLog()
{
    int rv = 0;

    if (UNITD_LOG_FILE) {
        /* Set log path */
        const char *unitdLogPath = NULL;
        if (!USER_INSTANCE)
            unitdLogPath = UNITD_LOG_PATH;
        else
            unitdLogPath = UNITD_USER_LOG_PATH;

        if ((rv = fclose(UNITD_LOG_FILE)) != 0) {
            unitdLogError(LOG_UNITD_ALL, "src/core/logger/logger.c", "unitdCloseLog", errno, strerror(errno),
                                         "Unable to close the log '%s'", unitdLogPath);
        }
        UNITD_LOG_FILE = NULL;
    }

    return rv;
}

void
unitdLogInfo(int options, const char *format, ...)
{
    va_list args;
    if (options & LOG_UNITD_CONSOLE) {
        fflush(stdout);
        va_start(args, format);
        printf(LIGHT_WHITE_COLOR);
        vprintf(format, args);
        printf(DEFAULT_COLOR);
        va_end(args);
        fflush(stdout);
    }
    if (UNITD_LOG_FILE && (options & LOG_UNITD_BOOT)) {
        fflush(UNITD_LOG_FILE);
        va_start(args, format);
        vfprintf(UNITD_LOG_FILE, LIGHT_WHITE_COLOR, NULL);
        vfprintf(UNITD_LOG_FILE, format, args);
        vfprintf(UNITD_LOG_FILE, DEFAULT_COLOR, NULL);
        va_end(args);
        fflush(UNITD_LOG_FILE);
    }
}

void
unitdLogWarning(int options, const char *format, ...)
{
    va_list args;
    if (options & LOG_UNITD_CONSOLE) {
        fflush(stdout);
        va_start(args, format);
        printf(YELLOW_COLOR);
        vprintf(format, args);
        printf(DEFAULT_COLOR);
        va_end(args);
        fflush(stdout);
    }
    if (UNITD_LOG_FILE && (options & LOG_UNITD_BOOT)) {
        fflush(UNITD_LOG_FILE);
        va_start(args, format);
        vfprintf(UNITD_LOG_FILE, YELLOW_COLOR, NULL);
        vfprintf(UNITD_LOG_FILE, format, args);
        vfprintf(UNITD_LOG_FILE, DEFAULT_COLOR, NULL);
        va_end(args);
        fflush(UNITD_LOG_FILE);
    }
}

void
unitdLogErrorStr(int options, const char *format, ...)
{
    va_list args;
    if (options & LOG_UNITD_CONSOLE) {
        fflush(stdout);
        va_start(args, format);
        printf(RED_COLOR);
        vprintf(format, args);
        printf(DEFAULT_COLOR);
        va_end(args);
        fflush(stdout);
    }
    if (UNITD_LOG_FILE && (options & LOG_UNITD_BOOT)) {
        fflush(UNITD_LOG_FILE);
        va_start(args, format);
        vfprintf(UNITD_LOG_FILE, RED_COLOR, NULL);
        vfprintf(UNITD_LOG_FILE, format, args);
        vfprintf(UNITD_LOG_FILE, DEFAULT_COLOR, NULL);
        va_end(args);
        fflush(UNITD_LOG_FILE);
    }
}

void
unitdLogSuccess(int options, const char *format, ...)
{
    va_list args;
    if (options & LOG_UNITD_CONSOLE) {
        fflush(stdout);
        va_start(args, format);
        printf(GREEN_COLOR);
        vprintf(format, args);
        printf(DEFAULT_COLOR);
        va_end(args);
        fflush(stdout);
    }
    if (UNITD_LOG_FILE && (options & LOG_UNITD_BOOT)) {
        fflush(UNITD_LOG_FILE);
        va_start(args, format);
        vfprintf(UNITD_LOG_FILE, GREEN_COLOR, NULL);
        vfprintf(UNITD_LOG_FILE, format, args);
        vfprintf(UNITD_LOG_FILE, DEFAULT_COLOR, NULL);
        va_end(args);
        fflush(UNITD_LOG_FILE);
    }
}

void
unitdLogError(int options, const char *transUnit, const char *funcName,
         int returnValue, const char *errDesc, const char *format, ...)
{
    va_list args;
    char returnValueStr[100];
    memset(&returnValueStr, ' ', sizeof(returnValueStr));

    if (options & LOG_UNITD_CONSOLE) {
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

    if (UNITD_LOG_FILE && (options & LOG_UNITD_BOOT)) {
        fflush(UNITD_LOG_FILE);
        va_start(args, format);
        if (returnValueStr[0] == ' ')
            sprintf(returnValueStr, "%d", returnValue);

        fprintf(UNITD_LOG_FILE, RED_COLOR);
        fprintf(UNITD_LOG_FILE, "\nAn error has occurred in :\n");
        fprintf(UNITD_LOG_FILE, "File: ");
        fprintf(UNITD_LOG_FILE, transUnit);
        fprintf(UNITD_LOG_FILE, "\nFunction: ");
        fprintf(UNITD_LOG_FILE, funcName);
        fprintf(UNITD_LOG_FILE, "\nReturn value: ");
        fprintf(UNITD_LOG_FILE, returnValueStr);
        fprintf(UNITD_LOG_FILE, "\nDescription: ");
        fprintf(UNITD_LOG_FILE, errDesc);
        fprintf(UNITD_LOG_FILE, "\n");
        vfprintf(UNITD_LOG_FILE, format, args);
        fprintf(UNITD_LOG_FILE, "\n");
        fprintf(UNITD_LOG_FILE, DEFAULT_COLOR);
        va_end(args);
        fflush(UNITD_LOG_FILE);
    }

}
