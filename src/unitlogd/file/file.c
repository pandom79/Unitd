/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

int
unitlogdOpenBootLog(const char *mode)
{
    if (!UNITLOGD_BOOT_LOG_FILE) {
        const char *logPath = UNITLOGD_BOOT_LOG_PATH;
        UNITLOGD_BOOT_LOG_FILE = fopen(logPath, mode);
        if (!UNITLOGD_BOOT_LOG_FILE) {
            logError(CONSOLE, "src/unitlogd/file/file.c", "unitlogdOpenBootLog", errno, strerror(errno),
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
            logError(CONSOLE, "src/unitlogd/file/file.c", "unitlogdCloseBootLog", errno, strerror(errno),
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
            logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/unitlogd/file/file.c", "unitlogdOpenLog", errno, strerror(errno),
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
            logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/unitlogd/file/file.c", "unitlogdCloseLog", errno, strerror(errno),
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
            logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/unitlogd/file/file.c", "unitlogdOpenIndex", errno, strerror(errno),
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
            logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/unitlogd/file/file.c", "unitlogdCloseIndex", errno, strerror(errno),
                     "Unable to close the file '%s'", logPath);
        }
        UNITLOGD_INDEX_FILE = NULL;
    }

    return rv;
}

void
logEntry(FILE **file, const char *line)
{
    assert(*file);

    fflush(*file);
    fprintf(*file, line, NULL);
    fflush(*file);
}

char*
getLogOffset()
{
    off_t offset = 0;
    char offsetStr[50] = {0};
    char *ret = NULL;

    assert(!UNITLOGD_LOG_FILE);
    unitlogdOpenLog("r");
    assert(UNITLOGD_LOG_FILE);

    if (fseeko(UNITLOGD_LOG_FILE, 0, SEEK_END) == -1) {
        logError(UNITLOGD_BOOT_LOG, "src/unitlogd/file/file.c", "getLogOffset", errno, strerror(errno),
                 "Fseeko func returned -1");
        goto out;
    }

    if ((offset = ftello(UNITLOGD_LOG_FILE)) == -1) {
        logError(UNITLOGD_BOOT_LOG, "src/unitlogd/file/file.c", "getLogOffset", errno, strerror(errno),
                 "Ftello func returned -1");
        goto out;
    }

    sprintf(offsetStr, "%lu", offset);
    assert(offsetStr);
    ret = stringNew(offsetStr);

    out:
        /* Release resources */
        unitlogdCloseLog();
        assert(!UNITLOGD_LOG_FILE);

        return ret;
}

bool
matchLogLine(bool isStart, IndexEntry *indexEntry)
{
    bool match = false;
    off_t offset = 0;
    size_t len = 0;
    char *offSetStr, *line;

    assert(UNITLOGD_LOG_FILE);
    assert(indexEntry);

    offSetStr = line = NULL;

    if (isStart)
        offSetStr = indexEntry->startOffset;
    else
        offSetStr = indexEntry->stopOffset;
    assert(offSetStr);

    offset = atol(offSetStr);

    if (fseeko(UNITLOGD_LOG_FILE, offset, SEEK_SET) == -1) {
        logError(UNITLOGD_BOOT_LOG, "src/unitlogd/file/file.c", "getLogOffset", errno, strerror(errno),
                 "Fseeko func returned -1");
        goto out;
    }

    if (getline(&line, &len, UNITLOGD_LOG_FILE) != -1) {
        if (((isStart && stringStartsWithStr(line, ENTRY_STARTED)) ||
            (!isStart && stringStartsWithStr(line, ENTRY_FINISHED))) &&
            stringContainsStr(line, indexEntry->bootId))
            match = true;
        else {
            logError(UNITLOGD_BOOT_LOG, "src/unitlogd/file/file.c", "checkLogLine", 1, strerror(1),
                     "The index row with %s offset doesn't match any log row", offSetStr);
        }
    }

    out:
        objectRelease(&line);

        return match;
}

int
execUlScript(Array **envVars, const char *operation)
{
    int rv = 0;

    assert(*envVars);

    /* Building command */
    char *cmd = stringNew(UNITLOGD_DATA_PATH);
    stringAppendStr(&cmd, "/scripts/unitlogd.sh");

    /* Creating script params */
    Array *scriptParams = arrayNew(objectRelease);
    arrayAdd(scriptParams, cmd); //0
    arrayAdd(scriptParams, stringNew(operation)); //1
    /* Must be null terminated */
    arrayAdd(scriptParams, NULL);

    /* Execute the script */
    rv = execScript(UNITLOGD_DATA_PATH, "/scripts/unitlogd.sh", scriptParams->arr, (*envVars)->arr);
    if (rv != 0)
        logError(CONSOLE, "src/unitlogd/file/file.c", "execUlScript", rv, strerror(rv),
                 "ExecScript error for the '%s' operation", operation);

    arrayRelease(&scriptParams);

    return rv;
}