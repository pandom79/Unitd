/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

char *BOOT_ID_STR;
FILE *UNITLOGD_KMSG_FILE;

char *getBootIdStr()
{
    char str[BOOT_ID_SIZE] = { 0 };
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

int createKmsgLog()
{
    int rv = 0;

    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "UNITLOGD_KMSG_PATH", UNITLOGD_KMSG_PATH);
    arrayAdd(envVars, NULL);
    rv = execUlScript(&envVars, "create-kmsg-log");

    arrayRelease(&envVars);
    return rv;
}

void appendKmsg()
{
    char *line = NULL;
    size_t len = 0;

    unitlogdOpenKmsg("r");
    assert(UNITLOGD_KMSG_FILE);
    while (getline(&line, &len, UNITLOGD_KMSG_FILE) != -1)
        processLine(line);

    objectRelease(&line);
    unitlogdCloseKmsg();
    assert(!UNITLOGD_KMSG_FILE);
}

void assertMacros()
{
    assert(UNITLOGD_PATH);
    assert(UNITLOGD_DATA_PATH);
    assert(UNITLOGD_LOG_PATH);
    assert(UNITLOGD_INDEX_PATH);
    assert(UNITLOGD_LOCK_PATH);
    assert(UNITLOGD_KMSG_PATH);
}

int unitlogdInit()
{
    int rv = 0;
    IndexEntry *indexStartEntry = NULL;
    char *logOffset = NULL;

    /**
     * Index integrity check. If it fails then exit.
     * At this point, unitlogctl should be able to regenerate it reading from unitlogd.log
     * since it contains the same entries if the latter is not corrupt too, otherwise,
     * clear all and restart.
    */
    if ((rv = indexIntegrityCheck()) != 0) {
        setIndexErr(true);
        goto out;
    }
    BOOT_ID_STR = getBootIdStr();
    logOffset = getLogOffset();
    if (!logOffset) {
        rv = 1;
        goto out;
    }
    /* Creating the index start entry */
    indexStartEntry = indexEntryNew(true, BOOT_ID_STR);
    indexStartEntry->startOffset = logOffset;
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

out:
    unitlogdCloseLog();
    unitlogdCloseIndex();
    assert(!UNITLOGD_LOG_FILE);
    assert(!UNITLOGD_INDEX_FILE);
    indexEntryRelease(&indexStartEntry);
    return rv;
}

int unitlogdShutdown()
{
    int rv = 0;
    IndexEntry *indexStopEntry = NULL;
    char *logOffset = NULL;

    assert(!UNITLOGD_LOG_FILE);
    assert(!UNITLOGD_INDEX_FILE);
    logOffset = getLogOffset();
    if (!logOffset) {
        rv = 1;
        goto out;
    }
    /* Creating the index stop entry */
    indexStopEntry = indexEntryNew(false, BOOT_ID_STR);
    indexStopEntry->stopOffset = logOffset;
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
