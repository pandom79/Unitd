/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

char *BOOT_ID_STR;
const char *DMESG_LOG_PATH;

char*
getBootIdStr()
{
    char str[BOOT_ID_SIZE] = {0};
    const char charset[] = BOOT_ID_CHARSET;
    int charsetSize = strlen(charset);

    srand(time(NULL));
    for (int n = 0; n < BOOT_ID_SIZE; n++) {
        int key = rand() % charsetSize;
        str[n] = charset[key];
    }
    str[BOOT_ID_SIZE] = '\0';

    return stringNew(str);
}

int
appendDmsg()
{
    int rv = 0, kernelInfo = LOG_KERN | LOG_INFO;
    FILE *fp = NULL;
    char *line = NULL;
    size_t len = 0;

    if ((fp = fopen(DMESG_LOG_PATH, "r")) == NULL) {
        logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/unitlogd/init/init.c", "appendDmsg",
                 errno, strerror(errno), "Unable to open '%s' file!", DMESG_LOG_PATH);
        rv = 1;
        return rv;
    }
    while (getline(&line, &len, fp) != -1) {
        char *buffer = stringNew("<");
        char kernelInfoStr[10] = {0};
        sprintf(kernelInfoStr, "%d", kernelInfo);
        assert(strlen(kernelInfoStr) > 0);
        stringConcat(&buffer, kernelInfoStr);
        stringConcat(&buffer, ">");
        stringConcat(&buffer, line);
        processLine(buffer);
        objectRelease(&buffer);
    }

    objectRelease(&line);
    fclose(fp);
    fp = NULL;
    return rv;
}

void
assertMacros()
{
    assert(UNITLOGD_PATH);
    assert(UNITLOGD_LOG_PATH);
    assert(UNITLOGD_INDEX_PATH);
    assert(UNITLOGD_BOOT_LOG_PATH);
}

int
unitlogdInit()
{
    int rv = 0;
    IndexEntry *indexStartEntry = NULL;
    char *logOffset = NULL;

    /* Index integrity check. If it fails then exit.
     * At this point, unitlogctl should be able to regenerate it reading from unitlogd.log
     * since it contains the same entries, obviously, if the latter is not corrupt too.
     * In this case, clear all and restart.
    */
    if ((rv = indexIntegrityCheck()) != 0) {
        logErrorStr(CONSOLE, "The file '%s' is corrupt!\n", UNITLOGD_INDEX_PATH);
        goto out;
    }

    /* Get boot id */
    BOOT_ID_STR = getBootIdStr();

    /* Get log offset */
    logOffset = getLogOffset();
    if (!logOffset) {
        rv = 1;
        goto out;
    }

    /* Creating the index start entry */
    indexStartEntry = indexEntryNew(true, BOOT_ID_STR);
    indexStartEntry->startOffset = logOffset;

    /* Open unitlogd and index in append mode because have been already created by init script */
    if (unitlogdOpenLog("a") != 0 || unitlogdOpenIndex("a") != 0) {
        rv = 1;
        goto out;
    }
    assert(UNITLOGD_LOG_FILE);
    assert(UNITLOGD_INDEX_FILE);

    /* Write the "start" index entry */
    if (writeEntry(true, indexStartEntry, true) != 0 ||
        writeEntry(true, indexStartEntry, false) != 0) {
        rv = 1;
        goto out;
    }

    /* Append dmesg */
    if ((rv = appendDmsg()) != 0)
        goto out;

    out:
        /* Close index and log */
        unitlogdCloseLog();
        unitlogdCloseIndex();
        assert(!UNITLOGD_LOG_FILE);
        assert(!UNITLOGD_INDEX_FILE);
        indexEntryRelease(&indexStartEntry);

        return rv;
}

int
unitlogdShutdown()
{
    int rv = 0;
    IndexEntry *indexStopEntry = NULL;
    char *logOffset = NULL;

    assert(!UNITLOGD_LOG_FILE);
    assert(!UNITLOGD_INDEX_FILE);

    /* Get log offset */
    logOffset = getLogOffset();
    if (!logOffset) {
        rv = 1;
        goto out;
    }

    /* Creating the index stop entry */
    indexStopEntry = indexEntryNew(false, BOOT_ID_STR);
    indexStopEntry->stopOffset = logOffset;

    /* Open unitlogd and index in append */
    if (unitlogdOpenLog("a") != 0 || unitlogdOpenIndex("a") != 0) {
        rv = 1;
        goto out;
    }
    assert(UNITLOGD_LOG_FILE);
    assert(UNITLOGD_INDEX_FILE);

    /* Write the "stop" index entry */
    rv = writeEntry(false, indexStopEntry, true);
    rv = writeEntry(false, indexStopEntry, false);

    out:
        unitlogdCloseLog();
        unitlogdCloseIndex();
        assert(!UNITLOGD_LOG_FILE);
        assert(!UNITLOGD_INDEX_FILE);
        indexEntryRelease(&indexStopEntry);

        return rv;
}
