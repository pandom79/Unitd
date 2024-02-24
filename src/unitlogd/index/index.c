/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

IndexEntry *indexEntryNew(bool isStarting, const char *bootIdStr)
{
    IndexEntry *indexEntry = NULL;

    indexEntry = calloc(1, sizeof(IndexEntry));
    assert(indexEntry);
    if (bootIdStr) {
        indexEntry->bootId = stringNew(bootIdStr);
        assert(indexEntry->bootId);
    }
    if (isStarting) {
        indexEntry->start = timeNew(NULL);
        assert(indexEntry->start);
    } else {
        indexEntry->stop = timeNew(NULL);
        assert(indexEntry->stop);
    }

    return indexEntry;
}

void indexEntryRelease(IndexEntry **indexEntry)
{
    if (*indexEntry) {
        objectRelease(&(*indexEntry)->bootId);
        timeRelease(&(*indexEntry)->start);
        objectRelease(&(*indexEntry)->startOffset);
        timeRelease(&(*indexEntry)->stop);
        objectRelease(&(*indexEntry)->stopOffset);
        objectRelease(indexEntry);
    }
}

int getIndex(Array **index, bool isIndex)
{
    char *line = NULL, *bootId = NULL;
    FILE *fp = NULL;
    size_t len = 0;
    int rv = 0, numline = 0;
    Array *values = NULL;
    bool isStartEntry = false;

    /* Open file */
    if (isIndex) {
        unitlogdOpenIndex("r");
        assert(UNITLOGD_INDEX_FILE);
        fp = UNITLOGD_INDEX_FILE;
    } else {
        unitlogdOpenLog("r");
        assert(UNITLOGD_LOG_FILE);
        fp = UNITLOGD_LOG_FILE;
    }
    assert(fp);
    assert(!(*index));
    *index = arrayNew(indexEntryRelease);
    while (getline(&line, &len, fp) != -1) {
        IndexEntry *indexEntry = NULL;
        off_t offset = 0;
        numline++;
        /* For both index and log, the file must start with 'ENTRY_STARTED'.
         * After that, for the index case,
         * we have to necessary have a line which starts with 'ENTRY_FINISHED',
         * while for the log case, we go to the next line and so on ...
        */
        if (isIndex) {
            if (stringStartsWithStr(line, ENTRY_STARTED) && !isStartEntry)
                isStartEntry = true;
            else if (stringStartsWithStr(line, ENTRY_FINISHED) && isStartEntry)
                isStartEntry = false;
            else
                rv = 1;
        } else {
            if (numline >= 1 && !isStartEntry && stringStartsWithStr(line, ENTRY_STARTED))
                isStartEntry = true;
            else if (numline > 1 && isStartEntry && stringStartsWithStr(line, ENTRY_FINISHED))
                isStartEntry = false;
            else if (numline > 1 && isStartEntry && !stringStartsWithStr(line, ENTRY_FINISHED))
                continue;
            else
                rv = 1;
        }
        if (rv == 1) {
            logError(CONSOLE | UNITLOGD_BOOT_LOG | SYSTEM, "src/unitlogd/index/index.c", "getIndex",
                     rv, strerror(rv), "An error has occurred at %d line (%s).", numline,
                     isIndex ? "index" : "log");
            goto out;
        }
        /* Retrieve row offset when we get it from log */
        if (!isIndex) {
            if ((offset = ftello(fp)) == -1) {
                rv = 1;
                logError(CONSOLE | UNITLOGD_BOOT_LOG | SYSTEM, "src/unitlogd/index/index.c",
                         "getIndex", errno, strerror(errno), "Ftello func returned -1");
                goto out;
            }
            offset -= strlen(line);
        }
        values = stringSplit(line, TOKEN_ENTRY, true);
        for (IndexEnum indexEnum = TYPE_ENTRY; indexEnum <= OFFSET; indexEnum++) {
            char *value = NULL;
            /* Get the value */
            value = arrayGet(values, indexEnum);
            if (value)
                stringTrim(value, NULL);
            switch (indexEnum) {
            case TYPE_ENTRY:
                if (isStartEntry)
                    indexEntry = indexEntryNew(true, NULL);
                else
                    indexEntry = indexEntryNew(false, NULL);
                arrayAdd(*index, indexEntry);
                break;
            case BOOT_ID:
                if (isStartEntry)
                    bootId = stringNew(value);
                /* The stop entry 'bootId' value must match the
                 * 'bootId' value of the just previous start entry */
                if (!isStartEntry && !stringEquals(value, bootId)) {
                    rv = 1;
                    logError(
                        CONSOLE | UNITLOGD_BOOT_LOG | SYSTEM, "src/unitlogd/index/index.c",
                        "getIndex", rv, strerror(rv),
                        "The '%s' bootId at line %d doesn't match the previous '%s' bootId at line %d",
                        value, numline, bootId, numline - 1);
                    goto out;
                }
                indexEntry->bootId = stringNew(value);
                break;
            case TIME:
                if (isValidNumber(value, true)) {
                    Time *currentTime = timeNew(NULL);
                    *currentTime->sec = atol(value);
                    if (isStartEntry)
                        timeSet(&indexEntry->start, currentTime);
                    else
                        timeSet(&indexEntry->stop, currentTime);
                    timeRelease(&currentTime);
                } else {
                    rv = 1;
                    logError(CONSOLE | UNITLOGD_BOOT_LOG | SYSTEM, "src/unitlogd/index/index.c",
                             "getIndex", rv, strerror(rv),
                             "The timestamp '%s' at line '%d' is not valid", value, numline);
                    goto out;
                }
                break;
            case OFFSET:
                if (isIndex) {
                    if (isValidNumber(value, true)) {
                        if (isStartEntry)
                            stringSet(&indexEntry->startOffset, value);
                        else
                            stringSet(&indexEntry->stopOffset, value);
                    } else {
                        rv = 1;
                        logError(CONSOLE | UNITLOGD_BOOT_LOG | SYSTEM, "src/unitlogd/index/index.c",
                                 "getIndex", rv, strerror(rv),
                                 "The offset '%s' at line '%d' is not valid", value, numline);
                        goto out;
                    }
                } else {
                    char offsetStr[50] = { 0 };
                    sprintf(offsetStr, "%lu", offset);
                    if (isStartEntry)
                        stringSet(&indexEntry->startOffset, offsetStr);
                    else
                        stringSet(&indexEntry->stopOffset, offsetStr);
                }
                break;
            }
        }
        if (!isStartEntry)
            objectRelease(&bootId);
        arrayRelease(&values);
    }

out:
    arrayRelease(&values);
    objectRelease(&bootId);
    objectRelease(&line);
    if (isIndex) {
        unitlogdCloseIndex();
        assert(!UNITLOGD_INDEX_FILE);
    } else {
        unitlogdCloseLog();
        assert(!UNITLOGD_LOG_FILE);
    }
    return rv;
}

