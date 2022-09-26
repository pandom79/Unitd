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
            logError(CONSOLE, "src/core/file/file.c", "unitdOpenLog", errno, strerror(errno),
                          "Unable to open the file '%s' in mode '%s'", unitdLogPath, mode);
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
            logError(CONSOLE, "src/core/file/file.c", "unitdCloseLog", errno, strerror(errno),
                                         "Unable to close the file '%s'", unitdLogPath);
        }
        UNITD_BOOT_LOG_FILE = NULL;
    }

    return rv;
}

int
unitlogdOpenBootLog(const char *mode)
{
    if (!UNITLOGD_BOOT_LOG_FILE) {
        const char *logPath = UNITLOGD_BOOT_LOG_PATH;
        UNITLOGD_BOOT_LOG_FILE = fopen(logPath, mode);
        if (!UNITLOGD_BOOT_LOG_FILE) {
            logError(CONSOLE, "src/core/file/file.c", "unitlogdOpenBootLog", errno, strerror(errno),
                          "Unable to open the file '%s' in mode '%s'", logPath, mode);
            return -1;
        }
    }

    return 0;
}

int
unitlogdCloseBootLog()
{
    int rv = 0;

    if (UNITLOGD_BOOT_LOG_FILE) {
        const char *logPath = UNITLOGD_BOOT_LOG_PATH;
        if ((rv = fclose(UNITLOGD_BOOT_LOG_FILE)) != 0) {
            logError(CONSOLE, "src/core/file/file.c", "unitlogdCloseBootLog", errno, strerror(errno),
                     "Unable to close the file '%s'", logPath);
        }
        UNITLOGD_BOOT_LOG_FILE = NULL;
    }

    return rv;
}

int
unitlogdOpenLog(const char *mode)
{
    if (!UNITLOGD_LOG_FILE) {
        const char *logPath = UNITLOGD_LOG_PATH;
        UNITLOGD_LOG_FILE = fopen(logPath, mode);
        if (!UNITLOGD_LOG_FILE) {
            logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/core/file/file.c", "unitlogdOpenLog", errno, strerror(errno),
                          "Unable to open the file '%s' in mode '%s'", logPath, mode);
            return -1;
        }
    }

    return 0;
}

int
unitlogdCloseLog()
{
    int rv = 0;

    if (UNITLOGD_LOG_FILE) {
        const char *logPath = UNITLOGD_LOG_PATH;
        if ((rv = fclose(UNITLOGD_LOG_FILE)) != 0) {
            logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/core/file/file.c", "unitlogdCloseLog", errno, strerror(errno),
                                         "Unable to close the file '%s'", logPath);
        }
        UNITLOGD_LOG_FILE = NULL;
    }

    return rv;
}


int
unitlogdOpenIndex(const char *mode)
{
    if (!UNITLOGD_INDEX_FILE) {
        const char *logPath = UNITLOGD_INDEX_PATH;
        UNITLOGD_INDEX_FILE = fopen(logPath, mode);
        if (!UNITLOGD_INDEX_FILE) {
            logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/core/file/file.c", "unitlogdOpenIndex", errno, strerror(errno),
                          "Unable to open the file '%s' in mode '%s'", logPath, mode);
            return -1;
        }
    }

    return 0;
}

int
unitlogdCloseIndex()
{
    int rv = 0;

    if (UNITLOGD_INDEX_FILE) {
        const char *logPath = UNITLOGD_INDEX_PATH;
        if ((rv = fclose(UNITLOGD_INDEX_FILE)) != 0) {
            logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/core/file/file.c", "unitlogdCloseIndex", errno, strerror(errno),
                     "Unable to close the file '%s'", logPath);
        }
        UNITLOGD_INDEX_FILE = NULL;
    }

    return rv;
}
