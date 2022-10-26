/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

bool UNITLOGCTL_DEBUG;

UlCommandData UL_COMMAND_DATA[] = {
    { SHOW_LOG, "show-log" },
    { LIST_BOOTS, "list-boots" },
    { SHOW_BOOT, "show-boot" },
    { INDEX_REPAIR, "index-repair" },
    { VACUUM, "vacuum" }
};
int UL_COMMAND_DATA_LEN = 5;

UlCommand
getUlCommand(const char *commandName)
{
    assert(commandName);
    for (int i = 0; i < UL_COMMAND_DATA_LEN; i++) {
        if (strcmp(commandName, UL_COMMAND_DATA[i].name) == 0)
            return i;
    }
    return NO_UL_COMMAND;
}

bool
getSkipCheckAdmin(UlCommand ulCommand)
{
    switch (ulCommand) {
        case SHOW_LOG:
        case LIST_BOOTS:
        case SHOW_BOOT:
            return true;
        default:
            return false;
    }
}

int
getBootsList(Array **bootsList)
{
    int rv = 0;

    rv = getIndex(bootsList, true);
    if (rv != 0)
        setIndexErr(true);

    return rv;
}

int
showBootsList()
{
    int rv, len, idx;
    Array *bootsList = NULL;

    rv = len = idx = 0;

    if ((rv = getBootsList(&bootsList)) != 0)
        goto out;

    /* HEADER */
    printf("%s%s%s", WHITE_UNDERLINE_COLOR, "IDX", DEFAULT_COLOR);
    printf("%s%*s%s", WHITE_UNDERLINE_COLOR, PADDING, "", DEFAULT_COLOR);

    printf("%s%s%s", WHITE_UNDERLINE_COLOR, "BOOT ID", DEFAULT_COLOR);
    printf("%s%*s%s", WHITE_UNDERLINE_COLOR, BOOT_ID_SIZE - WIDTH_BOOT_ID + PADDING, "", DEFAULT_COLOR);

    printf("%s%s%s", WHITE_UNDERLINE_COLOR, "STARTED", DEFAULT_COLOR);
    printf("%s%*s%s", WHITE_UNDERLINE_COLOR, WIDTH_DATE - WIDTH_STARTED + PADDING, "", DEFAULT_COLOR);

    printf("%s%s%s", WHITE_UNDERLINE_COLOR, "FINISHED", DEFAULT_COLOR);
    printf("%s%*s%s", WHITE_UNDERLINE_COLOR, WIDTH_DATE - WIDTH_FINISHED, "", DEFAULT_COLOR);

    printf("\n");

    /* CELLS */
    len = bootsList ? bootsList->size : 0;
    for (int i = 0; i < len; i += 2) {
        char idxStr[50] = {0};
        IndexEntry *startEntry = arrayGet(bootsList, i);
        IndexEntry *stopEntry = arrayGet(bootsList, i + 1);
        char *timeStamp = NULL;

        /* Idx */
        sprintf(idxStr, "%d", idx);
        assert(strlen(idxStr) > 0);
        printf("%s%*s", idxStr, (WIDTH_IDX - (int)strlen(idxStr)) + PADDING, "");

        /* Boot id */
        printf("%s%*s", startEntry->bootId, PADDING, "");

        /* Start time */
        timeStamp = stringGetTimeStamp(startEntry->start, false, "%d-%m-%Y %H:%M:%S");
        printf("%s%*s", timeStamp, PADDING, "");
        objectRelease(&timeStamp);

        /* Stop time */
        if (stopEntry) {
            timeStamp = stringGetTimeStamp(stopEntry->stop, false, "%d-%m-%Y %H:%M:%S");
            printf("%s", timeStamp);
            objectRelease(&timeStamp);
        }
        else
            printf("-%*s", WIDTH_DATE - 1, "");

        /* Increasing idx */
        idx++;

        /* New line */
        printf("\n");
    }

    printf("\n%d boots found\n", idx);

    out:
        arrayRelease(&bootsList);
        unitlogdCloseIndex();
        assert(!UNITLOGD_INDEX_FILE);
        return rv;
}