int writeEntry(bool isStarting, IndexEntry *indexEntry, bool isIndex)
{
    int rv = 0;
    char *buffer = NULL, timeStr[50] = { 0 };

    assert(isIndex ? UNITLOGD_INDEX_FILE : UNITLOGD_LOG_FILE);

    /* Building the buffer */
    buffer = stringNew(isStarting ? ENTRY_STARTED : ENTRY_FINISHED);
    if (isStarting)
        /* For the indentation */
        stringAppendStr(&buffer, " ");
    /* BOOT ID */
    stringAppendStr(&buffer, TOKEN_ENTRY);
    stringAppendStr(&buffer, indexEntry->bootId);
    stringAppendStr(&buffer, TOKEN_ENTRY);
    if (isStarting) {
        /* START TIME */
        sprintf(timeStr, "%lu", *indexEntry->start->sec);
        stringAppendStr(&buffer, timeStr);
        /* START OFFSET */
        if (isIndex) {
            stringAppendStr(&buffer, TOKEN_ENTRY);
            stringAppendStr(&buffer, indexEntry->startOffset);
        }
    } else {
        /* STOP TIME */
        sprintf(timeStr, "%lu", *indexEntry->stop->sec);
        stringAppendStr(&buffer, timeStr);
        /* STOP OFFSET */
        if (isIndex) {
            stringAppendStr(&buffer, TOKEN_ENTRY);
            stringAppendStr(&buffer, indexEntry->stopOffset);
        }
    }
    /* NEW LINE */
    stringAppendStr(&buffer, NEW_LINE);
    logEntry(isIndex ? &UNITLOGD_INDEX_FILE : &UNITLOGD_LOG_FILE, buffer);

    /* Release resources */
    objectRelease(&buffer);
    return rv;
}

