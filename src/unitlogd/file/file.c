/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

static struct flock *FLOCK = NULL;
static int FD_LOCK = -1;

int unitlogdOpenLog(const char *mode)
{
    if (!UNITLOGD_LOG_FILE) {
        const char *logPath = UNITLOGD_LOG_PATH;
        UNITLOGD_LOG_FILE = fopen(logPath, mode);
        if (!UNITLOGD_LOG_FILE) {
            logError(CONSOLE, "src/unitlogd/file/file.c", "unitlogdOpenLog", errno, strerror(errno),
                     "Unable to open the file '%s' in mode '%s'", logPath, mode);
            return -1;
        }
    }

    return 0;
}

int unitlogdCloseLog()
{
    int rv = 0;

    if (UNITLOGD_LOG_FILE) {
        const char *logPath = UNITLOGD_LOG_PATH;
        if ((rv = fclose(UNITLOGD_LOG_FILE)) != 0) {
            logError(CONSOLE, "src/unitlogd/file/file.c", "unitlogdCloseLog", errno,
                     strerror(errno), "Unable to close the file '%s'", logPath);
        }
        UNITLOGD_LOG_FILE = NULL;
    }

    return rv;
}

int unitlogdOpenIndex(const char *mode)
{
    if (!UNITLOGD_INDEX_FILE) {
        const char *logPath = UNITLOGD_INDEX_PATH;
        UNITLOGD_INDEX_FILE = fopen(logPath, mode);
        if (!UNITLOGD_INDEX_FILE) {
            logError(CONSOLE, "src/unitlogd/file/file.c", "unitlogdOpenIndex", errno,
                     strerror(errno), "Unable to open the file '%s' in mode '%s'", logPath, mode);
            return -1;
        }
    }

    return 0;
}

int unitlogdCloseIndex()
{
    int rv = 0;

    if (UNITLOGD_INDEX_FILE) {
        const char *logPath = UNITLOGD_INDEX_PATH;
        if ((rv = fclose(UNITLOGD_INDEX_FILE)) != 0) {
            logError(CONSOLE, "src/unitlogd/file/file.c", "unitlogdCloseIndex", errno,
                     strerror(errno), "Unable to close the file '%s'", logPath);
        }
        UNITLOGD_INDEX_FILE = NULL;
    }

    return rv;
}

int unitlogdOpenKmsg(const char *mode)
{
    if (!UNITLOGD_KMSG_FILE) {
        const char *kmsgPath = UNITLOGD_KMSG_PATH;
        UNITLOGD_KMSG_FILE = fopen(kmsgPath, mode);
        if (!UNITLOGD_KMSG_FILE) {
            logError(CONSOLE, "src/unitlogd/file/file.c", "unitlogdOpenKmsg", errno,
                     strerror(errno), "Unable to open the file '%s' in mode '%s'", kmsgPath, mode);
            return -1;
        }
    }

    return 0;
}

int unitlogdCloseKmsg()
{
    int rv = 0;

    if (UNITLOGD_KMSG_FILE) {
        const char *kmsgPath = UNITLOGD_KMSG_PATH;
        if ((rv = fclose(UNITLOGD_KMSG_FILE)) != 0) {
            logError(CONSOLE, "src/unitlogd/file/file.c", "unitlogdCloseKmsg", errno,
                     strerror(errno), "Unable to close the file '%s'", kmsgPath);
        }
        UNITLOGD_KMSG_FILE = NULL;
    }

    return rv;
}

void logEntry(FILE **file, const char *line)
{
    assert(*file);

    fflush(*file);
    fprintf(*file, line, NULL);
    fflush(*file);
}

char *getLogOffset()
{
    off_t offset = 0;
    char offsetStr[50] = { 0 }, *ret = NULL;

    assert(!UNITLOGD_LOG_FILE);

    unitlogdOpenLog("r");
    assert(UNITLOGD_LOG_FILE);
    if (fseeko(UNITLOGD_LOG_FILE, 0, SEEK_END) == -1) {
        logError(CONSOLE, "src/unitlogd/file/file.c", "getLogOffset", errno, strerror(errno),
                 "Fseeko func returned -1");
        goto out;
    }
    if ((offset = ftello(UNITLOGD_LOG_FILE)) == -1) {
        logError(CONSOLE, "src/unitlogd/file/file.c", "getLogOffset", errno, strerror(errno),
                 "Ftello func returned -1");
        goto out;
    }
    sprintf(offsetStr, "%lu", offset);
    ret = stringNew(offsetStr);

out:
    unitlogdCloseLog();
    assert(!UNITLOGD_LOG_FILE);
    return ret;
}