int
showLogLines(off_t startOffset, off_t stopOffset)
{
    int rv = 0;
    char *line = NULL;
    size_t len = 0;
    off_t currentOffset = -1;

    assert(startOffset >= 0);

    /* Open log */
    unitlogdOpenLog("r");
    assert(UNITLOGD_LOG_FILE);

    /* Set start offset */
    if (fseeko(UNITLOGD_LOG_FILE, startOffset, SEEK_SET) == -1) {
        logError(UNITLOGD_BOOT_LOG, "src/unitlogd/client/client.c", "showLogLines", errno, strerror(errno),
                 "Fseeko func returned -1");
        rv = errno;
        goto out;
    }
    while (getline(&line, &len, UNITLOGD_LOG_FILE) != -1) {

        /* Check if we achieved the stop offset */
        if (stopOffset != -1) {
            if ((currentOffset = ftello(UNITLOGD_LOG_FILE)) == -1) {
                logError(UNITLOGD_BOOT_LOG, "src/unitlogd/client/client.c", "showLogLines", errno,
                         strerror(errno), "Ftello func returned -1");
                rv = errno;
                goto out;
            }
            currentOffset -= strlen(line);
            if (currentOffset > stopOffset)
                break;
        }
        /* Discard index entries */
        if (!stringStartsWithStr(line, ENTRY_STARTED) && !stringStartsWithStr(line, ENTRY_FINISHED))
            printf("%s", line);
    }

    out:
        objectRelease(&line);
        unitlogdCloseLog();
        assert(!UNITLOGD_LOG_FILE);

        return rv;
}

int
sendToPager(int (*fn)(off_t, off_t), off_t startOffset, off_t stopOffset)
{
    int rv = 0;
    int pfds[2];
    pid_t pid;

    /* Pipe */
    if ((rv = pipe(pfds)) < 0) {
        logError(CONSOLE | SYSTEM, "src/unitlogd/client/client.c", "sendToPager", errno, strerror(errno),
                 "Pipe function returned a bad exit code");
        goto out;
    }
    /* Fork */
    pid = fork();
    if (pid < 0) {
        rv = errno;
        logError(CONSOLE | SYSTEM, "src/unitlogd/client/client.c", "sendToPager", errno, strerror(errno),
                 "Fork function returned a bad exit code");
        goto out;
    }
    else if (pid == 0) { /* child */
        close(pfds[0]);
        dup2(pfds[1], STDOUT_FILENO);
        close(pfds[1]);
        fn(startOffset, stopOffset);
    }
    else { /* parent */
        /* For the debug, we show the line number */
        char *args[] = { "less", UNITLOGCTL_DEBUG ? "-N" : NULL, NULL };
        close(pfds[1]);
        dup2(pfds[0], STDIN_FILENO);
        close(pfds[0]);
        execvp(args[0], args);
        logError(CONSOLE | SYSTEM, "src/unitlogd/client/client.c", "sendToPager", errno, strerror(errno),
                 "Execvp error");
        rv = 1;
        goto out;
    }

    out:
        return rv;

}

int
followLog()
{
    int rv = 0;

    if (UNITLOGCTL_DEBUG)
        logInfo(CONSOLE, "\n\n-- Follow the log --\n\n");

    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "UNITLOGD_LOG_PATH", UNITLOGD_LOG_PATH);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);
    /* Exec script */
    rv = execUlScript(&envVars, "follow");

    arrayRelease(&envVars);
    return rv;
}

int
showLog(bool pager, bool follow)
{
    int rv = 0;

    if (follow) {
        rv = followLog();
        goto out;
    }

    if (pager)
        rv = sendToPager(showLogLines, 0, -1);
    else
        rv = showLogLines(0, -1);

    out:

        return rv;
}