int indexIntegrityCheck()
{
    int rv = 0, len = 0;
    Array *index = NULL;
    IndexEntry *indexEntry = NULL;
    bool isStart = false;
    char *bootId = NULL;

    /* Get array from index */
    if ((rv = getIndex(&index, true)) != 0)
        goto out;
    /* For each index entry must be there a log entry according the offset value */
    unitlogdOpenLog("r");
    assert(UNITLOGD_LOG_FILE);
    len = index ? index->size : 0;
    for (int i = 0; i < len; i++) {
        isStart = false;
        indexEntry = arrayGet(index, i);
        if ((i % 2) == 0)
            isStart = true;
        if (!matchLogLine(isStart, indexEntry)) {
            rv = 1;
            goto out;
        }
    }
    unitlogdCloseLog();
    assert(!UNITLOGD_LOG_FILE);
    /* If len is odd, it means that unitlog daemon has not been properly stopped.
     * In this case, we append the index 'stop' entry to index and log file before to start.
    */
    if (len > 0 && (len % 2) != 0) {
        /* Get the last bootId */
        bootId = ((IndexEntry *)arrayGet(index, index->size - 1))->bootId;
        /* Populate a new index/log entry */
        indexEntry = indexEntryNew(false, bootId);
        indexEntry->stopOffset = getLogOffset();
        if (!indexEntry->stopOffset) {
            rv = 1;
            goto err;
        }
        /* Open log and index */
        unitlogdOpenLog("a");
        assert(UNITLOGD_LOG_FILE);
        unitlogdOpenIndex("a");
        assert(UNITLOGD_INDEX_FILE);
        /* Write the index entry */
        if (writeEntry(false, indexEntry, true) != 0 || writeEntry(false, indexEntry, false) != 0) {
            rv = 1;
            goto err;
        }
        indexEntryRelease(&indexEntry);
    }
    goto out;

err:
    indexEntryRelease(&indexEntry);

out:
    arrayRelease(&index);
    unitlogdCloseLog();
    assert(!UNITLOGD_LOG_FILE);
    unitlogdCloseIndex();
    assert(!UNITLOGD_INDEX_FILE);
    return rv;
}

int getMaxIdx(Array **index)
{
    int max = -1;
    int len = (*index) ? (*index)->size : 0;
    if (len > 0) {
        if ((len % 2) == 0)
            max = (len - 2) / 2;
        else
            max = (len - 1) / 2;
    }

    return max;
}

void setIndexErr(bool isIndex)
{
    if (isIndex) {
        logErrorStr(CONSOLE | SYSTEM, "The index file '%s' is corrupt!\n", UNITLOGD_INDEX_PATH);
        logInfo(CONSOLE | SYSTEM, "Please, run 'unitlogctl index-repair' to repair it.\n");
    } else {
        logErrorStr(CONSOLE | SYSTEM, "The log file '%s' is corrupt!\n", UNITLOGD_LOG_PATH);
        logInfo(CONSOLE | SYSTEM,
                "Please, run the following steps:\n"
                "[1] Stop unitlog daemon.\n"
                "[2] Run 'rm -rf %s'.\n"
                "[3] Restart the system.\n",
                UNITLOGD_PATH);
    }
}
