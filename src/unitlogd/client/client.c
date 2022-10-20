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
    { INDEX_REPAIR, "index-repair" }
};
int UL_COMMAND_DATA_LEN = 4;

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
    return getIndex(bootsList, true);
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
        assert(idxStr);
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

    if (pager)
        rv = sendToPager(showLogLines, 0, -1);
    else
        rv = showLogLines(0, -1);

    if (follow)
        rv = followLog();

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

    /* Get the index */
    if ((rv = getIndex(&index, true)) != 0)
        goto out;

    indexSize = index ? index->size : 0;
    if (indexSize > 0) {
        /* Find the max idx */
        if ((indexSize % 2) == 0)
            idxMax = (indexSize - 2) / 2;
        else
            idxMax = (indexSize - 1) / 2;

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
            }
        }
        else
            error = true;

        if (error) {
            logErrorStr(CONSOLE, "The argument '%s' is not valid!\n", bootIdx);
            if (idxMax == 0)
                logInfo(CONSOLE, "Please, enter a value between (0..).\n");
            else
                logInfo(CONSOLE, "Please, enter a value between (0..%d).\n", idxMax);
            rv = 1;
            goto out;
        }

        if (pager)
            rv = sendToPager(showLogLines, startOffset, stopOffset);
        else
            rv = showLogLines(startOffset, stopOffset);

        if (follow)
            rv = followLog();
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
    if ((rv = getIndex(&index, false)) != 0)
        goto out;

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
        if (rv == 0)
            logSuccess(CONSOLE, "Index file repaired successfully.\n");
    }

    out:
        arrayRelease(&index);
        unitlogdCloseIndex();
        assert(!UNITLOGD_INDEX_FILE);
        return rv;
}