int
showBoot(bool pager, bool follow, const char *bootIdx)
{
    int rv = 0, idx = -1, idxMax = -1, indexSize = 0;
    Array *index = NULL;
    bool error = false;
    off_t startOffset = 0, stopOffset = -1;

    assert(bootIdx);

    if (follow) {
        rv = followLog();
        goto out;
    }

    /* Get the index */
    if ((rv = getIndex(&index, true)) != 0) {
        setIndexErr(true);
        goto out;
    }

    indexSize = index ? index->size : 0;
    if (indexSize > 0) {
        /* Find the max idx */
        idxMax = getMaxIdx(&index);
        assert(idxMax != -1);

        if (isValidNumber(bootIdx, true)) {
            idx = atol(bootIdx);
            if (idx > idxMax)
                error = true;
            else {
                /* Get start and stop offset */
                IndexEntry *startIndexEntry = NULL;
                IndexEntry *stopIndexEntry = NULL;
                if (idx == 0) {
                    startIndexEntry = arrayGet(index, idx);
                    stopIndexEntry = arrayGet(index, idx + 1);
                }
                else {
                    startIndexEntry = arrayGet(index, idx * 2);
                    stopIndexEntry = arrayGet(index, idx * 2 + 1);
                }
                startOffset = atol(startIndexEntry->startOffset);
                if (stopIndexEntry)
                    stopOffset = atol(stopIndexEntry->stopOffset);

                if (UNITLOGCTL_DEBUG) {
                    logInfo(CONSOLE, "Start - Boot id = (%d - %s), Offset = %lu\n", idx, startIndexEntry->bootId, startOffset);
                    if (stopIndexEntry)
                        logInfo(CONSOLE, "Stop - Boot id = (%d - %s), Offset = %lu\n", idx, stopIndexEntry->bootId, stopOffset);
                }
            }
        }
        else
            error = true;

        if (error) {
            logErrorStr(CONSOLE, "The argument '%s' is not valid!\n", bootIdx);
            logInfo(CONSOLE, "Please, enter a value between (0%s%d).\n", RANGE_TOKEN, idxMax);
            rv = 1;
            goto out;
        }

        if (pager)
            rv = sendToPager(showLogLines, startOffset, stopOffset);
        else
            rv = showLogLines(startOffset, stopOffset);

    }
    else
        logWarning(CONSOLE, "The '%s' index file is empty!\n", UNITLOGD_INDEX_PATH);

    out:
        arrayRelease(&index);
        unitlogdCloseIndex();
        assert(!UNITLOGD_INDEX_FILE);

        return rv;
}

int
createIndexFile()
{
    int rv = 0;

    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "UNITLOGD_INDEX_PATH", UNITLOGD_INDEX_PATH);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);
    /* Exec script */
    rv = execUlScript(&envVars, "create-index");

    arrayRelease(&envVars);
    return rv;
}

int
indexRepair()
{
    int rv = 0, indexSize = 0;
    Array *index = NULL;
    IndexEntry *indexEntry = NULL;

    /* Get the index from log */
    if ((rv = getIndex(&index, false)) != 0) {
        setIndexErr(false);
        goto out;
    }

    /* Write the index entries */
    indexSize = index ? index->size : 0;
    if (indexSize > 0) {
        /* Create index file and set the owner and permissions */
        if ((rv = createIndexFile()) != 0)
            goto out;
        /* Open the index file in append mode */
        if (unitlogdOpenIndex("a") != 0) {
            rv = 1;
            goto out;
        }
        assert(UNITLOGD_INDEX_FILE);
        /* Writing a new index file content */
        for (int i = 0; i < indexSize; i++) {
            indexEntry = arrayGet(index, i);
            if (i % 2 == 0)
                rv = writeEntry(true, indexEntry, true);
            else
                rv = writeEntry(false, indexEntry, true);
            if (rv != 0)
                goto out;
        }
    }

    out:
        arrayRelease(&index);
        unitlogdCloseIndex();
        assert(!UNITLOGD_INDEX_FILE);
        return rv;
}

int
runTmpLogOperation(const char *operation)
{
    int rv = 0;

    assert(operation);

    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "UNITLOGD_LOG_PATH", UNITLOGD_LOG_PATH);
    addEnvVar(&envVars, "TMP_SUFFIX", TMP_SUFFIX);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);
    /* Exec script */
    rv = execUlScript(&envVars, operation);

    arrayRelease(&envVars);
    return rv;
}