bool matchLogLine(bool isStart, IndexEntry *indexEntry)
{
    bool match = false;
    off_t offset = 0;
    size_t len = 0;
    char *offSetStr = NULL, *line = NULL;

    assert(UNITLOGD_LOG_FILE);
    assert(indexEntry);

    if (isStart)
        offSetStr = indexEntry->startOffset;
    else
        offSetStr = indexEntry->stopOffset;
    assert(offSetStr);
    offset = atol(offSetStr);
    if (fseeko(UNITLOGD_LOG_FILE, offset, SEEK_SET) == -1) {
        logError(CONSOLE, "src/unitlogd/file/file.c", "getLogOffset", errno, strerror(errno),
                 "Fseeko func returned -1");
        goto out;
    }
    if (getline(&line, &len, UNITLOGD_LOG_FILE) != -1) {
        if (((isStart && stringStartsWithStr(line, ENTRY_STARTED)) ||
             (!isStart && stringStartsWithStr(line, ENTRY_FINISHED))) &&
            stringContainsStr(line, indexEntry->bootId))
            match = true;
        else {
            logError(CONSOLE, "src/unitlogd/file/file.c", "checkLogLine", 1, strerror(1),
                     "The index row with %s offset doesn't match any log row", offSetStr);
        }
    }

out:
    objectRelease(&line);
    return match;
}

int execUlScript(Array **envVars, const char *operation)
{
    int rv = 0;

    assert(*envVars);

    char *cmd = stringNew(UNITLOGD_DATA_PATH);
    stringAppendStr(&cmd, "/scripts/unitlogd.sh");
    Array *scriptParams = arrayNew(objectRelease);
    arrayAdd(scriptParams, cmd); //0
    arrayAdd(scriptParams, stringNew(operation)); //1
    arrayAdd(scriptParams, NULL);
    rv = execScript(UNITLOGD_DATA_PATH, "/scripts/unitlogd.sh", scriptParams->arr, (*envVars)->arr);
    if (rv != 0)
        logError(CONSOLE, "src/unitlogd/file/file.c", "execUlScript", rv, strerror(rv),
                 "ExecScript error for the '%s' operation", operation);

    arrayRelease(&scriptParams);
    return rv;
}

int getLockFileFd()
{
    int fd = -1;

    if ((fd = open(UNITLOGD_LOCK_PATH, O_WRONLY)) == -1) {
        logError(CONSOLE | SYSTEM, "src/unitlogd/file/file.c", "getLockFileFd", errno,
                 strerror(errno), "Unable to open '%s' file", UNITLOGD_LOCK_PATH);
    }

    return fd;
}

int handleLockFile(bool lock)
{
    int rv = 0;

    if (lock) {
        assert(!FLOCK);
        FLOCK = calloc(1, sizeof(struct flock));
        assert(FLOCK);
        FLOCK->l_type = F_WRLCK;
        FLOCK->l_whence = SEEK_SET;
        FLOCK->l_start = 0;
        FLOCK->l_len = 0;
        FLOCK->l_pid = getpid();
        if ((FD_LOCK = getLockFileFd()) == -1) {
            rv = 1;
            goto out;
        }
        if (DEBUG)
            logInfo(CONSOLE, "Try to get lock ...\n");
        if (fcntl(FD_LOCK, F_SETLKW, FLOCK) == -1) {
            rv = errno;
            logError(CONSOLE | SYSTEM, "src/unitlogd/file/file.c", "handleLockFile", errno,
                     strerror(errno), "Fcntl func returned -1 exit code (lock)");
            goto out;
        }
        if (DEBUG)
            logInfo(CONSOLE, "Got lock!\n");
    } else {
        assert(FLOCK);
        assert(FD_LOCK > 0);
        FLOCK->l_type = F_UNLCK; /* set to unlock same region */
        if (DEBUG)
            logInfo(CONSOLE, "Try to unlock ...\n");
        if (fcntl(FD_LOCK, F_SETLK, FLOCK) == -1) {
            rv = errno;
            logError(CONSOLE | SYSTEM, "src/unitlogd/file/file.c", "handleLockFile", errno,
                     strerror(errno), "Fcntl func returned -1 exit code (unlock)");
            goto out;
        }
        if (DEBUG)
            logInfo(CONSOLE, "Unlocked!\n");
    }

out:
    if (!lock || rv != 0) {
        close(FD_LOCK);
        objectRelease(&FLOCK);
    }
    return rv;
}

off_t getFileSize(const char *path)
{
    off_t ret = -1;
    struct stat st;

    assert(path);

    if (stat(path, &st) == -1) {
        logError(CONSOLE | SYSTEM, "src/unitlogd/file/file.c", "getFileSize", errno,
                 strerror(errno), "Stat func returned -1 exit code for '%s' file path", path);
    } else
        ret = st.st_size;

    return ret;
}

void writeKmsg(char *line)
{
    assert(line);

    unitlogdOpenKmsg("a");
    assert(UNITLOGD_KMSG_FILE);
    logEntry(&UNITLOGD_KMSG_FILE, line);
    logEntry(&UNITLOGD_KMSG_FILE, NEW_LINE);
    unitlogdCloseKmsg();
    assert(!UNITLOGD_KMSG_FILE);
}
