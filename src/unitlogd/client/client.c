/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

UlCommandData UL_COMMAND_DATA[] = {
    { SHOW_LOG, "show-log" },
    { LIST_BOOTS, "list-boots" }
};
int UL_COMMAND_DATA_LEN = 2;

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
showLogLines()
{
    int rv = 0;
    char *line = NULL;
    size_t len = 0;

    line = NULL;

    unitlogdOpenLog("r");
    assert(UNITLOGD_LOG_FILE);
    while (getline(&line, &len, UNITLOGD_LOG_FILE) != -1) {
        /* Discard index entries */
        if (!stringStartsWithStr(line, ENTRY_STARTED) && !stringStartsWithStr(line, ENTRY_FINISHED))
            printf("%s", line);
    }

    objectRelease(&line);
    unitlogdCloseLog();
    assert(!UNITLOGD_LOG_FILE);

    return rv;
}

int
sendToPager(int (*fn)())
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
        fn();
    }
    else { /* parent */
        char *args[] = { "less", NULL };
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
showLog(bool pager)
{
    int rv = 0;

    if (pager)
        rv = sendToPager(showLogLines);
    else
        rv = showLogLines();

    return rv;
}