int
cutLog(off_t startOffset, off_t stopOffset)
{
    int rv = 0;
    size_t len = 0;
    char *tmpLogPath, *line;
    FILE *tmpLogFp = NULL;
    off_t currentOffset = 0;

    assert(startOffset >= 0);
    assert(stopOffset > 0);

    tmpLogPath = line = NULL;

    /* Creating the path */
    tmpLogPath = stringNew(UNITLOGD_LOG_PATH);
    stringAppendStr(&tmpLogPath, TMP_SUFFIX);

    /* We create a temporary log file with the new content (UNITLOGD_LOG_PATH.tmp).
     * After that, we remove 'UNITLOGD_LOG_PATH' and
     * rename UNITLOGD_LOG_PATH.tmp -> UNITLOGD_LOG_PATH.
    */
    if ((rv = runTmpLogOperation("create-tmp-log")) != 0)
        goto out;

    tmpLogFp = fopen(tmpLogPath, "a");
    if (!tmpLogFp) {
        rv = errno;
        logError(CONSOLE | SYSTEM, "src/unitlogd/client/client.c", "cutLog",
                 errno, strerror(errno), "Unable to open '%s' in append mode!", tmpLogPath);
        goto out;

    }

    /* Open log */
    unitlogdOpenLog("r");
    assert(UNITLOGD_LOG_FILE);

    while (getline(&line, &len, UNITLOGD_LOG_FILE) != -1) {
        if ((currentOffset = ftello(UNITLOGD_LOG_FILE)) == -1) {
            rv = errno;
            logError(UNITLOGD_BOOT_LOG, "src/unitlogd/client/client.c", "cutLog", errno,
                     strerror(errno), "Ftello func returned -1");
            goto out;
        }
        currentOffset -= strlen(line);
        /* Discard the lines between start and stop offset */
        if (currentOffset < startOffset || currentOffset > stopOffset) {
            fprintf(tmpLogFp, line, NULL);
            fflush(tmpLogFp);
        }
    }
    /* Relase and close */
    objectRelease(&line);
    fclose(tmpLogFp);
    tmpLogFp = NULL;

    /* Rename the tmp log */
    rv = runTmpLogOperation("ren-tmp-log");

    out:
        objectRelease(&tmpLogPath);
        objectRelease(&line);
        if (tmpLogFp) {
            fclose(tmpLogFp);
            tmpLogFp = NULL;
        }
        unitlogdCloseLog();
        assert(!UNITLOGD_LOG_FILE);

        return rv;
}

