/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

int
unitdOpenLog(const char *mode)
{
    if (!UNITD_BOOT_LOG_FILE) {
        const char *unitdLogPath = !USER_INSTANCE ? UNITD_LOG_PATH : UNITD_USER_LOG_PATH;
        UNITD_BOOT_LOG_FILE = fopen(unitdLogPath, mode);
        if (!UNITD_BOOT_LOG_FILE) {
            logError(CONSOLE, "src/core/logger/logger.c", "unitdOpenLog", errno, strerror(errno),
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

    if (UNITD_BOOT_LOG_FILE) {
        const char *unitdLogPath = !USER_INSTANCE ? UNITD_LOG_PATH : UNITD_USER_LOG_PATH;
        if ((rv = fclose(UNITD_BOOT_LOG_FILE)) != 0) {
            logError(ALL, "src/core/logger/logger.c", "unitdCloseLog", errno, strerror(errno),
                                         "Unable to close the log '%s'", unitdLogPath);
        }
        UNITD_BOOT_LOG_FILE = NULL;
    }

    return rv;
}

int
unitlogdOpenLog(const char *mode)
{
    return 0;
}

int
unitlogdOpenBootLog(const char *mode)
{
//FIXME
    return 0;
}

int
unitlogdOpenIndex(const char *mode)
{
//FIXME
    return 0;
}

int
unitlogdCloseLog()
{
//FIXME
    return 0;
}

int
unitlogdCloseBootLog()
{
//FIXME
    return 0;
}

int
unitlogdCloseIndex()
{
//FIXME
    return 0;
}
