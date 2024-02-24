/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef INDEX_H
#define INDEX_H

typedef enum {
    TYPE_ENTRY = 0,
    BOOT_ID = 1,
    TIME = 2,
    OFFSET = 3,
} IndexEnum;

IndexEntry *indexEntryNew(bool, const char *bootIdStr);
void indexEntryRelease(IndexEntry **);
int writeEntry(bool, IndexEntry *, bool);
int indexIntegrityCheck();
int getIndex(Array **, bool);
int getMaxIdx(Array **);
void setIndexErr(bool);

#endif // INDEX_H