int
vacuum(const char *bootIdx)
{
    Array *index, *idxArr;
    int rv = 0, startIdx, stopIdx, maxIdx;
    off_t startOffset, stopOffset, oldLogSize, newLogSize, diffLogSize;
    bool rangeErr = false;
    IndexEntry *indexEntry = NULL;

    startIdx = stopIdx = maxIdx = -1;
    startOffset = stopOffset = oldLogSize = newLogSize = diffLogSize = -1;
    index = idxArr = NULL;

    assert(bootIdx);

    /* Check input */
    if (stringContainsStr(bootIdx, RANGE_TOKEN)) {
        idxArr = stringSplitFirst((char *)bootIdx, RANGE_TOKEN);
        int len = idxArr ? idxArr->size : 0;
        assert(len == 2);
        const char *startIdxStr = arrayGet(idxArr, 0);
        const char *stopIdxStr = arrayGet(idxArr, 1);
        /* Check range value */
        if (isValidNumber(startIdxStr, true) && isValidNumber(stopIdxStr, true)) {
            startIdx = atol(startIdxStr);
            stopIdx = atol(stopIdxStr);
            if (startIdx == stopIdx)
                rangeErr = true;
        }
        else
            rangeErr = true;
        if (rangeErr) {
            rv = 1;
            logErrorStr(CONSOLE, "The argument '%s' is not valid!\n", bootIdx);
            logInfo(CONSOLE, "Please, enter a valid numeric range.\n");
            goto out;
        }
    }
    else {
        if (isValidNumber(bootIdx, true))
            startIdx = atol(bootIdx);
        else {
            rv = 1;
            logErrorStr(CONSOLE, "The argument '%s' is not valid!\n", bootIdx);
            logInfo(CONSOLE, "Please, enter a valid numeric value.\n");
            goto out;
        }
    }

    /* Get index */
    if ((rv = getIndex(&index, true)) != 0) {
        setIndexErr(true);
        goto out;
    }

    /* Get and check max idx */
    maxIdx = getMaxIdx(&index);
    if (maxIdx > 0) {
        if (startIdx >= maxIdx) {
            rv = 1;
            logErrorStr(CONSOLE, "The argument '%s' is not valid!\n", bootIdx);
            logInfo(CONSOLE, "Please, enter a value between (0%s%d).\n", RANGE_TOKEN, maxIdx - 1);
            goto out;
        }
        else if (stopIdx != -1 && stopIdx >= maxIdx) {
            rv = 1;
            logErrorStr(CONSOLE, "The argument '%s' is not valid!\n", bootIdx);
            logInfo(CONSOLE, "Please, enter a numeric range between (0%s%d).\n", RANGE_TOKEN, maxIdx - 1);
            goto out;
        }
    }
    else {
        rv = 1;
        logWarning(CONSOLE, "Sorry, no vacuuming operations can be performed at this time.\n");
        logInfo(CONSOLE, "Please, try again later.\n");
        goto out;
    }

    /* Get 'start' offset according startIdx  */
    if (startIdx == 0)
        indexEntry = arrayGet(index, startIdx);
    else
        indexEntry = arrayGet(index, startIdx * 2);
    assert(indexEntry);
    startOffset = atol(indexEntry->startOffset);
    if (UNITLOGCTL_DEBUG)
        logInfo(CONSOLE, "Boot id = %s, startOffset  = %lu\n", indexEntry->bootId, startOffset);

    /* Get 'stop' offset according stopIdx */
    if (stopIdx != -1)
        indexEntry = arrayGet(index, stopIdx * 2 + 1);
    else
        indexEntry = arrayGet(index, startIdx * 2 + 1);
    assert(indexEntry);
    stopOffset = atol(indexEntry->stopOffset);
    if (UNITLOGCTL_DEBUG)
        logInfo(CONSOLE, "Boot id = %s, stopOffset  = %lu\n", indexEntry->bootId, stopOffset);

    /* Lock
     * We cannot cut the log if the unitlog daemon is writing it!
     * See startUnixThread func in socket.c.
    */
    if ((rv = handleLockFile(true)) != 0)
        goto out;

    /* From this point, whatever error occurred, we don't exit because we must always unlock. */

    /* Get old log size */
    if ((oldLogSize = getFileSize(UNITLOGD_LOG_PATH)) != -1) {
        /* Cut the log */
        if ((rv = cutLog(startOffset, stopOffset)) == 0) {
            /* Repair the index from new log content */
            if ((rv = indexRepair()) == 0) {
                /* Get new log size */
                if ((newLogSize = getFileSize(UNITLOGD_LOG_PATH)) != -1) {
                    diffLogSize = oldLogSize - newLogSize;
                    if (UNITLOGCTL_DEBUG) {
                        logInfo(CONSOLE, "Previous log size = %lu\n", oldLogSize);
                        logInfo(CONSOLE, "New log size      = %lu\n", newLogSize);
                        logInfo(CONSOLE, "Diff log size     = %lu\n", diffLogSize);
                    }
                    char *oldLogSizeStr = stringGetFileSize(oldLogSize);
                    char *newLogSizeStr = stringGetFileSize(newLogSize);
                    char *diffLogSizeStr = stringGetFileSize(diffLogSize);
                    logSuccess(CONSOLE, "Vacuuming done successfully!\n\n");

                    logInfo(CONSOLE, "Previous size : ");
                    logSuccess(CONSOLE, "%s\n", oldLogSizeStr);

                    logInfo(CONSOLE, "Current size  : ");
                    logSuccess(CONSOLE, "%s\n", newLogSizeStr);

                    logInfo(CONSOLE, "Freed size    : ");
                    logSuccess(CONSOLE, "%s\n\n", diffLogSizeStr);

                    objectRelease(&oldLogSizeStr);
                    objectRelease(&newLogSizeStr);
                    objectRelease(&diffLogSizeStr);
                }
            }
        }
    }

    /* UnLock */
    rv = handleLockFile(false);

    out:
        arrayRelease(&idxArr);
        arrayRelease(&index);

        return rv;
}
