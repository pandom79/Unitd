/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

UlCommandData UL_COMMAND_DATA[] = {
    { LIST_BOOTS, "list-boots" }
};
int UL_COMMAND_DATA_LEN = 1;

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
